#include <mkt/commands.h>
#include <mkt/app.h>
#include <mkt/threads.h>
#include <mkt/vars.h>
#include <mkt/utils.h>
#include <mkt/log.h>
#include <mkt/exceptions.h>

#ifdef MKT_INTERACTIVE
#ifdef __WINDOWS__
#include <editline_win/readline.h>
#else
  #include <editline/readline.h>
#endif
#endif

#include <boost/format.hpp>
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
    if(!mkt::has_var("PS1") || mkt::get_var("PS1").empty())
      mkt::set_var("PS1", "mkt> "); //default prompt
  }

  inline void cmd_check_name(const mkt::mkt_str& str)
  {
    mkt::check_identifier<mkt::command_error>(str);
  }
}

//Default system commands
namespace
{
  void cmd(const mkt::argument_vector& args)
  {
#ifdef MKT_INTERACTIVE
    using namespace std;
    using namespace boost;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    cmd_restore_prompt();

    char *line;
    while((line = readline(mkt::expand_vars(mkt::get_var("PS1")).c_str())))
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
            mkt::log("exceptions", e.what_str());
            mkt::set_var("_", e.what_str());
          }

        //output the contents of the retval to stdout
        mkt::mkt_str rv = mkt::get_var("_");
        if(!rv.empty()) cout << rv << endl;

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
    for(auto&& cur : local_args)
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
    for(auto&& cur : split_args)
      mkt::exec(cur);
  }

  void version(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::ret_val(mkt::version());
  }
}

//API implementation
namespace mkt
{
  MKT_DEF_MAP_CHANGE_SIGNAL(command_added);
  MKT_DEF_MAP_CHANGE_SIGNAL(command_removed);
  MKT_DEF_SIGNAL(command_exec_signal, command_pre_exec);
  MKT_DEF_SIGNAL(command_exec_signal, command_post_exec);

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
  void init_commands()
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
    add_command("version", ::version, "Returns the system version string.");    
  }

  void final_commands()
  {
    remove_command("version");
    remove_command("serial");
    remove_command("repeat");
    remove_command("help");
    remove_command("file");
#ifdef MKT_INTERACTIVE
    remove_command("cmd");
#endif

    delete _commands_data;
    _commands_data = 0;
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

    if(!wait_for_threads::at_start() &&
       !wait_for_threads::at_exit())
      command_added()(name);
  }

  void remove_command(const mkt_str& name)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    {
      unique_lock lock(cmds_lock());
      cmds().erase(name);
    }

    if(!wait_for_threads::at_start() &&
       !wait_for_threads::at_exit())
      command_removed()(name);
  }

  argument_vector get_commands()
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    shared_lock lock(cmds_lock());    
    argument_vector av;
    for(auto&& cur : cmds())
      av.push_back(cur.first);
    return av;
  }

  mkt_str get_command_description(const mkt_str& name)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    shared_lock lock(cmds_lock());    
    return std::get<1>(cmds()[name]);
  }
  
  bool has_command(const mkt_str& name)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    shared_lock lock(cmds_lock());
    return cmds().find(name) != cmds().end();
  }

  void exec(const argument_vector& args)
  {
    using namespace boost;

    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(args.empty()) throw command_error("Missing command string");

    //do variable expansion
    argument_vector local_args = args;
    for(auto&& arg : local_args)
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

    ptime pre_exec_time = now();

    // Finally call it.
    command_pre_exec()(local_args, thread_key());
    std::get<0>(cmd)(local_args);
    command_post_exec()(local_args, thread_key());

    // If the return value was not modified in the last command, reset it.
    ptime ret_val_mod_time = get_var_mod_time("_");
    if(pre_exec_time > ret_val_mod_time)
      ret_val("");
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
        typedef boost::array<boost::array<string, 2>, 8> codes_array;
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
        for(auto&& arg : args)
          for(auto&& code_array : codes)
            replace_all(arg, code_array[0], code_array[1]);
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
