#include <mkt/config.h>
#include <mkt/app.h>
#include <mkt/exceptions.h>

#include <boost/foreach.hpp>
#include <boost/thread.hpp>

namespace
{
  mkt::command_map     commands;
  boost::mutex         _commands_lock;

  mkt::argument_vector _av;
  boost::mutex         _av_lock;

  void help(const mkt::argument_vector& args)
  {
    using namespace std;
    using namespace mkt;

    argument_vector prog_args = argv();
    std::string prog_name = !prog_args.empty() ? prog_args[0] : "mkt";

    cout << "Version: " << version() << endl;
    cout << "Usage: " << prog_name << " <command> <command args>" << endl << endl;
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
    if(args.size()<2) cout << "Hello, world!" << endl;
    else if(args.size()==2) cout << "Hello, " << args[1] << endl;
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
  std::string version()
  {
    return std::string(MKT_VERSION);
  }
  
  void exec(const argument_vector& args)
  {
    using namespace boost;
    boost::mutex::scoped_lock lock(_commands_lock);
    if(args.empty()) throw command_line_error("Missing command string");
    std::string cmd = args[0];
    if(commands.find(cmd)==commands.end())
      throw command_line_error(str(format("Invalid command: %1%") % cmd));
    commands[cmd].get<0>()(args);
  }

  argument_vector argv()
  {
    boost::mutex::scoped_lock lock(_av_lock);
    return _av;
  }

  void argv(const argument_vector& av)
  {
    boost::mutex::scoped_lock lock(_av_lock);
    _av = av;
  }

  void argv(int argc, char **argv)
  {
    mkt::argument_vector args;
    for(int i = 0; i < argc; i++)
      args.push_back(argv[i]);
    mkt::argv(args);
  }
}
