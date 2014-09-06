#include <mkt/app.h>
#include <mkt/exceptions.h>

#ifdef MKT_INTERACTIVE
#ifdef __WINDOWS__
#include <editline_win/readline.h>
#else
  #include <editline/readline.h>
#endif
#endif

#include <boost/foreach.hpp>
#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>

#include <iostream>
#include <cstdlib>
#include <ctime>

#include <vector>
#include <map>
#include <algorithm>

namespace mkt
{
  typedef std::map<std::string, int64>  asset_map;
  asset_map                             _assets; //map of asset names to asset ids
  mutex                                 _asset_map_mutex;

  asset_map get_assets()
  {
    shared_lock lock(_asset_map_mutex);
    return _assets;
  }

  void set_assets(asset_map am)
  {
    unique_lock lock(_asset_map_mutex);
    _assets = am;
  }

  void set_asset_id(const std::string& asset_name, int64 asset_id)
  {
    unique_lock lock(_asset_map_mutex);
    _assets[asset_name] = asset_id;
  }

  int64 get_asset_id(const std::string& asset_name)
  {
    shared_lock lock(_asset_map_mutex);
    return _assets[asset_name]; 
  }

  std::vector<std::string> get_asset_names()
  {
    shared_lock lock(_asset_map_mutex);
    std::vector<std::string> names;
    BOOST_FOREACH(asset_map::value_type& cur, _assets)
      names.push_back(cur.first);
    return names;
  }

  MKT_DEF_EXCEPTION(account_error);

  class transaction
  {
  public:
    transaction(ptime cur_time = boost::posix_time::min_date_time, 
		int64 to_account_id = -1,
		int64 from_account_id = -1, 
		int64 asset_id = -1,
		double amount = 0.0)
      : _cur_time(cur_time), _to_account_id(to_account_id),
	_from_account_id(from_account_id), _asset_id(asset_id),
	_amount(amount) {}

    transaction(const transaction& rhs)
      : _cur_time(rhs._cur_time), _to_account_id(rhs._to_account_id),
	_from_account_id(rhs._from_account_id), _asset_id(rhs._asset_id),
	_amount(rhs._amount) {}

    transaction& operator=(const transaction& rhs)
    {
      if(this == &rhs) return *this;
      _cur_time = rhs._cur_time;
      _to_account_id = rhs._to_account_id;
      _from_account_id = rhs._from_account_id;
      _asset_id = rhs._asset_id;
      _amount = rhs._amount;
      return *this;
    }

  private:
    ptime _cur_time;
    int64 _to_account_id;
    int64 _from_account_id;
    int64 _asset_id;
    int64 _amount;
  };
  
  class accounts
  {
  public:
    typedef std::map<int64, double>   balances;     //map key is asset ID
    typedef std::map<int64, balances> account_map;  //map key is account ID

    static void exec_transaction(int64 to_account_id,
				 int64 from_account_id,
				 int64 asset_id,
				 double amount);
    static void init_account(int64 account_id);
    static double balance(int64 account_id, int64 asset_id);
    static bool has_account(int64 account_id);
    static std::vector<int64> get_account_ids();
    static int64 get_unique_id();

    static boost::signals2::signal<void (int64)>  account_changed;
    
  private:
    static account_map                       _account_map;
    static std::map<ptime, transaction>      _transaction_history;
    static mutex                             _account_map_mutex;

    accounts();
    accounts(const accounts&);
  };
  
  boost::signals2::signal<void (int64)>  accounts::account_changed;
  accounts::account_map                  accounts::_account_map;
  std::map<ptime, transaction>           accounts::_transaction_history;
  mutex                                  accounts::_account_map_mutex;
  
