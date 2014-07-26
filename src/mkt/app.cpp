#include <mkt/config.h>
#include <mkt/app.h>
#include <mkt/exceptions.h>

#include <boost/foreach.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>

#include <set>
#include <iostream>
#include <cstdlib>

//Commands related code
namespace
{
  mkt::command_map     commands;
  boost::mutex         _commands_lock;

  void help(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace mkt;

    argument_vector prog_args = argv();
    std::string prog_name = !prog_args.empty() ? prog_args[0] : "mkt";

    cout << "Version: " << version() << endl;
    cout << "Usage: " << prog_name << " <command> <command args>" << endl << endl;
    BOOST_FOREACH(command_map::value_type& cmd, commands)
      {
        cout << " - " << cmd.first << endl;
        cout << cmd.second.get<1>() << endl << endl;
      }
  }

  void hello(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace mkt;
    if(args.size()<2) cout << "Hello, world!" << endl;
    else if(args.size()==2) cout << "Hello, " << args[1] << endl;
    else throw command_line_error("Too many arguments");
  }

  class init_commands
  {
  public:
    init_commands()
    {
      using namespace std;
      using boost::str;
      using boost::format;
      using boost::make_tuple;
      
      commands["hello"] =
        boost::make_tuple(mkt::command_func(hello),
                          string("Prints hello world."));
      commands["help"] = 
        boost::make_tuple(mkt::command_func(help),
                          string("Prints command list."));
    }
  } init_commands_static_init;
}

namespace
{
  mkt::argument_vector        _av;
  boost::mutex                _av_lock;

  mkt::thread_map             _threads;
  mkt::thread_progress_map    _thread_progress;
  mkt::thread_key_map         _thread_keys;
  mkt::thread_info_map        _thread_info;
  boost::mutex                _threads_mutex;

  //This should only be called after we lock the threads mutex
  void update_thread_keys()
  {
    using namespace std;
    using namespace mkt;

    _thread_keys.clear();

    set<boost::thread::id> infoIds;
    BOOST_FOREACH(thread_info_map::value_type val, _thread_info)
      infoIds.insert(val.first);

    set<boost::thread::id> currentIds;
    BOOST_FOREACH(thread_map::value_type val, _threads)
      {
        thread_ptr ptr = val.second;
        if(ptr)
          {
            _thread_keys[ptr->get_id()]=val.first;

            //set the thread info to a default state if not already set
            if(_thread_info[ptr->get_id()].empty())
              _thread_info[ptr->get_id()] = "running";
          }

        currentIds.insert(ptr->get_id());
      }

    //compute thread ids that need to be removed from the threadInfo map
    set<boost::thread::id> infoIdsToRemove;
    set_difference(infoIds.begin(), infoIds.end(),
                   currentIds.begin(), currentIds.end(),
                   inserter(infoIdsToRemove,infoIdsToRemove.begin()));

    BOOST_FOREACH(boost::thread::id tid, infoIdsToRemove)
      _thread_info.erase(tid);
  }

  class wait_for_threads
  {
  public:
    wait_for_threads()
    {
      //Register a call to wait for all child threads to finish before exiting
      //the main thread.
      std::atexit(wait_for_threads::wait);
    }

    static void wait()
    {
      //Wait for all the threads to finish
      mkt::thread_map map = mkt::threads();
      BOOST_FOREACH(mkt::thread_map::value_type val, map)
        {
          try
            {
              std::cout << BOOST_CURRENT_FUNCTION
                        << " :: "
                        << "waiting for thread " << val.first
                        << std::endl;
              
              val.second->join();
            }
          catch(boost::thread_interrupted&) {}
        }
    }
  } wait_for_threads_static_init;
}

namespace mkt
{
  std::string version()
  {
    return std::string(MKT_VERSION);
  }
  
  void exec(const argument_vector& args)
  {
    using namespace boost;
    boost::mutex::scoped_lock lock(_commands_lock);
    if(args.empty()) throw command_line_error("Missing command string");
    std::string cmd = args[0];
    if(commands.find(cmd)==commands.end())
      throw command_line_error(str(format("Invalid command: %1%") % cmd));
    commands[cmd].get<0>()(args);
  }

  argument_vector argv()
  {
    boost::mutex::scoped_lock lock(_av_lock);
    return _av;
  }

  void argv(const argument_vector& av)
  {
    boost::mutex::scoped_lock lock(_av_lock);
    _av = av;
  }

  void argv(int argc, char **argv)
  {
    mkt::argument_vector args;
    for(int i = 0; i < argc; i++)
      args.push_back(argv[i]);
    mkt::argv(args);
  }

  /*
   * Thread API
   */
  map_change_signal threads_changed;

  thread_map threads()
  {
    boost::this_thread::interruption_point();
    boost::mutex::scoped_lock lock(_threads_mutex);
    return _threads;
  }

  thread_ptr threads(const std::string& key)
  {
    boost::this_thread::interruption_point();
    boost::mutex::scoped_lock lock(_threads_mutex);
    if(_threads.find(key)!=_threads.end())
      return _threads[key];
    return thread_ptr();
  }

