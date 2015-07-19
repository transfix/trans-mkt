#ifndef __MKT_EXCEPTIONS_H__
#define __MKT_EXCEPTIONS_H__

#include <boost/exception/exception.hpp>
#include <boost/format.hpp>

namespace mkt
{
  /***** Exceptions ****/
  class exception : public boost::exception
  {
  public:
    exception() {}
    virtual ~exception() throw() {}
    virtual const std::string& what_str() const throw () = 0;
    virtual const char *what () const throw()
    {
      return what_str().c_str();
    }
  };
}

#define MKT_DEF_EXCEPTION(name)                                                \
  class name : public mkt::exception		                               \
  {                                                                            \
  public:                                                                      \
    name () : _msg(#name) {}                                                   \
    name (const std::string& msg) :                                            \
      _msg(boost::str(boost::format(#name " exception: %1%") % msg)) {}        \
    virtual ~name() throw() {}                                                 \
    virtual const std::string& what_str() const throw() { return _msg; }       \
  private:                                                                     \
    std::string _msg;                                                          \
  }

namespace mkt
{
  //Standard, system-wide exceptions
  MKT_DEF_EXCEPTION(system_error);
  MKT_DEF_EXCEPTION(null_implementation_error);
}


#endif
