#include <mkt/app.h>
#include <mkt/exceptions.h>

#ifdef MKT_INTERACTIVE
#ifdef __WINDOWS__
#include <editline_win/readline.h>
#else
  #include <editline/readline.h>
#endif
#endif

#include <boost/current_function.hpp>
#include <boost/foreach.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/cstdint.hpp>

#include <iostream>
#include <cstdlib>

#include <vector>
#include <map>

namespace
{
  typedef boost::posix_time::ptime ptime;

#if 0
  class order
  {    
  public:
    enum order_type
      {
	INVALID_ORDER, ASK, BID
      };

    order() : 
      _order_type(INVALID_ORDER),
      _order_id(-1), _account_id(-1),
      _order_asset_type(-1), _payment_asset_type(-1),
      _open_time(boost::posix_time::min_date_time),
      _close_time(boost::posix_time::min_date_time),
      _volume(0.0), _cost(0.0), 
      _amount_filled(0.0) {}

    order(const order& rhs)
      : _order_type(rhs._order_type), 
	_order_id(rhs._order_id), _account_id(rhs._account_id),
	_order_asset_type(rhs._order_asset_type), 
	_payment_asset_type(rhs._payment_asset_type),
	_open_time(rhs._open_time), _close_time(rhs._close_time),
	_volume(rhs._volume), _cost(rhs._cost), 
	_amount_filled(0.0) {}

    order& operator=(const order& rhs)
    {
      if(this == &rhs) return *this;
      _order_type = rhs._order_type;
      _order_id = rhs._order_id;
      _account_id = rhs._account_id;
      _order_asset_id = rhs._order_asset_id;
      _payment_asset_id = rhs._payment_asset_id;
      _open_time = rhs._open_time;
      _close_time = rhs._close_time;
      _volume = rhs._volume;
      _cost = rhs._cost;
      _amount_filled = rhs._amount_filled;
      return *this;
    }

    order_type order_type() const 
    {
      boost::mutex::scoped_shared_lock lock(_mutex);
      return _order_type; 
    }
    void order_type(order_type ot)
    {
      boost::mutex::scoped_lock lock(_mutex);
      _order_type = ot;
    }

    int64 order_id() const
    {
      boost::mutex::scoped_shared_lock lock(_mutex);
      return _order_id;
    }
    void order_id(int64 id)
    {
      boost::mutex::scoped_lock lock(_mutex);
      _order_id = id;
    }

    int64 order_asset_id() const
    {
      boost::mutex::scoped_shared_lock lock(_mutex);
      return _order_asset_id;
    }
    void order_asset_id(int64 id)
    {
      boost::mutex::scoped_lock lock(_mutex);
      _order_asset_id = id;
    }

    int64 payment_asset_id() const
    {
      boost::mutex::scoped_shared_lock lock(_mutex);
      return _payment_asset_id;
    }
    void payment_asset_id(int64 id)
    {
      boost::mutex::scoped_lock lock(_mutex);
      _payment_asset_id = id;
    }

    ptime open_time() const
    {
      boost::mutex::scoped_shared_lock lock(_mutex);
      return _open_time;
    }
    void open_time(ptime ot)
    {
      boost::mutex::scoped_lock lock(_mutex);
      _open_time = ot;
    }

    ptime close_time() const
    {
      boost::mutex::scoped_shared_lock lock(_mutex);
      return _close_time;
    }
    void close_time(ptime ot)
    {
      boost::mutex::scoped_lock lock(_mutex);
      _close_time = ot;
    }

    double volume() const
    {
      boost::mutex::scoped_shared_lock lock(_mutex);
      return _volume;
    }
    void volume(double v)
    {
      boost::mutex::scoped_lock lock(_mutex);
      _volume = v;
    }

    double cost() const
    {
      boost::mutex::scoped_shared_lock lock(_mutex);
      return _cost;
    }
    void cost(double v)
    {
      boost::mutex::scoped_lock lock(_mutex);
      _cost = v;
    }

    double price() const 
    { 
      boost::mutex::scoped_shared_lock lock(_mutex);
      return _cost/_volume; 
    }

    double amount_filled() const
    {
      boost::mutex::scoped_shared_lock lock(_mutex);
      return _amount_filled;
    }
    void amount_filled(double v)
    {
      boost::mutex::scoped_lock lock(_mutex);
      _amount_filled = v;
    }

  private:
    order_type _orderType;

