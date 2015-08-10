#ifndef __MKT_UTILS_H__
#define __MKT_UTILS_H__

#include <mkt/config.h>
#include <mkt/types.h>
#include <mkt/threads.h>
#include <mkt/exceptions.h>

#include <boost/lexical_cast.hpp>

namespace mkt
{
  template <class T>
    inline T string_cast(const mkt_str& str_val)
    {
      thread_info ti(BOOST_CURRENT_FUNCTION);
      using namespace boost;
      T val;
      try
        {
          val = lexical_cast<T>(str_val);
        }
      catch(bad_lexical_cast&)
        {
          throw mkt::system_error(str(format("Invalid value type for string %1%")
                                             % str_val));
        }
      return val;
    }

  bool matches(const mkt_str& in_str, const mkt_str& regex_str = ".*");

  //TODO: these should be symmetrical...
  mkt_str ptime_to_str(const ptime& pt);
  ptime str_to_ptime(const mkt_str& s);
}

#endif
