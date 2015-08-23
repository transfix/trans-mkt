#ifndef __MKT_ECHO_H__
#define __MKT_ECHO_H__

#include <mkt/config.h>
#include <mkt/types.h>

#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filtering_stream.hpp>

namespace mkt
{
  /*
   * Echo API
   */
  typedef boost::function<void (const std::string&)> echo_func;
  typedef std::map<int64, echo_func>                 echo_map;
  void echo_register(int64 id, const echo_func& f);
  void echo_unregister(int64 id);

  typedef boost::signals2::signal<void (int64)> echo_func_map_changed_signal;
  echo_func_map_changed_signal& echo_function_registered();
  echo_func_map_changed_signal& echo_function_unregistered();

  //Passes the str argument to every function registered as an echo function
  //and specified by the variable __echo_functions which contains a comma separated list of 
  //echo function ids.
  void echo(const std::string& str);

  //Specifically calls a registered echo function.
  void echo(uint64 echo_function_id, const std::string& str);

  typedef boost::signals2::signal<void (uint64, const std::string& str)> echo_signal;
  echo_signal& echo_pre_exec();
  echo_signal& echo_post_exec();

  //use this class like the following:
  // mkt::out().stream() << "Hello, World!";
  // mkt::out(1).stream() << "Hello, World!"; //use echo function registered with id 1 only.
  class out
  {
  public:
    typedef boost::iostreams::filtering_ostream f_os;
    out(int64 echo_id = -1);
    ~out();
    f_os& stream() { return _out; }
  private:
    std::string _result;
    f_os _out;
    int64 _id;
  };

#define mkt_echo mkt::out().stream()
#define mkt_echo_(id) mkt::out(id).stream()

  void init_echo(); //set up standard echo functions
  void final_echo();

  bool echo_at_exit();
}

#endif
