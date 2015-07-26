#include <xch/assets.h>

#include <mkt/commands.h>
#include <mkt/echo.h>
#include <mkt/threads.h>
#include <mkt/vars.h>

#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>
#include <boost/foreach.hpp>

#include <sstream>

/*
 * Assets API module implementation
 */
 
//This module's exceptions
namespace xch 
{
  MKT_DEF_EXCEPTION(assets_error);
}

//This module's static data
namespace
{
  struct assets_data
  {
    xch::asset_map          _asset_map;
    xch::reverse_asset_map  _reverse_asset_map;
    xch::mutex              _asset_map_mutex;
  };
  assets_data              *_assets_data = 0;

  void _assets_cleanup()
  {
    delete _assets_data;
    _assets_data = 0;
  }

  assets_data* _get_assets_data()
  {
    if(!_assets_data)
      _assets_data = new assets_data;

    if(!_assets_data)
      throw xch::assets_error("Error allocating module static data!");
    return _assets_data;
  }

  xch::asset_map& asset_map_ref()
  {
    return _get_assets_data()->_asset_map;
  }

  xch::reverse_asset_map& reverse_asset_map_ref()
  {
    return _get_assets_data()->_reverse_asset_map;
  }

  xch::mutex& asset_map_mutex_ref()
  {
    return _get_assets_data()->_asset_map_mutex;
  }
}

namespace
{
  void set_asset_id(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string
    if(local_args.size() < 2)
      throw xch::assets_error("Missing asset name and/or asset id arguments");
    std::string asset_name = local_args[0];
    xch::asset_id_t asset_id = -1;
    try
      {
        asset_id = boost::lexical_cast<xch::asset_id_t>(local_args[1]);
      }
    catch(boost::bad_lexical_cast&)
      {
        throw xch::assets_error("Invalid asset id argument");
      }
    if(asset_name == "null")
      throw xch::assets_error("Asset name cannot be 'null'");
    xch::set_asset_id(asset_name, asset_id);
    mkt::ret_val(asset_id);
  }

  void get_asset_id(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string
    if(local_args.empty())
      throw xch::assets_error("Missing asset name argument");
    mkt::ret_val(xch::get_asset_id(local_args[0]));
  }

  void get_asset_name(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string
    if(local_args.empty())
      throw xch::assets_error("Missing asset id argument");
    xch::asset_id_t asset_id = -1;
    try
      {
        asset_id = boost::lexical_cast<xch::asset_id_t>(local_args[0]);
      }
    catch(boost::bad_lexical_cast&)
      {
        throw xch::assets_error("Invalid asset id argument");
      }
    mkt::ret_val(xch::get_asset_name(asset_id));
  }

  void get_assets(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    std::vector<std::string> names = xch::get_asset_names();
    std::stringstream ss;
    BOOST_FOREACH(std::string& name, names)
      ss << "\"" << name << "\", " 
	 << "\"" << xch::get_asset_id(name) << "\"" << std::endl;
    mkt::ret_val(ss.str());
  }

  void has_asset(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string
    if(local_args.empty())
      throw xch::assets_error("Missing asset id or asset name argument");

    xch::asset_id_t asset_id = -1;
    std::string asset_name("null");
    try
      {
        asset_id = boost::lexical_cast<xch::asset_id_t>(local_args[0]);
      }
    catch(boost::bad_lexical_cast&)
      {
        asset_name = local_args[0];
      }

    bool flag = xch::has_asset(asset_id) || 
      xch::has_asset(asset_name);
    mkt::ret_val(flag);
  }

  void remove_asset(const mkt::argument_vector& args)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string
    if(local_args.empty())
      throw xch::assets_error("Missing asset id or asset name argument");

    xch::asset_id_t asset_id = -1;
    std::string asset_name("null");
    try
      {
        asset_id = boost::lexical_cast<xch::asset_id_t>(local_args[0]);
        xch::remove_asset(asset_id);
	mkt::ret_val(asset_id);
      }
    catch(boost::bad_lexical_cast&)
      {
        asset_name = local_args[0];
        xch::remove_asset(asset_name);
	mkt::ret_val(asset_name);
      }    
  }
}

