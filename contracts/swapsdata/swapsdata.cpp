#include "swapsdata.hpp"

/**------------------------------------------------------------------------------------------------
 * @param converter
 * @param last_state
 * @param swap_data
 */
void swapsdata::update_day_buffer( name converter, day_buffer_row last_state, vector<swap_record> swap_data ) {
    day_buffer_table _buffer( get_self(), converter.value );
    const time_point_sec& timestamp = time_point_sec( (current_time_point().sec_since_epoch() / DAY_HISTORY_INTERVALS) * DAY_HISTORY_INTERVALS );
    auto itr = _buffer.find(timestamp.sec_since_epoch());

    if (itr == _buffer.end()) {
        _buffer.emplace( get_self(), [&]( auto& row ) {
            row.timestamp = timestamp;
            // volume_cumulative
            row.volume_cumulative = last_state.volume_cumulative;
            // price
            row.base_price = last_state.base_price;
            for (const swap_record swap_data_point : swap_data) {
                const auto &sym_code = swap_data_point.quantity.symbol.code();
                row.volume[sym_code] = swap_data_point.quantity;
                row.open_price[sym_code] = swap_data_point.price;
                row.high_price[sym_code] = swap_data_point.price;
                row.low_price[sym_code] = swap_data_point.price;
                row.close_price[sym_code] = swap_data_point.price;
            }
        });
    } else {
        /*
         * note, the assumption has been made that all map objects always have counters for all symbol codes initialised.
         * checks are skipped from this routing. If this is not true it is likely to break the contract
         */
        _buffer.modify( itr, same_payer, [&]( auto & row ) {
            for ( const swap_record swap_data_point : swap_data ) {
                const auto &sym_code = swap_data_point.quantity.symbol.code();
                // assuming entry is always made into map on record creation, skipping test if symbol in map
                row.volume[sym_code] += swap_data_point.quantity;
                // row.open_price[ sym_code ] set above
                row.high_price[ sym_code ] = max(row.high_price[ sym_code ], swap_data_point.price);
                row.low_price[ sym_code ] = min(row.low_price[ sym_code ], swap_data_point.price);
                row.close_price[ sym_code ] = swap_data_point.price;
            }
        });
    }
}

/**------------------------------------------------------------------------------------------------
 *
 * @param converter
 * @param swap_data
 */
void swapsdata::update_month_buffer( name converter, vector<swap_record> swap_data ) {
    month_buffer_table _buffer( get_self(), converter.value );
    const time_point_sec& timestamp = time_point_sec( (current_time_point().sec_since_epoch() / MONTH_HISTORY_INTERVALS) * MONTH_HISTORY_INTERVALS );
    auto itr = _buffer.find(timestamp.sec_since_epoch());

    if ( itr == _buffer.end() ) {
        _buffer.emplace( get_self(), [&]( auto& row ) {
            row.timestamp = timestamp;
            for (const swap_record swap_data_point : swap_data) {
                const auto& sym_code = swap_data_point.quantity.symbol.code();
                row.open_smart_price[sym_code] = swap_data_point.smart_price;
            }
        });
    }
}

/**------------------------------------------------------------------------------------------------
 *
 */
swapsdata::day_buffer_row swapsdata::get_last_state( name converter, vector<swap_record> swap_data, day_buffer_row base_data ) {
    trade_data_table _trade_data(get_self(), get_self().value);
    auto itr = _trade_data.find(converter.value);

    day_buffer_row last_state;

    if (itr == _trade_data.end()) {
        for (const swap_record swap_data_point : swap_data) {
            const auto &sym_code = swap_data_point.quantity.symbol.code();
            last_state.volume_cumulative[sym_code] = asset(0, swap_data_point.quantity.symbol);
            last_state.base_price[sym_code] = swap_data_point.price;
        }
    } else {
        auto td = *itr;
        for ( const swap_record swap_data_point : swap_data ) {
            const auto& sym_code = swap_data_point.quantity.symbol.code();
            auto base_volume = ( base_data.volume_cumulative.find( sym_code ) == base_data.volume_cumulative.end()) ?
                    asset(0, swap_data_point.quantity.symbol) : base_data.volume_cumulative[sym_code];

            last_state.volume_cumulative[sym_code] = ( td.volume_cumulative.find( sym_code ) == td.volume_cumulative.end()) ?
                    base_volume : td.volume_cumulative[sym_code];

            last_state.base_price[sym_code] = swap_data_point.price;
        }
    }

    return last_state;
}

/**------------------------------------------------------------------------------------------------
 * @param converter
 * @param swap_data
 * @param base_data
 */
