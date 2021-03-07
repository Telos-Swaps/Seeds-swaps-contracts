#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>

#include <math.h>
#include <string>

using namespace eosio;
using namespace std;

#define MONTH_HISTORY_INTERVALS 3600
#define DAY_HISTORY_INTERVALS 600

class [[eosio::contract]] swapsdata : public contract {
public:
    using contract::contract;

    /**
     * a record for each of swap pair
     */
    struct swap_record {
        asset    quantity;
        double   price;
        asset    liquidity_depth;
        double   smart_price;
    };

    /**
     * global market data record
     */
    struct [[eosio::table("tradedata")]] trade_data {
        name                       converter;
        time_point_sec             timestamp;
        map<symbol_code, asset>    volume_24h;
        map<symbol_code, asset>    volume_cumulative;
        map<symbol_code, double>   price;
        map<symbol_code, double>   price_change_24h;
        map<symbol_code, asset>    liquidity_depth;
        map<symbol_code, double>   smart_price;
        map<symbol_code, double>   smart_price_change_30d;

        uint64_t primary_key() const { return converter.value; }
    };
    typedef eosio::multi_index< "tradedata"_n, trade_data > trade_data_table;

    /**
     * 24 hour buffer of;
     * - cumulative volume
     * - spot price
     */
    struct [[eosio::table("daybuffer")]] day_buffer_row {
        time_point_sec             timestamp;
        map<symbol_code, asset>    volume_cumulative;
        map<symbol_code, double>   base_price;
        map<symbol_code, asset>    volume;
        map<symbol_code, double>   open_price;
        map<symbol_code, double>   high_price;
        map<symbol_code, double>   low_price;
        map<symbol_code, double>   close_price;

        uint64_t primary_key() const { return timestamp.sec_since_epoch(); }
    };
    typedef eosio::multi_index< "daybuffer"_n, day_buffer_row > day_buffer_table;

    /**
     *
     */
    struct [[eosio::table("monthbuffer")]] month_buffer_row {
        time_point_sec             timestamp;
        map<symbol_code, double>   open_smart_price;

        uint64_t primary_key() const { return timestamp.sec_since_epoch(); }
    };
    typedef eosio::multi_index< "monthbuffer"_n, month_buffer_row > month_buffer_table;

    /**
     *
     * @param converter
     * @param swap_data
     */
    [[eosio::action]]
    void log(name converter, vector<swap_record> swap_data);

    /**
     *
     * @param converter
     */
    [[eosio::action]]
    void reset(name converter);

private:
    void update_day_buffer( name converter, day_buffer_row last_state, vector<swap_record> swap_data );
    void update_month_buffer( name converter, vector<swap_record> swap_data );
    day_buffer_row get_last_state( name converter, vector<swap_record> swap_data, day_buffer_row base_data );
    void update_trade_data( name converter, vector<swap_record> swap_data, day_buffer_row base_data, month_buffer_row smart_base_data );
    month_buffer_row get_smart_base_state( name converter, vector<swap_record> swap_data );
    day_buffer_row get_base_state(name converter, vector<swap_record> swap_data);
};