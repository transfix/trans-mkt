#ifndef __MKT_MODULES_H__
#define __MKT_MODULES_H__

#include <mkt/vars.h>

/*
 * Modules API
 */
namespace mkt
{
  void init_modules();
  void import_module(const var_string& name);
  argument_vector loaded_modules();
  void expunge_module(const var_string& name);
}

#endif
