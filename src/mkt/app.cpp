#include <mkt/app.h>
#include <mkt/xmlrpc.h>
#include <mkt/threads.h>
#include <mkt/vars.h>
#include <mkt/commands.h>
#include <mkt/echo.h>
#include <mkt/modules.h>
#include <mkt/log.h>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/xtime.hpp>

#include <set>
#include <iostream>
#include <cstdlib>

namespace mkt
{
  std::string version()
  {
    return std::string(MKT_VERSION);
  }

  argument_vector argv()
  {
    using namespace boost;
    argument_vector av;
    int argc = mkt::var<int>("sys.argc");
    for(int i = 0; i < argc; i++)
      av.push_back(mkt::var(str(format("sys.argv_%1%") % i)));
    return av;
  }

  void argv(int argc, char **argv)
  {
    using namespace boost;
    mkt::argument_vector args;

    // initialize the whole app here...
    mkt::init_commands();
    mkt::init_echo();
    mkt::init_log();
    mkt::init_modules();

    mkt::var("sys.argc", argc);
    for(int i = 0; i < argc; i++)
      mkt::var(str(format("sys.argv_%1%") % i), argv[i]);
  }

  bool at_exit()
  {
    return 
      commands_at_exit() || vars_at_exit() || 
      threads_at_exit() || echo_at_exit() ||
      modules_at_exit() || log_at_exit();
  }
}
