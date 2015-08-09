#include <mkt/commands.h>
#include <mkt/app.h>
#include <mkt/threads.h>
#include <mkt/vars.h>
#include <mkt/echo.h>

#ifdef MKT_INTERACTIVE
#ifdef __WINDOWS__
#include <editline_win/readline.h>
#else
  #include <editline/readline.h>
#endif
#endif

#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/current_function.hpp>
#include <boost/array.hpp>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstdlib>

//This module's exceptions
namespace mkt
{
  MKT_DEF_EXCEPTION(file_error);
}

//This module's static data
namespace
{
  struct commands_data
  {
    mkt::command_map  _commands;
    mkt::mutex        _commands_lock;
  };
  commands_data     *_commands_data = 0;
  bool               _commands_atexit = false;
  bool               _commands_post_static_init = false;

  void _cmds_cleanup()
  {
    _commands_atexit = true;
    delete _commands_data;
    _commands_data = 0;
  }

  commands_data* _get_commands_data()
  {
    if(_commands_atexit) 
      throw mkt::command_error("Already at program exit!");

    if(!_commands_data)
      {
        _commands_data = new commands_data;
        std::atexit(_cmds_cleanup);
      }

    if(!_commands_data) 
      throw mkt::command_error("Missing static commands data!");

    return _commands_data;
  }

  //use this to access command map
  mkt::command_map& cmds()
  {
    return _get_commands_data()->_commands;
  }

  //use this to access command map mutex
  mkt::mutex& cmds_lock()
  {
    return _get_commands_data()->_commands_lock;
  }

  void cmd_restore_prompt()
  {
    if(!mkt::has_var("PS1") || mkt::var("PS1") == "")
      mkt::var("PS1", "mkt> "); //default prompt
  }
}

//Default system commands
namespace
{
  void cmd(const mkt::argument_vector& args)
  {
#ifdef MKT_INTERACTIVE
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    cmd_restore_prompt();

    char *line;
    while(line = readline(mkt::expand_vars(mkt::var("PS1")).c_str()))
      {
	mkt::mkt_str str_line(line);
        add_history(line);
        free(line);

        if(str_line == "exit" || str_line == "quit") break;

        try
          {
            mkt::ex(str_line);
          }
        catch(mkt::exception& e)
          {
            if(!e.what_str().empty()) 
              mkt::out().stream() 
                << "Error: " 
                << e.what_str() 
                << endl;
          }

	//output the contents of the retval to the output stream
	mkt::var_string rv = mkt::var("_");
	mkt::out().stream() << "_ <- \"" << rv << "\"" << endl;
	mkt::ret_val(mkt::var_string());

	cmd_restore_prompt();
      }
#else
    throw mkt::command_error("interactive mode unsupported");
#endif
  }

  void file(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove command string
    if(local_args.empty()) throw mkt::command_error("Missing file name.");

    mkt::exec_file(local_args);
  }

  void help(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    mkt::argument_vector prog_args = mkt::argv();
    mkt::mkt_str prog_name = !prog_args.empty() ? prog_args[0] : "mkt";

    stringstream ss;
    ss << endl;
    ss << "Version: " << mkt::version() << endl;
    ss << "Usage: " << prog_name << " <command> <command args>" << endl << endl;

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove command string

    //if no arguments, print all of the commands available.
    if(local_args.empty())
      {
        mkt::argument_vector cmd_list = mkt::get_commands(); 

        //make sure we have a multiple of 3 for table printing 3 columns
        while(cmd_list.size() % 3) cmd_list.push_back(string());
        
        ss << "Available commands:" << endl << endl;
        for(size_t i = 0; i < cmd_list.size(); i+=3)
          {
            ss
              << setw(25) << cmd_list[i+0] 
              << setw(25) << cmd_list[i+1] 
              << setw(25) << cmd_list[i+2]
              << endl;
          }
        
	ss
          << "\nUse the command \"help <command name>\" to get a description of each command" << endl;
      }
    else
      {
	if(!mkt::has_command(local_args[0]))
	  throw mkt::command_error("No such command " + local_args[0]);
	ss << mkt::get_command_description(local_args[0]) << endl;
      }
    
    mkt::ret_val(ss.str());
  }

  void repeat(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(args.size()<3) throw mkt::command_error("Missing arguments.");

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string
    trim(local_args[0]);
    int times = -1;
    try
      {
        times = lexical_cast<int>(local_args[0]);
      }
    catch(boost::bad_lexical_cast&)
      {
        //invalid <times> string
        throw mkt::command_error("Invalid argument for repeat.");
      }
    local_args.erase(local_args.begin()); //remove <times> string

    //now repeat...
    for(int i = 0; i < times; i++)
      mkt::exec(local_args);
  }

  void serial(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string to form the thread name

    //split the argument vector into separate command strings via 'then' keywords
    vector<mkt::argument_vector> split_args;
    mkt::argument_vector cur_command;
    BOOST_FOREACH(const string& cur, local_args)
      {
        if(cur != "then")
          cur_command.push_back(cur);
        else
          {
            split_args.push_back(cur_command);
            cur_command.clear();
          }
      }

    if(!cur_command.empty())
      split_args.push_back(cur_command);
    else if(split_args.empty())
      throw mkt::command_error("Missing command to execute.");
    
    //now execute the split commands serially
    BOOST_FOREACH(const mkt::argument_vector& cur, split_args)
      mkt::exec(cur);
  }

  void version(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::ret_val(mkt::version());
  }
  
