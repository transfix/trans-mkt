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
  typedef std::map<mkt_str, mkt_str>                variable_map;
  typedef std::vector<variable_map>                 variable_map_stack;
  typedef boost::tuple<
    mkt_str,  //thread key 
    mkt_str   //stack name
    > variable_map_stacks_key;
  typedef variable_map_stacks_key                   vms_key; //shorthand
  typedef std::map<vms_key, 
                   variable_map_stack>              variable_map_stacks;
  const mkt_str& vars_main_stackname();
  const vms_key& vars_variable_map_stacks_key_default();
  inline const vms_key& vms_key_def() //shorthand for the above
    { return vars_variable_map_stacks_key_default(); }

  void init_vars();
  void final_vars();

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

  mkt_str var(const mkt_str& varname, 
	      const var_context& context = var_context());
  
  void var(const mkt_str& varname, const mkt_str& val,
	   const var_context& context = var_context());

  void unset_var(const mkt_str& varname,
		 const var_context& context = var_context());

  bool has_var(const mkt_str& varname,
	       const var_context& context = var_context());

  argument_vector list_vars(const var_context& context = var_context());

  void push_vars(const vms_key& key = vms_key_def());
  void pop_vars(const vms_key& key = vms_key_def());
  size_t vars_stack_size(const vms_key& key = vms_key_def());

  typedef boost::
    signals2::
    signal<void (const mkt_str&, const var_context&)> 
    var_change_signal;
  var_change_signal& var_changed();
  
  //template shortcuts for casting var values
  template <class T>
    inline T var(const mkt_str& varname)
    {
      thread_info ti(BOOST_CURRENT_FUNCTION);
      using namespace boost;
      T val;
      mkt_str str_val = var(varname);
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
    inline void var(const mkt_str& varname, const T& val)
    {
      thread_info ti(BOOST_CURRENT_FUNCTION);
      mkt_str str_val;
      try
        {
          str_val = boost::lexical_cast<mkt_str>(val);
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
    inline bool var<bool>(const mkt_str& varname)
    {
      thread_info ti(BOOST_CURRENT_FUNCTION);
      mkt_str str_var = var(varname);
      if(str_var.empty()) return false;
      if(str_var == "true") return true;
      if(str_var == "false") return false;
      int i_var = var<int>(varname);
      return bool(i_var);
    }

  template <>
    inline void var<bool>(const mkt_str& varname, const bool& val)
    {
      thread_info ti(BOOST_CURRENT_FUNCTION);
      mkt_str str_val = val ? "true" : "false";
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
  argument_vector split(const mkt_str& args);

  //joins an argument vector into a single string
  mkt_str join(const argument_vector& args);    

  //expands any variable names in the string to their values
  mkt_str expand_vars(const mkt_str& args);

  bool vars_at_exit();
}

#endif
