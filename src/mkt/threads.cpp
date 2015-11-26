#include <mkt/threads.h>
#include <mkt/app.h>
#include <mkt/echo.h>
#include <mkt/commands.h>
#include <mkt/exceptions.h>
#include <mkt/vars.h>
#include <mkt/utils.h>

#include <boost/foreach.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/regex.hpp>
#include <boost/format.hpp>

#include <set>
#include <sstream>

//This module's exceptions
namespace mkt
{
  MKT_DEF_EXCEPTION(async_error);
  MKT_DEF_EXCEPTION(thread_error);
}

//This module's static data
namespace
{
  struct threads_data
  {
    mkt::thread_map                 _threads;
    mkt::thread_progress_map        _thread_progress;
    mkt::thread_key_map             _thread_keys;
    mkt::thread_info_map            _thread_info;
    mkt::mutex                      _threads_mutex;
  };
  threads_data*                     _threads_data = 0;
  bool                              _threads_atexit = false;
  typedef mkt::mkt_str mkt_str;
  const mkt_str                     _threads_default_keyname("unknown");
  const mkt_str                     _threads_main_thread_key("main_thread");
  mkt::thread_id                    _threads_main_thread_id = boost::this_thread::get_id();

  void _threads_cleanup()
  {
    _threads_atexit = true;
    delete _threads_data;
    _threads_data = 0;
  }

  threads_data* _get_threads_data()
  {
    if(_threads_atexit)
      throw mkt::thread_error("Already at program exit!");

    if(!_threads_data)
      {
        _threads_data = new threads_data;
        std::atexit(_threads_cleanup);
      }

    if(!_threads_data) 
      throw mkt::thread_error("Missing static thread data!");
    return _threads_data;
  }

  mkt::thread_map& threads_ref()
  {
    return _get_threads_data()->_threads;
  }

  mkt::thread_progress_map& thread_progress_ref()
  {
    return _get_threads_data()->_thread_progress;
  }

  mkt::thread_key_map& thread_keys_ref()
  {
    return _get_threads_data()->_thread_keys;
  }

  mkt::thread_info_map& thread_info_ref()
  {
    return _get_threads_data()->_thread_info;
  }

  mkt::mutex& threads_mutex_ref()
  {
    return _get_threads_data()->_threads_mutex;
  }
  
  //This should only be called after we lock the threads mutex
  void update_thread_keys()
  {
    using namespace std;
    using namespace mkt;

    thread_keys_ref().clear();

    std::set<thread_id> infoIds;
    BOOST_FOREACH(thread_info_map::value_type val, thread_info_ref())
      infoIds.insert(val.first);

    std::set<thread_id> currentIds;
    BOOST_FOREACH(thread_map::value_type val, threads_ref())
      {
        thread_ptr ptr = val.second;
        if(ptr)
          {
            thread_keys_ref()[ptr->get_id()]=val.first;

            //set the thread info to a default state if not already set
            if(thread_info_ref()[ptr->get_id()].empty())
              thread_info_ref()[ptr->get_id()] = "running";

	    currentIds.insert(ptr->get_id());
          }
      }

    //compute thread ids that need to be removed from the threadInfo map
    std::set<thread_id> infoIdsToRemove;
    set_difference(infoIds.begin(), infoIds.end(),
                   currentIds.begin(), currentIds.end(),
                   inserter(infoIdsToRemove,infoIdsToRemove.begin()));

    BOOST_FOREACH(thread_id tid, infoIdsToRemove)
      thread_info_ref().erase(tid);

    // set id -> key mapping and thread info for main_thread
    thread_keys_ref()[_threads_main_thread_id] = _threads_main_thread_key;
    if(thread_info_ref()[_threads_main_thread_id].empty())
      thread_info_ref()[_threads_main_thread_id] = "running";
  }
}

//Threading related commands
namespace
{
  //launches a command in a thread
  void async(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove command argument
    if(local_args.empty())
      throw mkt::async_error("No command to execute.");
    
    //check if the next argument is 'wait.'  If so, set the wait flag true
    bool wait = false;
    if(local_args[0] == "wait")
      {
        wait = true;
        local_args.erase(local_args.begin()); //remove "wait"
      }
    
    mkt_str local_args_str = boost::join(local_args," ");
    mkt::start_thread(local_args_str,
                      boost::bind(mkt::exec, local_args),
                      wait);
  }

