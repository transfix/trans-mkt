#include <mkt/app.h>
#include <mkt/xmlrpc.h>
#include <mkt/threads.h>
#include <mkt/vars.h>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/xtime.hpp>

#include <set>
#include <iostream>
#include <cstdlib>

namespace
{
  mkt::argument_vector        _av;
  mkt::mutex                  _av_lock;
}

namespace mkt
{
  std::string version()
  {
    return std::string(MKT_VERSION);
  }

  argument_vector split(const std::string& args)
  {
    using namespace std;
    using namespace boost::algorithm;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    //split along quote boundaries to support quotes around arguments with spaces.
    mkt::argument_vector av_strings;
    split(av_strings, args, is_any_of("\""), token_compress_on);

    mkt::argument_vector av;
    int idx = 0;
    BOOST_FOREACH(const string& cur, av_strings)
      {
        //Every even element is outside quotes and should be split
        if(idx % 2 == 0)
          {
            mkt::argument_vector av_cur;
            split(av_cur, cur, is_any_of(" "), token_compress_on);
            BOOST_FOREACH(string& s, av_cur) trim(s);
            av.insert(av.end(),
                      av_cur.begin(), av_cur.end());
            idx++;
          }
        else //just insert odd elements as they are
          av.push_back(cur);
      }

    //remove empty strings
    mkt::argument_vector av_clean;
    BOOST_FOREACH(const string& cur, av)
      if(!cur.empty()) av_clean.push_back(cur);
    return av_clean;
  }

  std::string join(const argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    return boost::join(args," ");
  }

  argument_vector argv()
  {
    using namespace boost;
    argument_vector av;
    int argc = mkt::var<int>("__argc");
    for(int i = 0; i < argc; i++)
      av.push_back(mkt::var(str(format("__argv_%1%") % i)));
    return av;
  }

  void argv(int argc, char **argv)
  {
    using namespace boost;
    mkt::argument_vector args;
    mkt::var("__argc", argc);
    for(int i = 0; i < argc; i++)
      mkt::var(str(format("__argv_%1%") % i), argv[i]);
  }
}
