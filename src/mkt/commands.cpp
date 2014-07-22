#include <mkt/config.h>
#include <mkt/commands.h>
#include <mkt/exceptions.h>

#include <boost/format.hpp>
#include <boost/foreach.hpp>

namespace
{
  mkt::command_map commands;

  void help(const mkt::argument_vector& args)
  {
    using namespace std;
	using namespace mkt;
    cout << "Usage: " << args[0] << " <command> <command args>" << endl << endl;
    BOOST_FOREACH(command_map::value_type& cmd, commands)
      {
        cout << " - " << cmd.first << endl;
        cout << cmd.second.get<1>() << endl << endl;
      }
  }

  void hello(const mkt::argument_vector& args)
  {
    using namespace std;
	using namespace mkt;
    if(args.empty()) cout << "Hello, world!" << endl;
    else if(args.size()==1) cout << "Hello, " << args[0] << endl;
    else throw command_line_error("Too many arguments");
  }

  class init_commands
  {
  public:
    init_commands()
    {
      using namespace std;
	  using namespace mkt;
      using boost::str;
      using boost::format;
      using boost::make_tuple;
      
      commands["hello"] =
        boost::make_tuple(command_func(hello),
                          string("Prints hello world."));
      commands["help"] = 
        boost::make_tuple(command_func(help),
                          string("Prints command list."));
    }
  } static_init;
}

namespace mkt
{
  void exec(const argument_vector& args)
  {
    if(args.empty()) throw command_line_error("Missing command string");
    std::string cmd = args[0];
	mkt::argument_vector localargs = args;
    localargs.erase(localargs.begin()); //erase the command string
    if(commands.find(cmd)==commands.end())
      throw command_line_error("Invalid command.");
    commands[cmd].get<0>()(args);
  }
}
