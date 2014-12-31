#ifndef __MKT_ASSETS_H__
#define __MKT_ASSETS_H__

#include <mkt/config.h>
#include <mkt/types.h>
#include <mkt/exceptions.h>

#include <map>
#include <string>

/*
  Asset API
 */
namespace mkt
{
  typedef std::map<std::string, int64>  asset_map;
  typedef std::map<int64, std::string>  reverse_asset_map;
  extern map_change_signal assets_changed;
  void init_assets();
  void set_asset_id(const std::string& asset_name, int64 asset_id);
  int64 get_asset_id(const std::string& asset_name);
  std::string get_asset_name(int64 asset_id);
  std::vector<std::string> get_asset_names();
  std::vector<int64> get_asset_ids();
  bool has_asset(int64 asset_id);
  bool has_asset(const std::string& asset_name); 
  void remove_asset(const std::string& asset_name);
  void remove_asset(int64 asset_id);
}

#endif
