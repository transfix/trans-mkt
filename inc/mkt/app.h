#ifndef __MKT_APP_H__
#define __MKT_APP_H__

#include <mkt/config.h>
#include <mkt/types.h>

#include <boost/scoped_ptr.hpp>

namespace mkt
{
  class thread_feedback;
  class wait_for_threads;

  class app
  {
  public:
    app(int argc, char **argv);
    virtual ~app();

    //version string
    static mkt_str version();

    //accessing the process' argument vector
    static argument_vector argv();

  protected:
    void initialize();
    void finalize();

    boost::scoped_ptr<wait_for_threads>  _w;
    boost::scoped_ptr<thread_feedback>   _tf;
  };

  inline mkt_str version() { return mkt::app::version(); }
  inline argument_vector argv() { return mkt::app::argv(); }
}

#endif
