#ifndef __XCH_TYPES_H__
#define __XCH_TYPES_H__

#include <mkt/types.h>

namespace xch
{
  typedef mkt::ptime              ptime;
  typedef mkt::int64              int64;
  typedef mkt::mutex              mutex;
  typedef mkt::unique_lock        unique_lock;
  typedef mkt::shared_lock        shared_lock;
  typedef mkt::map_change_signal  map_change_signal;
  typedef mkt::argument_vector    argument_vector;
}

#endif