    int64 _order_id;
    int64 _account_id;

    int64 _order_asset_id;
    int64 _payment_asset_id;

    ptime _open_time;
    ptime _close_time;

    //TODO: use fixed digit type
    double _volume; //order amount
    double _cost;   //amount to spend of payment asset to fill order

    double _amount_filled; //amount filled if closed

    boost::mutex _mutex;
  };
  
  class market
  {
  public:
    market(int64 oaid = -1, int64 paid = -1)
      : _order_asset_id(oaid), _payment_asset_id(paid)
    {}

    

    boost::signals2::signal<void ()>  orderbook_changed;

  private:
    void resolve_orderbook();

    int64 _order_asset_id;
    int64 _payment_asset_id;

    std::map<double, order>           _orderbook; //map key is price -> _cost / _volume
    boost::mutex                      _mutex;
  };

  std::map<std::string, market>       _markets;
  boost::mutex                        _markets_mutex;
#endif

  typedef std::map<std::string, int64> asset_map;
  asset_map                           _assets; //map of asset names to asset ids
  boost::mutex                        _asset_map_mutex;

  asset_map get_assets()
  {
    boost::mutex::scoped_shared_lock lock(_asset_map_mutex);
    return _assets;
  }

  void set_assets(asset_map am)
  {
    boost::mutex::scoped_lock lock(_asset_map_mutex);
    _assets = am;
  }

  void set_asset(std::string asset_name, int64 asset_id)
  {
    boost::mutex::scoped_lock lock(_asset_map_mutex);
    _assets[asset_name] = asset_id;
  }

  MKT_DEF_EXCEPTION(account_error);

  class transaction
  {
  public:
    transaction(ptime cur_time, int64 to_account_id,
		int64 from_account_id, int64 asset_id,
		double amount)
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
				 double amount)
    {
      {
	using namespace boost;
	mutex::scoped_lock lock(_account_map_mutex);

	if(from_account_id >= 0 &&
	   _account_map.find(from_account_id) == _account_map.end())
	  throw account_error(str(format("invalid from_account_id %1%")
				  % to_account_id));
	if(_account_map.find(to_account_id) == _account_map.end())
	  throw account_error(str(format("invalid to_account_id %1%")
				  % to_account_id));

	//a from_account_id less than zero creates money from nothing
	double from_account_balance = 
	  from_account_id > 0 ? 
	  _account_map[from_account_id][asset_id] : amount;
	double to_account_balance = _account_map[to_account_id][asset_id];

	from_account_balance -= amount;
	to_account_balance += amount;

	//do not support negative balances
	if(from_account_balance < 0.0)
	  throw account_error(str(format("not enough funds in account %1%") 
				  % from_account_id));

	//everything is ok, lets set the new balances now
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
    static void init_account(int64 account_id)
    {
      using namespace boost;
      if(account_id < 0) 
	throw account_error(str(format("invalid account_id %1%")
				% account_id));
      asset_map assets = get_assets();
      {
	mutex::scoped_lock lock(_account_map_mutex);
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

    static double balance(int64 account_id, int64 asset_id)
    {
      using namespace boost;
      mutex::scoped_shared_lock lock(_account_map_mutex);
      if(_account_map.find(account_id) == _account_map.end())
	throw account_error(str(format("invalid account_id %1%")
				% account_id));
      return _account_map[account_id][asset_id];
    }

    static bool has_account(int64 account_id)
    {
      boost::mutex::scoped_shared_lock lock(_account_map_mutex);
      return _account_map.find(account_id) != _account_map.end();
    }

    static std::vector<int64> get_account_ids()
    {
      boost::mutex::scoped_shared_lock lock(_account_map_mutex);
      std::vector<int64> ids;
      BOOST_FOREACH(const account_map::value_type& cur, _account_map)
	ids.push_back(cur.first);
      return ids;
    }

    static boost::signals2::signal<void (int64)>  account_changed;
    
  private:
    static account_map                       _account_map;
    static std::map<ptime, transaction>      _transaction_history;
    static boost::mutex                      _account_map_mutex;

    accounts();
    accounts(const accounts&);
  };

  accounts::account_map accounts::_account_map;
  std::map<ptime, transaction> accounts::_transaction_history;
  boost::mutex accounts::_account_map_mutex;

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

int main(int argc, char **argv)
{
  using namespace std;
  
  mkt::wait_for_threads w;
  mkt::argv(argc, argv);

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
