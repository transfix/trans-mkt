#include <xch/xch.h>

/*
  This is the module entry point. The module command 'import <modulename>'
  will look for dynamic libraries in paths listed in the sys.libs directory.
  On linux/unix, module names will be in filenames like lib<modulename>.so.
  On windows they'll be <modulename>.dll. 
*/
extern "C"
{
  void init_xch()
  {
    xch::init_assets();
    xch::init_accounts();
  }

  void final_xch()
  {
    // nothing in here for now
  }
}
