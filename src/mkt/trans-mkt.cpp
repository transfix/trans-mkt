#include <mkt/config.h>
#include <mkt/commands.h>
#include <mkt/exceptions.h>

#include <boost/current_function.hpp>
#include <boost/foreach.hpp>

#include <iostream>
#include <cstdlib>

void do_help(const std::string& prog)
{
  mkt::argument_vector help_args;
  help_args.push_back(prog);
  help_args.push_back("help");
  mkt::exec(help_args);
}

int main(int argc, char **argv)
{
  using namespace std;
  using namespace mkt;
  
  vector<string> args;
  for(int i = 0; i < argc; i++)
    args.push_back(argv[i]);

  try
    {
      //print help if not enough args
      if(args.size()<2)
        {
          do_help(argv[0]);
        }
      else
        {
          mkt::exec(args);
        }
    }
  catch(command_line_error& e)
    {
      if(!e.what_str().empty()) cout << "Error: " << e.what_str() << endl;
      cout << "Usage: " << argv[0] << " <command> <command args>" << endl;
      do_help(args[0]);
      return EXIT_FAILURE;
    }
  catch(std::exception& e)
    {
      cerr << "Exception: " << e.what() << endl;
    }

  return EXIT_SUCCESS;
}
