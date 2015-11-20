#include <mkt/log.h>
#include <mkt/commands.h>
#include <mkt/exceptions.h>
#include <mkt/threads.h>
#include <mkt/vars.h>
#include <mkt/modules.h>
#include <mkt/echo.h>

#include <boost/current_function.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/bind.hpp>

#include <sstream>

// This module's exceptions
namespace mkt
{
  MKT_DEF_EXCEPTION(log_error);
}

// This module's static data
namespace
{
  struct log_data
  {
    log_data() : _next_serial(0) {}

    mkt::log_entry_queues      _log_entry_queues;
    mkt::log_entry_serial_map  _log_entry_serial_map;
    mkt::uint64                _next_serial;
    
    //Big-lock on logging
    //TODO: break it down when it matters
    mkt::mutex                 _log_mutex;
  };
  log_data                    *_log_data = 0;
  bool                         _log_atexit = false;

  void _log_cleanup()
  {
    _log_atexit = true;
    delete _log_data;
    _log_data = 0;
  }

  log_data* _get_log_data()
  {
    if(_log_atexit)
      throw mkt::log_error("Already at program exit!");

    if(!_log_data)
      {
	_log_data = new log_data;
	std::atexit(_log_cleanup);
      }

    if(!_log_data)
      {
	throw mkt::log_error("error allocating static data");
      }

    return _log_data;
  }

  mkt::log_entry_queues& log_entry_queues_ref()
  {
    return _get_log_data()->_log_entry_queues;
  }

  mkt::log_entry_serial_map& log_entry_serial_map_ref()
  {
    return _get_log_data()->_log_entry_serial_map;
  }

  mkt::mutex& log_mutex_ref()
  {
    return _get_log_data()->_log_mutex;
  }

  mkt::uint64 _get_log_next_serial()
  {
    return _get_log_data()->_next_serial;
  }

  void _log_inc_serial()
  {
    _get_log_data()->_next_serial++;
  }

  // return current next log serial number and increment
  mkt::uint64 _next_serial()
  {
    mkt::uint64 ret = _get_log_next_serial();
    _log_inc_serial();
    return ret;
  }
}

// This module's commands
namespace
{
  void get_logs_cmd(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost;
    using namespace mkt;
    thread_info ti(BOOST_CURRENT_FUNCTION);

    if(args.size()<1) throw log_error("Missing arguments.");
    argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string
    if(local_args.size()>0)
      trim(local_args[0]);
    if(local_args.size()>1)
      trim(local_args[1]);
    if(local_args.size()>2)
      trim(local_args[2]);

    mkt_str queue = local_args.size()>0 ? 
      local_args[0] : 
      ".*";
    ptime begin = local_args.size()>1 ?
      str_to_ptime(local_args[1]) :
      ptime(gregorian::date(1970, 1, 1));
    ptime end = local_args.size()>2 ?
      str_to_ptime(local_args[2]) :
      posix_time::microsec_clock::universal_time();

    stringstream ss;
    log_entries_ptr le_p = get_logs(queue, begin, end);
    if(!le_p) throw log_error("Missing log entries!");
    BOOST_FOREACH(log_entries::value_type cur, *le_p)
      {
	if(!cur.second) continue;
	log_entry& le = *cur.second;
	ss << str(format("{%1%}, {%2%}, {%3%}, {%4%}")
		  % le.get<3>()                  // log queue
		  % ptime_to_str(cur.first)      // datetime
		  % mkt::thread_key(le.get<2>()) // thread of origination
		  % le.get<4>())                 // serial number
	   << endl;
      }
    ret_val(ss.str());
  }

  void get_log_queues_cmd(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost;
    using namespace mkt;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    
    argument_vector queues = get_log_queues();
    mkt_str q_str = join(queues);
    ret_val(q_str);
  }

  void log_cmd(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost;
    using namespace mkt;
    thread_info ti(BOOST_CURRENT_FUNCTION);

    if(args.size()<3) throw mkt::log_error("Missing arguments.");
    argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string
    trim(local_args[0]);
    mkt_str queue = local_args[0];
    local_args.erase(local_args.begin());
    mkt_str message = join(local_args);
    log(queue, message);
  }

