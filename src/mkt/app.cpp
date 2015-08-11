#include <mkt/app.h>
#include <mkt/threads.h>
#include <mkt/vars.h>
#include <mkt/commands.h>
#include <mkt/echo.h>
#include <mkt/modules.h>
#include <mkt/log.h>

#ifdef MKT_USING_XMLRPC
#include <mkt/xmlrpc.h>
#endif

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
  app::app(int argc, char **argv) : 
    _w(new wait_for_threads),
    _tf(new thread_feedback(BOOST_CURRENT_FUNCTION))
  {
    using namespace boost;
    mkt::argument_vector args;
      
    mkt::var("sys.argc", argc);
    for(int i = 0; i < argc; i++)
      mkt::var(str(format("sys.argv_%1%") % i), argv[i]);
    
    initialize();
  }

  app::~app() { finalize(); }

  mkt_str app::version()
  {
    return mkt_str(MKT_VERSION);
  }

  argument_vector app::argv()
  {
    using namespace boost;
    argument_vector av;
    int argc = mkt::var<int>("sys.argc");
    for(int i = 0; i < argc; i++)
      av.push_back(mkt::var(str(format("sys.argv_%1%") % i)));
    return av;
  }

  void app::initialize()
  {
    // initialize the whole app here...
    mkt::init_vars();
    mkt::init_commands();
    mkt::init_echo();
    mkt::init_threads();
    mkt::init_log();

#ifdef MKT_USING_XMLRPC
    mkt::init_xmlrpc();
#endif

    mkt::init_modules();
  }
    
  void app::finalize()
  {
    mkt::final_modules();

    //cleanup here...
#ifdef MKT_USING_XMLRPC
    mkt::final_xmlrpc();
#endif

    mkt::final_log();
    mkt::final_threads();
    mkt::final_echo();
    mkt::final_commands();
    mkt::final_vars();
  }
}