  void threads(const std::string& key, const thread_ptr& val)
  {
    boost::this_thread::interruption_point();

    if(has_thread(key))
      threads(key)->interrupt();

    {
      boost::mutex::scoped_lock lock(_threads_mutex);
      if(!val)
        _threads.erase(key);
      else
        _threads[key] = val;
      update_thread_keys();
    }

    threads_changed(key);
  }

  void threads(const thread_map& map)
  {
    boost::this_thread::interruption_point();
    {
      boost::mutex::scoped_lock lock(_threads_mutex);
      
      BOOST_FOREACH(thread_map::value_type t, _threads)
	if(t.second) t.second->interrupt();

      _threads = map;
      _thread_progress.clear();
      update_thread_keys();
    }

    threads_changed("all");
  }

  bool has_thread(const std::string& key)
  {
    boost::this_thread::interruption_point();
    boost::mutex::scoped_lock lock(_threads_mutex);
    if(_threads.find(key)!=_threads.end())
      return true;
    return false;
  }

  double thread_progress(const std::string& key)
  {
    boost::this_thread::interruption_point();
    boost::mutex::scoped_lock lock(_threads_mutex);

    boost::thread::id tid;
    if(key.empty())
      tid = boost::this_thread::get_id();
    else if(_threads.find(key)!=_threads.end() &&
            _threads[key])
      tid = _threads[key]->get_id();
    else
      return 0.0;
    
    return _thread_progress[tid];
  }

  void thread_progress(double progress)
  {
    thread_progress(std::string(),progress);
  }

  void thread_progress(const std::string& key, double progress)
  {
    boost::this_thread::interruption_point();

    //clamp progress to [0.0,1.0]
    progress = progress < 0.0 ? 
      0.0 : 
      progress > 1.0 ? 1.0 :
      progress;

    bool changed = false;
    {
      boost::mutex::scoped_lock lock(_threads_mutex);
      boost::thread::id tid;

      if(key.empty())
        {
          tid = boost::this_thread::get_id();
          changed = true;
        }
      else if(_threads.find(key)!=_threads.end() &&
              _threads[key])
        {
          tid = _threads[key]->get_id();
          changed = true;
        }
      
      if(changed)
        _thread_progress[tid]=progress;
    }

    if(changed)
      threads_changed(key);
  }

  void finish_thread_progress(const std::string& key)
  {
    boost::this_thread::interruption_point();
    {
      boost::mutex::scoped_lock lock(_threads_mutex);

      boost::thread::id tid;
      if(key.empty())
        tid = boost::this_thread::get_id();
      else if(_threads.find(key)!=_threads.end() &&
              _threads[key])
        tid = _threads[key]->get_id();
      else
        return;

      _thread_progress.erase(tid);
    }
    threads_changed(key);
  }

  std::string thread_key()
  {
    boost::this_thread::interruption_point();
    {
      boost::mutex::scoped_lock lock(_threads_mutex);
      if(_thread_keys.find(boost::this_thread::get_id())!=_thread_keys.end())
        return _thread_keys[boost::this_thread::get_id()];
      else
        return std::string("unknown");
    }
  }

  void remove_thread(const std::string& key)
  {
    threads(key,thread_ptr());
  }

  std::string unique_thread_key(const std::string& hint)
  {
    std::string h = hint.empty() ? "thread" : hint;
    //Make a unique key name to use by adding a number to the key
    std::string uniqueThreadKey = h;
    unsigned int i = 0;
    while(has_thread(uniqueThreadKey))
      uniqueThreadKey = 
        h + boost::lexical_cast<std::string>(i++);
    return uniqueThreadKey;
  }

  void set_thread_info(const std::string& key, const std::string& infostr)
  {
    boost::this_thread::interruption_point();
    {
      boost::mutex::scoped_lock lock(_threads_mutex);

      boost::thread::id tid;
      if(key.empty())
        tid = boost::this_thread::get_id();
      else if(_threads.find(key)!=_threads.end() &&
              _threads[key])
        tid = _threads[key]->get_id();
      else
        return;

      _thread_info[tid] = infostr;
    }
    threads_changed(key);
  }

  std::string get_thread_info(const std::string& key)
  {
    boost::this_thread::interruption_point();
    {
      boost::mutex::scoped_lock lock(_threads_mutex);
      
      boost::thread::id tid;
      if(key.empty())
        tid = boost::this_thread::get_id();
      else if(_threads.find(key)!=_threads.end() &&
              _threads[key])
        tid = _threads[key]->get_id();
      else
        return std::string();

      return _thread_info[tid];
    }
  }

  void this_thread_info(const std::string& infostr)
  {
    set_thread_info(std::string(),infostr);
  }
  
  std::string this_thread_info()
  {
    return get_thread_info();
  }

  thread_info::thread_info(const std::string& info)
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

  thread_feedback::thread_feedback(const std::string& info)
    : _thread_info(info)
  {
    thread_progress(0.0);
  }

  thread_feedback::~thread_feedback()
  {
    finish_thread_progress();
    remove_thread(thread_key());
  }

  void sleep(double ms)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    boost::xtime xt;
    boost::xtime_get( &xt, boost::TIME_UTC_ );
    xt.nsec += uint64(ms * std::pow(10.0,6.0));
    boost::thread::sleep( xt );
  }
}
