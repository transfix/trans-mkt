#include <mkt/utils.h>
#include <mkt/commands.h>
#include <mkt/vars.h>
#include <mkt/echo.h>
#include <mkt/modules.h>
#include <mkt/log.h>

#include <boost/regex.hpp>

#include <iostream>

namespace mkt
{
  bool matches(const mkt_str& in_str, const mkt_str& regex_str)
  {
    using namespace boost;
    thread_info(BOOST_CURRENT_FUNCTION);

    regex expr(regex_str.c_str());
    match_results<mkt_str::iterator> what;
    match_flag_type flags = match_default;
    try
      {
	mkt_str local_in_str(in_str);
	if(regex_search(local_in_str.begin(), 
			local_in_str.end(),
			what, expr, flags))
	  return true;
      }
    catch(...){}
    return false;
  }

  bool valid_identifier(const mkt_str& str)
  {
    // C identifier regex
    // http://bit.ly/1MExKtn
    return matches(str, "^[_a-zA-Z][_a-zA-Z0-9]{0,30}$");
  }

  namespace bt = boost::posix_time;
  const std::locale formats[] = {
    std::locale(std::locale::classic(),new bt::time_input_facet("%Y-%m-%d %H:%M:%S")),
    std::locale(std::locale::classic(),new bt::time_input_facet("%Y/%m/%d %H:%M:%S")),
    std::locale(std::locale::classic(),new bt::time_input_facet("%d.%m.%Y %H:%M:%S")),
    std::locale(std::locale::classic(),new bt::time_input_facet("%Y-%m-%d"))};
  const size_t formats_n = sizeof(formats)/sizeof(formats[0]);

  mkt_str ptime_to_str(const ptime& pt)
  {
    return bt::to_simple_string(pt);
  }

  ptime str_to_ptime(const mkt_str& s)
  {
    ptime pt = bt::ptime();
    for(size_t i=0; i<formats_n; ++i)
    {
        std::istringstream is(s);
        is.imbue(formats[i]);
        is >> pt;
        if(pt != bt::ptime()) break;
    }
    return pt;
  }

  bool at_exit()
  {
    return 
      commands_at_exit() || vars_at_exit() || 
      threads_at_exit() || echo_at_exit() ||
      modules_at_exit() || log_at_exit();
  }
}
