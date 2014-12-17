#include <mkt/app.h>
#include <mkt/commands.h>
#include <mkt/threads.h>
#include <mkt/echo.h>

#ifdef MKT_USING_XMLRPC
#include <mkt/xmlrpc.h>
#endif

#ifdef MKT_INTERACTIVE
#ifdef __WINDOWS__
#include <editline_win/readline.h>
#else
  #include <editline/readline.h>
#endif
#endif

#include <boost/current_function.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/foreach.hpp>

#include <iostream>
#include <cstdlib>

namespace
{
  void do_help()
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::ex("help");
  }

#ifdef MKT_INTERACTIVE
  void interactive()
  {
    using namespace std;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

    char *line;
    while((line = readline("mkt> ")))
      {
        std::string str_line(line);
        add_history(line);
        free(line);

        if(str_line == "exit" || str_line == "quit") break;

        try
          {
            mkt::ex(str_line);
          }
        catch(mkt::exception& e)
          {
            if(!e.what_str().empty()) 
              cout << "Error: " << e.what_str() << endl;          
          }
      }
  }
#endif
}

int main(int argc, char **argv)
{
  using namespace std;
  mkt::thread_info ti(BOOST_CURRENT_FUNCTION);

  mkt::wait_for_threads w;
  mkt::argv(argc, argv);

  mkt::init_echo();

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
