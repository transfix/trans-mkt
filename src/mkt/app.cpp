#include <mkt/app.h>
#include <mkt/exceptions.h>

#ifdef MKT_USING_XMLRPC
#include <mkt/xmlrpc.h>
#endif

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
#include <fstream>
#include <cstdlib>

namespace mkt
{
  MKT_DEF_EXCEPTION(async_error);
  MKT_DEF_EXCEPTION(command_error);
  MKT_DEF_EXCEPTION(file_error);
}

//Commands related code
namespace
{
  mkt::command_map     _commands;
  mkt::mutex           _commands_lock;

  //launches a command in a thread
  void async(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove command argument
    if(local_args.empty())
      throw mkt::async_error("No command to execute.");
    
    //check if the next argument is 'wait.'  If so, set the wait flag true
    bool wait = false;
    if(local_args[0] == "wait")
      {
        wait = true;
        local_args.erase(local_args.begin()); //remove "wait"
      }
    
    std::string local_args_str = boost::join(local_args," ");
    mkt::start_thread(local_args_str,
                      boost::bind(mkt::exec, local_args),
                      wait);
  }

  void async_file(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost::algorithm;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove command string
    if(local_args.empty()) throw mkt::command_error("Missing file name.");

    mkt::exec_file(local_args, true);
  }

  void echo(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin());
    std::string echo_str = 
      boost::algorithm::join(local_args," ");
    mkt::out().stream() << echo_str << std::endl;
  }

  void help(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    mkt::argument_vector prog_args = mkt::argv();
    std::string prog_name = !prog_args.empty() ? prog_args[0] : "mkt";

    mkt::out().stream() << "Version: " << mkt::version() << endl;
    mkt::out().stream() << "Usage: " << prog_name << " <command> <command args>" << endl << endl;
    BOOST_FOREACH(mkt::command_map::value_type& cmd, _commands)
      {
        mkt::out().stream() << " - " << cmd.first << endl;
        mkt::out().stream() << cmd.second.get<1>() << endl << endl;
      }
  }

  void has_var(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(args.size()<2) 
      throw mkt::command_error("Missing variable argument.");
    mkt::out().stream() << (mkt::has_var(args[1]) ? "true" : "false") << std::endl;
  }

  void interrupt(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    if(args.size()<2) throw mkt::command_error("Missing thread name.");

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string to form the thread name
    std::string thread_name = 
      boost::algorithm::join(local_args," ");

    mkt::thread_ptr thread = mkt::threads(thread_name);
    if(thread) thread->interrupt();
    else throw mkt::system_error("Null thread pointer.");
  }

  void repeat(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost;
    using namespace boost::algorithm;
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

  //Creates a command that syncs a variable on a remote host with the current value here
#ifdef MKT_USING_XMLRPC
  class syncer
  {
  public:
    syncer(const std::string& v, 
           const std::string& h)
      : _varname(v), _host(h) {}
    void operator()(const std::string& in_var)
    {
      using namespace boost::algorithm;
      if(_varname != in_var) return;
      
      std::string val = mkt::var(in_var);

      //do the remote var syncronization as an asyncronous command
      mkt::argument_vector remote_set_args;
      remote_set_args.push_back("async");
      remote_set_args.push_back("wait"); //TODO: wait up to a certain amount of threads then start interrupting
      remote_set_args.push_back("remote");

      trim(_host);
      mkt::argument_vector host_components;
      split(host_components, _host, is_any_of(":"), token_compress_on);
      if(host_components.empty()) throw mkt::system_error("Invalid host.");

      remote_set_args.push_back(host_components[0]);
      if(host_components.size()>=2)
        {
          remote_set_args.push_back("port");
          remote_set_args.push_back(host_components[1]);
        }
 
      remote_set_args.push_back("set");
      remote_set_args.push_back(in_var);
      remote_set_args.push_back(val);
      mkt::exec(remote_set_args);
    }

    bool operator==(const syncer& rhs) const
    {
      if(&rhs == this) return true;
      return 
        (_varname == rhs._varname) &&
        (_host == rhs._host);
    }

  private:
    std::string _varname;
    std::string _host;
  };
#endif

  void sync_var(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

#ifdef MKT_USING_XMLRPC
    if(args.size() < 3) throw mkt::command_error("Missing arguments");
    
    string varname = args[1];
    string remote_servers_string = args[2];

    mkt::argument_vector remote_servers;
    split(remote_servers, remote_servers_string, is_any_of(","), token_compress_on);
    
    //execute an echo with the specified string on each host listed
    BOOST_FOREACH(string& remote_server, remote_servers)
      {
        trim(remote_server); //get rid of whitespace between ',' chars
        mkt::argument_vector remote_host_components;
        split(remote_host_components, remote_server,
              is_any_of(":"), token_compress_on);
        if(remote_host_components.empty()) continue;
        int port = mkt::default_port();
        std::string host = remote_host_components[0];
        if(remote_host_components.size()>=2)
          port = lexical_cast<int>(remote_host_components[1]);

        mkt::var_changed.connect(syncer(varname, remote_server));
      }    
#else
    throw mkt::command_error("sync_var unsupported");
#endif
  }

  void parallel(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string to form the thread name

    //split the argument vector into separate command strings via 'and' keywords
    vector<mkt::argument_vector> split_args;
    mkt::argument_vector cur_command;
    BOOST_FOREACH(const string& cur, local_args)
      {
        if(cur != "and")
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
    
    //now execute the split commands in parallel
    BOOST_FOREACH(const mkt::argument_vector& cur, split_args)
      {
        mkt::argument_vector local_cur = cur;
        local_cur.insert(local_cur.begin(), "async");
        async(local_cur);
      }
  }

  void file(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost::algorithm;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove command string
    if(local_args.empty()) throw mkt::command_error("Missing file name.");

    mkt::exec_file(local_args);
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

  void list_threads(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::thread_map tm = mkt::threads();
    bool expand_info = false;

    //check for expand_info keyword after command arg
    if(args.size()>=2 && args[1]=="expand_info")
      expand_info = true;
    
    BOOST_FOREACH(mkt::thread_map::value_type& cur, tm)
      if(cur.second)
        {
          string ti = mkt::get_thread_info(cur.first);

          if(!expand_info)
            {
              //captures most function looking strings as output by BOOST_CURRENT_FUNCTION
              regex expr("(\\S+\\(?\\W*\\)?)\\((.*)\\)");
              match_results<string::iterator> what;
              match_flag_type flags = match_default;
              try
                {
                  if(regex_search(ti.begin(), ti.end(), what, expr, flags))
                    ti = string(what[1]);
                }
              catch(...){}
            }

          mkt::out().stream() << cur.first << " " 
                    << cur.second->get_id() << " " 
                    << ti << endl;
        }
  }

#ifdef MKT_USING_XMLRPC
  void local_ip(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::out().stream() << mkt::get_local_ip_address() << endl;
  }

  void remote(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);    
    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin());
    if(local_args.size() < 2)
      throw mkt::command_error("Missing arguments");
    
    std::string host = local_args[0];
    local_args.erase(local_args.begin());

    int port = 31337;
    //look for port keyword
    //TODO: support ':' host/port separator
    if(local_args[0] == "port")
      {
        local_args.erase(local_args.begin());
        if(local_args.empty())
          throw mkt::command_error("Missing arguments");
        port = boost::lexical_cast<int>(local_args[0]);
        local_args.erase(local_args.begin());
      }

    mkt::exec_remote(local_args, host, port);
  }  
#endif

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
      mkt::var(args[1]); //create an empty variable
    else
      mkt::var(args[1], args[2]); //actually do an assignment operation
  }

  void sleep(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    if(args.size()<2) 
      throw mkt::command_error("Missing argument for sleep");
    mkt::sleep(boost::lexical_cast<int64_t>(args[1]));
  }

