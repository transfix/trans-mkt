#include <mkt/modules.h>
#include <mkt/commands.h>
#include <mkt/types.h>
#include <mkt/echo.h>
#include <mkt/utils.h>

#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/function.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/regex.hpp>
#include <boost/array.hpp>

#ifdef __UNIX__
#include <dlfcn.h>
#endif

// TODO: support Windows

// exceptions
namespace mkt
{
  MKT_DEF_EXCEPTION(modules_error);
}

// static data
namespace
{
  typedef void* handle;
  typedef void (*func)();

  struct module_data
  {
    mkt::mkt_str                            filename;
    handle                                  h;
    func                                    init;
    func                                    final;
  };

  typedef std::map<mkt::mkt_str, module_data> module_map_t;

  struct modules_data
  {
    module_map_t                            _module_map;
    mkt::mutex                              _modules_mutex;
  };
  modules_data                             *_modules_data = 0;
  bool                                      _modules_atexit = false;
  
  void _modules_cleanup()
  {
    _modules_atexit = true;

    if(!_modules_data)
      throw mkt::modules_error("Missing static modules data!");

    //Expunge all loaded modules before deleting local static data. It is the module's modules
    //responsibility to call final functions of loaded modules.
    BOOST_FOREACH(const module_map_t::value_type& cur, 
		  _modules_data->_module_map)
      {
	const mkt::mkt_str& mod_name = cur.first;
	const module_data& md = cur.second;
	std::cout << "Unloading module " << mod_name << std::endl;
	md.final();
	if(::dlclose(md.h))
	  std::cout << str(boost::format("Error closing module %1%: %2%\n")
			   % mod_name
			   % ::dlerror()) << std::endl;
      }

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

  module_map_t& module_map()
  {
    return _get_modules_data()->_module_map;
  }

  mkt::mutex& modules_mutex()
  {
    return _get_modules_data()->_modules_mutex;
  }

  module_data module(const mkt::mkt_str& mod_name)
  {
    using namespace mkt;
    using namespace boost;
    module_data md;

    {
      mkt::unique_lock lock(modules_mutex());
      if(module_map().find(mod_name) !=
	 module_map().end())
	throw modules_error(str(format("No such module %1%")
				% mod_name));
      md = module_map()[mod_name];
    }
    
    return md;
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
    mkt::ret_val(mkt::import_module(local_args[0]));
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
    mkt::ret_val(mkt::join(lm));
  }
}

// Modules API implementation
namespace mkt 
{
  MKT_DEF_MAP_CHANGE_SIGNAL(modules_changed);
  MKT_DEF_MAP_CHANGE_SIGNAL(module_pre_init);
  MKT_DEF_MAP_CHANGE_SIGNAL(module_post_init);
  MKT_DEF_MAP_CHANGE_SIGNAL(module_pre_final);
  MKT_DEF_MAP_CHANGE_SIGNAL(module_post_final);

  void init_modules() 
  {
    using namespace std;
    using namespace mkt;

    add_command("import", ::import, "import <module>\nLoads a module into this process.");
    add_command("expunge", ::expunge, "expunge <module>\nUnloads a module from this process.");
    add_command("loaded_modules", ::loaded_modules, "Returns list of loaded modules.");

    //set the default module path to be the CWD
    if(!has_var("sys_ld"))
      set_var("sys_ld", ".");
  }

  void final_modules()
  {
    //TODO: cleanup, remove modules and commands, etc.
  }

