#ifndef __MKT_APP_H__
#define __MKT_APP_H__

#include <mkt/config.h>
#include <mkt/exceptions.h>
#include <mkt/types.h>

namespace mkt
{
  //version string
  std::string version();

  //splits a string into an argument vector
  argument_vector split(const std::string& args);

  //joins an argument vector into a single string
  std::string join(const argument_vector& args);

  //accessing the process' argument vector
  argument_vector argv();
  void argv(int argc, char **argv);
}

#endif
