#ifndef __MKT_MODULES_H__
#define __MKT_MODULES_H__

#include <mkt/vars.h>

/*
 * Modules API
 */
namespace mkt
{
  void init_modules();
  var_string import_module(const var_string& name);
  argument_vector loaded_modules();
  void expunge_module(const var_string& name);
  bool is_loaded_module(const var_string& name);

  extern map_change_signal modules_changed;
  extern map_change_signal module_pre_init;
  extern map_change_signal module_post_init;
  extern map_change_signal module_pre_final;
  extern map_change_signal module_post_final;

  //returns true if string is a valid module name
  bool valid_module_name(const std::string& str);
}

#endif
