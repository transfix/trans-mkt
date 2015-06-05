#include <mkt/vars.h>
#include <mkt/threads.h>
#include <mkt/exceptions.h>
#include <mkt/echo.h>
#include <mkt/commands.h>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/regex.hpp>
#include <boost/array.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>

#include <boost/program_options/parsers.hpp>
#include <cctype>

//split_winmain is ifdef'ed out on Linux, so lets just add it here.
namespace
{
    // Take a command line string and splits in into tokens, according
    // to the rules windows command line processor uses.
    // 
    // The rules are pretty funny, see
    //    http://article.gmane.org/gmane.comp.lib.boost.user/3005
    //    http://msdn.microsoft.com/library/en-us/vccelng/htm/progs_12.asp
    std::vector<std::string> split_winmain(const std::string& input)
    {
        std::vector<std::string> result;

        std::string::const_iterator i = input.begin(), e = input.end();
        for(;i != e; ++i)
            if (!isspace((unsigned char)*i))
                break;
       
        if (i != e) {
   
            std::string current;
            bool inside_quoted = false;
            bool empty_quote = false;
            int backslash_count = 0;
            
            for(; i != e; ++i) {
                if (*i == '"') {
                    // '"' preceded by even number (n) of backslashes generates
                    // n/2 backslashes and is a quoted block delimiter
                    if (backslash_count % 2 == 0) {
                        current.append(backslash_count / 2, '\\');
                        empty_quote = inside_quoted && current.empty();
                        inside_quoted = !inside_quoted;
                        // '"' preceded by odd number (n) of backslashes generates
                        // (n-1)/2 backslashes and is literal quote.
                    } else {
                        current.append(backslash_count / 2, '\\');                
                        current += '"';                
                    }
                    backslash_count = 0;
                } else if (*i == '\\') {
                    ++backslash_count;
                } else {
                    // Not quote or backslash. All accumulated backslashes should be
                    // added
                    if (backslash_count) {
                        current.append(backslash_count, '\\');
                        backslash_count = 0;
                    }
                    if (isspace((unsigned char)*i) && !inside_quoted) {
                        // Space outside quoted section terminate the current argument
                        result.push_back(current);
                        current.resize(0);
                        empty_quote = false; 
                        for(;i != e && isspace((unsigned char)*i); ++i)
                            ;
                        --i;
                    } else {                  
                        current += *i;
                    }
                }
            }

            // If we have trailing backslashes, add them
            if (backslash_count)
                current.append(backslash_count, '\\');
        
            // If we have non-empty 'current' or we're still in quoted
            // section (even if 'current' is empty), add the last token.
            if (!current.empty() || inside_quoted || empty_quote)
                result.push_back(current);        
        }
        return result;
    }
}

//This module's exceptions
namespace mkt
{
  MKT_DEF_EXCEPTION(vars_error);
}

//This module's static data
namespace
{
  struct vars_data
  {
    mkt::variable_map_stacks    _var_map_stacks;

    //big lock on the variable system- every thread needs to wait to access its potentially
    //thread local variable map stack...
    //TODO: lets change this when it matters! - 20150604
    mkt::mutex                  _var_map_mutex;
  };
  vars_data                    *_vars_data = 0;
  bool                          _vars_atexit = false;
  const std::string&            _vars_main_stackname("main");

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

  using namespace boost::tuples;

  mkt::variable_map& var_map_ref(const mkt::variable_map_stacks_key& key = 
				 mkt::vars_variable_map_stacks_key_default())
  {
    if(_get_vars_data()->_var_map_stacks[key].empty())
      _get_vars_data()->_var_map_stacks[key].push_back(mkt::variable_map());
    return _get_vars_data()->_var_map_stacks[key].back();
  }

  void var_map_push(const mkt::variable_map_stacks_key& key = 
		    mkt::vars_variable_map_stacks_key_default())
  {
    _get_vars_data()->_var_map_stacks[key].push_back(var_map_ref());
  }

  void var_map_pop(const mkt::variable_map_stacks_key& key = 
		   mkt::vars_variable_map_stacks_key_default())
  {
    std::vector<mkt::variable_map> &vms = _get_vars_data()->_var_map_stacks[key];
    if(!vms.empty())
      vms.pop_back();
  }

