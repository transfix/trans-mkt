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

  bool echo_at_exit();
}

#endif
