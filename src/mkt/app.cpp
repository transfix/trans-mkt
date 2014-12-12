#include <mkt/app.h>
#include <mkt/xmlrpc.h>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/regex.hpp>
#include <boost/array.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/xtime.hpp>

#include <set>
#include <iostream>
#include <cstdlib>

namespace
{
  mkt::argument_vector        _av;
  mkt::mutex                  _av_lock;

  mkt::thread_map             _threads;
  mkt::thread_progress_map    _thread_progress;
  mkt::thread_key_map         _thread_keys;
  mkt::thread_info_map        _thread_info;
  mkt::mutex                  _threads_mutex;

  mkt::variable_map           _var_map;
  mkt::mutex                  _var_map_mutex;

  mkt::echo_map               _echo_map;
  mkt::mutex                  _echo_map_mutex;

  //This should only be called after we lock the threads mutex
  void update_thread_keys()
  {
    using namespace std;
    using namespace mkt;

    _thread_keys.clear();

    std::set<boost::thread::id> infoIds;
    BOOST_FOREACH(thread_info_map::value_type val, _thread_info)
      infoIds.insert(val.first);

    std::set<boost::thread::id> currentIds;
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
    std::set<boost::thread::id> infoIdsToRemove;
    set_difference(infoIds.begin(), infoIds.end(),
                   currentIds.begin(), currentIds.end(),
                   inserter(infoIdsToRemove,infoIdsToRemove.begin()));

    BOOST_FOREACH(boost::thread::id tid, infoIdsToRemove)
      _thread_info.erase(tid);
  }
}

namespace mkt
{
  std::string version()
  {
    return std::string(MKT_VERSION);
  }

  argument_vector split(const std::string& args)
  {
    using namespace std;
    using namespace boost::algorithm;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    mkt::argument_vector av;
    split(av, args, is_any_of(" "), token_compress_on);
    //remove empty strings
    mkt::argument_vector av_clean;
    BOOST_FOREACH(const string& cur, av)
      if(!cur.empty()) av_clean.push_back(cur);
    return av_clean;
  }

  std::string join(const argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    return boost::join(args," ");
  }

  argument_vector argv()
  {
    using namespace boost;
    argument_vector av;
    int argc = mkt::var<int>("__argc");
    for(int i = 0; i < argc; i++)
      av.push_back(mkt::var(str(format("__argv_%1%") % i)));
    return av;
  }

  void argv(int argc, char **argv)
  {
    using namespace boost;
    mkt::argument_vector args;
    mkt::var("__argc", argc);
    for(int i = 0; i < argc; i++)
      mkt::var(str(format("__argv_%1%") % i), argv[i]);
  }

  /*
   * Thread API
   */
  map_change_signal threads_changed;

  //use this to trigger the threads_changed signal because
  //problems happen when signals are envoked during program exit.
  void trigger_threads_changed(const std::string& key)
  {
    bool ae = wait_for_threads::at_exit();
    if(!ae)
      threads_changed(key);
  }

  thread_map threads()
  {
    boost::this_thread::interruption_point();
    unique_lock lock(_threads_mutex);
    return _threads;
  }

  thread_ptr threads(const std::string& key)
  {
    boost::this_thread::interruption_point();
    unique_lock lock(_threads_mutex);
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
      unique_lock lock(_threads_mutex);
      if(!val)
        _threads.erase(key);
      else
        _threads[key] = val;
      update_thread_keys();
    }

