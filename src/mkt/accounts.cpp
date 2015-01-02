#include <mkt/commands.h>
#include <mkt/echo.h>
#include <mkt/threads.h>
#include <mkt/assets.h>
#include <mkt/accounts.h>

#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>

#include <cstdlib>
#include <ctime>

/*
 * Accounts API module implementation
 */

//This module's exceptions
namespace mkt
{
  MKT_DEF_EXCEPTION(accounts_error);
}

//This module's static data
namespace
{
  typedef mkt::asset_id_t                          asset_id_t;
  typedef mkt::account_id_t                        account_id_t;
  typedef mkt::transaction_id_t                    transaction_id_t;
  typedef mkt::ptime                               ptime;
  typedef mkt::transaction                         transaction;
  typedef std::map<asset_id_t, double>             balances;           //map key is asset ID
  typedef std::map<account_id_t, balances>         account_map;        //map key is account ID
  typedef std::multimap<ptime, transaction>        transaction_map;    //map key is transaction time
  typedef std::map<transaction_id_t, transaction>  transaction_id_map; //map key is transaction id

  struct accounts_data
  {
    account_map                 _account_map;
    transaction_map             _transaction_history;
    transaction_id_map          _transactions;
    mkt::mutex                  _account_map_mutex;
  };
  accounts_data                *_accounts_data = 0;
  bool                          _accounts_atexit = false;

  void _accounts_cleanup()
  {
    _accounts_atexit = true;
    delete _accounts_data;
    _accounts_data = 0;
  }

  accounts_data* _get_accounts_data()
  {
    if(_accounts_atexit)
      throw mkt::accounts_error("Already at program exit!");

    if(!_accounts_data)
      {
        _accounts_data = new accounts_data;
        std::atexit(_accounts_cleanup);
      }

    if(!_accounts_data)
      throw mkt::accounts_error("Missing static variable data!");
    return _accounts_data;
  }

  account_map& account_map_ref()
  {
    return _get_accounts_data()->_account_map;
  }

  transaction_map& transaction_history_ref()
  {
    return _get_accounts_data()->_transaction_history;
  }

  transaction_id_map& transactions_ref()
  {
    return _get_accounts_data()->_transactions;
  }

  mkt::mutex& account_map_mutex_ref()
  {
    return _get_accounts_data()->_account_map_mutex;
  }
}

//Account related commands
namespace
{
  void init_account(const mkt::argument_vector& args)
  {
    using namespace boost;
    mkt::thread_info ti(BOOST_CURRENT_FUNCTION);
    mkt::argument_vector local_args = args;
    local_args.erase(local_args.begin()); //remove the command string
    mkt::account_id_t account_id = -1;
    if(local_args.empty())
      {
        account_id = mkt::get_unique_account_id();
      }
    else
      {
        try
          {
            account_id = boost::lexical_cast<mkt::account_id_t>(local_args[0]);
          }
        catch(boost::bad_lexical_cast&)
          {
            throw mkt::accounts_error(str(format("Invalid account id %1%")
                                          % local_args[0]));
          }
      }

    mkt::init_account(account_id);
    mkt::out().stream() << "Account " 
                        << account_id 
                        << " initialized." << std::endl;
  }

  class init_commands
  {
  public:
    init_commands()
    {
      using namespace std;
      using namespace mkt;

      add_command("init_account", init_account, 
                  "init_account [<account id>]\nInitializes a new account and initializes "
                  "balances of known assets to 0.0. If no account_id is specified, the system will generate one.");
    }
  } init_commands_static_init;
}


//Accounts API implementation
namespace mkt
{
  boost::signals2::signal<void (account_id_t)>  account_changed;

  //no-op to force static init of this translation unit
  void init_accounts() {}
  
  void exec_transaction(account_id_t to_account_id,
                        account_id_t from_account_id,
                        asset_id_t   asset_id,
                        double       amount)
  {
    {
      using namespace boost;
      unique_lock lock(account_map_mutex_ref());

      if(from_account_id >= 0 &&
	 account_map_ref().find(from_account_id) == account_map_ref().end())
	throw accounts_error(str(format("invalid from_account_id %1%")
                                 % to_account_id));
      if(account_map_ref().find(to_account_id) == account_map_ref().end())
	throw accounts_error(str(format("invalid to_account_id %1%")
                                 % to_account_id));
      if(from_account_id == to_account_id)
	throw accounts_error("to_account_id == from_account_id");

      //a from_account_id less than zero creates money from nothing
      double from_account_balance = 
	from_account_id >= 0 ? 
	account_map_ref()[from_account_id][asset_id] : amount;
      double to_account_balance = account_map_ref()[to_account_id][asset_id];

      from_account_balance -= amount;
      to_account_balance += amount;

      //do not support negative balances
      if(from_account_balance < 0.0)
	throw accounts_error(str(format("not enough funds in account %1%") 
                                 % from_account_id));

      //everything is ok, lets set the new balances now
      if(from_account_id >= 0)
	account_map_ref()[from_account_id][asset_id] = from_account_balance;
      account_map_ref()[to_account_id][asset_id] = to_account_balance;

      ptime cur_time = boost::posix_time::microsec_clock::universal_time();
      transaction t(cur_time, to_account_id, from_account_id, asset_id, amount);
      transaction_history_ref().insert(transaction_map::value_type(cur_time, t));
      transactions_ref()[t.id()] = t;
    }

    if(from_account_id >= 0) account_changed(from_account_id);
    account_changed(to_account_id);
  }