#ifdef MKT_USING_XMLRPC
  void server(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    
    int port = 31337;
    if(args.size()>=2)
      port = boost::lexical_cast<int>(args[1]);
    
    mkt::run_xmlrpc_server(port);
  }
#endif

  void unset(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(args.size()<2)
      throw mkt::command_error("Missing arguments for unset");
    mkt::unset_var(args[1]);
  }

  void unsync_var(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

#ifdef MKT_USING_XMLRPC
    if(args.size() < 3) throw mkt::command_error("Missing arguments");
    
    string varname = args[1];
    string remote_servers_string = args[2];

    mkt::argument_vector remote_servers;
    split(remote_servers, remote_servers_string, is_any_of(","), token_compress_on);
    
    //disconnect the syncer associated with each remote host that matches
    BOOST_FOREACH(string& remote_server, remote_servers)
      {
        trim(remote_server); //get rid of whitespace between ',' chars
        mkt::argument_vector remote_host_components;
        split(remote_host_components, remote_server,
              is_any_of(":"), token_compress_on);
        if(remote_host_components.empty()) continue;
        int port = mkt::default_port();
        std::string host = remote_host_components[0];
        if(remote_host_components.size()>=2)
          port = lexical_cast<int>(remote_host_components[1]);

        mkt::var_changed.disconnect(syncer(varname, remote_server));
      }
#else
    throw mkt::command_error("unsync_var unsupported");    
#endif
  }

  class init_commands
  {
  public:
    init_commands()
    {
      using namespace std;
      using namespace mkt;
      
      add_command("async", async,
                  "Executes a command in another thread and"
                  " returns immediately. If 'wait' is before\n"
                  "the command, this command will execute only after"
                  " a command with the same thread name is finished running.");
      add_command("async_file", async_file,
                  "Executes commands listed in a file in parallel.");
      add_command("echo", echo,
                  "Prints out all arguments after echo to standard out.");
      add_command("file", file,
                  "file <file path> - \n"
                  "Executes commands listed in a file, line by line sequentially.");
      add_command("has_var", has_var, "Returns true or false whether the variable exists or not.");
      add_command("help", help, "Prints command list.");
      add_command("interrupt", interrupt, "Interrupts a running thread.");
      add_command("threads", list_threads, "Lists running threads by name.");
#ifdef MKT_USING_XMLRPC
      add_command("local_ip", local_ip, 
                  "Prints the local ip address of the default interface.");
      add_command("parallel", parallel,
                  "Executes a series of commands in parallel, separated by a 'and' keyword.");
      add_command("remote", remote, 
                  "remote <host> [port <port>] <command> -\n"
                  "Executes a command on the specified host.");
#endif
      add_command("repeat", repeat, "repeat <num times> <command> -\nRepeat command.");
      add_command("serial", serial, "Execute commands serially separated by a 'then' keyword.");
#ifdef MKT_USING_XMLRPC
      add_command("server", server, 
                  "server <port>\nStart an xmlrpc server at the specified port.");
#endif
      add_command("set", set, "set [<varname> <value>]\n"
                  "Sets a variable to the value specified.  If none, prints all variables in the system.");
      add_command("sleep", sleep, "sleep <milliseconds>\nSleep for the time specified.");
      add_command("sync_var", sync_var, "sync_var <varname> <remote host comma separated list> -\n"
                  "Keeps variables syncronized across hosts.");
      add_command("unset", unset, "unset <varname>\nRemoves a variable from the system.");
      add_command("unsync_var", unsync_var, "unsync_var <varname> <remote host comma separated list> -\n"
                  "Disconnects variable syncronization across hosts.");
                  
    }
  } init_commands_static_init;
}

