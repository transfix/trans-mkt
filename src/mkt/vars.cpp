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

#include <sstream>

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
  const mkt::var_string&        _vars_main_stackname("main");
  const mkt::variable_map_stacks_key& _vars_variable_map_stacks_key_default(
	     boost::make_tuple(mkt::thread_key(),
			       mkt::vars_main_stackname()));

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

  size_t var_map_stack_size(const mkt::vms_key& key = 
			    mkt::vms_key_def())
  {
    return _get_vars_data()->_var_map_stacks[key].size();
  }

  mkt::variable_map& var_map_ref(const mkt::var_context& context =
				 mkt::var_context())
  {
    size_t stack_depth = context.stack_depth();
    const mkt::vms_key& key = context.key();

    if(_get_vars_data()->_var_map_stacks[key].empty())
      _get_vars_data()->_var_map_stacks[key].push_back(mkt::variable_map());
    if(stack_depth > var_map_stack_size(key) - 1)
      throw mkt::vars_error("Invalid stack_depth");
    int64_t idx = var_map_stack_size(key) - 1 - stack_depth;
    return _get_vars_data()->_var_map_stacks[key][idx];
  }

  void var_map_push(const mkt::vms_key& key = 
		    mkt::vms_key_def())
  {
    std::vector<mkt::variable_map> &vms = 
      _get_vars_data()->_var_map_stacks[key];
    mkt::variable_map vm = var_map_ref(mkt::var_context(0,key));

    //collect the local var names from the current stack frame
    std::set<mkt::var_string> local_vars;
    BOOST_FOREACH(const mkt::variable_map::value_type& cur, vm)
      {
	if(cur.first[0]=='_')
	  local_vars.insert(cur.first);
      }

    //now remove them from the variable map copy
    BOOST_FOREACH(const mkt::var_string& var_name, local_vars)
      vm.erase(var_name);

    vms.push_back(vm);
  }

  void var_map_pop(const mkt::vms_key& key = 
		   mkt::vms_key_def())
  {
    std::vector<mkt::variable_map> &vms = 
      _get_vars_data()->_var_map_stacks[key];

    //copy all non local vars to one step up the stack so we
    //dont lose changes to globals. Also copy the local _ var to
    //act as a return value.
    mkt::variable_map vm = var_map_ref(mkt::var_context(0,key));
    std::set<mkt::var_string> local_vars;
    BOOST_FOREACH(const mkt::variable_map::value_type& cur, vm)
      {
	if(cur.first[0]=='_' && cur.first!="_")
	  local_vars.insert(cur.first);
      }

    //remove flagged local vars from the variable map copy
    BOOST_FOREACH(const mkt::var_string& var_name, local_vars)
      vm.erase(var_name);

    if(!vms.empty()) vms.pop_back();

    //copy to the new stack level
    mkt::variable_map lower_vm = var_map_ref(mkt::var_context(0,key));
    BOOST_FOREACH(const mkt::variable_map::value_type& cur, vm)
      {
	lower_vm[cur.first] = cur.second;
      }
    var_map_ref(mkt::var_context(0,key)) = lower_vm;
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
    bool val = mkt::has_var(args[1], mkt::var_context(1));
    mkt::ret_val(val);
  }

#if 0
  void pop(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::pop_vars();
    mkt::ret_val(mkt::vars_stack_size());
  }

  void push(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::push_vars();
    mkt::ret_val(mkt::vars_stack_size());
  }