  void accounts::exec_transaction(int64 to_account_id,
				  int64 from_account_id,
				  int64 asset_id,
				  double amount)
  {
    {
      using namespace boost;
      unique_lock lock(_account_map_mutex);

      if(from_account_id >= 0 &&
	 _account_map.find(from_account_id) == _account_map.end())
	throw account_error(str(format("invalid from_account_id %1%")
				% to_account_id));
      if(_account_map.find(to_account_id) == _account_map.end())
	throw account_error(str(format("invalid to_account_id %1%")
				% to_account_id));
      if(from_account_id == to_account_id)
	throw account_error("to_account_id == from_account_id");

      //a from_account_id less than zero creates money from nothing
      double from_account_balance = 
	from_account_id >= 0 ? 
	_account_map[from_account_id][asset_id] : amount;
      double to_account_balance = _account_map[to_account_id][asset_id];

      from_account_balance -= amount;
      to_account_balance += amount;

      //do not support negative balances
      if(from_account_balance < 0.0)
	throw account_error(str(format("not enough funds in account %1%") 
				% from_account_id));

      //everything is ok, lets set the new balances now
      if(from_account_id >= 0)
	_account_map[from_account_id][asset_id] = from_account_balance;
      _account_map[to_account_id][asset_id] = to_account_balance;

      ptime cur_time = boost::posix_time::microsec_clock::universal_time();
      _transaction_history[cur_time]
	= transaction(cur_time, to_account_id, from_account_id, 
		      asset_id, amount);
    }

    if(from_account_id >= 0) account_changed(from_account_id);
    account_changed(to_account_id);
  }

  //initializes an account with zero balances for all assets known by the system
  void accounts::init_account(int64 account_id)
  {
    using namespace boost;
    if(account_id < 0) 
      throw account_error(str(format("invalid account_id %1%")
			      % account_id));
    asset_map assets = get_assets();
    {
      unique_lock lock(_account_map_mutex);
      ptime cur_time = boost::posix_time::microsec_clock::universal_time();
      BOOST_FOREACH(asset_map::value_type& cur, assets)
	{
	  int64 asset_id = cur.second;
	  _account_map[account_id][asset_id] = 0.0;
	  _transaction_history[cur_time]
	    = transaction(cur_time, account_id, -1, asset_id, 0.0);
	}
    }
    
    account_changed(account_id);
  }

  //returns balance for account/asset pair.
  double accounts::balance(int64 account_id, int64 asset_id)
  {
    using namespace boost;
    shared_lock lock(_account_map_mutex);
    if(_account_map.find(account_id) == _account_map.end())
      throw account_error(str(format("invalid account_id %1%")
			      % account_id));
    return _account_map[account_id][asset_id];
  }

  bool accounts::has_account(int64 account_id)
  {
    shared_lock lock(_account_map_mutex);
    return _account_map.find(account_id) != _account_map.end();
  }

  std::vector<int64> accounts::get_account_ids()
  {
    shared_lock lock(_account_map_mutex);
    std::vector<int64> ids;
    BOOST_FOREACH(const account_map::value_type& cur, _account_map)
      ids.push_back(cur.first);
    return ids;
  }

  int64 accounts::get_unique_id()
  {
    using namespace std;
    int64 account_id = -1;
    srand(time(0));
    do
      {
	account_id = rand();
      }
    while(has_account(account_id));
    return account_id;
  }

  MKT_DEF_EXCEPTION(market_error);

  class market
  {
  public:
    enum order_type
      {
	INVALID_ORDER, ASK, BID
      };

    market(int64 oaid = -1, int64 paid = -1)
      : _order_asset_id(oaid), _payment_asset_id(paid)
    {}

    int64_t add_order(order_type ot,
		      int64 account_id, double volume, double cost)
    {
      order_ptr optr(new order(ot, account_id, volume, cost));
      {
	unique_lock lock(_mutex);
	double price = optr->price();
	switch(ot)
	  {
	  case ASK: _ask_orderbook[price] = optr; break;
	  case BID: _bid_orderbook[price] = optr; break;
	  default: throw market_error("invalid order_type");
	  }
	_orders[optr->_order_id] = optr;
      }
      resolve_orderbook();
      orderbook_changed();
    }

    boost::signals2::signal<void ()>  orderbook_changed;

  private:    
    static int64 get_next_order_id()
    {
      static int64 id = 13371337;
      static mutex local_mutex;
      unique_lock lock(local_mutex);
      return id++;
    }

    struct order
    {
      order() :
	_order_type(INVALID_ORDER),
	_order_id(get_next_order_id()), _account_id(-1),
	_open_time(boost::posix_time::min_date_time),
	_close_time(boost::posix_time::min_date_time),
	_volume(0.0), _cost(0.0), 
	_available_volume(0.0) {}

      order(const order& rhs)
	: _order_type(rhs._order_type), 
	  _order_id(rhs._order_id), _account_id(rhs._account_id),
	  _open_time(rhs._open_time), _close_time(rhs._close_time),
	  _volume(rhs._volume), _cost(rhs._cost), 
	  _available_volume(rhs._available_volume) {}      

