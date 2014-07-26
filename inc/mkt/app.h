#include <boost/function.hpp>
#include <boost/tuple/tuple.hpp>

#include <string>
#include <vector>
#include <map>

namespace mkt
{
  //version string
  std::string version();

  /*
   * Command related types
   */
  
  typedef std::vector<std::string> argument_vector;
  typedef boost::function<void (const argument_vector&)> command_func;
  typedef boost::tuple<command_func, std::string>        command;
  typedef std::map<std::string, command>                 command_map;

  //accessing and modifying the argument vector for the running process.
  argument_vector argv();
  void argv(const argument_vector& av);

  //Executes a command given the program's argv vector as an argument_vector.
  //That is, the first string is the program exec and the second is the command, with
  //everything after being the command's arguments.
  void exec(const argument_vector& args);
}
