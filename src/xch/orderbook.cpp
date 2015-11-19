#include <xch/assets.h>
#include <xch/accounts.h>

#include <mkt/app.h>
#include <mkt/commands.h>
#include <mkt/threads.h>

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
#include <set>
#include <algorithm>
#include <limits>

namespace xch
{
  ///////// -------- market

  MKT_DEF_EXCEPTION(market_error);

  class market
  {
  public:
    enum order_type
      {
	INVALID_ORDER, ASK, BID
      };

    market(int64 oaid = -1, int64 paid = -1);
    int64 add_order(order_type ot, int64 account_id, 
		    double volume, double cost);
    void cancel_order(int64 order_id);
    std::set<int64> get_open_order_ids();
    std::set<int64> get_closed_order_ids();
    std::set<int64> get_order_ids();
    
    order_type get_order_type(int64 order_id);
    int64      get_order_account_id(int64 order_id);
    ptime      get_order_open_time(int64 order_id);
    ptime      get_order_close_time(int64 order_id);
    double     get_order_volume(int64 order_id);
    double     get_order_cost(int64 order_id);
    double     get_order_available_volume(int64 order_id);
    double     get_order_price(int64 order_id);
    bool       get_order_closed(int64 order_id);

    //changing the order or payment asset will reset the orderbooks
    int64 order_asset_id() const;
    void  order_asset_id(int64 oaid);
    int64 payment_asset_id() const;
    void  payment_asset_id(int64 paid);
    
    //fees
    double ask_fee() const;
    void   ask_fee(double fee);
    double bid_fee() const;
    void   bid_fee(double fee);

    //market event signals
    boost::signals2::signal<void ()>               orderbook_changed;
    boost::signals2::signal<void (int64)>          order_added;
    boost::signals2::signal<void (int64)>          order_removed;
    boost::signals2::signal<void (double, double)> trade_executed;

  protected:
    static int64 get_next_order_id();

    class order
    {
    public:
      order();
      order(const order& rhs);
      order(order_type ot, int64 acct_id, 
	    double volume, double cost);
      order& operator=(const order& rhs);

      //if the close time is set, consider it closed
      bool closed();      
      void close(bool filled = false);
      double price();

      //synchronized access
      order_type get_order_type() const;
      void set_order_type(order_type ot);
      int64 order_id() const;
      void order_id(int64 id);
      int64 account_id() const;
      void account_id(int64 id);
      ptime open_time() const;
      void open_time(ptime ot);
      ptime close_time() const;
      void close_time(ptime ot);
      double volume() const;
      void volume(double v);
      double cost() const;
      void cost(double c);
      double available_volume() const;
      void available_volume(double av);

    private:
      order_type _order_type;

      int64 _order_id;   //this order's id number for lookup
      int64 _account_id; //market participant account associated with this order

      ptime _open_time;
      ptime _close_time;

      //TODO: use fixed digit type
      double _volume; //order amount
      double _cost;   //amount to spend of payment asset to fill order

      //asks are selling, so this tends towards zero for ask orders
      //buys are buying, so this tends towards _volume for bid orders
      double _available_volume;

      mutable mutex                 _mutex;
    };

    //This function is the main market engine, it executes orders if any can be matched
    //on the book.
    void resolve_orderbook();

    //clears all orders
    void reset_orderbook();

    int64                                     _order_asset_id;
    int64                                     _payment_asset_id;

    typedef boost::shared_ptr<order>          order_ptr;
    typedef std::multimap<ptime, order_ptr>   order_time_map;
    typedef order_time_map::value_type        order_time_pair;
    //map key is price == _cost / _volume
    typedef std::map<double, order_time_map>  orderbook_map;
    orderbook_map                             _ask_orderbook;
    orderbook_map                             _bid_orderbook;

    typedef std::map<int64, order_ptr>        order_id_map;
    order_id_map                              _orders; //order_id -> order map
    order_time_map                            _removed_orders; //unfilled orders go here to die

    //percentage fees
    double                                    _ask_fee;
    double                                    _bid_fee;

    //compare order
#if 0
    struct cmp_order
    {
      bool operator()(const order_map::value_type& left, 
		      const order_map::value_type& right) const
      {
	return left.first < right.first;
      }
    };
#endif

    int64                         _account_id;

    mutable mutex                 _mutex;

    order_ptr get_order(int64 order_id);
    void remove_order(int64 order_id);
    void remove_cancelled_orders();    //TODO: call this periodically from another thread
  };

