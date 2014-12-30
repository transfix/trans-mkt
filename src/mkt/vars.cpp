#include <mkt/vars.h>
#include <mkt/threads.h>
#include <mkt/exceptions.h>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/regex.hpp>
#include <boost/array.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>

namespace mkt
{
  MKT_DEF_EXCEPTION(vars_error);
}

namespace
{
  struct vars_data
  {
    mkt::variable_map           _var_map;
    mkt::mutex                  _var_map_mutex;
  };
  vars_data                    *_vars_data = 0;
  bool                          _vars_atexit = false;

  void _vars_cleanup()
  {
    _vars_atexit = true;
    delete _vars_data;
    _vars_data = 0;
  }

  vars_data* _get_vars_data()
  {
    if(_vars_atexit)
      throw mkt::vars_error("Already at program exit!");

    if(!_vars_data)
      {
        _vars_data = new vars_data;
        std::atexit(_vars_cleanup);
      }

    if(!_vars_data)
      throw mkt::vars_error("Missing static variable data!");
    return _vars_data;
  }

  mkt::variable_map& var_map_ref()
  {
    return _get_vars_data()->_var_map;
  }

  mkt::mutex& var_map_mutex_ref()
  {
    return _get_vars_data()->_var_map_mutex;
  }
}

//variable API
namespace mkt
{
  std::string var(const std::string& varname)
  {
    bool creating = false;
    std::string val;
    {
      unique_lock lock(var_map_mutex_ref());
      if(var_map_ref().find(varname)==var_map_ref().end()) creating = true;
      val = var_map_ref()[varname];
    }
    
    if(creating) var_changed(varname);
    return val;
  }

  void var(const std::string& varname, const std::string& val)
  {
    using namespace boost::algorithm;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(var_map_mutex_ref());
      std::string local_varname(varname); trim(local_varname);
      std::string local_val(val); trim(local_val);
      if(var_map_ref()[local_varname] == local_val) return; //nothing to do
      var_map_ref()[local_varname] = local_val;
    }
    var_changed(varname);
  }

  void unset_var(const std::string& varname)
  {
    using namespace boost::algorithm;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(var_map_mutex_ref());
      std::string local_varname(varname); trim(local_varname);
      var_map_ref().erase(local_varname);
    }
    var_changed(varname);
  }

  bool has_var(const std::string& varname)
  {
    using namespace boost::algorithm;
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);    
    std::string local_varname(varname); trim(local_varname);
    if(var_map_ref().find(local_varname)==var_map_ref().end())
      return false;
    else return true;
  }

  argument_vector list_vars()
  {
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);
    argument_vector vars;
    BOOST_FOREACH(const variable_map::value_type& cur, var_map_ref())
      {
        std::string cur_varname = cur.first;
        boost::algorithm::trim(cur_varname);
        vars.push_back(cur_varname);
      }
    return vars;
  }

  map_change_signal var_changed;

  argument_vector split(const std::string& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    return boost::program_options::split_winmain(args);
  }

  std::string join(const argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    return boost::join(args," ");
  }

  std::string expand_vars(const std::string& args)
  {
    using namespace std;
    using namespace boost;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    string local_args(args);

    boost::array<regex, 2> exprs = 
      { 
        regex("\\s*(\\$(\\w+))\\s*"), 
        regex("\\$\\{(\\w+)\\}") 
      };

    bool found;
    do
      {
        found = false;
        BOOST_FOREACH(regex& expr, exprs)
          {
            match_results<string::iterator> what;
            match_flag_type flags = match_default;
            try
              {
                //http://www.boost.org/doc/libs/1_57_0/libs/regex/doc/html/boost_regex/ref/regex_search.html
                if(regex_search(local_args.begin(), 
                                local_args.end(), what, expr, flags))
                  {
                    //do the expansion
                    string var_name = string(what[2]);
                    if(!has_var(var_name)) continue; //TODO: what if this throws
                    found = true;
                    string expanded_arg = var(var_name);
                    local_args.replace(what[1].first, what[1].second,
                                       expanded_arg);
                  }
              }
            catch(...){}
          }
      }
    while(found);
    
    return local_args;
  }

  bool vars_at_exit() { return _vars_atexit; }
}
