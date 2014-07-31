#include <mkt/config.h>
#include <mkt/app.h>
#include <mkt/exceptions.h>

#ifdef MKT_USING_XMLRPC
#include <mkt/xmlrpc.h>
#endif

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <set>
#include <iostream>
#include <fstream>
#include <cstdlib>

namespace mkt
{
  MKT_DEF_EXCEPTION(async_error);
  MKT_DEF_EXCEPTION(command_error);
  MKT_DEF_EXCEPTION(system_error);
  MKT_DEF_EXCEPTION(file_error);
}

//Commands related code
namespace
{
  mkt::command_map     _commands;
  boost::mutex         _commands_lock;

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
      wait = true;
    
    std::string local_args_str = boost::join(local_args," ");
    mkt::start_thread(local_args_str,
                      boost::bind(mkt::exec, local_args),
                      wait);
  }

  void echo(const mkt::argument_vector& args)
  {
    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin());
    std::string echo_str = 
      boost::algorithm::join(local_args," ");
    std::cout << echo_str << std::endl;
  }

  void help(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    mkt::argument_vector prog_args = mkt::argv();
    std::string prog_name = !prog_args.empty() ? prog_args[0] : "mkt";

    cout << "Version: " << mkt::version() << endl;
    cout << "Usage: " << prog_name << " <command> <command args>" << endl << endl;
    BOOST_FOREACH(mkt::command_map::value_type& cmd, _commands)
      {
        cout << " - " << cmd.first << endl;
        cout << cmd.second.get<1>() << endl << endl;
      }
  }

  void hello(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    if(args.size()<2) cout << "Hello, world!" << endl;
    else if(args.size()==2) cout << "Hello, " << args[1] << endl;
    else throw mkt::command_error("Too many arguments.");
  }

  void interrupt(const mkt::argument_vector& args)
  {
    using namespace std;
    if(args.size()<2) throw mkt::command_error("Missing thread name.");

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string to form the thread name
    std::string thread_name = 
      boost::algorithm::join(local_args," ");

    mkt::thread_ptr thread = mkt::threads(thread_name);
    if(thread) thread->interrupt();
    else throw mkt::system_error("Null thread pointer.");
  }

  void serial(const mkt::argument_vector& args)
  {
    using namespace std;

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

  void parallel(const mkt::argument_vector& args)
  {
    using namespace std;

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

  //TODO: make 'file' a subcommand of parallel and serial
  //TODO: check for # comments
  void file(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost::algorithm;

    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin());
    if(local_args.empty()) throw mkt::command_error("Missing file name.");

    string filename = local_args[0];
    ifstream inf(filename.c_str());
    if(!inf) throw mkt::file_error("Could not open" + filename);

    unsigned int line_num = 0;
    while(!inf.eof())
      { 
        string line;
        getline(inf, line); 
        line_num++;
        mkt::argument_vector av;
        split(av, line, is_any_of(" "), token_compress_on);

        //remove empty strings
        mkt::argument_vector av_clean;
        BOOST_FOREACH(const string& cur, av)
          if(!cur.empty()) av_clean.push_back(cur);
        
        if(!av.empty())
          mkt::exec(av_clean);
      }    
  }
  
  //commands TODO:
  //file - read commands from a file
  //pinger - repeatedly ping a command
  //url - read commands from a url
  //alias - give a name to a command so you can refer to an argument vector by a single name
  //list_thread_ids
  //set/get map key value pairs
  //list map keys
  //find key
  //remove key
  //save/restore map to/from file (or remote resource?)
  //current_time

  //command interpreter?? gnu readline

  //output function - figure out best way to pipe output from server commands to local commands

  void list_threads(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::thread_map tm = mkt::threads();
    BOOST_FOREACH(mkt::thread_map::value_type& cur, tm)
      std::cout << cur.first << std::endl;
  }

#ifdef MKT_USING_XMLRPC
  void local_ip(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    cout << mkt::get_local_ip_address() << endl;
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
      add_command("echo", echo,
                  "Prints out all arguments after echo to standard out.");
      add_command("file", file,
                  "file <file path> - \n"
                  "Executes commands listed in a file, line by line sequentially.");
      add_command("hello", hello, "Prints hello world.");
      add_command("help", help, "Prints command list.");
      add_command("interrupt", interrupt, "Interrupts a running thread.");
      add_command("list_threads", list_threads, "Lists running threads by name.");
#ifdef MKT_USING_XMLRPC
      add_command("local_ip", local_ip, 
                  "Prints the local ip address of the default interface.");
      add_command("parallel", parallel,
                  "Executes a series of commands in parallel, separated by a 'and' keyword.");
      add_command("remote", remote, 
                  "remote <host> [port <port>] <command> -\n"
                  "Executes a command on the specified host.");
#endif
      add_command("serial", serial, "Execute commands serially separated by a 'then' keyword.");
#ifdef MKT_USING_XMLRPC
      add_command("server", server, 
                  "server <port>\nStart an xmlrpc server at the specified port.");
#endif
      add_command("sleep", sleep, "sleep <milliseconds>\nSleep for the time specified.");
                  
    }
  } init_commands_static_init;
}

