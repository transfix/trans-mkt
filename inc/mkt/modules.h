#ifndef __MKT_MODULES_H__
#define __MKT_MODULES_H__

#include <mkt/vars.h>

/*
 * Modules API
 */
namespace mkt
{
  void init_modules();
  void final_modules();

  mkt_str import_module(const mkt_str& name);
  argument_vector loaded_modules();
  void expunge_module(const mkt_str& name);
  bool is_loaded_module(const mkt_str& name);

  map_change_signal& modules_changed();
  map_change_signal& module_pre_init();
  map_change_signal& module_post_init();
  map_change_signal& module_pre_final();
  map_change_signal& module_post_final();

  //returns true if string is a valid module name
  bool valid_module_name(const std::string& str);

  bool modules_at_exit();
}

#endif
