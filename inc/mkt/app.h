#ifndef __MKT_APP_H__
#define __MKT_APP_H__

#include <mkt/config.h>
#include <mkt/exceptions.h>
#include <mkt/types.h>

namespace mkt
{
  //version string
  std::string version();

  //accessing the process' argument vector
  argument_vector argv();
  void argv(int argc, char **argv);

  //Use this to determine if the program is exiting
  bool at_exit();
}

#endif
