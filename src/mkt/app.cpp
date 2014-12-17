#include <mkt/app.h>
#include <mkt/xmlrpc.h>
#include <mkt/threads.h>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/regex.hpp>
#include <boost/array.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/xtime.hpp>

#include <set>
#include <iostream>
#include <cstdlib>

namespace
{
  mkt::argument_vector        _av;
  mkt::mutex                  _av_lock;

  mkt::variable_map           _var_map;
  mkt::mutex                  _var_map_mutex;

  mkt::echo_map               _echo_map;
  mkt::mutex                  _echo_map_mutex;
}

namespace mkt
{
  std::string version()
  {
    return std::string(MKT_VERSION);
  }

  argument_vector split(const std::string& args)
  {
    using namespace std;
    using namespace boost::algorithm;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    //split along quote boundaries to support quotes around arguments with spaces.
    mkt::argument_vector av_strings;
    split(av_strings, args, is_any_of("\""), token_compress_on);

    mkt::argument_vector av;
    int idx = 0;
    BOOST_FOREACH(const string& cur, av_strings)
      {
        //Every even element is outside quotes and should be split
        if(idx % 2 == 0)
          {
            mkt::argument_vector av_cur;
            split(av_cur, cur, is_any_of(" "), token_compress_on);
            BOOST_FOREACH(string& s, av_cur) trim(s);
            av.insert(av.end(),
                      av_cur.begin(), av_cur.end());
            idx++;
          }
        else //just insert odd elements as they are
          av.push_back(cur);
      }

    //remove empty strings
    mkt::argument_vector av_clean;
    BOOST_FOREACH(const string& cur, av)
      if(!cur.empty()) av_clean.push_back(cur);
    return av_clean;
  }

  std::string join(const argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    return boost::join(args," ");
  }

  argument_vector argv()
  {
    using namespace boost;
    argument_vector av;
    int argc = mkt::var<int>("__argc");
    for(int i = 0; i < argc; i++)
      av.push_back(mkt::var(str(format("__argv_%1%") % i)));
    return av;
  }

  void argv(int argc, char **argv)
  {
    using namespace boost;
    mkt::argument_vector args;
    mkt::var("__argc", argc);
    for(int i = 0; i < argc; i++)
      mkt::var(str(format("__argv_%1%") % i), argv[i]);
  }

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

  void echo_register(int64 id, const echo_func& f)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    unique_lock lock(_echo_map_mutex);
    _echo_map[id] = f;
  }

  void echo_unregister(int64 id)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    unique_lock lock(_echo_map_mutex);
    _echo_map[id] = echo_func();
  }

  void echo(const std::string& str)
  {
    using namespace std;
    using namespace boost;
    using namespace boost::algorithm;

    thread_info ti(BOOST_CURRENT_FUNCTION);

    const string echo_functions_varname("__echo_functions");
    string echo_functions_value = var(echo_functions_varname);
    mkt::argument_vector echo_functions_strings;
    split(echo_functions_strings, echo_functions_value, is_any_of(","), token_compress_on);

    //execute each echo function
    BOOST_FOREACH(string& echo_function_id_string, echo_functions_strings)
      {
        trim(echo_function_id_string);
        if(echo_function_id_string.empty()) continue;

        try
          {
            uint64 echo_function_id = lexical_cast<uint64>(echo_function_id_string);
            echo(echo_function_id, str);
          }
        catch(const bad_lexical_cast &)
          {
            throw system_error(boost::str(format("Invalid echo_function_id %1%")
                                          % echo_function_id_string));                
          }
      }
  }

  void echo(uint64 echo_function_id, const std::string& str)
  {
    using namespace std;
    using namespace boost;

    thread_info ti(BOOST_CURRENT_FUNCTION);
    unique_lock lock(_echo_map_mutex);
    if(_echo_map[echo_function_id])
      _echo_map[echo_function_id](str);
  }

  //the default echo function
  void do_echo(std::ostream* is, const std::string& str)
  {
    using namespace boost;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    if(is && !var<bool>("__quiet")) (*is) << str;
  }

  void init_echo()
  {
    //setup echo so it outputs to console
    echo_register(0, boost::bind(mkt::do_echo, &std::cout, _1));
    echo_register(1, boost::bind(mkt::do_echo, &std::cerr, _1));
#ifdef MKT_USING_XMLRPC
    echo_register(2, mkt::do_remote_echo);
#endif
    var("__echo_functions", "0, 2");
  }
}
