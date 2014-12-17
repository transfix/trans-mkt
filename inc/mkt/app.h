#ifndef __MKT_APP_H__
#define __MKT_APP_H__

#include <mkt/config.h>
#include <mkt/exceptions.h>
#include <mkt/types.h>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/current_function.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <iomanip>

namespace mkt
{
  //version string
  std::string version();

  //splits a string into an argument vector
  argument_vector split(const std::string& args);

  //joins an argument vector into a single string
  std::string join(const argument_vector& args);

  //accessing the process' argument vector
  argument_vector argv();
  void argv(int argc, char **argv);

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

  //specializations for bool
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