  market::market(int64 oaid, int64 paid)
    : _order_asset_id(oaid), _payment_asset_id(paid),
      _ask_fee(0.1), _bid_fee(0.1), //TODO: set fees via system var
      _account_id(xch::get_unique_account_id())
  {
    init_account(_account_id); //account to collect fees with
  }
  
  int64 market::add_order(order_type ot, int64 account_id, 
			  double volume, double cost)
  {
    order_ptr optr(new order(ot, account_id, volume, cost));
    double price = optr->price();
    int64 order_id = optr->order_id();

    {
      unique_lock lock(_mutex);
      switch(ot)
	{
	case ASK: 
	  _ask_orderbook[price].insert(order_time_pair(optr->open_time(), optr));
	  _orders[order_id] = optr;
	  break;
	case BID: 
	  _bid_orderbook[price].insert(order_time_pair(optr->open_time(), optr));
	  _orders[order_id] = optr;
	  break;
	default: throw market_error("invalid order_type");
	}
    }
    resolve_orderbook();
    orderbook_changed();
    order_added(order_id);
    return order_id;
  }

  market::order_ptr market::get_order(int64 order_id)
  {
    using namespace boost;
    
    order_ptr o;
    {
      unique_lock lock(_mutex);
      if(_orders.find(order_id) == _orders.end())
	throw market_error(str(format("unknown order %1%")
			       % order_id));
      o = _orders[order_id];
      if(!o)
	throw market_error(str(format("invalid order %1%")
			       % order_id));
    }

    return o;
  }

  void market::remove_order(int64 order_id)
  {
    using namespace boost;
    {
      unique_lock lock(_mutex);
      if(_orders.find(order_id) == _orders.end())
        return; //nothing to do

      order_ptr optr = _orders[order_id];
      if(!optr)
        throw market_error(str(format("invalid order %1%")
			       % order_id));
      optr->close();
      order_type ot = optr->get_order_type();
      ptime cur_time = optr->close_time();
      double price = optr->price();
      switch(ot)
	{
	case ASK:
          {
            _removed_orders.insert(order_time_map::value_type(cur_time, optr));
            
            order_time_map& same_price_orders = _ask_orderbook[price];
            order_time_map::iterator erase_iter = same_price_orders.end();
            for(order_time_map::iterator i = same_price_orders.begin();
                i != same_price_orders.end();
                ++i)
              if(i->second == optr)
                {
                  erase_iter = i;
                  break;
                }
            same_price_orders.erase(erase_iter);
            
            _orders.erase(order_id);
          }
	  break;
	case BID:
          {
            _removed_orders.insert(order_time_map::value_type(cur_time, optr));
            
            order_time_map& same_price_orders = _bid_orderbook[price];
            order_time_map::iterator erase_iter = same_price_orders.end();
            for(order_time_map::iterator i = same_price_orders.begin();
                i != same_price_orders.end();
                ++i)
              if(i->second == optr)
                {
                  erase_iter = i;
                  break;
                }
            same_price_orders.erase(erase_iter);
            
            _orders.erase(order_id);
          }
	  break;
	default: 
	  throw market_error(str(format("invalid order_type in order %1%")
				 % order_id));
	}
    }
    orderbook_changed();
    order_removed(order_id);
  }

  void market::remove_cancelled_orders()
  {
    std::set<int64> cancelled_order_ids;

    //collect cancelled order ids
    {
      unique_lock lock(_mutex);
      BOOST_FOREACH(order_id_map::value_type& cur,
		    _orders)
	{
	  order_ptr optr = cur.second;
          if(optr && optr->closed())
	    cancelled_order_ids.insert(optr->order_id());
	}
    }
    
    //now remove each one
    BOOST_FOREACH(int64 order_id, cancelled_order_ids)
      remove_order(order_id);
  }

  void market::cancel_order(int64 order_id)
  {
    using namespace boost;
    {
      unique_lock lock(_mutex);
      if(_orders.find(order_id) == _orders.end())
	throw market_error(str(format("unknown order %1%")
			       % order_id));
      order_ptr optr = _orders[order_id];
      if(!optr)
	throw market_error(str(format("invalid order %1%")
			       % order_id));
      optr->close();      
    }
    orderbook_changed();
  }

  std::set<int64> market::get_open_order_ids()
  {
    std::set<int64> order_ids;
    {
      unique_lock lock(_mutex);
      BOOST_FOREACH(order_id_map::value_type& cur,
		    _orders)
        {
          order_ptr optr = cur.second;
          if(optr && !optr->closed())
            order_ids.insert(optr->order_id());
        }
    }
    return order_ids;
  }

