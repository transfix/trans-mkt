#include <mkt/app.h>
#include <mkt/exceptions.h>

#ifdef MKT_INTERACTIVE
//#include <editline/readline.h>
#ifdef __WINDOWS__
  #include "../../inc/editline/readline.h"
#else
  #include <editline/readline.h>
#endif
#endif

#include <boost/current_function.hpp>
#include <boost/foreach.hpp>

#include <iostream>
#include <cstdlib>

void do_help()
{
  mkt::exec(mkt::argument_vector(1,"help"));
}

#ifdef MKT_INTERACTIVE
void interactive()
{
  using namespace std;
  char *line;
  while((line = readline("mkt> ")))
    {
      std::string str_line(line);
      add_history(line);
      free(line);

      if(str_line == "exit" || str_line == "quit") break;

      try
        {
          mkt::argument_vector args = mkt::split(str_line);
          if(!args.empty())
            mkt::exec(args);
        }
      catch(mkt::exception& e)
        {
          if(!e.what_str().empty()) 
            cout << "Error: " << e.what_str() << endl;          
        }
    }
}
#endif

int main(int argc, char **argv)
{
  using namespace std;
  
  mkt::wait_for_threads w;
  mkt::argv(argc, argv);

  try
    {
      try
        {
          //print help if not enough args
          if(argc<2)
            {
#ifdef MKT_INTERACTIVE
              interactive();
#else
              do_help();
#endif

            }
          else
            {
              mkt::argument_vector args = mkt::argv();
              args.erase(args.begin()); //remove the program argument
              mkt::exec(args);
            }
        }
      catch(mkt::exception& e)
        {
          if(!e.what_str().empty()) cout << "Error: " << e.what_str() << endl;
          do_help();
          return EXIT_FAILURE;
        }
    }
  catch(std::exception& e)
    {
      cerr << "Exception: " << e.what() << endl;
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}