  void get_log_cmd(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost;
    using namespace mkt;
    thread_info ti(BOOST_CURRENT_FUNCTION);

    if(args.size()<2) throw mkt::log_error("Missing arguments.");
    argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string
    trim(local_args[0]);
    uint64 serial_no = string_cast<uint64>(local_args[0]);
    log_entry_ptr le_p = get_log(serial_no);
    ret_val(le_p ?
	    mkt_str(str(format("%1%: {%2%}, "
			       "%3%: {%4%}, "
			       "%5%: {%6%}, "
			       "%7%: {%8%}")
			% "queue" % le_p->get<3>()
			% "thread_key" % thread_key(le_p->get<2>())
			% "serial" % le_p->get<4>()
			% "msg" % le_p->get<0>())) :
	    mkt_str());
  }
}

// Log API implementation
namespace mkt
{
  MKT_DEF_MAP_CHANGE_SIGNAL(log_queue_changed);

  void var_changed_slot(const mkt_str& varname, const mkt_str& t_key)
  {
    using namespace boost;
    log("vars",str(format("%1%: {%2%}, {t_key: %3%}")
		   % varname
		   % get_var(varname)
		   % t_key));
  }

  void command_slot(const mkt_str& queue,
		    const argument_vector& cmd,
		    const mkt_str& t_key)
  {
    using namespace boost;
    log(queue,str(format("%1%: {%2%}")
		  % t_key
		  % join(cmd)));
  }

  void thread_exception_slot(const mkt_str& t_key,
			     const mkt_str& ex_s)
  {
    using namespace boost;
    log("threads",str(format("%1%: Exception: %2%")
		      % t_key
		      % ex_s));
  }

  void thread_init_slot(const mkt_str& caller_key,
			const mkt_str& this_key)
  {
    using namespace boost;
    log("threads",str(format("thread_initialized: %1% -> %2%")
		      % caller_key
		      % this_key));
  }

  void thread_final_slot(const mkt_str& caller_key,
			 const mkt_str& this_key)
  {
    using namespace boost;
    log("threads",str(format("thread_finalized: %1% -> %2%")
		      % caller_key
		      % this_key));
  }

