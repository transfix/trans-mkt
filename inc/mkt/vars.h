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
      if(!d_ptr) data(mkt_str());
      try
	{
	  // Try casting to string then doing a lexical cast.
	  // If it is not a string then try casting directly.
	  try
	    {
	      mkt_str val_s = boost::any_cast<mkt_str>(*d_ptr);
	      val = string_cast<T>(val_s);
	    }
	  catch(boost::bad_any_cast&)
	    {
	      val = boost::any_cast<T>(*d_ptr);
	    }

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

  // each thread has its own variable_map
  typedef std::map<mkt_str, variable_map>           variable_maps;

  void init_vars();
  void final_vars();

  mkt_str get_var(const mkt_str& varname, 
		  const mkt_str& t_key = thread_key());
  
  void set_var(const mkt_str& varname, const mkt_str& val,
	       const mkt_str& t_key = thread_key());

  void unset_var(const mkt_str& varname,
		 const mkt_str& t_key = thread_key());

  bool has_var(const mkt_str& varname,
	       const mkt_str& t_key = thread_key());

  argument_vector list_vars(const mkt_str& t_key = thread_key());

  void vars_copy(const mkt_str& to_t_key,
		 const mkt_str& from_t_key = thread_key(),
		 bool no_locals = true,
		 bool only_if_newer = true);

  typedef boost::
    signals2::
    signal<void (const mkt_str& /* varname */, 
		 const mkt_str& /* thread_key */)> 
    var_change_signal;
  var_change_signal& var_changed();
  
  //template shortcuts for casting var values
  template <class T>
    inline T get_var(const mkt_str& varname)
    {
      thread_info ti(BOOST_CURRENT_FUNCTION);
      using namespace boost;
      T val;
      mkt_str str_val = get_var(varname);
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
    inline void set_var(const mkt_str& varname, const T& val)
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
      set_var(varname, str_val);
    }

  //specializations for bool
  template <> 
    inline bool get_var<bool>(const mkt_str& varname)
    {
      thread_info ti(BOOST_CURRENT_FUNCTION);
      mkt_str str_var = get_var(varname);
      if(str_var.empty()) return false;
      if(str_var == "true") return true;
      if(str_var == "false") return false;
      int i_var = get_var<int>(varname);
      return bool(i_var);
    }

  template <>
    inline void set_var<bool>(const mkt_str& varname, const bool& val)
    {
      thread_info ti(BOOST_CURRENT_FUNCTION);
      mkt_str str_val = val ? "true" : "false";
      set_var(varname, str_val);
    }

  //sets the return value for the current command
  template<class T>
    inline void ret_val(const T& val = T())
    {
      thread_info ti(BOOST_CURRENT_FUNCTION);
      set_var("_", val);
    }

  //TODO: add functions to get variable type and access/mod time

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
