#ifndef __MKT_TYPES_H__
#define __MKT_TYPES_H__

#include <mkt/config.h>

#include <boost/cstdint.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

//basic types used by the system
namespace mkt
{
  typedef boost::int64_t             int64;
  typedef boost::uint64_t            uint64;
  typedef boost::posix_time::ptime   ptime;
  typedef boost::shared_mutex        mutex;
  typedef boost::unique_lock<mutex>  unique_lock;
  typedef boost::shared_lock<mutex>  shared_lock;
  typedef std::vector<std::string>   argument_vector;
}

#endif