      order(order_type ot, int64 acct_id, 
	    double volume, double cost)
	: _order_type(ot), _order_id(get_next_order_id()),
	  _account_id(acct_id),
	  _open_time(boost::posix_time::microsec_clock::universal_time()),
	  _close_time(boost::posix_time::min_date_time),
	  _volume(volume), _cost(cost),
	  _available_volume(volume)
      {}

      order& operator=(const order& rhs)
      {
	if(this == &rhs) return *this;
	_order_type = rhs._order_type;
	_order_id = rhs._order_id;
	_account_id = rhs._account_id;
	_open_time = rhs._open_time;
	_close_time = rhs._close_time;
	_volume = rhs._volume;
	_cost = rhs._cost;
	_available_volume = rhs._available_volume;
	return *this;
      }

      //if the close time is set, consider it closed
      bool closed()
      {
	return _close_time != boost::posix_time::min_date_time;
      }

      void close()
      {
	if(closed()) return;
	if(_order_type == ASK)
	  _available_volume = 0.0;
	else if(_order_type == BID)
	  _available_volume = _volume;
	_close_time =
	  boost::posix_time::microsec_clock::universal_time();
      }

      double price()
      {
	return _cost / _volume;
      }

      order_type _order_type;

      int64 _order_id;
      int64 _account_id;

      ptime _open_time;
      ptime _close_time;

      //TODO: use fixed digit type
      double _volume; //order amount
      double _cost;   //amount to spend of payment asset to fill order

      //asks are selling, so this tends towards zero for ask orders
      //buys are buying, so this tends towards _volume for bid orders
      double _available_volume;
    };

    //This function is the main market engine, it executes orders if any can be matched
    //on the book.
    void resolve_orderbook()
    {
      unique_lock lock(_mutex);
      double min_ask_price = 0.0;
      double max_bid_price = 0.0;
      order_ptr min_ask_ptr;
      order_ptr max_bid_ptr;
 
      do
	{
	  bool all_closed = false;

	  order_map::iterator min_ask_iter =
	    _ask_orderbook.begin();

	  //nothing to do if no ask orders
	  if(min_ask_iter == _ask_orderbook.end())
	    break;

	  //find first non closed ask order
	  do
	    {
	      min_ask_price = min_ask_iter->first;
	      min_ask_ptr   = min_ask_iter->second;

	      if(!min_ask_ptr ||
		 min_ask_ptr->closed())
		++min_ask_iter;
	      if(min_ask_iter == _ask_orderbook.end())
		{
		  all_closed = true;
		  break;
		}
	    }
	  while(min_ask_ptr->closed());
	  
	  //nothing to do if all ask orders have been closed
	  if(all_closed) break;
	  
	  order_map::reverse_iterator max_bid_iter =
	    _bid_orderbook.rbegin();
	  
	  //nothing to do if no bid orders
	  if(max_bid_iter == _bid_orderbook.rend())
	    break;

	  //find first non closed bid order
	  do
	    {
	      max_bid_price = max_bid_iter->first;
	      max_bid_ptr   = max_bid_iter->second;

	      if(!max_bid_ptr ||
		 max_bid_ptr->closed())
		++max_bid_iter;
	      if(max_bid_iter == _bid_orderbook.rend())
		{
		  all_closed = true;
		  break;
		}
	    }
	  while(max_bid_ptr->closed());
	  
	  //nothing to do if all bid orders have been closed
	  if(all_closed) break;

	  if(!min_ask_ptr)
	    throw market_error("null order in orderbook");
	  if(!max_bid_ptr)
	    throw market_error("null order in orderbook");

	  //nothing to do if there is a spread
	  if(min_ask_price > max_bid_price)
	    break;

	  double ask_available = min_ask_ptr->_available_volume;
	  double bid_available = max_bid_ptr->_available_volume;

	  double new_bid_available =
	    std::min(bid_available + ask_available, max_bid_ptr->_volume);
	  double dv = new_bid_available - bid_available; //amount consumed by the bid
	  double new_ask_available = 
	    std::max(ask_available - dv, 0.0);
	  
	  min_ask_ptr->_available_volume = new_ask_available;
	  max_bid_ptr->_available_volume = new_bid_available;

	  //close the ask or bid order if they are filled up to a certain epsilon
	  const double epsilon = 0.001;
	  if(new_ask_available < epsilon)
	    min_ask_ptr->close();
	  if(max_bid_ptr->_volume - new_bid_available < epsilon)
	    max_bid_ptr->close();
	  
	  order_ptr oldest_ptr = 
	    min_ask_ptr->_open_time < max_bid_ptr->_open_time ?
	    min_ask_ptr : max_bid_ptr;

	  double dv_cost = 
	    dv * oldest_ptr->price();

	  //finally execute the exchange
	  accounts::exec_transaction(min_ask_ptr->_account_id,
				     max_bid_ptr->_account_id,
				     _payment_asset_id,
				     dv_cost);
	  accounts::exec_transaction(max_bid_ptr->_account_id,
				     min_ask_ptr->_account_id,
				     _order_asset_id,
				     dv);
	}
      while(min_ask_price <= max_bid_price);
    }

