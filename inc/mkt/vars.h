#ifndef __MKT_VARS_H__
#define __MKT_VARS_H__

#include <mkt/config.h>
#include <mkt/types.h>
#include <mkt/threads.h>
#include <mkt/exceptions.h>

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include <map>

namespace mkt
{
  /*
   * Variable API
   */
  typedef std::map<std::string, std::string>        variable_map;
  typedef std::vector<variable_map>                 variable_map_stack;
  typedef boost::tuple<std::string, std::string>    variable_map_stacks_key;
  typedef std::map<variable_map_stacks_key, 
                   variable_map_stack>              variable_map_stacks;
  const std::string& vars_main_stackname();
  const variable_map_stacks_key& vars_variable_map_stacks_key_default();
  std::string var(const std::string& varname,
		  const variable_map_stacks_key& key = vars_variable_map_stacks_key_default());
  void var(const std::string& varname, const std::string& val,
	   const variable_map_stacks_key& key = vars_variable_map_stacks_key_default());
  void unset_var(const std::string& varname,
		 const variable_map_stacks_key& key = vars_variable_map_stacks_key_default());
  bool has_var(const std::string& varname,
	       const variable_map_stacks_key& key = vars_variable_map_stacks_key_default());
  argument_vector list_vars(const variable_map_stacks_key& key = vars_variable_map_stacks_key_default());
  void push_vars(const variable_map_stacks_key& key = vars_variable_map_stacks_key_default());
  void pop_vars(const variable_map_stacks_key& key = vars_variable_map_stacks_key_default());
  size_t vars_stack_size(const variable_map_stacks_key& key = vars_variable_map_stacks_key_default());
  extern map_change_signal var_changed;

  template <class T>
    inline T string_cast(const std::string& str_val)
    {
      using namespace boost;
      T val;
      try
        {
          val = lexical_cast<T>(str_val);
        }
      catch(bad_lexical_cast&)
        {
          throw mkt::system_error(str(format("Invalid value type for string %1%")
                                             % str_val));
        }
      return val;
    }

  //template shortcuts for casting var values
  template <class T>
    inline T var(const std::string& varname)
    {
      using namespace boost;
      T val;
      std::string str_val = var(varname);
      try
        {
          val = string_cast<T>(str_val);
        }
      catch(mkt::system_error&)
        {
          throw mkt::system_error(str(format("Invalid value type for variable %1%: %2%")
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

  //Splits a string into an argument vector, taking into account
  //quote characters for argument values with spaces.
  argument_vector split(const std::string& args);

  //joins an argument vector into a single string
  std::string join(const argument_vector& args);    

  //expands any variable names in the string to their values
  std::string expand_vars(const std::string& args);

  bool vars_at_exit();
}

#endif