  void async_file(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost::algorithm;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove command string
    if(local_args.empty()) throw mkt::command_error("Missing file name.");

    mkt::exec_file(local_args, true);
  }

  void parallel(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string to form the thread name

    //split the argument vector into separate command strings via 'and' keywords
    vector<mkt::argument_vector> split_args;
    mkt::argument_vector cur_command;
    BOOST_FOREACH(const string& cur, local_args)
      {
        if(cur != "and")
          cur_command.push_back(cur);
        else
          {
            split_args.push_back(cur_command);
            cur_command.clear();
          }
      }

    if(!cur_command.empty())
      split_args.push_back(cur_command);
    else if(split_args.empty())
      throw mkt::command_error("Missing command to execute.");
    
    //now execute the split commands in parallel
    BOOST_FOREACH(const mkt::argument_vector& cur, split_args)
      {
        mkt::argument_vector local_cur = cur;
        local_cur.insert(local_cur.begin(), "async");
	mkt::exec(local_cur);
      }
  }

  void interrupt(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    if(args.size()<2) throw mkt::command_error("Missing thread name.");

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string to form the thread name
    mkt_str thread_name = 
      boost::algorithm::join(local_args," ");

    mkt::thread_ptr thread = mkt::threads(thread_name);
    if(thread) thread->interrupt();
    else throw mkt::system_error("Null thread pointer.");
  }

  void list_threads(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    const mkt::thread_map& tm = mkt::threads();
    bool expand_info = false;

    //check for expand_info keyword after command arg
    if(args.size()>=2 && args[1]=="expand_info")
      expand_info = true;
    
    stringstream ss;

    // get number of active threads
    size_t t_count = 0;
    BOOST_FOREACH(const mkt::thread_map::value_type& cur, tm)
      if(cur.second) t_count++;

    size_t t_cur = 0;
    BOOST_FOREACH(const mkt::thread_map::value_type& cur, tm)
      if(cur.second)
        {
          string ti = mkt::get_thread_info(cur.first);

          if(!expand_info)
            {
              //captures most function looking strings as output by BOOST_CURRENT_FUNCTION
              regex expr("(\\S+\\(?\\W*\\)?)\\((.*)\\)");
              match_results<string::iterator> what;
              match_flag_type flags = match_default;
              try
                {
                  if(regex_search(ti.begin(), ti.end(), what, expr, flags))
                    ti = string(what[1]);
                }
              catch(...){}
            }

	  ss << (t_cur==0?"\n":"")
	     << boost::str(boost::format("{%1%} {%2%} {%3%}")
			   % cur.first
			   % cur.second->get_id()
			   % ti)
	     << (t_count>1?",\n":"");

	  t_cur++;
        }

    mkt::ret_val(ss.str());
  }

  void sleep_cmd(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    if(args.size()<2) 
      throw mkt::command_error("Missing argument for sleep");
    mkt::sleep(mkt::string_cast<int64_t>(args[1]));
  }
}

/*
 * Thread API
 */
namespace mkt
{
  MKT_DEF_MAP_CHANGE_SIGNAL(threads_changed);
  MKT_DEF_MAP_CHANGE_SIGNAL(thread_progress_changed);
  MKT_DEF_MAP_CHANGE_SIGNAL(thread_info_changed);
  MKT_DEF_SIGNAL(thread_call_signal, thread_initialized);
  MKT_DEF_SIGNAL(thread_call_signal, thread_finalized);
  MKT_DEF_SIGNAL(thread_exception_signal, thread_exception);

  void init_threads()
  {
    add_command("async", ::async,
		"Executes a command in another thread and"
		" returns immediately. If 'wait' is before\n"
		"the command, this command will execute only after"
		" a command with the same thread name is finished running.");
    add_command("async_file", ::async_file,
		"Executes commands listed in a file in parallel.");
    add_command("parallel", ::parallel,
		"Executes a series of commands in parallel, separated by an 'and' keyword.");
    add_command("interrupt", ::interrupt, "Interrupts a running thread.");
    add_command("threads", ::list_threads, "Lists running threads by name.");
    add_command("sleep", sleep_cmd, "sleep <milliseconds>\nSleep for the time specified.");

    // add thread_map entry for the main thread
    threads(threads_main_thread_key(), thread_ptr());
  }

