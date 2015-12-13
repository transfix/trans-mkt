#include <mkt/vars.h>
#include <mkt/threads.h>
#include <mkt/exceptions.h>
#include <mkt/echo.h>
#include <mkt/commands.h>
#include <mkt/utils.h>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/regex.hpp>
#include <boost/array.hpp>
#include <boost/program_options.hpp>

#include <boost/program_options/parsers.hpp>
#include <cctype>

#include <sstream>
#include <algorithm>

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

//This module's static data
namespace
{
  struct vars_data
  {
    mkt::variable_maps          _var_maps;

    //big lock on the variable system- every thread needs to wait to access its potentially
    //thread local variable map stack...
    //TODO: lets change this when it matters! - 20150604
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
      throw mkt::vars_error("error allocating static data");

    return _vars_data;
  }

  mkt::variable_map& var_map_ref(const mkt::mkt_str& key = mkt::thread_key())
  {
    return _get_vars_data()->_var_maps[key];
  }

  // duplicates the requested variable map
  mkt::variable_map var_copy_map(const mkt::mkt_str& key = mkt::thread_key(),
                                 bool no_locals = true)
  {
    mkt::variable_map vm = var_map_ref(key);

    if(no_locals)
      {
        //collect the local var names from the current var map
        std::set<mkt::mkt_str> local_vars;
        for(auto&& cur : vm) 
          if(cur.first[0]=='_')
            local_vars.insert(cur.first);
        
        //now remove them from the variable map copy
        for(auto&& var_name : local_vars)
          vm.erase(var_name);
      }

    return vm;
  }

  // copies the map at the specified from_key to the one at to_key
  void var_copy_map(const mkt::mkt_str& to_key,
                    const mkt::mkt_str& from_key,
                    bool no_locals = true,
                    bool only_if_newer = true)
  {
    mkt::variable_map from_map = var_copy_map(from_key, no_locals);
    mkt::variable_map& to_map = var_map_ref(to_key);

    for(auto&& cur : from_map)
      {
	if(!cur.second) continue;
        const mkt::variable_value& from_val = *cur.second;
        if(only_if_newer && to_map.find(cur.first) != to_map.end())
          {
	    if(!to_map[cur.first]) continue;
            const mkt::variable_value& to_val = *to_map[cur.first];
            if(to_val.mod_time() > from_val.mod_time())
              continue;
          }
        to_map[cur.first] = cur.second;
      }
  }

  inline mkt::mutex& var_map_mutex_ref()
  {
    return _get_vars_data()->_var_map_mutex;
  }

  inline void var_check_name(const mkt::mkt_str& varname)
  {
    mkt::check_identifier<mkt::vars_error>(varname);
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
    bool val = mkt::has_var(args[1]); // TODO: thread key
    mkt::ret_val(val);
  }

  void set(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    // TODO: thread key

    if(args.size() == 1)
      {
        mkt::argument_vector vars = mkt::list_vars();
        std::stringstream ss;
        // TODO: commands to read var attributes
        for(auto&& cur_var : vars)
          ss << "set " << cur_var 
             << " \"" << mkt::get_var(cur_var) << "\"" << std::endl;
        mkt::ret_val(ss.str());
        return;
      }
    else if(args.size() == 2)
      mkt::set_var(args[1], mkt::mkt_str("")); //create an empty variable
    else
      mkt::set_var(args[1], args[2]); //actually do an assignment operation
    
    //return the value that was set
    mkt::ret_val(mkt::get_var(args[1]));
  }

  void get(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(args.size()<2) 
      throw mkt::command_error("Missing variable argument.");
    if(!mkt::has_var(args[1]))
      throw mkt::vars_error("No such argument " + args[1]);
    // TODO: thread key
    mkt::ret_val(mkt::get_var(args[1]));
  }

  void unset(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(args.size()<2)
      throw mkt::command_error("Missing arguments for unset");

    // TODO: thread key

    mkt::mkt_str ret = mkt::get_var(args[1]);
    mkt::unset_var(args[1]);
    
    //return the value that was unset
    mkt::ret_val(ret);
  }
}

//variable API implementation
namespace mkt
{
  MKT_DEF_SIGNAL(var_change_signal, var_changed);

  // copy vars from caller thread to new thread on init
  void vars_copy_on_thread_init(const mkt_str& caller_key,
                                const mkt_str& this_key)
  {
    vars_copy(this_key, caller_key);
  }

  // copy vars from new thread to caller thread on finalize
  void vars_copy_on_thread_final(const mkt_str& caller_key,
                                 const mkt_str& this_key)
  {
    vars_copy(caller_key, this_key);
  }

