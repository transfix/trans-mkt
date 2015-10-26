#ifndef __MKT_UTILS_H__
#define __MKT_UTILS_H__

#include <mkt/config.h>
#include <mkt/types.h>
#include <mkt/threads.h>
#include <mkt/exceptions.h>
#include <mkt/app.h>

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
  bool valid_identifier(const mkt_str& str);

  template<class Exception = system_error>
  void check_identifier(const mkt_str& str)
  {
    if(!valid_identifier(str))
      throw Exception("Invalid identifier: " + str);
  }

  //TODO: these should be symmetrical...
  mkt_str ptime_to_str(const ptime& pt);
  ptime str_to_ptime(const mkt_str& s);
  
  inline ptime now() { return boost::posix_time::microsec_clock::universal_time(); }

  //Use this to determine if the program is exiting
  bool at_exit();
}

// Guard access to calling the signal to avoid bad 
// stuff when triggering signals after main() returns.
#define MKT_DEF_SIGNAL(sig_type, sig_name)                              \
  sig_type& sig_name()                                                  \
  {                                                                     \
    static sig_type sig_name ## _;                                      \
    bool as = mkt::wait_for_threads::at_start();                        \
    bool ae = mkt::wait_for_threads::at_exit();                         \
    if(as)                                                              \
      throw mkt::system_error(                                          \
        mkt::mkt_str(BOOST_CURRENT_FUNCTION) +                          \
	"Invalid operation at program start for signal " +		\
	mkt::mkt_str(#sig_name));					\
    else if(ae)                                                         \
      throw mkt::system_error(                                          \
	mkt::mkt_str(BOOST_CURRENT_FUNCTION) +                          \
	"Invalid operation at program end for signal " +		\
	mkt::mkt_str(#sig_name));                                       \
    return sig_name ## _;				                \
  }

#define MKT_DEF_MAP_CHANGE_SIGNAL(sig_name)                             \
  MKT_DEF_SIGNAL(map_change_signal, sig_name)


#endif
