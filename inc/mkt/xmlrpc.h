#ifndef __MKT_XMLRPC_H__
#define __MKT_XMLRPC_H__

#include <mkt/app.h>

#include <string>

namespace mkt
{
  void init_xmlrpc();
  void final_xmlrpc();

  std::string get_local_ip_address();
  int default_port();
  void run_xmlrpc_server(int port = 31337);
  void exec_remote(const argument_vector& args,
                   std::string host = "localhost", int port = default_port());

  void do_remote_echo(const std::string& str); //echo function
}

#endif
