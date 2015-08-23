#ifndef __MKT_TYPES_H__
#define __MKT_TYPES_H__

#include <mkt/config.h>

#include <boost/cstdint.hpp>
#include <boost/date_time.hpp>
#include <boost/signals2.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/any.hpp>

#include <string>
#include <vector>

//basic types used by the system
namespace mkt
{
  typedef boost::int64_t                 int64;
  typedef boost::uint64_t                uint64;
  typedef boost::posix_time::ptime       ptime;
  typedef boost::shared_mutex            mutex;
  typedef boost::unique_lock<mutex>      unique_lock;
  typedef boost::shared_lock<mutex>      shared_lock;
  typedef std::string                    mkt_str;
  typedef boost::shared_ptr<mkt_str>     mkt_str_ptr;
  typedef std::vector<mkt_str>           argument_vector;
  typedef argument_vector                arg_vec; // shorthand
  typedef boost::signals2::signal<void (const mkt_str&)> map_change_signal;
  typedef boost::any                     any;
  typedef boost::shared_ptr<any>         any_ptr;
}

#endif
