#include <boost/function.hpp>
#include <boost/tuple/tuple.hpp>

#include <string>
#include <vector>
#include <map>

namespace mkt
{
  /*
   * Commands
   */
  
  typedef std::vector<std::string> argument_vector;
  typedef boost::function<void (const argument_vector&)> command_func;
  typedef boost::tuple<command_func, std::string>        command;
  typedef std::map<std::string, command>                 command_map;

  //Executes a command, where the first string in the vector is the name of the
  //command and the rest are arguments.
  void exec(const argument_vector& args);
}