void swapsdata::update_trade_data( name converter, vector<swap_record> swap_data, day_buffer_row base_data, month_buffer_row smart_base_data ) {
    trade_data_table _trade_data( get_self(), get_self().value );

    // data for 24hr buffer
    day_buffer_row last_state = get_last_state( converter, swap_data, base_data );

    //
    auto itr = _trade_data.find(converter.value);

    if ( itr == _trade_data.end() ) {
        _trade_data.emplace( get_self(), [&]( auto& row ) {
            row.converter = converter;
            row.timestamp = current_time_point();
            for ( const swap_record swap_data_point : swap_data ) {
                const auto& sym_code = swap_data_point.quantity.symbol.code();
                // volume_24h
                row.volume_24h[sym_code] = swap_data_point.quantity;
                // volume_cumulative
                row.volume_cumulative[sym_code] = swap_data_point.quantity;
                // price
                row.price[sym_code] = swap_data_point.price;
                // price_change_24h
                row.price_change_24h[sym_code] = 0.0;
                // liquidity depth
                row.liquidity_depth[sym_code] = swap_data_point.liquidity_depth;
                // smart_price
                row.smart_price[sym_code] = swap_data_point.smart_price;
                // smart_price_change_30d
                row.smart_price_change_30d[sym_code] = 0.0;
            }
        });

    } else {
        _trade_data.modify( itr, same_payer, [&]( auto & row ) {
            row.timestamp = current_time_point();

            for ( const swap_record swap_data_point : swap_data ) {
                const auto& sym_code = swap_data_point.quantity.symbol.code();
                // base_data.volume_cumulative[sym_code] - is sufficient
                auto base_volume = ( base_data.volume_cumulative.find( sym_code ) == base_data.volume_cumulative.end()) ?
                                   asset(0, swap_data_point.quantity.symbol) : base_data.volume_cumulative[sym_code];
                // volume_24h
                row.volume_24h[sym_code] = last_state.volume_cumulative[sym_code] + swap_data_point.quantity - base_volume;
                // volume_cumulative
                row.volume_cumulative[sym_code] = last_state.volume_cumulative[sym_code] + swap_data_point.quantity;
                // price
                row.price[sym_code] = swap_data_point.price;

                auto base_price = ( base_data.base_price.find( sym_code ) == base_data.base_price.end()) ?
                                  swap_data_point.price : base_data.base_price[sym_code];
                // price_change_24h
                row.price_change_24h[sym_code] = swap_data_point.price - base_price;
                // liquidity depth
                row.liquidity_depth[sym_code] = swap_data_point.liquidity_depth;
                // smart_price
                row.smart_price[sym_code] = swap_data_point.smart_price;

                // smart_price_change_30d
                row.smart_price_change_30d[sym_code] = swap_data_point.smart_price - smart_base_data.open_smart_price[sym_code];
            }
        });
    }

    update_day_buffer( converter, last_state, swap_data );
    update_month_buffer( converter, swap_data );
}

/**------------------------------------------------------------------------------------------------
 * @param converter
 * @param swap_data
 * @return
 */

swapsdata::month_buffer_row swapsdata::get_smart_base_state(name converter, vector<swap_record> swap_data) {
    month_buffer_table _buffer( get_self(), converter.value );

    const time_point_sec& timestamp = time_point_sec( (current_time_point().sec_since_epoch() / MONTH_HISTORY_INTERVALS) * MONTH_HISTORY_INTERVALS );

    // Smart returns based on 30 day history
    if ( _buffer.find(timestamp.sec_since_epoch()) == _buffer.end() ) {
        auto it = _buffer.begin();
        while ( (it != _buffer.end()) && (it->timestamp.sec_since_epoch() < ( current_time_point()-days(30) ).sec_since_epoch()) ) {
            it = _buffer.erase(it);
        }
    }

    // combine 30d data
    auto it = _buffer.begin();
    if ( it == _buffer.end() ) {
        month_buffer_row smart_base_data;
        smart_base_data.timestamp = timestamp;
        for ( const swap_record swap_data_point : swap_data ) {
            auto sym_code = swap_data_point.quantity.symbol.code();
            smart_base_data.open_smart_price[sym_code] = swap_data_point.smart_price;
        }
        return smart_base_data;
    } else {
        return *it;
    }
}


/**------------------------------------------------------------------------------------------------
 * @param converter
 * @param swap_data
 * @return
 */
swapsdata::day_buffer_row swapsdata::get_base_state(name converter, vector<swap_record> swap_data) {
    day_buffer_table _buffer( get_self(), converter.value );

    const time_point_sec& timestamp = time_point_sec( (current_time_point().sec_since_epoch() / DAY_HISTORY_INTERVALS) * DAY_HISTORY_INTERVALS );

    // Limit history to 24 hours (1 day), only clean up if new hour
    if ( _buffer.find(timestamp.sec_since_epoch()) == _buffer.end() ) {
        auto it = _buffer.begin();
        while ( (it != _buffer.end()) && (it->timestamp.sec_since_epoch() < ( current_time_point()-days(1) ).sec_since_epoch()) ) {
            it = _buffer.erase(it);
        }
    }

    // combine 24h data
    auto it = _buffer.begin();
    if ( it == _buffer.end() ) {
        day_buffer_row base_data;
        base_data.timestamp = timestamp;
        for ( const swap_record swap_data_point : swap_data ) {
            auto sym_code = swap_data_point.quantity.symbol.code();
            base_data.volume_cumulative[sym_code] = asset(0, swap_data_point.quantity.symbol );
            base_data.base_price[sym_code] = swap_data_point.price;
        }
        return base_data;
    } else {
        return *it;
    }
}

/**------------------------------------------------------------------------------------------------
 * @param converter
 * @param swap_data
 */
void swapsdata::log(name converter, vector<swap_record> swap_data) {
    // TODO this contract could be called by a fake converter to consume contract RAM. Shut down this exploit
    // TODO we should use converter whitelist and ignore actions in converter is not whitelisted
    check(has_auth(converter), "this action can only be called by a swaps converter");
    update_trade_data( converter, swap_data, get_base_state(converter, swap_data), get_smart_base_state(converter, swap_data) );
}

/**------------------------------------------------------------------------------------------------
 *
 */
void swapsdata::reset(name converter) {
    require_auth(get_self());

    trade_data_table _trade_data(get_self(), get_self().value);
    auto itr = _trade_data.find(converter.value);
    _trade_data.erase(itr);

    day_buffer_table day_buffer( get_self(), converter.value );
    auto d_it = day_buffer.begin();
    while (d_it != day_buffer.end()) {
        d_it = day_buffer.erase(d_it);
    }

    month_buffer_table month_buffer( get_self(), converter.value );
    auto m_it = month_buffer.begin();
    while (m_it != month_buffer.end()) {
        m_it = month_buffer.erase(m_it);
    }
}