  void final_threads()
  {
    remove_command("async");
    remove_command("async_file");
    remove_command("parallel");
    remove_command("interrupt");
    remove_command("threads");
    remove_command("sleep");
    
    // remove threads, interrupting them
    arg_vec t_keys = thread_keys();
    BOOST_FOREACH(const mkt_str& key, t_keys)
      remove_thread(key);

    // remove entry for main_thread
    remove_thread(threads_main_thread_key());
  }

  const mkt_str& threads_default_keyname()
  {
    return _threads_default_keyname;
  }

  const mkt_str& threads_main_thread_key()
  {
    return _threads_main_thread_key;
  }

  thread_map threads()
  {
    boost::this_thread::interruption_point();
    unique_lock lock(threads_mutex_ref());
    return threads_ref();
  }

  thread_ptr threads(const mkt_str& key)
  {
    boost::this_thread::interruption_point();
    unique_lock lock(threads_mutex_ref());
    if(threads_ref().find(key)!=threads_ref().end())
      return threads_ref()[key];
    return thread_ptr();
  }

  void threads(const mkt_str& key, const thread_ptr& val)
  {
    boost::this_thread::interruption_point();

    if(has_thread(key) && threads(key))
      {
	std::cout << BOOST_CURRENT_FUNCTION << ": interrupting " << key << std::endl;
	threads(key)->interrupt();
      }

    {
      unique_lock lock(threads_mutex_ref());
      threads_ref()[key] = val;
      update_thread_keys();
    }

    threads_changed()(key);
  }

  void threads(const thread_map& map)
  {
    boost::this_thread::interruption_point();
    arg_vec t_keys = thread_keys();
    std::set<mkt_str> t_keys_set(t_keys.begin(), t_keys.end());

    {
      unique_lock lock(threads_mutex_ref());
      
      BOOST_FOREACH(thread_map::value_type t, threads_ref())
	if(t.second) t.second->interrupt();
      
      threads_ref() = map;
      thread_progress_ref().clear();
      update_thread_keys();
    }

    // get any new keys
    t_keys = thread_keys();
    t_keys_set.insert(t_keys.begin(), t_keys.end());

    BOOST_FOREACH(const mkt_str& key, t_keys_set)
      threads_changed()(key);
  }

  bool has_thread(const mkt_str& key)
  {
    boost::this_thread::interruption_point();
    unique_lock lock(threads_mutex_ref());
    if(threads_ref().find(key)!=threads_ref().end())
      return true;
    return false;
  }

  double thread_progress(const mkt_str& key)
  {
    boost::this_thread::interruption_point();
    unique_lock lock(threads_mutex_ref());

    thread_id tid;
    if(key.empty())
      tid = boost::this_thread::get_id();
    else if(threads_ref().find(key)!=threads_ref().end() &&
            threads_ref()[key])
      tid = threads_ref()[key]->get_id();
    else
      return 0.0;
    
    return thread_progress_ref()[tid];
  }

  void thread_progress(double progress)
  {
    thread_progress(mkt_str(),progress);
  }

  void thread_progress(const mkt_str& key, double progress)
  {
    boost::this_thread::interruption_point();

    if(wait_for_threads::at_start() ||
       wait_for_threads::at_exit())
      return;

    //clamp progress to [0.0,1.0]
    progress = progress < 0.0 ? 
      0.0 : 
      progress > 1.0 ? 1.0 :
      progress;

    bool changed = false;
    {
      unique_lock lock(threads_mutex_ref());
      thread_id tid;

      if(key.empty())
        {
          tid = boost::this_thread::get_id();
          changed = true;
        }
      else if(threads_ref().find(key)!=threads_ref().end() &&
              threads_ref()[key])
        {
          tid = threads_ref()[key]->get_id();
          changed = true;
        }
      
      if(changed)
        thread_progress_ref()[tid]=progress;
    }

    if(changed)
      thread_progress_changed()(key);
  }

  void finish_thread_progress(const mkt_str& key)
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(threads_mutex_ref());

      thread_id tid;
      if(key.empty())
        tid = boost::this_thread::get_id();
      else if(threads_ref().find(key)!=threads_ref().end() &&
              threads_ref()[key])
        tid = threads_ref()[key]->get_id();
      else
        return;

