/**
 *  @file
 *  @copyright
 */
#pragma once

#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>
#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>

using namespace eosio;
using namespace std;

/**
 * @defgroup BancorConverter BancorConverter
 * @brief Bancor Converter
 * @details The Bancor converter allows conversions between a smart token and tokens
 * that are defined as its reserves and between the different reserves directly.
 */

CONTRACT BancorConverter : public eosio::contract {
    public:
        using contract::contract;

        /**
         *
         */
        struct swap_record {
            asset    quantity;
            double   price;
            asset    liquidity_depth;
            double   smart_price;
        };

        /**
         * @defgroup Converter_Settings_Table Settings Table
         * @brief This table stores stats on the settings of the converter
         * @details Both SCOPE and PRIMARY KEY are `_self`, so this table is effectively a singleton.
         *
         * - smart_contract : contract account name of the smart token governed by the converter
         * - smart_currency : currency of the smart token governed by the converter
         * - smart_enabled : true if the smart token can be converted to/from, false if not
         * - enabled : true if conversions are enabled, false if not
         * - network : bancor network contract name
         * - require_balance : require creating new balance for the calling account should fail
         * - max_fee : maximum conversion fee percentage, 0-30000, 4-pt precision a la eosio.asset
         * - fee : conversion fee for this converter

         */
        TABLE settings_t {
            name smart_contract;
            asset smart_currency;
            bool smart_enabled;
            bool enabled;
            name network;
            bool require_balance;
            uint64_t max_fee;
            uint64_t fee;

            uint64_t primary_key() const { return "settings"_n.value; }
        };

        /**
          * @defgroup Converter_Reserves_Table Reserves Table
          * @brief This table stores stats on the reserves of the converter, the actual balance is owned by converter account within the accounts
          * @details SCOPE of this table is `_self`
          *
          * - contract : Token contract for the currency
          * - currency : Symbol of the tokens in this reserve
          *              PRIMARY KEY is `currency.symbol.code().raw()`
          * - ratio    : Reserve ratio
          * - sale_enabled : Are transactions enabled on this reserve
          */
        TABLE reserve_t {
            name contract;
            asset currency;
            uint64_t ratio;
            bool sale_enabled;

            uint64_t primary_key() const { return currency.symbol.code().raw(); }
        };

        /**
         * @brief initializes the converter settings
         * @details can only be called once, by the contract account
         * @param smart_contract - contract account name of the smart token governed by the converter
         * @param smart_currency - currency of the smart token governed by the converter
         * @param smart_enabled - true if the smart token can be converted to/from, false if not
         * @param enabled - true if conversions are enabled, false if not
         * @param require_balance - true if conversions that require creating new balance for the calling account should fail, false if not
         * @param network - bancor network contract name
         * @param max_fee - maximum conversion fee percentage, 0-30000, 4-pt precision a la eosio.asset
         * @param fee - conversion fee percentage, must be lower than the maximum fee, same precision
         */
        ACTION init(name smart_contract, asset smart_currency, bool smart_enabled, bool enabled, name network, bool require_balance, uint64_t max_fee, uint64_t fee);

        /**
         * @brief updates the converter settings
         * @details can only be called by the contract account
         * @param smart_enabled - true if the smart token can be converted to/from, false if not
         * @param enabled - true if conversions are enabled, false if not
         * @param require_balance - true if conversions that require creating new balance for the calling account should fail, false if not
         * @param fee - conversion fee percentage, must be lower than the maximum fee, same precision
         */
        ACTION update(bool smart_enabled, bool enabled, bool require_balance, uint64_t fee);

        /**
         * @brief initializes a new reserve in the converter
         * @details can also be used to update an existing reserve, can only be called by the contract account
         * @param contract - reserve token contract name
         * @param currency - reserve token currency symbol
         * @param ratio - reserve ratio, percentage, 0-1000000, precision a la max_fee
         * @param sale_enabled - true if purchases are enabled with the reserve, false if not
         */
        ACTION setreserve(name contract, symbol currency, uint64_t ratio, bool sale_enabled);

        /**
         * @brief deletes an empty reserve
         * @param currency - reserve token currency symbol
         */
        ACTION delreserve(symbol_code currency);

        /**
         * @brief transfer intercepts
         * @details `memo` in csv format, may contain an extra keyword (e.g. "setup") following a semicolon at the end of the conversion path; 
         * indicates special transfer which otherwise would be interpreted as a standard conversion
         * @param from - the sender of the transfer
         * @param to - the receiver of the transfer
         * @param quantity - the quantity for the transfer
         * @param memo - the memo for the transfer
         */
        [[eosio::on_notify("*::transfer")]]
        void on_transfer(name from, name to, asset quantity, std::string memo);

    private:
        using transfer_action = action_wrapper<name("transfer"), &BancorConverter::on_transfer>;
        typedef eosio::multi_index<"settings"_n, settings_t> settings;
        typedef eosio::multi_index<"reserves"_n, reserve_t> reserves;

        constexpr static double RATIO_DENOMINATOR = 1000000.0;
        constexpr static double FEE_DENOMINATOR = 1000000.0;

        void convert(name from, eosio::asset quantity, std::string memo, name code);
        const reserve_t& get_reserve(uint64_t name, const settings_t& settings);

        asset get_balance(name contract, name owner, symbol_code sym);
        uint64_t get_balance_amount(name contract, name owner, symbol_code sym);
        asset get_supply(name contract, symbol_code sym);

        void verify_min_return(eosio::asset quantity, std::string min_return);
        void verify_entry(name account, name currency_contract, eosio::asset currency);

        double calculate_fee(double amount, uint64_t fee, uint8_t magnitude);
        double calculate_purchase_return(double balance, double deposit_amount, double supply, int64_t ratio);
        double calculate_sale_return(double balance, double sell_amount, double supply, int64_t ratio);
        double quick_convert(double balance, double in, double toBalance);

        float stof(const char* s);

        static double asset_to_double( const asset quantity ) {
            if ( quantity.amount == 0 ) return 0.0;
            return quantity.amount / pow(10, quantity.symbol.precision());
        }
};