namespace
{
  mkt::argument_vector        _av;
  mkt::mutex                  _av_lock;

  mkt::thread_map             _threads;
  mkt::thread_progress_map    _thread_progress;
  mkt::thread_key_map         _thread_keys;
  mkt::thread_info_map        _thread_info;
  mkt::mutex                  _threads_mutex;

  mkt::variable_map           _var_map;
  mkt::mutex                  _var_map_mutex;

  mkt::echo_map               _echo_map;
  mkt::mutex                  _echo_map_mutex;

  //This should only be called after we lock the threads mutex
  void update_thread_keys()
  {
    using namespace std;
    using namespace mkt;

    _thread_keys.clear();

    std::set<boost::thread::id> infoIds;
    BOOST_FOREACH(thread_info_map::value_type val, _thread_info)
      infoIds.insert(val.first);

    std::set<boost::thread::id> currentIds;
    BOOST_FOREACH(thread_map::value_type val, _threads)
      {
        thread_ptr ptr = val.second;
        if(ptr)
          {
            _thread_keys[ptr->get_id()]=val.first;

            //set the thread info to a default state if not already set
            if(_thread_info[ptr->get_id()].empty())
              _thread_info[ptr->get_id()] = "running";
          }

        currentIds.insert(ptr->get_id());
      }

    //compute thread ids that need to be removed from the threadInfo map
    std::set<boost::thread::id> infoIdsToRemove;
    set_difference(infoIds.begin(), infoIds.end(),
                   currentIds.begin(), currentIds.end(),
                   inserter(infoIdsToRemove,infoIdsToRemove.begin()));

    BOOST_FOREACH(boost::thread::id tid, infoIdsToRemove)
      _thread_info.erase(tid);
  }
}

namespace mkt
{
  std::string version()
  {
    return std::string(MKT_VERSION);
  }
  
  void add_command(const std::string& name,
                   const command_func& func,
                   const std::string& desc)
  {
    using namespace boost;
    unique_lock lock(_commands_lock);
    _commands[name] = make_tuple(func, desc);
  }

  void remove_command(const std::string& name)
  {
    unique_lock lock(_commands_lock);
    _commands.erase(name);
  }

  argument_vector get_commands()
  {
    unique_lock lock(_commands_lock);    
    argument_vector av;
    BOOST_FOREACH(command_map::value_type& cur, _commands)
      av.push_back(cur.first);
    return av;
  }

