#include <mkt/app.h>
#include <mkt/exceptions.h>

#include <boost/current_function.hpp>
#include <boost/foreach.hpp>

#include <iostream>
#include <cstdlib>

void do_help()
{
  mkt::exec(mkt::argument_vector(1,"help"));
}

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
              do_help();
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