namespace
{
  mkt::argument_vector        _av;
  boost::mutex                _av_lock;

  mkt::thread_map             _threads;
  mkt::thread_progress_map    _thread_progress;
  mkt::thread_key_map         _thread_keys;
  mkt::thread_info_map        _thread_info;
  boost::mutex                _threads_mutex;

  //This should only be called after we lock the threads mutex
  void update_thread_keys()
  {
    using namespace std;
    using namespace mkt;

    _thread_keys.clear();

    set<boost::thread::id> infoIds;
    BOOST_FOREACH(thread_info_map::value_type val, _thread_info)
      infoIds.insert(val.first);

    set<boost::thread::id> currentIds;
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
    set<boost::thread::id> infoIdsToRemove;
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
    mutex::scoped_lock lock(_commands_lock);
    _commands[name] = make_tuple(func, desc);
  }

  void remove_command(const std::string& name)
  {
    boost::mutex::scoped_lock lock(_commands_lock);
    _commands.erase(name);
  }

  argument_vector get_commands()
  {
    boost::mutex::scoped_lock lock(_commands_lock);    
    argument_vector av;
    BOOST_FOREACH(command_map::value_type& cur, _commands)
      av.push_back(cur.first);
    return av;
  }

  void exec(const argument_vector& args)
  {
    using namespace boost;
    command cmd;
    {
      mutex::scoped_lock lock(_commands_lock);
      if(args.empty()) throw command_error("Missing command string");
      std::string cmd_str = args[0];
      if(_commands.find(cmd_str)==_commands.end())
        throw command_error(str(format("Invalid command: %1%") % cmd_str));
      cmd = _commands[cmd_str];
    }
    cmd.get<0>()(args);
  }

  argument_vector argv()
  {
    boost::mutex::scoped_lock lock(_av_lock);
    return _av;
  }

  void argv(const argument_vector& av)
  {
    boost::mutex::scoped_lock lock(_av_lock);
    _av = av;
  }

  void argv(int argc, char **argv)
  {
    mkt::argument_vector args;
    for(int i = 0; i < argc; i++)
      args.push_back(argv[i]);
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
    boost::mutex::scoped_lock lock(_threads_mutex);
    return _threads;
  }

  thread_ptr threads(const std::string& key)
  {
    boost::this_thread::interruption_point();
    boost::mutex::scoped_lock lock(_threads_mutex);
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
      boost::mutex::scoped_lock lock(_threads_mutex);
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
      boost::mutex::scoped_lock lock(_threads_mutex);
      
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
    boost::mutex::scoped_lock lock(_threads_mutex);
    if(_threads.find(key)!=_threads.end())
      return true;
    return false;
  }

  double thread_progress(const std::string& key)
  {
    boost::this_thread::interruption_point();
    boost::mutex::scoped_lock lock(_threads_mutex);

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
      boost::mutex::scoped_lock lock(_threads_mutex);
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
      boost::mutex::scoped_lock lock(_threads_mutex);

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
      boost::mutex::scoped_lock lock(_threads_mutex);
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
      boost::mutex::scoped_lock lock(_threads_mutex);

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
      boost::mutex::scoped_lock lock(_threads_mutex);
      
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
            std::cout << BOOST_CURRENT_FUNCTION
                      << " :: "
                      << "waiting for thread " << val.first
                      << std::endl;
            
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
}