  void exec(const argument_vector& args)
  {
    using namespace boost;
    using namespace boost::algorithm;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    argument_vector local_args = 
      split_vars(expand_vars(args));

    command cmd;
    {
      unique_lock lock(_commands_lock);
      if(local_args.empty()) throw command_error("Missing command string");
      std::string cmd_str = local_args[0];
      if(_commands.find(cmd_str)==_commands.end())
        throw command_error(str(format("Invalid command: %1%") % cmd_str));
      cmd = _commands[cmd_str];
    }
    cmd.get<0>()(local_args);
  }

  void ex(const std::string& cmd)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::argument_vector args = mkt::split(cmd);
    if(!args.empty())
      mkt::exec(args);
  }

  void exec_file(const argument_vector& file_args, bool parallel)
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
            if(!line.empty() && line[0]=='#') continue;

            mkt::argument_vector av;
            split(av, line, is_any_of(" "), token_compress_on);
            
            //remove empty strings
            mkt::argument_vector av_clean;
            BOOST_FOREACH(const string& cur, av)
              if(!cur.empty()) av_clean.push_back(cur);
        
            if(!av_clean.empty())
              {
                if(!parallel)
                  mkt::exec(av_clean);
                else
                  mkt::start_thread(mkt::join(av_clean),
                                    boost::bind(mkt::exec, av_clean));
              }
          }
        catch(mkt::exception& e)
          {
            throw mkt::command_error(str(format("file error on line %1%: %2%") 
                                         % line_num
                                         % e.what_str()));
          }
      }
  }

  argument_vector split(const std::string& args)
  {
    using namespace std;
    using namespace boost::algorithm;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    mkt::argument_vector av;
    split(av, args, is_any_of(" "), token_compress_on);
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
    unique_lock lock(_av_lock);
    return _av;
  }

  void argv(const argument_vector& av)
  {
    unique_lock lock(_av_lock);
    _av = av;
  }

  void argv(int argc, char **argv)
  {
    using namespace boost;
    mkt::argument_vector args;

    //set main_argc system variable
    argument_vector set_args;
    set_args.push_back("set");
    set_args.push_back("main_argc");
    set_args.push_back(lexical_cast<std::string>(argc));
    mkt::exec(set_args);
    
    for(int i = 0; i < argc; i++)
      {
        args.push_back(argv[i]);

        //set main_argv system variables
        argument_vector set_args;
        set_args.push_back("set");
        set_args.push_back(str(format("main_argv_%1%") % i));
        set_args.push_back(argv[i]);
        mkt::exec(set_args);
      }

    mkt::argv(args);
  }

  /*
   * Thread API
   */
  map_change_signal threads_changed;

  //use this to trigger the threads_changed signal because
  //problems happen when signals are envoked during program exit.
  void trigger_threads_changed(const std::string& key)
  {
    bool ae = wait_for_threads::at_exit();
    if(!ae)
      threads_changed(key);
  }

  thread_map threads()
  {
    boost::this_thread::interruption_point();
    unique_lock lock(_threads_mutex);
    return _threads;
  }

  thread_ptr threads(const std::string& key)
  {
    boost::this_thread::interruption_point();
    unique_lock lock(_threads_mutex);
    if(_threads.find(key)!=_threads.end())
      return _threads[key];
    return thread_ptr();
  }

  void threads(const std::string& key, const thread_ptr& val)
  {
    boost::this_thread::interruption_point();

    if(has_thread(key))
      threads(key)->interrupt();

    {
      unique_lock lock(_threads_mutex);
      if(!val)
        _threads.erase(key);
      else
        _threads[key] = val;
      update_thread_keys();
    }

    trigger_threads_changed(key);
  }

  void threads(const thread_map& map)
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(_threads_mutex);
      
      BOOST_FOREACH(thread_map::value_type t, _threads)
	if(t.second) t.second->interrupt();

      _threads = map;
      _thread_progress.clear();
      update_thread_keys();
    }

    trigger_threads_changed("all");
  }

  bool has_thread(const std::string& key)
  {
    boost::this_thread::interruption_point();
    unique_lock lock(_threads_mutex);
    if(_threads.find(key)!=_threads.end())
      return true;
    return false;
  }

  double thread_progress(const std::string& key)
  {
    boost::this_thread::interruption_point();
    unique_lock lock(_threads_mutex);

    boost::thread::id tid;
    if(key.empty())
      tid = boost::this_thread::get_id();
    else if(_threads.find(key)!=_threads.end() &&
            _threads[key])
      tid = _threads[key]->get_id();
    else
      return 0.0;
    
    return _thread_progress[tid];
  }

  void thread_progress(double progress)
  {
    thread_progress(std::string(),progress);
  }

  void thread_progress(const std::string& key, double progress)
  {
    boost::this_thread::interruption_point();

    //clamp progress to [0.0,1.0]
    progress = progress < 0.0 ? 
      0.0 : 
      progress > 1.0 ? 1.0 :
      progress;

    bool changed = false;
    {
      unique_lock lock(_threads_mutex);
      boost::thread::id tid;

      if(key.empty())
        {
          tid = boost::this_thread::get_id();
          changed = true;
        }
      else if(_threads.find(key)!=_threads.end() &&
              _threads[key])
        {
          tid = _threads[key]->get_id();
          changed = true;
        }
      
      if(changed)
        _thread_progress[tid]=progress;
    }

    if(changed)
      trigger_threads_changed(key);
  }

  void finish_thread_progress(const std::string& key)
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(_threads_mutex);

      boost::thread::id tid;
      if(key.empty())
        tid = boost::this_thread::get_id();
      else if(_threads.find(key)!=_threads.end() &&
              _threads[key])
        tid = _threads[key]->get_id();
      else
        return;

      _thread_progress.erase(tid);
    }
    trigger_threads_changed(key);
  }

  std::string thread_key()
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(_threads_mutex);
      if(_thread_keys.find(boost::this_thread::get_id())!=_thread_keys.end())
        return _thread_keys[boost::this_thread::get_id()];
      else
        return std::string("unknown");
    }
  }

  void remove_thread(const std::string& key)
  {
    threads(key,thread_ptr());
  }

  std::string unique_thread_key(const std::string& hint)
  {
    std::string h = hint.empty() ? "thread" : hint;
    //Make a unique key name to use by adding a number to the key
    std::string uniqueThreadKey = h;
    unsigned int i = 0;
    while(has_thread(uniqueThreadKey))
      uniqueThreadKey = 
        h + boost::lexical_cast<std::string>(i++);
    return uniqueThreadKey;
  }

  void set_thread_info(const std::string& key, const std::string& infostr)
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(_threads_mutex);

      boost::thread::id tid;
      if(key.empty())
        tid = boost::this_thread::get_id();
      else if(_threads.find(key)!=_threads.end() &&
              _threads[key])
        tid = _threads[key]->get_id();
      else
        return;

      _thread_info[tid] = infostr;
    }
    trigger_threads_changed(key);
  }

  std::string get_thread_info(const std::string& key)
  {
    boost::this_thread::interruption_point();
    {
      unique_lock lock(_threads_mutex);
      
      boost::thread::id tid;
      if(key.empty())
        tid = boost::this_thread::get_id();
      else if(_threads.find(key)!=_threads.end() &&
              _threads[key])
        tid = _threads[key]->get_id();
      else
        return std::string();

      return _thread_info[tid];
    }
  }

  void this_thread_info(const std::string& infostr)
  {
    set_thread_info(std::string(),infostr);
  }
  
  std::string this_thread_info()
  {
    return get_thread_info();
  }

  thread_info::thread_info(const std::string& info)
  {
    _orig_info = this_thread_info();
    _orig_progress = thread_progress();
    this_thread_info(info);
  }

  thread_info::~thread_info()
  {
    this_thread_info(_orig_info);
    thread_progress(_orig_progress);
  }

  thread_feedback::thread_feedback(const std::string& info)
    : _info(info)
  {
    thread_progress(0.0);
  }

  thread_feedback::~thread_feedback()
  {
    finish_thread_progress();
    remove_thread(thread_key());
  }

  wait_for_threads::~wait_for_threads()
  {
    _at_exit = true;
    wait();
  }

  bool wait_for_threads::at_exit() { return _at_exit; }

  void wait_for_threads::wait()
  {
    //Wait for all the threads to finish
    mkt::thread_map map = mkt::threads();
    BOOST_FOREACH(mkt::thread_map::value_type val, map)
      {
        try
          {
            mkt::out().stream() << BOOST_CURRENT_FUNCTION
                      << " :: "
                      << "waiting for thread " << val.first
                      << std::endl;
            
            if(val.second)
              val.second->join();
          }
        catch(boost::thread_interrupted&) {}
      }
  }

  bool wait_for_threads::_at_exit = false;

  void sleep(int64 ms)
  {
    thread_info ti(BOOST_CURRENT_FUNCTION);
    boost::this_thread::sleep( boost::posix_time::milliseconds(ms) );
  }

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
    var("__echo_functions", "0, 1, 2");
  }
}
