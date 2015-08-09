#ifndef __MKT_VARS_H__
#define __MKT_VARS_H__

#include <mkt/config.h>
#include <mkt/types.h>
#include <mkt/threads.h>
#include <mkt/exceptions.h>
#include <mkt/utils.h>

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include <map>

namespace mkt
{
  /*
   * Variable API
   */
  typedef mkt_str                                   var_string;
  typedef std::map<var_string, var_string>          variable_map;
  typedef std::vector<variable_map>                 variable_map_stack;
  typedef boost::tuple<
    var_string,  //thread key 
    var_string   //stack name
    > variable_map_stacks_key;
  typedef variable_map_stacks_key                   vms_key; //shorthand
  typedef std::map<vms_key, 
                   variable_map_stack>              variable_map_stacks;
  const var_string& vars_main_stackname();
  const vms_key& vars_variable_map_stacks_key_default();
  inline const vms_key& vms_key_def() //shorthand for the above
    { return vars_variable_map_stacks_key_default(); }

  class var_context
  {
  public:
    var_context(size_t stack_depth = 0, 
		const vms_key& key = vms_key_def())
      : _stack_depth(stack_depth), _key(key) {}

    var_context(const var_context& c)
      : _stack_depth(c._stack_depth), _key(c._key) {}
  
    size_t stack_depth() const { return _stack_depth; }
    var_context& stack_depth(size_t d) { _stack_depth = d; return *this; }

    const vms_key& key() const { return _key; }
    var_context& key(const vms_key& k) { _key = k; return *this; }

    bool operator==(const var_context& rhs) const
    {
      if(this == &rhs) return true;
      return 
	_stack_depth == rhs._stack_depth &&
	_key == rhs._key;
    }

    bool operator!=(const var_context& rhs) const
    {
      return !(*this == rhs);
    }

  private:
    size_t _stack_depth;
    vms_key _key;
  };

  var_string var(const var_string& varname, 
		 const var_context& context = var_context());

  void var(const var_string& varname, const var_string& val,
	   const var_context& context = var_context());

  void unset_var(const var_string& varname,
		 const var_context& context = var_context());

  bool has_var(const var_string& varname,
	       const var_context& context = var_context());

  argument_vector list_vars(const var_context& context = var_context());

  void push_vars(const vms_key& key = vms_key_def());
  void pop_vars(const vms_key& key = vms_key_def());
  size_t vars_stack_size(const vms_key& key = vms_key_def());

  typedef boost::
    signals2::
    signal<void (const var_string&, const var_context&)> 
    var_change_signal;
  extern var_change_signal var_changed;
  
  //template shortcuts for casting var values
  template <class T>
    inline T var(const var_string& varname)
    {
      thread_info ti(BOOST_CURRENT_FUNCTION);
      using namespace boost;
      T val;
      var_string str_val = var(varname);
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
    inline void var(const var_string& varname, const T& val)
    {
      thread_info ti(BOOST_CURRENT_FUNCTION);
      var_string str_val;
      try
        {
          str_val = boost::lexical_cast<var_string>(val);
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
    inline bool var<bool>(const var_string& varname)
    {
      thread_info ti(BOOST_CURRENT_FUNCTION);
      var_string str_var = var(varname);
      if(str_var.empty()) return false;
      if(str_var == "true") return true;
      if(str_var == "false") return false;
      int i_var = var<int>(varname);
      return bool(i_var);
    }

  template <>
    inline void var<bool>(const var_string& varname, const bool& val)
    {
      thread_info ti(BOOST_CURRENT_FUNCTION);
      var_string str_val = val ? "true" : "false";
      var(varname, str_val);
    }

  //sets the return value for the current command
  template<class T>
    inline void ret_val(const T& val = T())
    {
      thread_info ti(BOOST_CURRENT_FUNCTION);
      var("_", val);
    }

  //Splits a string into an argument vector, taking into account
  //quote characters for argument values with spaces.
  argument_vector split(const var_string& args);

  //joins an argument vector into a single string
  var_string join(const argument_vector& args);    

  //expands any variable names in the string to their values
  var_string expand_vars(const var_string& args);

  bool vars_at_exit();
}

#endif