  mkt_str import_module(const mkt_str& mod_name)
  {
    using namespace std;
    using namespace boost;
    using namespace boost::algorithm;
    using namespace boost::filesystem;
    
    thread_info ti(BOOST_CURRENT_FUNCTION);

    if(!valid_module_name(mod_name))
      throw modules_error("Invalid module name " + mod_name);

    // throw an error if we have already imported this module name
    {
      unique_lock lock(modules_mutex());
      if(module_map().find(mod_name) !=
	 module_map().end())
	throw modules_error(str(format("Already loaded %1%")
				% mod_name));
    }

    // check all known locations of possible modules
    mkt_str mod_path = get_var("sys_ld");
    argument_vector paths;
    split(paths, mod_path, is_any_of(":"), token_compress_on);
    path module_path;
    bool found = false;
    argument_vector searched_paths;
    BOOST_FOREACH(mkt_str& cur_path, paths)
      {
	trim(cur_path);
	path p(cur_path);

	boost::array<mkt_str, 5> mod_filenames =
	  {
	    "lib" + mod_name + ".so",
	    mod_name + ".so",
	    mod_name + ".mkt",
	    mod_name + ".dll",
	    mod_name
	  };

	BOOST_FOREACH(mkt_str& mod_filename, mod_filenames)
	  {
	    module_path = p / mod_filename;
	    searched_paths.push_back(mkt_str(module_path.c_str()));
	    if(exists(module_path))
	      {
		found = true;
		break;
	      }
	  }

	if(found) break;
      }

    if(!found)
      throw modules_error(str(format("%1%: Could not find module %2% from %3%")
			      % BOOST_CURRENT_FUNCTION
			      % mod_name
			      % boost::join(searched_paths,", ")));

    module_data md;

#ifdef __UNIX__
    handle h = ::dlopen(module_path.c_str(), RTLD_NOW);
    if(!h) throw modules_error(str(format("Error opening %1%: %2%")
				   % string(module_path.c_str())
				   % ::dlerror()));
    md.h = h;
    void* init_func = ::dlsym(h, string("mkt_init_" + mod_name).c_str());
    if(!init_func)
      {
	std::string err(::dlerror());
	::dlclose(md.h);
	throw modules_error(str(format("Could not load function mkt_init_%1% from library %2%: %3%")
				% mod_name
				% module_path.c_str()
				% err));
      }
    md.init = func(init_func);
    void* final_func = ::dlsym(h, string("mkt_final_" + mod_name).c_str());
    if(!final_func)
      {
	std::string err(::dlerror());
	::dlclose(md.h);
	throw modules_error(str(format("Could not load function mkt_final_%1% from library %2%: %3%")
				% mod_name
				% module_path.c_str()
				% err));
      }
    md.final = func(final_func);
    
    md.filename = mkt_str(module_path.c_str());

    {
      unique_lock lock(modules_mutex());
      module_map()[mod_name] = md;
    }
    modules_changed()(mod_name);
#else
    throw not_implemented_error(str(format("%1%: Only UNIX dynamic library support for now...")
				    % BOOST_CURRENT_FUNCTION));
#endif

    // call module init function
    module_pre_init()(mod_name);
    md.init();
    module_post_init()(mod_name);

    return mkt_str(module_path.c_str());
  }

  argument_vector loaded_modules()
  {
    argument_vector av;

    {
      unique_lock lock(modules_mutex());
      BOOST_FOREACH(const module_map_t::value_type& cur, 
		    module_map())
	{
	  av.push_back(cur.first);
	}
    }

    return av;
  }

  void expunge_module(const mkt_str& mod_name)
  {
    using namespace boost;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    
    if(!is_loaded_module(mod_name))
      throw modules_error("No such module " + mod_name);    

    module_data md;
    {
      unique_lock lock(modules_mutex());
      md = module_map()[mod_name];
      module_map().erase(mod_name);
    }

    // call module final function
    module_pre_final()(mod_name);
    md.final();
    module_post_final()(mod_name);
    if(::dlclose(md.h))
      throw modules_error(str(format("Error closing module %1%: %2%\n")
			      % mod_name
			      % ::dlerror()));
  }

  bool is_loaded_module(const mkt_str& mod_name)
  {
    unique_lock lock(modules_mutex());
    if(module_map().find(mod_name) ==
       module_map().end())
      return false;
    return true;
  }

  bool valid_module_name(const std::string& str)
  {
    return valid_identifier(str);
  }

  bool modules_at_exit() { return _modules_atexit; }
}
