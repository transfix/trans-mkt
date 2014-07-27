#include <boost/function.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/signals2.hpp>
#include <boost/cstdint.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/xtime.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/current_function.hpp>

#include <string>
#include <vector>
#include <map>

namespace mkt
{
  typedef boost::int64_t  int64;
  typedef boost::uint64_t uint64;

  //version string
  std::string version();

  /*
   * Command related types
   */
  
  typedef std::vector<std::string>                       argument_vector;
  typedef boost::function<void (const argument_vector&)> command_func;
  typedef boost::tuple<command_func, std::string>        command;
  typedef std::map<std::string, command>                 command_map;

  //Executes a command. The first string is the command, with everything
  //after being the command's arguments.
  void exec(const argument_vector& args);

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

  //Used to easily manage saving/restoring thread info as we
  //traverse a threads stack.
  class thread_info
  {
  public:
    thread_info(const std::string& info = "running");
    ~thread_info();
  private:
    std::string _orig_info;
    double _orig_progress;
  };

  //Instantiate one of these on the shallowest point of your 
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
        thread_feedback tf(BOOST_CURRENT_FUNCTION);
        _t();
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
}
