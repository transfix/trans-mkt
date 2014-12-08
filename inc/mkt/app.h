#ifndef __MKT_APP_H__
#define __MKT_APP_H__

#include <mkt/config.h>
#include <mkt/exceptions.h>

#include <boost/function.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/signals2.hpp>
#include <boost/cstdint.hpp>
#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/current_function.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <iomanip>

namespace mkt
{
  typedef boost::int64_t             int64;
  typedef boost::uint64_t            uint64;
  typedef boost::posix_time::ptime   ptime;
  typedef boost::shared_mutex        mutex;
  typedef boost::unique_lock<mutex>  unique_lock;
  typedef boost::shared_lock<mutex>  shared_lock;

  //version string
  std::string version();

  /*
   * Command related types
   */
  
  typedef std::vector<std::string>                       argument_vector;
  typedef boost::function<void (const argument_vector&)> command_func;
  typedef boost::tuple<command_func, std::string>        command;
  typedef std::map<std::string, command>                 command_map;

  void add_command(const std::string& name,
                   const command_func& func,
                   const std::string& desc);
  void remove_command(const std::string& name);
  argument_vector get_commands();

  //Executes a command. The first string is the command, with everything
  //after being the command's arguments.
  void exec(const argument_vector& args);

  //Executes commands listed in a file where the first argument is the filename. 
  //If parallel is true, commands are executed in separate threads.
  void exec_file(const argument_vector& args, bool parallel = false);

  //splits a string into an argument vector
  argument_vector split(const std::string& args);

  //joins an argument vector into a single string
  std::string join(const argument_vector& args);

  //accessing the process' argument vector
  argument_vector argv();
  void argv(const argument_vector& av);
  void argv(int argc, char **argv);

  /*
   * Thread API
   */
  typedef boost::signals2::signal<void (const std::string&)> map_change_signal;
  typedef boost::shared_ptr<boost::thread>                   thread_ptr;
  typedef std::map<std::string, thread_ptr>                  thread_map;
  typedef std::map<boost::thread::id, double>                thread_progress_map;
  typedef std::map<boost::thread::id, std::string>           thread_key_map;
  typedef std::map<boost::thread::id, std::string>           thread_info_map;

  extern map_change_signal threads_changed;

  thread_map threads();
  thread_ptr threads(const std::string& key);
  void threads(const std::string& key, const thread_ptr& val);
  void threads(const thread_map& map);
  bool has_thread(const std::string& key);
  double thread_progress(const std::string& key = std::string());
  void thread_progress(double progress); //0.0 - 1.0
  void thread_progress(const std::string& key, double progress);
  void finish_thread_progress(const std::string& key = std::string());
  std::string thread_key(); //returns the thread key for this thread
  void remove_thread(const std::string& key);
  std::string unique_thread_key(const std::string& hint = std::string());

  //set a string to associate with the thread to state it's current activity
  void set_thread_info(const std::string& key, const std::string& infostr);
  
  std::string get_thread_info(const std::string& key = std::string());
  void this_thread_info(const std::string& infostr);
  std::string this_thread_info();

  //Used to easily manage saving/restoring thread info as a thread
  //is executed.
  class thread_info
  {
  public:
    thread_info(const std::string& info = "running");
    ~thread_info();
  private:
    std::string _orig_info;
    double _orig_progress;
  };

  //Meant to be instantiated at the shallowest scope of a
  //thread's stack
  class thread_feedback
  {
  public:
    thread_feedback(const std::string& info = "running");    
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
    init_thread(const T& t) : _t(t) {}
      void operator()()
      {
        try
          {
            thread_feedback tf(BOOST_CURRENT_FUNCTION);
            _t();
          }
        catch(...)
          {
            //TODO: log this event...
          }
      }
    private:
      T _t;
    };

  //T is a class with operator()
  template<class T>
  void start_thread(const std::string& key, const T& t, bool wait = true)
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

  //instantiate this in main() to wait for all threads to finish before quitting the process.
  class wait_for_threads
  {
  public:
    ~wait_for_threads();
    static bool at_exit();
    static void wait();
  private:
    static bool _at_exit;
  };

  void sleep(int64 ms);

  /*
   * Variable API
   */
  typedef std::map<std::string, std::string> variable_map;
  std::string var(const std::string& varname);
  void var(const std::string& varname, const std::string& val);
  void unset_var(const std::string& varname);
  bool has_var(const std::string& varname);
  argument_vector list_vars();
  extern map_change_signal var_changed;

  //template shortcuts for casting var values
  template <class T>
    inline T var(const std::string& varname)
    {
      T val;
      std::string str_val = var(varname);
      try
        {
          val = boost::lexical_cast<T>(str_val);
        }
      catch(boost::bad_lexical_cast&)
        {
          throw mkt::system_error(boost::str(boost::format("Invalid value type for variable %1%: %2%")
                                             % varname % str_val));
        }
      return val;
    }
  
  template <class T>
    inline void var(const std::string& varname, const T& val)
    {
      std::string str_val;
      try
        {
          str_val = boost::lexical_cast<std::string>(val);
        }
      catch(boost::bad_lexical_cast&)
        {
          throw mkt::system_error(boost::str(boost::format("Invalid value type for variable %1%: %2%")
                                             % varname % val));
        }
      var(varname, str_val);
    }

  //special versions for bool
  template <> 
    inline bool var<bool>(const std::string& varname)
    {
      std::string str_var = var(varname);
      if(str_var.empty()) return false;
      if(str_var == "true") return true;
      if(str_var == "false") return false;
      int i_var = var<int>(varname);
      return bool(i_var);
    }

  template <>
    inline void var<bool>(const std::string& varname, const bool& val)
    {
      std::string str_val = val ? "true" : "false";
      var(varname, str_val);
    }
    

  //expands variable names to their values
  argument_vector expand_vars(const argument_vector& args);

  //splits arguments with spaces into an argument_vector and replaces that argument
  //with the vector.  The argument after a keyword 'split' gets split in this way.
  argument_vector split_vars(const argument_vector& args);

  /*
   * Echo API
   */
  typedef boost::function<void (const std::string&)> echo_func;
  typedef std::map<int64, echo_func>                 echo_map;
  void echo_register(int64 id, const echo_func& f);
  void echo_unregister(int64 id);

  //Passes the str argument to every function registered as an echo function
  //and specified by the variable __echo_functions which contains a comma separated list of 
  //echo function ids.
  void echo(const std::string& str);

  //Specifically calls a registered echo function.
  void echo(uint64 echo_function_id, const std::string& str);

  //use this class like the following:
  // mkt::out().stream() << "Hello, World!";
  // mkt::out(1).stream() << "Hello, World!"; //use echo function registered with id 1 only.
  class out
  {
  public:
    out(int64 echo_id = -1) : 
    _out(boost::iostreams::back_inserter(_result)), _id(echo_id) {}
    ~out() { _out.flush(); if(_id >= 0) echo(_id, _result); else echo(_result); }
    boost::iostreams::filtering_ostream& stream() { return _out; }
  private:
    std::string _result;
    boost::iostreams::filtering_ostream _out;
    int64 _id;
  };

  void init_echo(); //set up standard echo functions
}

#endif
