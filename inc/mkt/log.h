#ifndef __MKT_LOG_H__
#define __MKT_LOG_H__

#include <mkt/config.h>
#include <mkt/types.h>

#include <boost/tuple/tuple.hpp>

namespace mkt
{
  /*
   * Logging API
   */

  typedef boost::tuple<
    mkt_str,         // message
    any              // data
    > log_entry;

  typedef boost::shared_ptr<log_entry>         log_entry_ptr;
  typedef std::multimap<ptime, log_entry_ptr>  log_entries;
  typedef boost::shared_ptr<log_entries>       log_entries_ptr;
  typedef std::map<mkt_str, log_entries_ptr>   log_entry_queues;  

  void init_log();
  void final_log();

  // add a log entry to the system
  void log(const mkt_str& queue, const mkt_str& message, const any& data = boost::any());

  // return all log entries for a particular queue that fall between [begin, end)
  log_entries_ptr get_logs(const mkt_str& queue, ptime begin, ptime end);

  // return all log queue names known by the system
  argument_vector get_log_queues();

  bool log_at_exit();
}

#endif
