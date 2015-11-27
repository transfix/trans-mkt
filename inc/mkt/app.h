#ifndef __MKT_APP_H__
#define __MKT_APP_H__

#include <mkt/config.h>
#include <mkt/types.h>

#include <memory>

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

    std::unique_ptr<wait_for_threads>  _w;
    std::unique_ptr<thread_feedback>   _tf;
  };

  inline mkt_str version() { return mkt::app::version(); }
  inline argument_vector argv() { return mkt::app::argv(); }
}

#endif
