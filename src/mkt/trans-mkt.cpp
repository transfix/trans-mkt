#include <mkt/app.h>
#include <mkt/commands.h>
#include <mkt/threads.h>
#include <mkt/echo.h>
#include <mkt/modules.h>

//#include <xch/assets.h>
//#include <xch/accounts.h>

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

  void interactive()
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::ex("cmd");
  }
}

int main(int argc, char **argv)
{
  using namespace std;
  mkt::app a(argc, argv);

  //xch::init_assets();
  //xch::init_accounts();

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
              mkt::argument_vector args = a.argv();
              args.erase(args.begin()); //remove the program argument
              mkt::exec(args);
            }
        }
      catch(mkt::exception& e)
        {
          if(!e.what_str().empty()) 
            mkt::out(1).stream() << "Error: " << e.what_str() << endl;
          do_help();
          return EXIT_FAILURE;
        }
    }
  catch(std::exception& e)
    {
      mkt::out(1).stream() << "Exception: " << e.what() << endl;
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}
