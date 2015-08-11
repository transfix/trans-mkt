#ifndef __MKT_THREADS_H__
#define __MKT_THREADS_H__

#include <mkt/config.h>
#include <mkt/types.h>

#include <boost/thread.hpp>
#include <boost/foreach.hpp>

namespace mkt
{
  /*
   * Thread API
   */
  typedef boost::shared_ptr<boost::thread>       thread_ptr;
  typedef std::map<mkt_str, thread_ptr>          thread_map;
  typedef boost::thread::id                      thread_id;
  typedef std::map<thread_id, double>            thread_progress_map;
  typedef std::map<thread_id, mkt_str>           thread_key_map;
  typedef std::map<thread_id, mkt_str>           thread_info_map;

  map_change_signal& threads_changed();

  void init_threads();
  void final_threads();

  thread_map threads();
  thread_ptr threads(const mkt_str& key);
  void threads(const mkt_str& key, const thread_ptr& val);
  void threads(const thread_map& map);
  bool has_thread(const mkt_str& key);
  double thread_progress(const mkt_str& key = mkt_str());
  void thread_progress(double progress); //0.0 - 1.0
  void thread_progress(const mkt_str& key, double progress);
  void finish_thread_progress(const mkt_str& key = mkt_str());
  mkt_str thread_key(thread_id tid = boost::this_thread::get_id());
  const mkt_str& threads_default_keyname();
  void remove_thread(const mkt_str& key);
  mkt_str unique_thread_key(const mkt_str& hint = mkt_str());

  //set a string to associate with the thread to state it's current activity
  void set_thread_info(const mkt_str& key, const mkt_str& infostr);
  
  mkt_str get_thread_info(const mkt_str& key = mkt_str());
  void this_thread_info(const mkt_str& infostr);
  mkt_str this_thread_info();

  //Used to easily manage saving/restoring thread info as a thread
  //is executed.
  class thread_info
  {
  public:
    thread_info(const mkt_str& info = "running");
    ~thread_info();
  private:
    mkt_str _orig_info;
    double _orig_progress;
  };

  //Meant to be instantiated at the shallowest scope of a
  //thread's stack
  class thread_feedback
  {
  public:
    thread_feedback(const mkt_str& info = "running");    
    ~thread_feedback();
  private:
    thread_info _info;
  };

  //helper for start_thread that sets up a thread_feedback
  //object for the thread.
  template<class T>
    class init_thread
    {
    public:
      init_thread(const T& t) : 
	_t(t), _caller_thread_key(thread_key()) {}
      void operator()()
      {
        try
          {
            thread_feedback tf(BOOST_CURRENT_FUNCTION);
	    //TODO: copy vars from calling thread's var map stack to this
	    //one.
	    
            _t();
          }
        catch(...)
          {
            //TODO: log this event...
          }
      }
    private:
      T _t;
      mkt_str _caller_thread_key;
    };

  //T is a function object
  template<class T>
  void start_thread(const mkt_str& key, const T& t, bool wait = true)
    {
      //If waiting and an existing thread with this key is running,
      //stop the existing running thread with this key and wait for
      //it to actually stop before starting a new one.  Else just use
      //a unique key.
      if(wait && has_thread(key))
        {
          thread_ptr tptr = threads(key);
          tptr->interrupt(); //initiate thread quit
          tptr->join();      //wait for it to quit
        }

      threads(wait ? key : unique_thread_key(key),
              thread_ptr(new boost::thread(init_thread<T>(t))));
    }

  //Instantiate this in main() to wait for all threads to finish before quitting the process.
  //Failing to do so will also prevent threads_changed signals from firing.
  class wait_for_threads
  {
  public:
    wait_for_threads();
    ~wait_for_threads();
    static bool at_start();
    static bool at_exit();
    static void wait();
  private:
    static bool _at_start;
    static bool _at_exit;
  };

  void sleep(int64 ms);

  bool threads_at_exit();
}

#endif
