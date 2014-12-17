#include <mkt/vars.h>
#include <mkt/threads.h>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/regex.hpp>
#include <boost/array.hpp>
#include <boost/foreach.hpp>

namespace
{
  mkt::variable_map           _var_map;
  mkt::mutex                  _var_map_mutex;
}

namespace mkt
{
  //variable API
  std::string var(const std::string& varname)
  {
    bool creating = false;
    std::string val;
    {
      unique_lock lock(_var_map_mutex);
      if(_var_map.find(varname)==_var_map.end()) creating = true;
      val = _var_map[varname];
    }
    
    if(creating) var_changed(varname);
    return val;
  }

  void var(const std::string& varname, const std::string& val)
  {
    using namespace boost::algorithm;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(_var_map_mutex);
      std::string local_varname(varname); trim(local_varname);
      std::string local_val(val); trim(local_val);
      if(_var_map[local_varname] == local_val) return; //nothing to do
      _var_map[local_varname] = local_val;
    }
    var_changed(varname);
  }

  void unset_var(const std::string& varname)
  {
    using namespace boost::algorithm;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(_var_map_mutex);
      std::string local_varname(varname); trim(local_varname);
      _var_map.erase(local_varname);
    }
    var_changed(varname);
  }

  bool has_var(const std::string& varname)
  {
    using namespace boost::algorithm;
    unique_lock lock(_var_map_mutex);
    thread_info ti(BOOST_CURRENT_FUNCTION);    
    std::string local_varname(varname); trim(local_varname);
    if(_var_map.find(local_varname)==_var_map.end())
      return false;
    else return true;
  }

  argument_vector list_vars()
  {
    unique_lock lock(_var_map_mutex);
    thread_info ti(BOOST_CURRENT_FUNCTION);
    argument_vector vars;
    BOOST_FOREACH(const variable_map::value_type& cur, _var_map)
      {
        std::string cur_varname = cur.first;
        boost::algorithm::trim(cur_varname);
        vars.push_back(cur_varname);
      }
    return vars;
  }

  map_change_signal var_changed;

  argument_vector expand_vars(const argument_vector& args)
  {
    using namespace std;
    using namespace boost;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    argument_vector local_args(args);

    boost::array<regex, 2> exprs = 
      { 
        regex("\\W*\\$(\\w+)\\W*"), 
        regex("\\$\\{(\\w+)\\}") 
      };

    BOOST_FOREACH(string& arg, local_args)
      {
        BOOST_FOREACH(regex& expr, exprs)
          {
            match_results<string::iterator> what;
            match_flag_type flags = match_default;
            try
              {
                if(regex_search(arg.begin(), arg.end(), what, expr, flags))
                  arg = var(string(what[1])); //do the expansion
              }
            catch(...){}
          }
      }

    return local_args;
  }

  argument_vector split_vars(const argument_vector& args)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    argument_vector local_args(args);

    //TODO: look for special keyword 'split' and split the argument right afterward into an argument
    //vector and add it to the overall vector.  This way variables that represent commands can be split
    //into an argument vector and executed.

    return local_args;
  }
}
