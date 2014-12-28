#ifndef __MKT_COMMANDS_H__
#define __MKT_COMMANDS_H__

#include <mkt/config.h>
#include <mkt/exceptions.h>
#include <mkt/types.h>

#include <boost/function.hpp>
#include <boost/tuple/tuple.hpp>

namespace mkt
{
  /*
   * Command execution API
   */
  MKT_DEF_EXCEPTION(command_error);
  typedef boost::function<void (const argument_vector&)> command_func;
  typedef boost::tuple<command_func, std::string>        command;
  typedef std::map<std::string, command>                 command_map;

  void add_command(const std::string& name,
                   const command_func& func,
                   const std::string& desc);
  void remove_command(const std::string& name);
  argument_vector get_commands();

  //Executes a command. The first string is the command, with everything
  //after being the command's arguments.
  void exec(const argument_vector& args);
  
  //Executes a command first splitting up a single string into arguments.
  void ex(const std::string& cmd);

  //Executes commands listed in a file where the first argument is the filename. 
  //If parallel is true, commands are executed in separate threads.
  void exec_file(const argument_vector& args, bool parallel = false);

  //Use this to determine if this module sees that the program is quitting. 
  //Trying to execute a command at that time will result in an exception 
  //and termination without finishing the rest of registered std::atexit 
  //functions.
  bool commands_at_exit();
}

#endif