  std::set<int64> market::get_closed_order_ids()
  {
    std::set<int64> order_ids;
    {
      unique_lock lock(_mutex);
      BOOST_FOREACH(order_id_map::value_type& cur,
		    _orders)
        {
          order_ptr optr = cur.second;
          if(optr && optr->closed())
            order_ids.insert(optr->order_id());
        }
    }
    return order_ids;
  }

  std::set<int64> market::get_order_ids()
  {
    std::set<int64> order_ids;
    {
      unique_lock lock(_mutex);
      BOOST_FOREACH(order_id_map::value_type& cur,
		    _orders)
        if(cur.second)
          order_ids.insert(cur.second->order_id());
    }
    return order_ids;
  }

  market::order_type market::get_order_type(int64 order_id)
  {
    order_ptr o = get_order(order_id);
    return o ? o->get_order_type() : INVALID_ORDER;
  }

  int64 market::get_order_account_id(int64 order_id)
  {
    order_ptr o = get_order(order_id);
    return o ? o->account_id() : -1;
  }

  ptime market::get_order_open_time(int64 order_id)
  {
    order_ptr o = get_order(order_id);
    return o ? o->open_time() 
      : boost::posix_time::min_date_time;
  }

  ptime market::get_order_close_time(int64 order_id)
  {
    order_ptr o = get_order(order_id);
    return o ? o->close_time() 
      : boost::posix_time::min_date_time;
  }

  double market::get_order_volume(int64 order_id)
  {
    order_ptr o = get_order(order_id);
    return o ? o->volume()
      : -std::numeric_limits<double>::max();
  }

  double market::get_order_cost(int64 order_id)
  {
    order_ptr o = get_order(order_id);
    return o ? o->cost()
      : -std::numeric_limits<double>::max();
  }

  double market::get_order_available_volume(int64 order_id)
  {
    order_ptr o = get_order(order_id);
    return o ? o->available_volume()
      : -std::numeric_limits<double>::max();
  }

  double market::get_order_price(int64 order_id)
  {
    order_ptr o = get_order(order_id);
    return o ? o->price()
      : -std::numeric_limits<double>::max();
  }

  bool market::get_order_closed(int64 order_id)
  {
    order_ptr o = get_order(order_id);
    return o ? o->closed()
      : false;
  }

  int64 market::order_asset_id() const
  {
    shared_lock lock(_mutex);
    return _order_asset_id;
  }

  void market::order_asset_id(int64 oaid)
  {
    reset_orderbook();
    {
      unique_lock lock(_mutex);
      _order_asset_id = oaid;
    }
  }

  int64 market::payment_asset_id() const
  {
    shared_lock lock(_mutex);
    return _payment_asset_id;
  }

  void market::payment_asset_id(int64 paid)
  {
    reset_orderbook();
    {
      unique_lock lock(_mutex);
      _payment_asset_id = paid;
    }
  }

  double market::ask_fee() const
  {
    shared_lock lock(_mutex);
    return _ask_fee;
  }

  void market::ask_fee(double fee)
  {
    unique_lock lock(_mutex);
    _ask_fee = fee;
  }

  double market::bid_fee() const
  {
    shared_lock lock(_mutex);
    return _bid_fee;
  }

  void market::bid_fee(double fee)
  {
    unique_lock lock(_mutex);
    _bid_fee = fee;
  }

