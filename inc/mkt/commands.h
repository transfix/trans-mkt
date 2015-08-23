#ifndef __MKT_COMMANDS_H__
#define __MKT_COMMANDS_H__

#include <mkt/config.h>
#include <mkt/exceptions.h>
#include <mkt/vars.h>
#include <mkt/types.h>
#include <mkt/threads.h>

#include <boost/function.hpp>
#include <boost/tuple/tuple.hpp>

namespace mkt
{
  /*
   * Command execution API
   */
  MKT_DEF_EXCEPTION(command_error);
  typedef boost::function<void (const argument_vector&)> command_func;
  typedef boost::tuple<command_func, mkt_str>            command;
  typedef std::map<mkt_str, command>                     command_map;

  void init_commands();
  void final_commands();

  void add_command(const mkt_str& name,
                   const command_func& func,
                   const mkt_str& desc);
  void remove_command(const mkt_str& name);
  argument_vector get_commands();
  mkt_str get_command_description(const mkt_str& name);
  bool has_command(const mkt_str& name);

  //Executes a command. The first string is the command, with everything
  //after being the command's arguments.
  void exec(const argument_vector& args);
  
  //Executes a command first splitting up a single string into arguments.
  void ex(const mkt_str& cmd, bool escape = true);

  //Executes commands listed in a file where the first argument is the filename. 
  //If parallel is true, commands are executed in separate threads.
  void exec_file(const argument_vector& args, bool parallel = false, 
		 bool escape = true);

  //Use this to determine if this module sees that the program is quitting. 
  //Trying to execute a command at that time will result in an exception 
  //and termination without finishing the rest of registered std::atexit 
  //functions.
  bool commands_at_exit();

  map_change_signal&   command_added();
  map_change_signal&   command_removed();
  typedef boost::
    signals2::
    signal<void (const argument_vector&, const thread_id&, const mkt_str&)> 
    command_exec_signal;
  command_exec_signal& command_pre_exec();
  command_exec_signal& command_post_exec();
}

#endif
