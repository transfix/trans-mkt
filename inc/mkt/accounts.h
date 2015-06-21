#ifndef __MKT_ACCOUNTS_H__
#define __MKT_ACCOUNTS_H__

#include <mkt/config.h>
#include <mkt/types.h>
#include <mkt/exceptions.h>
#include <mkt/assets.h>

#include <map>
#include <vector>
#include <string>

/*
  Account API
 */
namespace mkt
{
  typedef mkt::int64 account_id_t;
  typedef mkt::int64 transaction_id_t;

  extern map_change_signal assets_changed;
  void init_accounts(); //call to initialize this module
  void exec_transaction(account_id_t to_account_id,
                        account_id_t from_account_id,
                        asset_id_t asset_id,
                        double amount);
  void init_account(account_id_t account_id);
  double balance(account_id_t account_id, asset_id_t asset_id);
  bool has_account(account_id_t account_id);
  std::vector<account_id_t> get_account_ids();
  account_id_t get_unique_account_id();

  class transaction
  {
  public:
    transaction(ptime cur_time = boost::posix_time::min_date_time, 
                account_id_t to_account_id = -1,
                account_id_t from_account_id = -1, 
                asset_id_t asset_id = -1,
                double amount = 0.0);
    transaction(const transaction& rhs);
    transaction& operator=(const transaction& rhs);

    ptime cur_time() const { return _cur_time; }
    account_id_t to_account_id() const { return _to_account_id; }
    account_id_t from_account_id() const { return _from_account_id; }
    asset_id_t asset_id() const { return _asset_id; }
    double amount() const { return _amount; }
    transaction_id_t id() const { return _transaction_id; }

    static int64 get_next_transaction_id();

  private:
    ptime              _cur_time;
    account_id_t       _to_account_id;
    account_id_t       _from_account_id;
    asset_id_t         _asset_id;
    double             _amount;
    transaction_id_t   _transaction_id;
  };

  struct transaction_query
  {
    transaction_query(ptime b = boost::posix_time::min_date_time,
                      ptime e = boost::posix_time::max_date_time,
                      account_id_t tid = -1,
                      account_id_t fid = -1,
                      asset_id_t aid = -1,
                      double min_a = -std::numeric_limits<double>::max(),
                      double max_a = std::numeric_limits<double>::max());
    transaction_query(const transaction_query& rhs);
    transaction_query& operator=(const transaction_query& rhs);
    
    transaction_query& begin(ptime b) { _begin = b; return *this; }
    transaction_query& end(ptime e) { _end = e; return *this; }
    transaction_query& to_account_id(account_id_t tid) { _to_account_id = tid; return *this; }
    transaction_query& from_account_id(account_id_t fid) { _from_account_id; return *this; }
    transaction_query& asset_id(asset_id_t aid) { _asset_id = aid; return *this; }
    transaction_query& min_amount(double min_a) { _min_amount = min_a; return *this; }
    transaction_query& max_amount(double max_a) { _max_amount = max_a; return *this; }

    std::vector<transaction> get_transactions() const;

  private:
    ptime        _begin;
    ptime        _end;
    account_id_t _to_account_id;
    account_id_t _from_account_id;
    asset_id_t   _asset_id;
    double       _min_amount;
    double       _max_amount;      
    friend class accounts;
  };

  //returns a list of transactions from the transaction history based on the
  //parameters.  With no parameters it will return all transactions.
  std::vector<transaction> get_transactions(const transaction_query& params = 
                                            transaction_query());
}

#endif
