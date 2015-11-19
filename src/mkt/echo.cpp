#include <mkt/echo.h>
#include <mkt/threads.h>
#include <mkt/vars.h>
#include <mkt/commands.h>

#ifdef MKT_USING_XMLRPC
#include <mkt/xmlrpc.h>
#endif

#include <boost/current_function.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>

//This module's exceptions
namespace mkt
{
  MKT_DEF_EXCEPTION(echo_error);
}

//This module's static data
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

//Echo commands
namespace
{
  void echo(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin());

    if(local_args.empty())
      throw mkt::echo_error("Nothing to echo");
    
    bool newline = true;
    //Check if the first argument is an "-n". If it is, eat it and don't put a newline.
    if(local_args[0] == "-n")
      {
        newline = false;
        local_args.erase(local_args.begin());
      }

    //Check if the first argument is an integer. If it is, make it the echo function id to use.
    int echo_id = -1;
    try
      {
        echo_id = boost::lexical_cast<int>(local_args[0]);
        local_args.erase(local_args.begin());
      }
    catch(boost::bad_lexical_cast&) {}

    std::string echo_str = 
      boost::algorithm::join(local_args," ");
    mkt::out(echo_id).stream() << echo_str;
    if(newline) mkt::out(echo_id).stream() << std::endl;
  }
}

//Echo API implementation
namespace mkt
{
  MKT_DEF_SIGNAL(echo_func_map_changed_signal, echo_function_registered);
  MKT_DEF_SIGNAL(echo_func_map_changed_signal, echo_function_unregistered);
  MKT_DEF_SIGNAL(echo_signal, echo_pre_exec);
  MKT_DEF_SIGNAL(echo_signal, echo_post_exec);

  void echo_register(int64 id, const echo_func& f)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(echo_map_mutex_ref());
      echo_map_ref()[id] = f;
    }
    echo_function_registered()(id);
  }

  void echo_unregister(int64 id)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(echo_map_mutex_ref());
      echo_map_ref()[id] = echo_func();
    }
    echo_function_unregistered()(id);
  }

  void echo(const std::string& str)
  {
    using namespace std;
    using namespace boost;
    using namespace boost::algorithm;

    thread_info ti(BOOST_CURRENT_FUNCTION);

    const mkt_str echo_functions_varname("sys_echo_functions");
    mkt_str echo_functions_value = get_var(echo_functions_varname);
    mkt::argument_vector echo_functions_strings;
    split(echo_functions_strings, echo_functions_value, is_any_of(","), token_compress_on);

    //execute each echo function
    BOOST_FOREACH(mkt_str& echo_function_id_string, echo_functions_strings)
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

    echo_pre_exec()(echo_function_id, str);
    {
      unique_lock lock(echo_map_mutex_ref());
      if(echo_map_ref()[echo_function_id])
	echo_map_ref()[echo_function_id](str);
    }
    echo_post_exec()(echo_function_id, str);
  }

  //the default echo function
  void do_echo(std::ostream* os, const std::string& str)
  {
    using namespace boost;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    if(os && !get_var<bool>("sys_echo_quiet")) 
      {
        (*os) << str;
      }
  }

  out::out(int64 echo_id) : 
    _out(boost::iostreams::back_inserter(_result)), 
    _id(echo_id)
  {}

  out::~out()
  { 
    _out.flush(); 
    if(_id >= 0) 
      echo(_id, _result); 
    else echo(_result); 
  }

  void init_echo()
  {
    using namespace std;
    using namespace mkt;
    add_command("echo", ::echo,
		"Prints out all arguments after echo to standard out.");

    //setup echo so it outputs to console
    echo_register(0, boost::bind(mkt::do_echo, &std::cout, _1));
    echo_register(1, boost::bind(mkt::do_echo, &std::cerr, _1));
#ifdef MKT_USING_XMLRPC
    echo_register(2, mkt::do_remote_echo);
#endif
    set_var("sys_echo_functions", mkt_str("0, 2"));
  }

  void final_echo()
  {
    // TODO: any cleanup
  }

  bool echo_at_exit() { return _echo_atexit; }
}
