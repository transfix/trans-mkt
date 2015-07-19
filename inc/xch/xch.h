#ifndef __XCH_XCH_H__
#define __XCH_XCH_H__

extern "C" void init_xch();   //called to load the module
extern "C" void final_xch();  //called to unload the module

#include <xch/assets.h>
#include <xch/accounts.h>

#endif