  //initializes an account with zero balances for all assets known by the system
  void init_account(account_id_t account_id)
  {
    using namespace boost;
    if(account_id < 0) 
      throw accounts_error(str(format("invalid account_id %1%")
                               % account_id));

    if(has_account(account_id))
      throw accounts_error(str(format("Account %1% already exists.")
                               % account_id));

    asset_map assets = get_assets();
    if(assets.empty())
      throw accounts_error("No assets have been registered.");
    
    {
      unique_lock lock(account_map_mutex_ref());
      ptime cur_time = boost::posix_time::microsec_clock::universal_time();
      BOOST_FOREACH(asset_map::value_type& cur, assets)
	{
	  asset_id_t asset_id = cur.second;
	  account_map_ref()[account_id][asset_id] = 0.0;
	  transaction t(cur_time, account_id, -1, asset_id, 0.0);
	  transaction_history_ref().insert(transaction_map::value_type(cur_time, t));
	  transactions_ref()[t.id()] = t;
	}
    }
    
    account_changed(account_id);
  }

  //returns balance for account/asset pair.
  double balance(account_id_t account_id, asset_id_t asset_id)
  {
    using namespace boost;
    shared_lock lock(account_map_mutex_ref());
    if(account_map_ref().find(account_id) == account_map_ref().end())
      throw accounts_error(str(format("invalid account_id %1%")
			      % account_id));
    return account_map_ref()[account_id][asset_id];
  }

  bool has_account(account_id_t account_id)
  {
    shared_lock lock(account_map_mutex_ref());
    return account_map_ref().find(account_id) != account_map_ref().end();
  }

  std::vector<account_id_t> get_account_ids()
  {
    shared_lock lock(account_map_mutex_ref());
    std::vector<account_id_t> ids;
    BOOST_FOREACH(const account_map::value_type& cur, account_map_ref())
      ids.push_back(cur.first);
    return ids;
  }

  account_id_t get_unique_account_id()
  {
    using namespace std;
    account_id_t account_id = -1;
    srand(time(0));
    do
      {
	account_id = rand();
      }
    while(has_account(account_id));
    return account_id;
  }

  transaction::transaction(ptime cur_time,
                           account_id_t to_account_id,
                           account_id_t from_account_id,
                           asset_id_t asset_id,
                           double amount)
    : _cur_time(cur_time), _to_account_id(to_account_id),
      _from_account_id(from_account_id), _asset_id(asset_id),
      _amount(amount), 
      _transaction_id(get_next_transaction_id()) {}

  transaction::transaction(const transaction& rhs)
    : _cur_time(rhs._cur_time), _to_account_id(rhs._to_account_id),
      _from_account_id(rhs._from_account_id), _asset_id(rhs._asset_id),
      _amount(rhs._amount), 
      _transaction_id(rhs._transaction_id) {}

  transaction& transaction::operator=(const transaction& rhs)
  {
    if(this == &rhs) return *this;
    _cur_time = rhs._cur_time;
    _to_account_id = rhs._to_account_id;
    _from_account_id = rhs._from_account_id;
    _asset_id = rhs._asset_id;
    _amount = rhs._amount;
    _transaction_id = rhs._transaction_id;
    return *this;
  }

  transaction_id_t transaction::get_next_transaction_id()
  {
    static transaction_id_t id = 0;
    static mutex local_mutex;
    unique_lock lock(local_mutex);
    return id++;
  }

  transaction_query::transaction_query(ptime b,
                                       ptime e,
                                       account_id_t tid,
                                       account_id_t fid,
                                       asset_id_t aid,
                                       double min_a,
                                       double max_a)
    : _begin(b), _end(e), 
      _to_account_id(tid), _from_account_id(fid),
      _asset_id(aid), _min_amount(min_a), _max_amount(max_a) {}

  transaction_query::transaction_query(const transaction_query& rhs) 
    : _begin(rhs._begin), _end(rhs._end),
      _to_account_id(rhs._to_account_id), _from_account_id(rhs._from_account_id),
      _asset_id(rhs._asset_id), _min_amount(rhs._min_amount), _max_amount(rhs._max_amount) {}

  transaction_query& transaction_query::operator=(const transaction_query& rhs)
  {
    if(this == &rhs) return *this;
    _begin = rhs._begin;
    _end = rhs._end;
    _to_account_id = rhs._to_account_id;
    _from_account_id = rhs._from_account_id;
    _asset_id = rhs._asset_id;
    _min_amount = rhs._min_amount;
    _max_amount = rhs._max_amount;
    return *this;
  }

  std::vector<transaction> transaction_query::get_transactions() const
  {
    shared_lock lock(account_map_mutex_ref());
    
    std::vector<transaction> transactions;
    transaction_map::iterator earliest_iter =
      transaction_history_ref().lower_bound(_begin);
    transaction_map::iterator latest_iter =
      transaction_history_ref().upper_bound(_end);
    for(transaction_map::iterator i = earliest_iter;
	i != latest_iter;
	++i)
      {
	transaction& t = i->second;
	if(_to_account_id != -1 &&
	   _to_account_id != t.to_account_id())
	  continue;
	if(_from_account_id != -1 &&
	   _from_account_id != t.from_account_id())
	  continue;
	if(_asset_id != -1 &&
	   _asset_id != t.asset_id())
	  continue;
	if(_min_amount > t.amount())
	  continue;
	if(_max_amount < t.amount())
	  continue;
	transactions.push_back(t);
      }
      
    return transactions;
  }

  std::vector<transaction> get_transactions(const transaction_query& params)
  {
    return params.get_transactions();
  }
}