  size_t var_map_stack_size(const mkt::variable_map_stacks_key& key = 
			    mkt::vars_variable_map_stacks_key_default())
  {
    return _get_vars_data()->_var_map_stacks[key].size();
  }

  mkt::mutex& var_map_mutex_ref()
  {
    return _get_vars_data()->_var_map_mutex;
  }
}

//Variable related commands
namespace
{
  void has_var(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(args.size()<2) 
      throw mkt::command_error("Missing variable argument.");
    mkt::out().stream() << (mkt::has_var(args[1]) ? "true" : "false") << std::endl;
  }

  void pop(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::pop_vars();
  }

  void push(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::push_vars();
  }

  void set(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(args.size() == 1) //just print all variables
      {
        mkt::argument_vector vars = mkt::list_vars();
        BOOST_FOREACH(const std::string& cur_var, vars)
          {
            mkt::out().stream() << "set " << cur_var << " \"" << mkt::var(cur_var) << "\"" << std::endl;
          }
      }
    else if(args.size() == 2)
      mkt::var(args[1], ""); //create an empty variable
    else
      mkt::var(args[1], args[2]); //actually do an assignment operation
  }

  void unset(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(args.size()<2)
      throw mkt::command_error("Missing arguments for unset");
    mkt::unset_var(args[1]);
  }

  class init_commands
  {
  public:
    init_commands()
    {
      using namespace std;
      using namespace mkt;

      add_command("has_var", has_var, "Returns true or false whether the variable exists or not.");
      add_command("pop", pop, "Pops the variable stack.");
      add_command("push", push, "Pushes the variable stack.");
      add_command("set", set, "set [<varname> <value>]\n"
                  "Sets a variable to the value specified.  If none, prints all variables in the system.");
      add_command("unset", unset, "unset <varname>\nRemoves a variable from the system.");
    }
  } init_commands_static_init;
}

//variable API implementation
namespace mkt
{
  const std::string& vars_main_stackname() { return _vars_main_stackname; }
  const variable_map_stacks_key& vars_variable_map_stacks_key_default()
  {
    return variable_map_stacks_key(threads_default_keyname(),
				   vars_main_stackname());
  }

  std::string var(const std::string& varname,
		  const variable_map_stacks_key& key)
  {
    bool creating = false;
    std::string val;
    {
      unique_lock lock(var_map_mutex_ref());
      if(var_map_ref(key).find(varname)==var_map_ref(key).end()) creating = true;
      val = var_map_ref(key)[varname];
    }
    
    if(creating) var_changed(varname);
    return val;
  }

  void var(const std::string& varname, const std::string& val,
	   const variable_map_stacks_key& key)
  {
    using namespace boost::algorithm;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(var_map_mutex_ref());
      std::string local_varname(varname); trim(local_varname);
      std::string local_val(val); trim(local_val);
      if(var_map_ref(key)[local_varname] == local_val) return; //nothing to do
      var_map_ref(key)[local_varname] = local_val;
    }
    var_changed(varname);
  }

  void unset_var(const std::string& varname,
		 const variable_map_stacks_key& key)
  {
    using namespace boost::algorithm;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(var_map_mutex_ref());
      std::string local_varname(varname); trim(local_varname);
      var_map_ref(key).erase(local_varname);
    }
    var_changed(varname);
  }

  bool has_var(const std::string& varname,
	       const variable_map_stacks_key& key)
  {
    using namespace boost::algorithm;
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);    
    std::string local_varname(varname); trim(local_varname);
    if(var_map_ref(key).find(local_varname)==var_map_ref(key).end())
      return false;
    else return true;
  }

  argument_vector list_vars(const variable_map_stacks_key& key)
  {
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);
    argument_vector vars;
    BOOST_FOREACH(const variable_map::value_type& cur, var_map_ref(key))
      {
        std::string cur_varname = cur.first;
        boost::algorithm::trim(cur_varname);
        vars.push_back(cur_varname);
      }
    return vars;
  }

  void push_vars(const variable_map_stacks_key& key)
  {
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);
    var_map_push(key);
  }

  void pop_vars(const variable_map_stacks_key& key)
  {
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);
    var_map_pop(key);
  }

  size_t vars_stack_size(const variable_map_stacks_key& key)
  {
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);
    return var_map_stack_size(key);    
  }

  map_change_signal var_changed;

  argument_vector split(const std::string& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    return ::split_winmain(args);
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
