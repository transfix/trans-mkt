#include <mkt/threads.h>
#include <mkt/app.h>
#include <mkt/echo.h>
#include <mkt/exceptions.h>

#include <boost/foreach.hpp>

namespace mkt
{
  MKT_DEF_EXCEPTION(thread_error);
}

namespace
{
  struct threads_data
  {
    mkt::thread_map             _threads;
    mkt::thread_progress_map    _thread_progress;
    mkt::thread_key_map         _thread_keys;
    mkt::thread_info_map        _thread_info;
    mkt::mutex                  _threads_mutex;
  };
  threads_data                 *_threads_data = 0;
  bool                          _threads_atexit = false;

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

    std::set<boost::thread::id> infoIds;
    BOOST_FOREACH(thread_info_map::value_type val, thread_info_ref())
      infoIds.insert(val.first);

    std::set<boost::thread::id> currentIds;
    BOOST_FOREACH(thread_map::value_type val, threads_ref())
      {
        thread_ptr ptr = val.second;
        if(ptr)
          {
            thread_keys_ref()[ptr->get_id()]=val.first;

            //set the thread info to a default state if not already set
            if(thread_info_ref()[ptr->get_id()].empty())
              thread_info_ref()[ptr->get_id()] = "running";
          }

        currentIds.insert(ptr->get_id());
      }

    //compute thread ids that need to be removed from the threadInfo map
    std::set<boost::thread::id> infoIdsToRemove;
    set_difference(infoIds.begin(), infoIds.end(),
                   currentIds.begin(), currentIds.end(),
                   inserter(infoIdsToRemove,infoIdsToRemove.begin()));

    BOOST_FOREACH(boost::thread::id tid, infoIdsToRemove)
      thread_info_ref().erase(tid);
  }
}

namespace mkt
{
  /*
   * Thread API
   */
  map_change_signal threads_changed;

  //Use this to trigger the threads_changed signal because
  //problems happen when signals are envoked during program start/exit due to static
  //initialization order.
  void trigger_threads_changed(const std::string& key)
  {
    bool as = wait_for_threads::at_start();
    bool ae = wait_for_threads::at_exit();
    if(!as && !ae)
      threads_changed(key);
  }

  thread_map threads()
  {
    boost::this_thread::interruption_point();
    unique_lock lock(threads_mutex_ref());
    return threads_ref();
  }

  thread_ptr threads(const std::string& key)
  {
    boost::this_thread::interruption_point();
    unique_lock lock(threads_mutex_ref());
    if(threads_ref().find(key)!=threads_ref().end())
      return threads_ref()[key];
    return thread_ptr();
  }

  void threads(const std::string& key, const thread_ptr& val)
  {
    boost::this_thread::interruption_point();

    if(has_thread(key))
      threads(key)->interrupt();

    {
      unique_lock lock(threads_mutex_ref());
      if(!val)
        threads_ref().erase(key);
      else
        threads_ref()[key] = val;
      update_thread_keys();
    }

    trigger_threads_changed(key);
  }

  void threads(const thread_map& map)
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(threads_mutex_ref());
      
      BOOST_FOREACH(thread_map::value_type t, threads_ref())
	if(t.second) t.second->interrupt();

      threads_ref() = map;
      thread_progress_ref().clear();
      update_thread_keys();
    }

    trigger_threads_changed("all");
  }

  bool has_thread(const std::string& key)
  {
    boost::this_thread::interruption_point();
    unique_lock lock(threads_mutex_ref());
    if(threads_ref().find(key)!=threads_ref().end())
      return true;
    return false;
  }

  double thread_progress(const std::string& key)
  {
    boost::this_thread::interruption_point();
    unique_lock lock(threads_mutex_ref());

    boost::thread::id tid;
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
      unique_lock lock(threads_mutex_ref());
      boost::thread::id tid;

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
      trigger_threads_changed(key);
  }

  void finish_thread_progress(const std::string& key)
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(threads_mutex_ref());

      boost::thread::id tid;
      if(key.empty())
        tid = boost::this_thread::get_id();
      else if(threads_ref().find(key)!=threads_ref().end() &&
              threads_ref()[key])
        tid = threads_ref()[key]->get_id();
      else
        return;

      thread_progress_ref().erase(tid);
    }
    trigger_threads_changed(key);
  }

  std::string thread_key()
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(threads_mutex_ref());
      if(thread_keys_ref().find(boost::this_thread::get_id())!=thread_keys_ref().end())
        return thread_keys_ref()[boost::this_thread::get_id()];
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
      unique_lock lock(threads_mutex_ref());

      boost::thread::id tid;
      if(key.empty())
        tid = boost::this_thread::get_id();
      else if(threads_ref().find(key)!=threads_ref().end() &&
              threads_ref()[key])
        tid = threads_ref()[key]->get_id();
      else
        return;

      thread_info_ref()[tid] = infostr;
    }
    trigger_threads_changed(key);
  }

  std::string get_thread_info(const std::string& key)
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(threads_mutex_ref());
      
      boost::thread::id tid;
      if(key.empty())
        tid = boost::this_thread::get_id();
      else if(threads_ref().find(key)!=threads_ref().end() &&
              threads_ref()[key])
        tid = threads_ref()[key]->get_id();
      else
        return std::string();

      return thread_info_ref()[tid];
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