  //commands TODO:
  //macro - command list, creates a new command. like serial but it doesnt call.  uses 'then' keyword.
  //        $argc represents number of macro arguments when called, and $argv_0000 ... $argv_9999 represents the args 
  //stack - keep a variable stack, add commands to read from the stack.  use $0...$n for arguments, make them thread private
  //handle string sourrounded by `backticks` as a shell command who's output ends up as the string contents
  //read url into variable
  //read file into variable
  //eval - commands from variable, run embedded lua program and store it's console output into a variable
  //write output to file (or remote resource?)
  //current_time

  //use muparser for math expression evaluation
  //cpp-netlib for http requests & http server

  //Default set of commands.
  class init_commands
  {
  public:
    init_commands()
    {
      using namespace std;
      using namespace mkt;
      
#ifdef MKT_INTERACTIVE
      add_command("cmd", cmd, "starts an interactive command prompt.");
#endif
      add_command("file", file,
                  "file <file path> - \n"
                  "Executes commands listed in a file, line by line sequentially.");
      add_command("help", help, "Prints command list.");
      add_command("repeat", repeat, "repeat <num times> <command> -\nRepeat command.");
      add_command("serial", serial, "Execute commands serially separated by a 'then' keyword.");
      add_command("version", version, "Returns the system version string.");
    }
  } init_commands_static_init;
}

//API implementation
namespace mkt
{
  map_change_signal   command_added;
  map_change_signal   command_removed;
  command_exec_signal command_pre_exec;
  command_exec_signal command_post_exec;

  void init_commands()
  {
    _commands_post_static_init = true; //safe to fire signals
  }

  void add_command(const mkt_str& name,
                   const command_func& func,
                   const mkt_str& desc)
  {
    using namespace boost;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(cmds_lock());
      cmds()[name] = make_tuple(func, desc);
    }
    if(_commands_post_static_init)
      command_added(name);
  }

  void remove_command(const mkt_str& name)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(cmds_lock());
      cmds().erase(name);
    }
    if(_commands_post_static_init)
      command_removed(name);
  }

  argument_vector get_commands()
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    shared_lock lock(cmds_lock());    
    argument_vector av;
    BOOST_FOREACH(command_map::value_type& cur, cmds())
      av.push_back(cur.first);
    return av;
  }

  mkt_str get_command_description(const mkt_str& name)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    shared_lock lock(cmds_lock());    
    return cmds()[name].get<1>();
  }
  
  bool has_command(const mkt_str& name)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    shared_lock lock(cmds_lock());
    return cmds().find(name) != cmds().end();
  }

  //TODO: co-routines by using different stack names.
  void exec(const argument_vector& args)
  {
    using namespace boost;

    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(args.empty()) throw command_error("Missing command string");

    //do variable expansion
    argument_vector local_args = args;
    BOOST_FOREACH(mkt_str& arg, local_args)
      arg = expand_vars(arg);

    //get the command function
    command cmd;
    {
      unique_lock lock(cmds_lock());
      mkt_str cmd_str = local_args[0];
      if(cmds().find(cmd_str)==cmds().end())
        throw command_error(str(format("Invalid command: %1%") % cmd_str));
      cmd = cmds()[cmd_str];
    }

    //finally call it
    push_vars();
    var("_", ""); //reset the return val for this stack frame
    command_pre_exec(local_args, vars_main_stackname());
    cmd.get<0>()(local_args);
    command_post_exec(local_args, vars_main_stackname());
    pop_vars();
  }

  void ex(const mkt_str& cmd, bool escape)
  {
    using namespace std;
    using namespace boost;
    using namespace boost::algorithm;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt_str local_cmd(cmd); trim(local_cmd);

    //no-op for empty cmd lines and comments
    if(local_cmd.empty() || local_cmd[0]=='#') return;

    mkt::argument_vector args = mkt::split(local_cmd);

    //handle escape codes in each argument in the vector
    if(escape)
      {
	typedef array<array<string, 2>, 8> codes_array;
	codes_array codes =
	  {
	    {
	      {"\\n", "\n"},
	      {"\\r", "\r"},
	      {"\\t", "\t"},
	      {"\\v", "\v"},
	      {"\\b", "\b"},
	      {"\\f", "\f"},
	      {"\\a", "\a"},
	      {"\\\\", "\\"},
	    }
	  };
	BOOST_FOREACH(string& arg, args)
	  {
	    BOOST_FOREACH(codes_array::value_type& code_array, codes)
	      {
		replace_all(arg, code_array[0], code_array[1]);
	      }
	  }
      }

    mkt::exec(args);
  }

  void exec_file(const argument_vector& file_args, bool parallel,
		 bool escape)
  {
    using namespace std;
    using namespace boost;
    using namespace boost::algorithm;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    if(file_args.empty()) throw mkt::system_error("Missing filename.");
    
    string filename = file_args[0];
    ifstream inf(filename.c_str());
    if(!inf) throw mkt::file_error("Could not open " + filename);

    unsigned int line_num = 0;
    while(!inf.eof())
      {
        try
          {
            string line;
            getline(inf, line);
            trim(line);

            line_num++;

            //use '#' for comments
            if(line.empty() || line[0]=='#') continue;

            if(!parallel)
              mkt::ex(line, escape);
            else
              mkt::start_thread(line, boost::bind(mkt::ex, line, escape));
          }
        catch(mkt::exception& e)
          {
            throw mkt::command_error(str(format("file error on line %1%: %2%") 
                                         % line_num
                                         % e.what_str()));
          }
      }
  }

  bool commands_at_exit() { return _commands_atexit; }
}