    int64 _order_asset_id;
    int64 _payment_asset_id;

    typedef boost::shared_ptr<order>    order_ptr;
    //map key is price == _cost / _volume
    typedef std::map<double, order_ptr> order_map;
    order_map       _ask_orderbook;
    order_map       _bid_orderbook;

    std::map<int64, order_ptr>        _orders; //order_id -> order map

    //compare order
    struct cmp_order
    {
      bool operator()(const order_map::value_type& left, 
		      const order_map::value_type& right) const
      {
	return left.first < right.first;
      }
    };

    mutable mutex                     _mutex;
  };

#if 0
  std::map<std::string, market>       _markets;
  mutex                               _markets_mutex;
#endif

  //TODO:
  //commands:
  //buy - post a bid limit order
  //sell - post an ask limit order
  //

  //functions:
  //resolve orderbook - after every change to orderbook, check if any buys match sells
  //if so, execute the transaction changing the respective balances of the market participants
  //involved in the order
}

namespace
{
  void do_help()
  {
    mkt::exec(mkt::argument_vector(1,"help"));
  }

#ifdef MKT_INTERACTIVE
  void interactive()
  {
    using namespace std;
    char *line;
    while((line = readline("mkt> ")))
      {
        std::string str_line(line);
        add_history(line);
        free(line);

        if(str_line == "exit" || str_line == "quit") break;

        try
          {
            mkt::argument_vector args = mkt::split(str_line);
            if(!args.empty())
              mkt::exec(args);
          }
        catch(mkt::exception& e)
          {
            if(!e.what_str().empty()) 
              cout << "Error: " << e.what_str() << endl;          
          }
      }
  }
#endif
}

void print_accounts()
{
  std::vector<mkt::int64> account_ids = mkt::accounts::get_account_ids();
  std::vector<std::string> asset_names = mkt::get_asset_names();
  BOOST_FOREACH(mkt::int64 id, account_ids)
    {
      std::cout << "id: " << id << std::endl;
      BOOST_FOREACH(std::string& asset_name, asset_names)
	std::cout << "\tasset: " << asset_name << " " << "balance: " 
		  << mkt::accounts::balance(id, mkt::get_asset_id(asset_name)) << std::endl;
    }
}

int main(int argc, char **argv)
{
  using namespace std;
  
  mkt::wait_for_threads w;
  mkt::argv(argc, argv);

  mkt::set_asset_id("XBT", 31337);
  mkt::set_asset_id("XDG", 31338);
  mkt::set_asset_id("XBC", 31339);

  for(int i = 0; i < 5; i++)
    mkt::accounts::init_account(mkt::accounts::get_unique_id());

  //init accounts with some random balance
  std::vector<mkt::int64> account_ids = mkt::accounts::get_account_ids();
  BOOST_FOREACH(mkt::int64 id, account_ids)
    mkt::accounts::exec_transaction(id, -1, mkt::get_asset_id("XBT"), (rand() % 666)/333.0);
  print_accounts();

  mkt::accounts::exec_transaction(account_ids[0], account_ids[1], 
				  mkt::get_asset_id("XBT"), 
				  mkt::accounts::balance(account_ids[1], mkt::get_asset_id("XBT")));
  print_accounts();

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
              mkt::argument_vector args = mkt::argv();
              args.erase(args.begin()); //remove the program argument
              mkt::exec(args);
            }
        }
      catch(mkt::exception& e)
        {
          if(!e.what_str().empty()) cout << "Error: " << e.what_str() << endl;
          do_help();
          return EXIT_FAILURE;
        }
    }
  catch(std::exception& e)
    {
      cerr << "Exception: " << e.what() << endl;
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}
