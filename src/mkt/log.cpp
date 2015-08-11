#include <mkt/log.h>
#include <mkt/commands.h>
#include <mkt/exceptions.h>
#include <mkt/threads.h>
#include <mkt/vars.h>

#include <boost/current_function.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/trim.hpp>

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
    mkt::log_entry_queues      _log_entry_queues;
    
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

  mkt::mutex& log_mutex_ref()
  {
    return _get_log_data()->_log_mutex;
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

    ptime begin = local_args.size()>0 ?
      str_to_ptime(local_args[0]) :
      ptime(gregorian::date(1970, 1, 1));
    ptime end = local_args.size()>1 ?
      str_to_ptime(local_args[1]) :
      posix_time::microsec_clock::universal_time();
    mkt_str queue = local_args.size()>2 ? 
      local_args[2] : 
      ".*";

    stringstream ss;
    log_entries_ptr le_p = get_logs(begin, end, queue);
    if(!le_p) return;
    BOOST_FOREACH(log_entries::value_type cur, *le_p)
      {
	if(!cur.second) continue;
	log_entry& le = *cur.second;
	ss << str(format("\"%1%\", \"%2%\", \"%3%\"")
		  % ptime_to_str(cur.first)
		  % mkt::thread_key(le.get<2>())
		  % le.get<0>())
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
}

// Log API implementation
namespace mkt
{
  void var_changed_slot(const mkt_str& varname, const var_context& context)
  {
    using namespace boost;
    log("vars",str(format("variable changed: %1% %2% %3% %4%")
		   % varname 
		   % context.stack_depth() 
		   % context.key().get<0>()
		   % context.key().get<1>()));
  }

  class map_change_log_slot
  {
  public:
    map_change_log_slot(const mkt_str& queue_str = mkt_str("default"),
			const mkt_str& slot_str = mkt_str()) : 
      _queue_str(queue_str),
      _str(slot_str) {}
    void operator()(const mkt_str& changed_key)
    {
      using namespace boost;
      log(_queue_str, str(format("%1% %2%")
			  % _str
			  % changed_key));
    }
  private:
    mkt_str _queue_str;
    mkt_str _str;
  };

  void init_log()
  {
    using namespace std;
    using namespace mkt;
    
    add_command("get_logs", get_logs_cmd, 
		"get_logs [begin time] [end time] [queue regex]\n"
		"Returns all logs between begin time and end time. "
		"If begin time isn't specified, the posix time epoch is assumed. "
		"If end time isn't specified, 'now' is assumed. [queue regex] is a "
		"regular expression used to select which log queue to read from. "
		"Default is '.*'");
    add_command("get_log_queues", get_log_queues_cmd,
		"Returns a list of log queues");
    add_command("log", log_cmd, 
		"log <queue name> <message>\n"
		"Posts a message to the specified message queue.");

    var_changed().connect(var_changed_slot);
    command_added().
      connect(map_change_log_slot("commands","command_added"));
    command_removed().
      connect(map_change_log_slot("commands","command_removed"));
  }

  void final_log()
  {

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
				       boost::this_thread::get_id()));
      le.insert(log_entries::value_type(cur_time, le_p));
    }
  }

  log_entries_ptr get_logs(const ptime& begin, const ptime& end, const mkt_str& queue_regex)
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
  
  bool log_at_exit() { return _log_atexit; }
}
