#ifndef __MKT_VARS_H__
#define __MKT_VARS_H__

#include <mkt/config.h>
#include <mkt/types.h>
#include <mkt/exceptions.h>

#include <map>

namespace mkt
{
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
}

#endif