    trigger_threads_changed(key);
  }

  void threads(const thread_map& map)
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(_threads_mutex);
      
      BOOST_FOREACH(thread_map::value_type t, _threads)
	if(t.second) t.second->interrupt();

      _threads = map;
      _thread_progress.clear();
      update_thread_keys();
    }

    trigger_threads_changed("all");
  }

  bool has_thread(const std::string& key)
  {
    boost::this_thread::interruption_point();
    unique_lock lock(_threads_mutex);
    if(_threads.find(key)!=_threads.end())
      return true;
    return false;
  }

  double thread_progress(const std::string& key)
  {
    boost::this_thread::interruption_point();
    unique_lock lock(_threads_mutex);

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
      unique_lock lock(_threads_mutex);
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
      trigger_threads_changed(key);
  }

  void finish_thread_progress(const std::string& key)
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(_threads_mutex);

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
    trigger_threads_changed(key);
  }

  std::string thread_key()
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(_threads_mutex);
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
      unique_lock lock(_threads_mutex);

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
    trigger_threads_changed(key);
  }

  std::string get_thread_info(const std::string& key)
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(_threads_mutex);
      
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
    : _info(info)
  {
    thread_progress(0.0);
  }

  thread_feedback::~thread_feedback()
  {
    finish_thread_progress();
    remove_thread(thread_key());
  }

  wait_for_threads::~wait_for_threads()
  {
    _at_exit = true;
    wait();
  }

  bool wait_for_threads::at_exit() { return _at_exit; }

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

  bool wait_for_threads::_at_exit = false;

  void sleep(int64 ms)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    boost::this_thread::sleep( boost::posix_time::milliseconds(ms) );
  }

  std::string var(const std::string& varname)
  {
    bool creating = false;
    std::string val;
    {
      unique_lock lock(_var_map_mutex);
      if(_var_map.find(varname)==_var_map.end()) creating = true;
      val = _var_map[varname];
    }
    
    if(creating) var_changed(varname);
    return val;
  }

  void var(const std::string& varname, const std::string& val)
  {
    using namespace boost::algorithm;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(_var_map_mutex);
      std::string local_varname(varname); trim(local_varname);
      std::string local_val(val); trim(local_val);
      if(_var_map[local_varname] == local_val) return; //nothing to do
      _var_map[local_varname] = local_val;
    }
    var_changed(varname);
  }

  void unset_var(const std::string& varname)
  {
    using namespace boost::algorithm;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(_var_map_mutex);
      std::string local_varname(varname); trim(local_varname);
      _var_map.erase(local_varname);
    }
    var_changed(varname);
  }

  bool has_var(const std::string& varname)
  {
    using namespace boost::algorithm;
    unique_lock lock(_var_map_mutex);
    thread_info ti(BOOST_CURRENT_FUNCTION);    
    std::string local_varname(varname); trim(local_varname);
    if(_var_map.find(local_varname)==_var_map.end())
      return false;
    else return true;
  }

  argument_vector list_vars()
  {
    unique_lock lock(_var_map_mutex);
    thread_info ti(BOOST_CURRENT_FUNCTION);
    argument_vector vars;
    BOOST_FOREACH(const variable_map::value_type& cur, _var_map)
      {
        std::string cur_varname = cur.first;
        boost::algorithm::trim(cur_varname);
        vars.push_back(cur_varname);
      }
    return vars;
  }

  map_change_signal var_changed;

  argument_vector expand_vars(const argument_vector& args)
  {
    using namespace std;
    using namespace boost;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    argument_vector local_args(args);

    boost::array<regex, 2> exprs = 
      { 
        regex("\\W*\\$(\\w+)\\W*"), 
        regex("\\$\\{(\\w+)\\}") 
      };

    BOOST_FOREACH(string& arg, local_args)
      {
        BOOST_FOREACH(regex& expr, exprs)
          {
            match_results<string::iterator> what;
            match_flag_type flags = match_default;
            try
              {
                if(regex_search(arg.begin(), arg.end(), what, expr, flags))
                  arg = var(string(what[1])); //do the expansion
              }
            catch(...){}
          }
      }

    return local_args;
  }

  argument_vector split_vars(const argument_vector& args)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    argument_vector local_args(args);

    //TODO: look for special keyword 'split' and split the argument right afterward into an argument
    //vector and add it to the overall vector.  This way variables that represent commands can be split
    //into an argument vector and executed.

    return local_args;
  }

  void echo_register(int64 id, const echo_func& f)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    unique_lock lock(_echo_map_mutex);
    _echo_map[id] = f;
  }

  void echo_unregister(int64 id)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    unique_lock lock(_echo_map_mutex);
    _echo_map[id] = echo_func();
  }

  void echo(const std::string& str)
  {
    using namespace std;
    using namespace boost;
    using namespace boost::algorithm;

    thread_info ti(BOOST_CURRENT_FUNCTION);

    const string echo_functions_varname("__echo_functions");
    string echo_functions_value = var(echo_functions_varname);
    mkt::argument_vector echo_functions_strings;
    split(echo_functions_strings, echo_functions_value, is_any_of(","), token_compress_on);

    //execute each echo function
    BOOST_FOREACH(string& echo_function_id_string, echo_functions_strings)
      {
        trim(echo_function_id_string);
        if(echo_function_id_string.empty()) continue;

        try
          {
            uint64 echo_function_id = lexical_cast<uint64>(echo_function_id_string);
            echo(echo_function_id, str);
          }
        catch(const bad_lexical_cast &)
          {
            throw system_error(boost::str(format("Invalid echo_function_id %1%")
                                          % echo_function_id_string));                
          }
      }
  }

  void echo(uint64 echo_function_id, const std::string& str)
  {
    using namespace std;
    using namespace boost;

    thread_info ti(BOOST_CURRENT_FUNCTION);
    unique_lock lock(_echo_map_mutex);
    if(_echo_map[echo_function_id])
      _echo_map[echo_function_id](str);
  }

  //the default echo function
  void do_echo(std::ostream* is, const std::string& str)
  {
    using namespace boost;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    if(is && !var<bool>("__quiet")) (*is) << str;
  }

  void init_echo()
  {
    //setup echo so it outputs to console
    echo_register(0, boost::bind(mkt::do_echo, &std::cout, _1));
    echo_register(1, boost::bind(mkt::do_echo, &std::cerr, _1));
#ifdef MKT_USING_XMLRPC
    echo_register(2, mkt::do_remote_echo);
#endif
    var("__echo_functions", "0, 1, 2");
  }
}
