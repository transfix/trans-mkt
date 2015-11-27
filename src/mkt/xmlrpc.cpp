#include <mkt/xmlrpc.h>
#include <mkt/commands.h>
#include <mkt/threads.h>
#include <mkt/vars.h>
#include <mkt/echo.h>

#include <xmlrpc/XmlRpc.h>

#include <boost/thread.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio.hpp>

#include <iostream>

// xmlrpc module exceptions
namespace mkt
{
  MKT_DEF_EXCEPTION(network_error);
}

//xmlrpc commands related code
namespace
{
  //Creates a command that syncs a variable on a remote host with the current value here
  class syncer
  {
  public:
    syncer(const mkt::mkt_str& v, 
           const mkt::mkt_str& h)
      : _varname(v), _host(h) {}
    void operator()(const mkt::mkt_str& in_var,
                    const mkt::mkt_str& t_key)
    {
      using namespace boost::algorithm;
      if(_varname != in_var) return;
      
      mkt::mkt_str val = mkt::get_var(in_var, t_key);

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
 
      // TODO: thread key
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
    mkt::mkt_str _varname;
    mkt::mkt_str _host;
  };

  void sync_var(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    if(args.size() < 3) throw mkt::command_error("Missing arguments");
    
    string varname = args[1];
    string remote_servers_string = args[2];

    mkt::argument_vector remote_servers;
    split(remote_servers, remote_servers_string, is_any_of(","), token_compress_on);
    
    //execute an echo with the specified string on each host listed
    for(auto&& remote_server : remote_servers)
      {
        trim(remote_server); //get rid of whitespace between ',' chars
        mkt::argument_vector remote_host_components;
        split(remote_host_components, remote_server,
              is_any_of(":"), token_compress_on);
        if(remote_host_components.empty()) continue;
        //TODO: do something with port
        int port = mkt::default_port();
        std::string host = remote_host_components[0];
        if(remote_host_components.size()>=2)
          port = lexical_cast<int>(remote_host_components[1]);

        mkt::var_changed().connect(syncer(varname, remote_server));
      }    
  }

  void unsync_var(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace boost;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    if(args.size() < 3) throw mkt::command_error("Missing arguments");
    
    string varname = args[1];
    string remote_servers_string = args[2];

    mkt::argument_vector remote_servers;
    split(remote_servers, remote_servers_string, is_any_of(","), token_compress_on);
    
    //disconnect the syncer associated with each remote host that matches
    for(auto&& remote_server : remote_servers)
      {
        trim(remote_server); //get rid of whitespace between ',' chars
        mkt::argument_vector remote_host_components;
        split(remote_host_components, remote_server,
              is_any_of(":"), token_compress_on);
        if(remote_host_components.empty()) continue;
        //TODO: do something with port
        int port = mkt::default_port();
        std::string host = remote_host_components[0];
        if(remote_host_components.size()>=2)
          port = lexical_cast<int>(remote_host_components[1]);

        mkt::var_changed().disconnect(syncer(varname, remote_server));
      }
  }

  void local_ip(const mkt::argument_vector& args)
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::ret_val(mkt::get_local_ip_address());
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

  void server(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    
    int port = 31337;
    if(args.size()>=2)
      port = boost::lexical_cast<int>(args[1]);
    
    mkt::run_xmlrpc_server(port);
  }
}

// xmlrpc module API implementation
namespace mkt
{
  //From http://bit.ly/ADIcC1
  std::string get_local_ip_address()
  {
    using namespace boost::asio;

    ip::address addr;
    try
      {
        io_service netService;
        ip::udp::resolver   resolver(netService);
        ip::udp::resolver::query query(ip::udp::v4(), "sublevels.net", "");
        ip::udp::resolver::iterator endpoints = resolver.resolve(query);
        ip::udp::endpoint ep = *endpoints;
        ip::udp::socket socket(netService);
        socket.connect(ep);
        addr = socket.local_endpoint().address();
      } 
    catch (std::exception& e)
      {
        throw network_error(e.what());
      }
    return addr.to_string();
  } 

  int default_port() { return 31337; }

  MKT_DEF_EXCEPTION(xmlrpc_server_error);
  MKT_DEF_EXCEPTION(xmlrpc_server_error_listen);
  MKT_DEF_EXCEPTION(xmlrpc_server_terminate);

#define XMLRPC_METHOD_PROTOTYPE(name, description)              \
  class name : public XmlRpc::XmlRpcServerMethod                \
  {                                                             \
  public:                                                       \
    name(XmlRpc::XmlRpcServer *s) :                             \
      XmlRpc::XmlRpcServerMethod(#name, s) {}                   \
    void execute(XmlRpc::XmlRpcValue &params,                   \
                 XmlRpc::XmlRpcValue &result);                  \
    std::string help() { return std::string(description); }     \
  };

#define XMLRPC_METHOD_DEFINITION(name)                                  \
  void xmlrpc_server_thread::name::execute(XmlRpc::XmlRpcValue &params, \
                                           XmlRpc::XmlRpcValue &result)