//assets API impelmentation
namespace xch 
{
  map_change_signal assets_changed;

  void init_assets()
  {
    using namespace mkt;
    add_command("get_assets", ::get_assets, 
		"Prints all assets name and id pairs.");
    add_command("get_asset_id", ::get_asset_id, 
		"get_asset_id [<asset name>]\nPrints asset id associated with specified asset name");
    add_command("get_asset_name", ::get_asset_name, 
		"get_asset_name [<asset id>]\nPrints asset name associated with specified asset id");
    add_command("has_asset", ::has_asset,
		"has_asset [<asset name>|<asset id>]\n"
		"Prints true or false whether a system has a specified asset");
    add_command("remove_asset", ::remove_asset, 
		"remove_asset [<asset name>|<asset id>]\nRemove specified asset");
    add_command("set_asset_id", ::set_asset_id, 
		"set_asset_id [<asset name>] [<asset id>]\nBinds a name with an asset id");
  }

  void final_assets()
  {
    mkt::remove_command("get_assets");
    mkt::remove_command("get_asset_id");
    mkt::remove_command("get_asset_name");
    mkt::remove_command("has_asset");
    mkt::remove_command("remove_asset");
    mkt::remove_command("set_asset_id");
    
    _assets_cleanup();
  }
  
  void set_asset_id(const std::string& asset_name, asset_id_t asset_id)
  {
    using namespace boost;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(has_asset(asset_id))
      throw assets_error(str(format("There is already an asset with id %1%")
                             % asset_id));

    remove_asset(asset_name);
    
    {
      unique_lock lock(asset_map_mutex_ref());
      asset_map_ref()[asset_name] = asset_id;
      reverse_asset_map_ref()[asset_id] = asset_name;
    }

    assets_changed(asset_name);
  }

  asset_id_t get_asset_id(const std::string& asset_name)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(!has_asset(asset_name)) return -1;

    {
      shared_lock lock(asset_map_mutex_ref());
      return asset_map_ref()[asset_name]; 
    }
  }

  std::string get_asset_name(asset_id_t asset_id)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    if(!has_asset(asset_id)) return "null";

    {
      shared_lock lock(asset_map_mutex_ref());
      return reverse_asset_map_ref()[asset_id];
    }
  }

  std::vector<std::string> get_asset_names()
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    shared_lock lock(asset_map_mutex_ref());
    std::vector<std::string> names;
    BOOST_FOREACH(asset_map::value_type& cur, asset_map_ref())
      {
        if(!cur.first.empty())
          names.push_back(cur.first);
      }
    return names;
  }

  std::vector<asset_id_t> get_asset_ids()
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    shared_lock lock(asset_map_mutex_ref());
    std::vector<asset_id_t> ids;
    BOOST_FOREACH(reverse_asset_map::value_type& cur, 
                  reverse_asset_map_ref())
      {
        if(cur.first != -1)
          ids.push_back(cur.first);
      }
    return ids;
  }

  bool has_asset(asset_id_t asset_id)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    shared_lock lock(asset_map_mutex_ref());
    return reverse_asset_map_ref().find(asset_id) !=
      reverse_asset_map_ref().end();
  }

  bool has_asset(const std::string& asset_name)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    shared_lock lock(asset_map_mutex_ref());
    return asset_map_ref().find(asset_name) !=
      asset_map_ref().end();
  }

  void remove_asset(const std::string& asset_name)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    asset_id_t asset_id = get_asset_id(asset_name);
    {
      unique_lock lock(asset_map_mutex_ref());
      asset_map_ref().erase(asset_name);
      reverse_asset_map_ref().erase(asset_id);
    }
    assets_changed(asset_name);
  }

  void remove_asset(asset_id_t asset_id)
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    std::string asset_name = get_asset_name(asset_id);
    {
      unique_lock lock(asset_map_mutex_ref());
      asset_map_ref().erase(asset_name);
      reverse_asset_map_ref().erase(asset_id);      
    }
    assets_changed(asset_name);
  }

  asset_map get_assets() { return asset_map_ref(); }
}