  template<class Key_Type>
  class map_change_log_slot
  {
  public:
    map_change_log_slot(const mkt_str& queue_str = mkt_str("default"),
			const mkt_str& slot_str = mkt_str()) : 
      _queue_str(queue_str),
      _str(slot_str) {}
    void operator()(const Key_Type& changed_key)
    {
      using namespace boost;
      log(_queue_str, str(format("%1% %2%")
			  % _str
			  % changed_key));
    }
    bool operator==(const map_change_log_slot& rhs) const
    {
      if(_queue_str == rhs._queue_str && _str == rhs._str) return true;
      return false;
    }
  private:
    mkt_str _queue_str;
    mkt_str _str;
  };
  typedef map_change_log_slot<mkt_str> mcls_str;
  typedef map_change_log_slot<int64> mcls_int64;

#define MKT_MCLS_CONNECT(module, slot_name, key_type)			      \
  slot_name().connect(map_change_log_slot<key_type>(#module, #slot_name))
#define MKT_MCLS_DISCONNECT(module, slot_name, key_type)		      \
  slot_name().disconnect(map_change_log_slot<key_type>(#module, #slot_name))

#define MKT_MCLS_STR_CONNECT(module, slot_name) 	\
  MKT_MCLS_CONNECT(module, slot_name, mkt_str)
#define MKT_MCLS_STR_DISCONNECT(module, slot_name)	\
  MKT_MCLS_DISCONNECT(module, slot_name, mkt_str)

  void init_log()
  {
    using namespace std;
    using namespace mkt;
    
    add_command("get_logs", get_logs_cmd, 
		"get_logs [queue regex] [begin time] [end time]\n"
		"Returns all logs between begin time and end time.\n"
		"[queue regex] is a regular expression used to select which log "
		"queue to read from.\n"
		"If begin time isn't specified, the posix time epoch is assumed.\n"
		"If end time isn't specified, 'now' is assumed.\n"
		"Default is '.*'");
    add_command("get_log", get_log_cmd,
		"get_log [serial_no]\n"
		"Returns the log entry with the specified serial number.");
    add_command("log_queues", get_log_queues_cmd,
		"Returns a list of log queues");
    add_command("log", log_cmd, 
		"log <queue name> <message>\n"
		"Posts a message to the specified message queue.");

    var_changed().connect(var_changed_slot);
    MKT_MCLS_STR_CONNECT(commands, command_added);
    MKT_MCLS_STR_CONNECT(commands, command_removed);
    MKT_MCLS_STR_CONNECT(modules, modules_changed);
    MKT_MCLS_STR_CONNECT(modules, module_pre_init);
    MKT_MCLS_STR_CONNECT(modules, module_post_init);
    MKT_MCLS_STR_CONNECT(modules, module_pre_final);
    MKT_MCLS_STR_CONNECT(modules, module_post_final);
    MKT_MCLS_CONNECT(echo, echo_function_registered, int64);
    command_pre_exec().connect(boost::bind(command_slot,"command_pre_exec",
					   _1, _2));
    command_post_exec().connect(boost::bind(command_slot,"command_post_exec",
					    _1, _2));
    MKT_MCLS_STR_CONNECT(threads, threads_changed);
    thread_exception().connect(thread_exception_slot);
    thread_initialized().connect(thread_init_slot);
    thread_finalized().connect(thread_final_slot);
  }

  void final_log()
  {
    remove_command("get_logs");
    remove_command("get_log_queues");
    remove_command("log");

    var_changed().disconnect(var_changed_slot);
    MKT_MCLS_STR_DISCONNECT(commands, command_added);
    MKT_MCLS_STR_DISCONNECT(commands, command_removed);
    MKT_MCLS_STR_DISCONNECT(modules, modules_changed);
    MKT_MCLS_STR_DISCONNECT(modules, module_pre_init);
    MKT_MCLS_STR_DISCONNECT(modules, module_post_init);
    MKT_MCLS_STR_DISCONNECT(modules, module_pre_final);
    MKT_MCLS_STR_DISCONNECT(modules, module_post_final);
    MKT_MCLS_DISCONNECT(echo, echo_function_registered, int64);
    command_pre_exec().disconnect(boost::bind(command_slot,"command_pre_exec",
					      _1, _2));
    command_post_exec().disconnect(boost::bind(command_slot,"command_post_exec",
					       _1, _2));
    MKT_MCLS_STR_DISCONNECT(threads, threads_changed);
    thread_exception().disconnect(thread_exception_slot);
    thread_initialized().disconnect(thread_init_slot);
    thread_finalized().disconnect(thread_final_slot);
  }

  void log(const mkt_str& queue, const mkt_str& message, const any& data)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    check_identifier<mkt::log_error>(queue);

    {
      unique_lock lock(log_mutex_ref());
      log_entry_queues& leq = log_entry_queues_ref();
      
      if(!leq[queue]) leq[queue].reset(new log_entries);
      log_entries& le = *leq[queue];

      ptime cur_time = boost::posix_time::microsec_clock::universal_time();
      log_entry_ptr le_p(new log_entry(message, data,
				       boost::this_thread::get_id(),
				       queue,
				       _next_serial()));
      le.insert(log_entries::value_type(cur_time, le_p));
      log_entry_serial_map_ref()[le_p->get<4>()] = le_p;
    }

    log_queue_changed()(queue);
  }

  log_entries_ptr get_logs(const mkt_str& queue_regex,
			   const ptime& begin, 
			   const ptime& end)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);

    log_entries_ptr ret;
    ret.reset(new log_entries);
    argument_vector queues = get_log_queues();

    BOOST_FOREACH(mkt_str& cur, queues)
      {
	if(!matches(cur, queue_regex)) continue;
	else
	  {
	    unique_lock lock(log_mutex_ref());
	    log_entry_queues& leq = log_entry_queues_ref();
	    log_entries& le = *leq[cur];
	    log_entries::iterator earliest = le.lower_bound(begin);
	    log_entries::iterator latest = le.upper_bound(end);
	    ret->insert(earliest, latest);
	  }
      }

    return ret;
  }

  argument_vector get_log_queues()
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    argument_vector queues;
    {
      unique_lock lock(log_mutex_ref());
      log_entry_queues& leq = log_entry_queues_ref();
      BOOST_FOREACH(const log_entry_queues::value_type& cur, leq)
	{
	  queues.push_back(cur.first);
	}
    }
    return queues;
  }

  log_entry_ptr get_log(uint64 serial_no)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    log_entry_ptr out_val;

    // Avoid creating new entries in the map by
    // first checking if an entry with the requested
    // serial_no exists.
    {
      shared_lock lock(log_mutex_ref());
      if(log_entry_serial_map_ref().find(serial_no) ==
	 log_entry_serial_map_ref().end())
	return out_val;
    }

    {
      unique_lock lock(log_mutex_ref());
      out_val = log_entry_serial_map_ref()[serial_no];
    }
    return out_val;
  }
  
  bool log_at_exit() { return _log_atexit; }
}
