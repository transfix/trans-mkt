#include <xch/xch.h>

/*
  This is the module entry point. The module command 'import <modulename>'
  will look for dynamic libraries in paths listed in the sys_ld path list.
  On linux/unix, module names will be in filenames like lib<modulename>.so.
  On windows they'll be <modulename>.dll. 
*/
extern "C"
{
  void mkt_init_xch()
  {
    xch::init_assets();
    xch::init_accounts();
  }

  void mkt_final_xch()
  {
    xch::final_assets();
    xch::final_accounts();
  }
}