      thread_progress_ref().erase(tid);
    }
    thread_progress_changed()(key);
  }

  mkt_str thread_key(thread_id tid)
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(threads_mutex_ref());
      if(thread_keys_ref().find(tid)!=thread_keys_ref().end())
        return thread_keys_ref()[tid];
      else
        return mkt_str(threads_default_keyname());
    }
  }

  arg_vec thread_keys()
  {
    boost::this_thread::interruption_point();
    arg_vec ret_val;
    {
      unique_lock lock(threads_mutex_ref());
      BOOST_FOREACH(thread_map::value_type& t, threads_ref())
	ret_val.push_back(t.first);
    }
    return ret_val;
  }

  void remove_thread(const mkt_str& key, bool do_interrupt)
  {
    boost::this_thread::interruption_point();

    if(do_interrupt && has_thread(key) && threads(key))
      {
	std::cout << BOOST_CURRENT_FUNCTION << ": interrupting " << key << std::endl;
	threads(key)->interrupt();
      }

    {
      unique_lock lock(threads_mutex_ref());
      threads_ref().erase(key);
      update_thread_keys();
    }

    threads_changed()(key);
  }

  mkt_str unique_thread_key(const mkt_str& hint)
  {
    mkt_str h = hint.empty() ? "thread" : hint;
    //Make a unique key name to use by adding a number to the key
    mkt_str uniqueThreadKey = h;
    unsigned int i = 1;
    while(has_thread(uniqueThreadKey))
      uniqueThreadKey = h + 
	"__" + 
	boost::lexical_cast<mkt_str>(i++) +
	"__";
    return uniqueThreadKey;
  }

  void set_thread_info(const mkt_str& key, const mkt_str& infostr)
  {
    boost::this_thread::interruption_point();

    if(wait_for_threads::at_start() ||
       wait_for_threads::at_exit()) return;

    {
      unique_lock lock(threads_mutex_ref());

      thread_id tid;
      if(key.empty())
        tid = boost::this_thread::get_id();
      else if(threads_ref().find(key)!=threads_ref().end() &&
              threads_ref()[key])
        tid = threads_ref()[key]->get_id();
      else
        return;

      thread_info_ref()[tid] = infostr;
    }

    thread_info_changed()(key);
  }

  mkt_str get_thread_info(const mkt_str& key)
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(threads_mutex_ref());
      
      thread_id tid;
      if(key.empty())
        tid = boost::this_thread::get_id();
      else if(threads_ref().find(key)!=threads_ref().end() &&
              threads_ref()[key])
        tid = threads_ref()[key]->get_id();
      else
        return mkt_str();

      return thread_info_ref()[tid];
    }
  }

  void this_thread_info(const mkt_str& infostr)
  {
    set_thread_info(mkt_str(),infostr);
  }
  
  mkt_str this_thread_info()
  {
    return get_thread_info();
  }

  thread_info::thread_info(const mkt_str& info)
  {
    _orig_info = this_thread_info();
    _orig_progress = thread_progress();
    this_thread_info(info);
  }

  thread_info::~thread_info()
  {
    this_thread_info(_orig_info);
    thread_progress(_orig_progress);
  }

  thread_feedback::thread_feedback(const mkt_str& info)
    : _info(info)
  {
    thread_progress(0.0);
  }

  thread_feedback::~thread_feedback()
  {
    finish_thread_progress();
    remove_thread(thread_key(), false);
  }

  wait_for_threads::wait_for_threads()
  {
    _at_start = false;
  }

  wait_for_threads::~wait_for_threads()
  {
    _at_exit = true;
    wait();
  }

  bool wait_for_threads::_at_start = true;
  bool wait_for_threads::_at_exit  = false;

  bool wait_for_threads::at_start() { return _at_start; }
  bool wait_for_threads::at_exit()  { return _at_exit; }

  void wait_for_threads::wait()
  {
    //Wait for all the threads to finish
    mkt::thread_map map = mkt::threads();
    BOOST_FOREACH(mkt::thread_map::value_type val, map)
      {
        try
          {
            mkt::out().stream() << BOOST_CURRENT_FUNCTION
                      << " :: "
                      << "waiting for thread " << val.first
                      << std::endl;
            
            if(val.second)
              val.second->join();
          }
        catch(boost::thread_interrupted&) {}
      }
  }

  void sleep(int64 ms)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    boost::this_thread::sleep( boost::posix_time::milliseconds(ms) );
  }

  bool threads_at_exit() { return _threads_atexit; }
}