  void market::resolve_orderbook()
  {
    double min_ask_price = 0.0;
    double max_bid_price = 0.0;
    order_ptr min_ask_ptr;
    order_ptr max_bid_ptr;
 
    do
      {
	bool all_closed = false;

	orderbook_map::iterator min_ask_iter;

        {
          unique_lock lock(_mutex);
          min_ask_iter = _ask_orderbook.begin();
          
          //nothing to do if no ask orders
          if(min_ask_iter == _ask_orderbook.end())
            break;
        }

        //TODO: FINISH ME!!!!

	//find first non closed ask order
	do
	  {
            min_ask_price = min_ask_iter->first;
            order_time_map& otr = min_ask_iter->second;
            order_time_map::iterator otr_first = otr.begin();
	    min_ask_ptr = otr_first != otr.end() ?
              otr_first->second : order_ptr();

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
	  
	orderbook_map::reverse_iterator max_bid_iter =
	  _bid_orderbook.rbegin();
	  
	//nothing to do if no bid orders
	if(max_bid_iter == _bid_orderbook.rend())
	  break;

	//find first non closed bid order
	do
	  {
	    max_bid_price = max_bid_iter->first;
            order_time_map& otr = max_bid_iter->second;
            order_time_map::iterator otr_first = otr.begin();
	    max_bid_ptr   = otr_first != otr.end() ? 
              otr_first->second : order_ptr();

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
	  throw market_error("null ask order in orderbook");
	if(!max_bid_ptr)
	  throw market_error("null bid order in orderbook");

	//nothing to do if there is a spread
	if(min_ask_price > max_bid_price)
	  break;

	double ask_available = min_ask_ptr->available_volume();
	double bid_available = max_bid_ptr->available_volume();

	double new_bid_available =
	  std::min(bid_available + ask_available, max_bid_ptr->volume());
	double dv = new_bid_available - bid_available; //amount consumed by the bid
	double new_ask_available = 
	  std::max(ask_available - dv, 0.0);
	  
	min_ask_ptr->available_volume(new_ask_available);
	max_bid_ptr->available_volume(new_bid_available);

	//close the ask or bid order if they are filled up to a certain epsilon
	const double epsilon = 0.001; //TODO: use system var for epsilon
	if(new_ask_available < epsilon)
	  min_ask_ptr->close(true);
	if(max_bid_ptr->volume() - new_bid_available < epsilon)
	  max_bid_ptr->close(true);
	  
	order_ptr oldest_ptr = 
	  min_ask_ptr->open_time() < max_bid_ptr->open_time() ?
	  min_ask_ptr : max_bid_ptr;

	double exec_price = oldest_ptr->price();
	double dv_cost = dv * exec_price;

	double dv_fee = dv * (_bid_fee / 100.0);
	double dv_cost_fee = dv_cost * (_ask_fee / 100.0);

	//subtract the fee from both sides
	dv -= dv_fee;
	dv_cost -= dv_cost_fee;

	//finally execute the exchange
	//TODO: add this to a queue and process later (maybe in another thread)

	//fees
	exec_transaction(_account_id,
			 max_bid_ptr->account_id(),
			 _payment_asset_id,
			 dv_cost_fee);
	exec_transaction(_account_id,
			 min_ask_ptr->account_id(),
			 _order_asset_id,
			 dv_fee);
	//actual transaction
	exec_transaction(min_ask_ptr->account_id(),
			 max_bid_ptr->account_id(),
			 _payment_asset_id,
			 dv_cost);
	exec_transaction(max_bid_ptr->account_id(),
			 min_ask_ptr->account_id(),
			 _order_asset_id,
			 dv);

	//send a signal about the trade
	trade_executed(exec_price, dv);

	//TODO: market history transaction log
      }
    while(min_ask_price <= max_bid_price);
  }

  int64 market::get_next_order_id()
  {
    static int64 id = 13371337;
    static mutex local_mutex;
    unique_lock lock(local_mutex);
    return id++;
  }

  void market::reset_orderbook()
  {
    std::set<int64> order_ids =
      get_order_ids();
    BOOST_FOREACH(int64 order_id, order_ids)
      remove_order(order_id);
    {
      unique_lock lock(_mutex);
      _removed_orders.clear();
    }
  }

  market::order::order()
    : _order_type(INVALID_ORDER),
      _order_id(get_next_order_id()), _account_id(-1),
      _open_time(boost::posix_time::min_date_time),
      _close_time(boost::posix_time::min_date_time),
      _volume(-std::numeric_limits<double>::max()), 
      _cost(-std::numeric_limits<double>::max()), 
      _available_volume(0.0) {}

  market::order::order(const order& rhs)
    : _order_type(rhs._order_type), 
      _order_id(rhs._order_id), _account_id(rhs._account_id),
      _open_time(rhs._open_time), _close_time(rhs._close_time),
      _volume(rhs._volume), _cost(rhs._cost), 
      _available_volume(rhs._available_volume) {}

  market::order::order(order_type ot, int64 acct_id, 
		       double volume, double cost)
    : _order_type(ot), _order_id(get_next_order_id()),
      _account_id(acct_id),
      _open_time(boost::posix_time::microsec_clock::universal_time()),
      _close_time(boost::posix_time::min_date_time),
      _volume(volume), _cost(cost),
      _available_volume(volume) {}

  market::order& market::order::operator=(const order& rhs)
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
  bool market::order::closed()
  {
    shared_lock lock(_mutex);
    return _close_time != boost::posix_time::min_date_time;
  }

  void market::order::close(bool filled)
  {
    if(closed()) return;

    {
      unique_lock lock(_mutex);

      //if filled flag set, mark order as fully filled
      if(filled)
	{
	  if(_order_type == ASK)
	    _available_volume = 0.0;
	  else if(_order_type == BID)
	    _available_volume = _volume;
	}
      _close_time =
	boost::posix_time::microsec_clock::universal_time();
    }
  }

  double market::order::price()
  {
    shared_lock lock(_mutex);    
    return _cost / _volume;
  }

  market::order_type market::order::get_order_type() const
  {
    shared_lock lock(_mutex);
    return _order_type;
  }

  void market::order::set_order_type(market::order_type ot)
  {
    unique_lock lock(_mutex);
    _order_type = ot;
  }

  int64 market::order::order_id() const
  {
    shared_lock lock(_mutex);
    return _order_id;
  }

  void market::order::order_id(int64 id)
  {
    unique_lock lock(_mutex);
    _order_id = id;
  }

  int64 market::order::account_id() const
  {
    shared_lock lock(_mutex);
    return _account_id;
  }

  void market::order::account_id(int64 id)
  {
    unique_lock lock(_mutex);
    _account_id = id;
  }

  ptime market::order::open_time() const
  {
    shared_lock lock(_mutex);
    return _open_time;
  }

  void market::order::open_time(ptime ot)
  {
    unique_lock lock(_mutex);
    _open_time = ot;
  }

  ptime market::order::close_time() const
  {
    shared_lock lock(_mutex);
    return _close_time;
  }

  void market::order::close_time(ptime ot)
  {
    unique_lock lock(_mutex);
    _close_time = ot;
  }

  double market::order::volume() const
  {
    shared_lock lock(_mutex);
    return _volume;
  }

  void market::order::volume(double v)
  {
    unique_lock lock(_mutex);
    _volume = v;
  }

  double market::order::cost() const
  {
    shared_lock lock(_mutex);
    return _cost;
  }

  void market::order::cost(double c)
  {
    unique_lock lock(_mutex);
    _cost = c;
  }  

  double market::order::available_volume() const
  {
    shared_lock lock(_mutex);
    return _available_volume;
  }

  void market::order::available_volume(double av)
  {
    unique_lock lock(_mutex);
    _available_volume = av;
  }  
  
  typedef boost::shared_ptr<market>   market_ptr;
  std::map<std::string, market_ptr>   _markets;
  mutex                               _markets_mutex;

  //functions:
  //resolve orderbook - after every change to orderbook, check if any buys match sells
  //if so, execute the transaction changing the respective balances of the market participants
  //involved in the order
}

//commands related code
namespace
{
  //TODO:
  //need to figure out I/O situation and how it's related to the simple variable system we have going here...

  //commands:
  //set_asset_id - create asset with id. if no id argument, get one from system if none yet set, else do nothing
  //get_asset_id - returns (prints) asset id of specified asset
  //init_account - create a new account and resets all balances to zero. if no account id is specified, get one from the system
  //buy - post a bid limit order
  //sell - post an ask limit order
  //

  void set_asset_id(const xch::argument_vector& av)
  {
    
  }

}

namespace
{
  void do_help()
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::ex("help");
  }

  void interactive()
  {
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::ex("cmd");
  }
}

void print_accounts()
{
  std::vector<mkt::int64> account_ids = xch::get_account_ids();
  std::vector<std::string> asset_names = xch::get_asset_names();
  BOOST_FOREACH(mkt::int64 id, account_ids)
    {
      std::cout << "id: " << id << std::endl;
      BOOST_FOREACH(std::string& asset_name, asset_names)
	std::cout << "\tasset: " << asset_name << " " << "balance: " 
		  << xch::balance(id, xch::get_asset_id(asset_name)) << std::endl;
    }
}

int main(int argc, char **argv)
{
  using namespace std;
  
  mkt::wait_for_threads w;
  mkt::app a(argc, argv);

  //debug
  xch::set_asset_id("XBT", 31337);
  xch::set_asset_id("XDG", 31338);
  xch::set_asset_id("XBC", 31339);

  for(int i = 0; i < 5; i++)
    xch::init_account(xch::get_unique_account_id());

#if 0
  //init accounts with some random balance
  std::vector<mkt::int64> account_ids = xch::get_account_ids();
  BOOST_FOREACH(mkt::int64 id, account_ids)
    xch::exec_transaction(id, -1, mkt::get_asset_id("XBT"), (rand() % 666)/333.0);
  print_accounts();

  xch::exec_transaction(account_ids[0], account_ids[1], 
				  mkt::get_asset_id("XBT"), 
				  xch::balance(account_ids[1], mkt::get_asset_id("XBT")));
  print_accounts();
#endif
  std::vector<mkt::int64> account_ids = xch::get_account_ids();

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
