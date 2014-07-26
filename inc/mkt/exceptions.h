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

#define MKT_DEF_EXCEPTION(name) \
  class name : public exception \
  { \
  public: \
    name () : _msg(#name) {} \
    name (const std::string& msg) : \
      _msg(boost::str(boost::format(#name " exception: %1%") % msg)) {} \
    virtual ~name() throw() {} \
    virtual const std::string& what_str() const throw() { return _msg; } \
  private: \
    std::string _msg; \
  }

  MKT_DEF_EXCEPTION(command_line_error);
}