#endif

  void set(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    //Note: for this function to work, we need to set variables in the stack
    //one up from here, because any local vars set in this stack frame var map will
    //get destroyed, and any non locals would get copied up the stack anyway.

    //If no arguments, just print a script that would set all variables in the 
    //stack frame above this one as they are. 
    if(args.size() == 1)
      {
        mkt::argument_vector vars = mkt::list_vars(mkt::var_context(1));
	std::stringstream ss;
        BOOST_FOREACH(const mkt::var_string& cur_var, vars)
          {
            ss << "set " << cur_var 
	       << " \"" << mkt::var(cur_var, mkt::var_context(1)) << "\"" << std::endl;
          }
	mkt::ret_val(ss.str());
	return;
      }
    else if(args.size() == 2)
      mkt::var(args[1], "", mkt::var_context(1)); //create an empty variable
    else
      mkt::var(args[1], args[2], mkt::var_context(1)); //actually do an assignment operation
    
    //Erase this stack frame so vars don't get promoted upwards and erase the
    //globals set via this command.
    //TODO: this all doesn't sit well with me... fix it!!!!!!!
    mkt::argument_vector vars = mkt::list_vars(mkt::var_context(0));
    BOOST_FOREACH(const mkt::var_string& cur_var, vars)
      {
	mkt::unset_var(cur_var, mkt::var_context(0));
      }

    //return the value that was set
    mkt::ret_val(mkt::var(args[1], mkt::var_context(1)));
  }

  void get(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(args.size()<2) 
      throw mkt::command_error("Missing variable argument.");
    if(!mkt::has_var(args[1]))
      throw mkt::vars_error("No such argument " + args[1]);
    size_t depth = args.size()>2 ? 
      (mkt::string_cast<size_t>(args[2])+1) : 1;
    mkt::ret_val(mkt::var(args[1], mkt::var_context(depth)));
  }

  void unset(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(args.size()<2)
      throw mkt::command_error("Missing arguments for unset");

    mkt::var_string ret = mkt::var(args[1], mkt::var_context(1));
    mkt::unset_var(args[1], mkt::var_context(1));
    
    //return the value that was unset
    mkt::ret_val(ret);
  }

  void stack_size(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::ret_val(mkt::vars_stack_size());
  }

  class init_commands
  {
  public:
    init_commands()
    {
      using namespace std;
      using namespace mkt;

      add_command("get", get, "get <varname>\nReturns the contents of a variable.");
      add_command("has_var", has_var, "Returns true or false whether the variable exists or not.");
      //add_command("pop", pop, "Pops the variable stack.");
      //add_command("push", push, "Pushes the variable stack.");
      add_command("set", set, "set [<varname> <value>]\n"
                  "Sets a variable to the value specified.  If none, prints all variables in the system.");
      add_command("stack_size", stack_size, 
		  "Returns the number of levels in the stack for the current thread.");
      add_command("unset", unset, "unset <varname>\nRemoves a variable from the system.");
    }
  } init_commands_static_init;
}

//variable API implementation
//TODO: need to verify var names such that they are strings like C identifiers
namespace mkt
{
  const var_string& vars_main_stackname() { return _vars_main_stackname; }
  const vms_key& vars_variable_map_stacks_key_default()
  {
    return _vars_variable_map_stacks_key_default; 
  }

  var_string var(const var_string& varname,
		 const var_context& context)
  {
    bool creating = false;
    var_string val;
    {
      unique_lock lock(var_map_mutex_ref());
      variable_map& vm = var_map_ref(context);
      if(vm.find(varname)==vm.end()) creating = true;
      val = vm[varname];
    }
    
    if(creating) var_changed(varname, context);
    return val;
  }

  void var(const var_string& varname, const var_string& val,
	   const var_context& context)
  {
    using namespace boost::algorithm;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(var_map_mutex_ref());
      var_string local_varname(varname); trim(local_varname);
      var_string local_val(val);
      variable_map& vm = var_map_ref(context);
      if(vm[local_varname] == local_val) return; //nothing to do
      vm[local_varname] = local_val;
    }
    var_changed(varname, context);
  }

  void unset_var(const var_string& varname,
		 const var_context& context)
  {
    using namespace boost::algorithm;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(var_map_mutex_ref());
      var_string local_varname(varname); trim(local_varname);
      var_map_ref(context).erase(local_varname);
    }
    var_changed(varname, context);
  }

  bool has_var(const var_string& varname,
	       const var_context& context)
  {
    using namespace boost::algorithm;
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);    
    var_string local_varname(varname); trim(local_varname);
    variable_map& vm = var_map_ref(context);
    if(vm.find(local_varname)==vm.end())
      return false;
    else return true;
  }

  argument_vector list_vars(const var_context& context)
  {
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);
    argument_vector vars;
    variable_map& vm = var_map_ref(context);
    BOOST_FOREACH(const variable_map::value_type& cur, vm)
      {
        var_string cur_varname = cur.first;
        boost::algorithm::trim(cur_varname);
        vars.push_back(cur_varname);
      }
    return vars;
  }

  void push_vars(const vms_key& key)
  {
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);
    var_map_push(key);
  }

  void pop_vars(const vms_key& key)
  {
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);
    var_map_pop(key);
  }

  size_t vars_stack_size(const vms_key& key)
  {
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);
    return var_map_stack_size(key);    
  }

  var_change_signal var_changed;

  argument_vector split(const var_string& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    return ::split_winmain(args);
  }

  var_string join(const argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    return boost::join(args," ");
  }

  var_string expand_vars(const var_string& args)
  {
    using namespace std;
    using namespace boost;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    var_string local_args(args);

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
                // http://bit.ly/1RLqTVV
                if(regex_search(local_args.begin(), 
                                local_args.end(), what, expr, flags))
                  {
                    //do the expansion
                    var_string var_name = string(what[2]);
                    if(!has_var(var_name)) continue; //TODO: what if this throws
                    found = true;
                    var_string expanded_arg = var(var_name);
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
