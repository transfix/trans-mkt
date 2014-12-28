#include <mkt/echo.h>
#include <mkt/threads.h>
#include <mkt/vars.h>

#ifdef MKT_USING_XMLRPC
#include <mkt/xmlrpc.h>
#endif

#include <boost/current_function.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>

namespace mkt
{
  MKT_DEF_EXCEPTION(echo_error);
}

namespace
{
  struct echo_data
  {
    mkt::echo_map               _echo_map;
    mkt::mutex                  _echo_map_mutex;
  };
  echo_data                    *_echo_data = 0;
  bool                          _echo_atexit = false;

  void _echo_cleanup()
  {
    _echo_atexit = true;
    delete _echo_data;
    _echo_data = 0;
  }

  echo_data* _get_echo_data()
  {
    if(_echo_atexit)
      throw mkt::echo_error("Already at program exit!");

    if(!_echo_data)
      {
        _echo_data = new echo_data;
        std::atexit(_echo_cleanup);
      }

    if(!_echo_data)
      throw mkt::echo_error("Missing static variable data!");
    return _echo_data;
  }

  mkt::echo_map& echo_map_ref()
  {
    return _get_echo_data()->_echo_map;
  }

  mkt::mutex& echo_map_mutex_ref()
  {
    return _get_echo_data()->_echo_map_mutex;
  }
}

namespace mkt
{
  void echo_register(int64 id, const echo_func& f)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    unique_lock lock(echo_map_mutex_ref());
    echo_map_ref()[id] = f;
  }

  void echo_unregister(int64 id)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    unique_lock lock(echo_map_mutex_ref());
    echo_map_ref()[id] = echo_func();
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
    unique_lock lock(echo_map_mutex_ref());
    if(echo_map_ref()[echo_function_id])
      echo_map_ref()[echo_function_id](str);
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

  bool echo_at_exit() { return _echo_atexit; }
}