  void init_vars()
  {
    using namespace std;
    using namespace mkt;

    add_command("get", ::get, "get <varname>\nReturns the contents of a variable.");
    add_command("has_var", ::has_var, "Returns true or false whether the variable exists or not.");
    add_command("set", ::set, "set [<varname> <value>]\n"
                "Sets a variable to the value specified.  If none, prints all variables in the system.");
    add_command("unset", ::unset, "unset <varname>\nRemoves a variable from the system.");

    thread_initialized().connect(vars_copy_on_thread_init);
    thread_finalized().connect(vars_copy_on_thread_final);
  }

  void final_vars()
  {
    remove_command("unset");
    remove_command("set");
    remove_command("has_var");
    remove_command("get");

    thread_initialized().disconnect(vars_copy_on_thread_init);
    thread_finalized().disconnect(vars_copy_on_thread_final);

    delete _vars_data;
    _vars_data = 0;
  }

  mkt_str get_var(const mkt_str& varname,
                  const mkt_str& t_key)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    bool creating = false;
    mkt_str val;
    {
      unique_lock lock(var_map_mutex_ref());
      variable_map& vm = var_map_ref(t_key);
      if(vm.find(varname)==vm.end()) creating = true;
      if(creating) var_check_name(varname);
      if(!vm[varname]) vm[varname].reset(new variable_value);
      val = vm[varname]->data();
    }
    
    if(creating) var_changed()(varname, t_key);
    return val;
  }

  void set_var(const mkt_str& varname, const mkt_str& val,
               const mkt_str& t_key)
  {
    using namespace boost::algorithm;
    thread_info ti(BOOST_CURRENT_FUNCTION);

    var_check_name(varname);
    {
      unique_lock lock(var_map_mutex_ref());
      mkt_str local_varname(varname); trim(local_varname);
      variable_map& vm = var_map_ref(t_key);
      if(vm.find(local_varname) != vm.end() &&
	 vm[local_varname] &&
         vm[local_varname]->data() == val) return; //nothing to do
      variable_value_ptr& v_val_ptr = vm[local_varname];
      if(!v_val_ptr) v_val_ptr.reset(new variable_value);
      v_val_ptr->data(val);
    }
    var_changed()(varname, t_key);
  }

  void unset_var(const mkt_str& varname,
                 const mkt_str& t_key)
  {
    using namespace boost::algorithm;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(var_map_mutex_ref());
      mkt_str local_varname(varname); trim(local_varname);
      var_map_ref(t_key).erase(local_varname);
    }
    var_changed()(varname, t_key);
  }

  bool has_var(const mkt_str& varname,
               const mkt_str& t_key)
  {
    using namespace boost::algorithm;
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);    
    mkt_str local_varname(varname); trim(local_varname);
    variable_map& vm = var_map_ref(t_key);
    if(vm.find(local_varname)==vm.end())
      return false;
    else return true;
  }

  argument_vector list_vars(const mkt_str& t_key)
  {
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);
    argument_vector vars;
    variable_map& vm = var_map_ref(t_key);
    for(auto&& cur : vm)
      {
        mkt_str cur_varname = cur.first;
        boost::algorithm::trim(cur_varname);
        vars.push_back(cur_varname);
      }
    return vars;
  }

  void vars_copy(const mkt_str& to_t_key,
                 const mkt_str& from_t_key,
                 bool no_locals,
                 bool only_if_newer)
  {
    unique_lock lock(var_map_mutex_ref());
    thread_info ti(BOOST_CURRENT_FUNCTION);
    var_copy_map(to_t_key, from_t_key, no_locals, only_if_newer);
  }

  argument_vector split(const mkt_str& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    return ::split_winmain(args);
  }

  mkt_str join(const argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    return boost::join(args," ");
  }

  // TODO: thread key
  mkt_str expand_vars(const mkt_str& args)
  {
    using namespace std;
    using namespace boost;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt_str local_args(args);

    boost::array<regex, 2> exprs = 
      { 
        regex("\\s*(\\$(\\w+))\\s*"), 
        regex("\\$\\{(\\w+)\\}") 
      };

    bool found;
    do
      {
        found = false;
        for(auto&& expr : exprs)
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
                    mkt_str var_name = string(what[2]);
                    if(!has_var(var_name)) continue; //TODO: what if this throws
                    found = true;
                    mkt_str expanded_arg = get_var(var_name);
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

  ptime get_var_mod_time(const mkt_str& varname,
			 const mkt_str& t_key)
  {
    using namespace boost;
    thread_info ti(BOOST_CURRENT_FUNCTION);
    ptime mod_time;
    {
      unique_lock lock(var_map_mutex_ref());
      variable_map& vm = var_map_ref(t_key);
      if(vm.find(varname)==vm.end())
	throw vars_error(str(format("%1: no such variable %2 (%3)")
			     % BOOST_CURRENT_FUNCTION
			     % varname
			     % t_key));
      variable_value_ptr& v_val_ptr = vm[varname];
      if(!v_val_ptr) v_val_ptr.reset(new variable_value);
      mod_time = v_val_ptr->mod_time();
    }
    
    return mod_time;
  }

  bool vars_at_exit() { return _vars_atexit; }
}
