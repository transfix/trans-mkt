#ifndef __XCH_ASSETS_H__
#define __XCH_ASSETS_H__

#include <xch/types.h>

#include <mkt/exceptions.h>

#include <map>
#include <string>

/*
  Asset API
 */
namespace xch 
{
  typedef mkt::int64                         asset_id_t;
  typedef std::map<std::string, asset_id_t>  asset_map;
  typedef std::map<asset_id_t, std::string>  reverse_asset_map;
  extern  mkt::map_change_signal             assets_changed;
  void init_assets();
  void final_assets();
  void set_asset_id(const std::string& asset_name, asset_id_t asset_id);
  asset_id_t get_asset_id(const std::string& asset_name);
  std::string get_asset_name(asset_id_t asset_id);
  std::vector<std::string> get_asset_names();
  std::vector<asset_id_t> get_asset_ids();
  bool has_asset(asset_id_t asset_id);
  bool has_asset(const std::string& asset_name); 
  void remove_asset(const std::string& asset_name);
  void remove_asset(asset_id_t asset_id);

  asset_map get_assets();
}

#endif