  class xmlrpc_server_thread
  {
  public:
    xmlrpc_server_thread(int p) : _port(p) {}
    void operator()() const
    {
      using namespace boost;
      thread_info ti(BOOST_CURRENT_FUNCTION);

      //instantiate the server and its methods.
      XmlRpc::XmlRpcServer s;
      mkt_exec mkt_exec_method(&s);
      
      //Start the server, and run it indefinitely.
      //For some reason, time_from_string and boost_regex creashes if the main thread is waiting in atexit().
      //So, make sure main() has a cvcapp.wait_for_threads() call at the end.
      XmlRpc::setVerbosity(0);
      if(!s.bindAndListen(_port)) 
        throw xmlrpc_server_error_listen(str(format("could not bind to port %d") % _port));
      s.enableIntrospection(true);

      try
        {
          //loop with interruption points so we can gracefully terminate
          while(1)
            {
              boost::this_thread::interruption_point();
              s.work(0.2); //work for 200ms
            }
        }
      catch(boost::thread_interrupted&)
        {
          mkt::out().stream() << "xmlrpc_server_thread interrupted" << std::endl;
          s.shutdown();
        }
    }
  private:
    int _port;
    XMLRPC_METHOD_PROTOTYPE(mkt_exec, "Executes a command via mkt::exec");
  };

  XMLRPC_METHOD_DEFINITION(mkt_exec)
  {
    using namespace std;
    using namespace boost::algorithm;

    string command = std::string(params[0]);
    argument_vector av;
    split(av, command, is_any_of(" "),
          token_compress_on);

    try
      {
        mkt::exec(av);
        result[0] = mkt::get_var("_");
        result[1] = string("success");
      }
    catch(mkt::exception& e)
      {
        string res = boost::str(boost::format("Error: %1%")
                                % e.what_str());
        if(!e.what_str().empty()) 
          mkt::out().stream() << res << endl;
        result[0] = res;
        result[1] = string("error");
      }
  }

  void run_xmlrpc_server(int port)
  {
    xmlrpc_server_thread xst(port);
    xst();
  }

  MKT_DEF_EXCEPTION(xmlrpc_remote_error);

  void exec_remote(const argument_vector& args, std::string host, int port)
  {
    XmlRpc::XmlRpcClient c(host.c_str(),port,2.0);
    XmlRpc::XmlRpcValue params, result;
    std::string args_str = boost::algorithm::join(args, " ");
    params[0] = args_str;
    c.execute("mkt_exec", params, result);
    mkt::ret_val(std::string(result[0])); //passing return values across the network
    if(result[1] == std::string("error"))
      throw xmlrpc_remote_error(result[0]);
  }

  //Calls echo on any mkt xmlrpc servers listed in the sys_remote_echo system variable
  //as a comma separated list of the form:
  // server0:31337, server1:31338, server2:31339
  void do_remote_echo(const std::string& str)
  {
    using namespace boost;
    using namespace boost::algorithm;
    const mkt::mkt_str remote_echo_varname("sys_remote_echo");
    if(mkt::has_var(remote_echo_varname))
      {
        // TODO: thread key
        mkt::mkt_str remote_echo_value = mkt::get_var(remote_echo_varname);
        mkt::argument_vector remote_servers;
        split(remote_servers, remote_echo_value, is_any_of(","), token_compress_on);

        //execute an echo with the specified string on each host listed
        for(auto&& remote_server : remote_servers)
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

            mkt::argument_vector launch_remote_echo_args;
            launch_remote_echo_args.push_back("async");
            launch_remote_echo_args.push_back("remote");
            launch_remote_echo_args.push_back(host);
            launch_remote_echo_args.push_back("port");
            launch_remote_echo_args.push_back(lexical_cast<std::string>(port));
            launch_remote_echo_args.push_back("echo");
            launch_remote_echo_args.push_back(str);
            mkt::exec(launch_remote_echo_args);
          }
      }
  }

  void init_xmlrpc()
  {
    using namespace std;
    using namespace mkt;
    
    add_command("local_ip", ::local_ip, 
                "Prints the local ip address of the default interface.");
    add_command("remote", ::remote, 
                "remote <host> [port <port>] <command> -\n"
                "Executes a command on the specified host.");
    add_command("server", ::server, 
                "server <port>\nStart an xmlrpc server at the specified port.");
    add_command("sync_var", ::sync_var, "sync_var <varname> <remote host comma separated list> -\n"
                "Keeps variables syncronized across hosts.");
    add_command("unsync_var", ::unsync_var, "unsync_var <varname> <remote host comma separated list> -\n"
                "Disconnects variable syncronization across hosts.");    
  }

  void final_xmlrpc()
  {
    remove_command("local_ip");
    remove_command("remote");
    remove_command("server");
    remove_command("sync_var");
    remove_command("unsync_var");
  }
}
