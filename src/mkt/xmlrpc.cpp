#include <mkt/xmlrpc.h>
#include <mkt/exceptions.h>

#include <xmlrpc/XmlRpc.h>

#include <boost/thread.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/asio.hpp>

#include <iostream>

namespace mkt
{
  MKT_DEF_EXCEPTION(network_error);

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
        result[0] = std::string("success");
      }
    catch(mkt::exception& e)
      {
        if(!e.what_str().empty()) 
          mkt::out().stream() << "Error: " << e.what_str() << endl;
        result[0] = std::string("error");
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
    if(result[0] == std::string("error"))
      throw xmlrpc_remote_error("remote error.");
  }
}
