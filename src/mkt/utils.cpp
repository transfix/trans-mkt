#include <mkt/utils.h>

#include <iostream>

namespace mkt
{
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
}
