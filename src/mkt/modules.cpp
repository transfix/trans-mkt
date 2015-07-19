#include <mkt/modules.h>
#include <mkt/commands.h>
#include <mkt/types.h>

#include <boost/foreach.hpp>

// exceptions
namespace mkt
{
  MKT_DEF_EXCEPTION(modules_error);
}

// static data
namespace
{
  typedef void* handle;

  struct modules_data
  {
    std::map<mkt::var_string, handle>     _module_map;
    mkt::argument_vector                  _modules_search_path;
    mkt::mutex                            _modules_mutex;
  };
  modules_data                           *_modules_data = 0;
  bool                                    _modules_atexit = false;
  
  void _modules_cleanup()
  {
    _modules_atexit = true;
    delete _modules_data;
    _modules_data = 0;
  }

  modules_data* _get_modules_data()
  {
    if(_modules_atexit)
      throw mkt::modules_error("Already at program exit!");

    if(!_modules_data)
      {
	_modules_data = new modules_data;
	std::atexit(_modules_cleanup);
      }

    if(!_modules_data)
      throw mkt::modules_error("Missing static modules data!");

    return _modules_data;
  }

  std::map<mkt::var_string, handle>& module_map()
  {
    return _get_modules_data()->_module_map;
  }

  mkt::argument_vector& modules_search_path()
  {
    return _get_modules_data()->_modules_search_path;
  }

  mkt::mutex& modules_mutex()
  {
    return _get_modules_data()->_modules_mutex;
  }
}

// module related commands
namespace
{
  void import(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove command string
    if(local_args.empty()) throw mkt::command_error("Missing module name.");
    mkt::import_module(local_args[0]);
  }

  void expunge(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove command string
    if(local_args.empty()) throw mkt::command_error("Missing module name.");
    mkt::expunge_module(local_args[0]);
  }

  void loaded_modules(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::argument_vector lm = mkt::loaded_modules();
    std::stringstream ss;
    BOOST_FOREACH(const std::string& module_name, lm)
      {
	ss << "\"" << module_name << "\"" << std::endl;
      }
    mkt::ret_val(ss.str());
  }

  class init_commands
  {
  public:
    init_commands()
    {
      using namespace std;
      using namespace mkt;

      add_command("import", import, "import <module>\nLoads a module into this process.");
      add_command("expunge", expunge, "expunge <module>\nUnloads a module from this process.");
      add_command("loaded_modules", loaded_modules, "Returns list of loaded modules.");
    }
  } init_commands_static_init;
}

// Modules  API implementation
namespace mkt
{
  void init_modules() {}

  void import_module(const var_string& name)
  {
    throw mkt::null_implementation_error("no impl");
  }

  argument_vector loaded_modules()
  {
    argument_vector av;
    throw mkt::null_implementation_error("no impl");
    return av;
  }

  void expunge_module(const var_string& name)
  {
    throw mkt::null_implementation_error("no impl");
  }
}
