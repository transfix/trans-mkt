#ifndef __MKT_VARS_H__
#define __MKT_VARS_H__

#include <mkt/config.h>
#include <mkt/types.h>
#include <mkt/threads.h>
#include <mkt/exceptions.h>
#include <mkt/utils.h>

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/format.hpp>

#include <map>

namespace mkt
{
  /*
   * Variable API
   */
  MKT_DEF_EXCEPTION(vars_error);

  struct variable_value : boost::tuple
  <
    any_ptr,  // value data
    mkt_str,  // value type identifier
    ptime,    // modification time
    ptime     // access time
  >
  {
    inline variable_value(const any_ptr& p = any_ptr(), 
			  const mkt_str& t = mkt_str(),
			  const ptime& m = now(),
			  const ptime& a = now())
      : boost::tuple<any_ptr, mkt_str, ptime, ptime>(p,t,m,a) {}
    inline variable_value(const variable_value& vv)
      : boost::tuple<any_ptr, mkt_str, ptime, ptime>(vv) {}
    
    template<class T>
    inline T data()
    {
      T val;
      any_ptr& d_ptr = get<0>();
      //default to empty string for now
      if(!d_ptr) d_ptr.reset(new any(mkt_str()));
      try
	{
	  val = boost::any_cast<T>(*d_ptr);
	  ptime& access_time = get<3>();
	  access_time = now();
	}
      catch(boost::bad_any_cast&)
	{
	  throw mkt::vars_error(
	    boost::str(boost::format("Error casting variable data to %1%.")
		       % typeid(T).name()));
	}
      return val;
    }

    inline mkt_str data() { return data<mkt_str>(); }

    template<class T>
    inline void data(const T& in)
    {
      any_ptr& d_ptr = get<0>();
      if(!d_ptr) d_ptr.reset(new any());
      *d_ptr = in;

      mkt_str& val_type = get<1>();
      val_type = mkt_str(typeid(T).name());

      ptime& mod_time = get<2>();
      mod_time = now();

      ptime& access_time = get<3>();
      access_time = now();
    }

    const mkt_str& type() const { return get<1>(); }
    const ptime& mod_time() const { return get<2>(); }
    const ptime& access_time() const { return get<3>(); }
  };

  typedef std::map<mkt_str, variable_value>         variable_map;
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

  // Default var_context() is highest point on this thread's main stack
  struct var_context : boost::tuple<size_t, vms_key>
  {
    inline var_context(size_t stack_depth = 0, 
		       const vms_key& key = vms_key_def())
      : boost::tuple<size_t, vms_key>(stack_depth, key) {}
    inline var_context(const var_context& c)
      : boost::tuple<size_t, vms_key>(c) {}
    inline size_t stack_depth() const { return get<0>(); }
    inline var_context& stack_depth(size_t d) { get<0>() = d; return *this; }
    inline const vms_key& key() const { return get<1>(); }
    inline var_context& key(const vms_key& k) { get<1>() = k; return *this; }    
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
  void vars_copy(const var_context& to_context,
		 const var_context& from_context = var_context(),
		 bool no_locals = true,
		 bool only_if_newer = true);

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
          throw mkt::system_error(
	    str(format("Invalid value type for variable %1%: %2%")
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
          throw mkt::system_error(
	    boost::str(boost::format("Invalid value type for variable %1%: %2%")
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
