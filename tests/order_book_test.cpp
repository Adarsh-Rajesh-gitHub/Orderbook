#include "order_book.hpp"
#include <cassert>
#include <iostream>

void test_partial_resting_fill() {
    OrderBook b;

    b.limit_sell(1, 100, 10);
    auto fills = b.limit_buy(2, 100, 4);

    assert(fills.size() == 1);
    assert(fills[0].resting_id == 1);
    assert(fills[0].incoming_id == 2);
    assert(fills[0].price == 100);
    assert(fills[0].quantity == 4);

    // Verify 6 actually remain.
    auto second = b.limit_buy(3, 100, 10);
    assert(second.size() == 1);
    assert(second[0].quantity == 6);
}

void test_incoming_partial_fill() {
    OrderBook b;

    b.limit_sell(1, 100, 4);
    b.limit_buy(2, 100, 10);

    assert(b.best_bid().has_value());
    assert(*b.best_bid() == 100);
}

void test_multilevel_fill() {
    OrderBook b;

    b.limit_sell(1, 100, 5);
    b.limit_sell(2, 101, 5);

    auto fills = b.limit_buy(3, 101, 8);

    assert(fills.size() == 2);

    assert(fills[0].resting_id == 1);
    assert(fills[0].price == 100);
    assert(fills[0].quantity == 5);

    assert(fills[1].resting_id == 2);
    assert(fills[1].price == 101);
    assert(fills[1].quantity == 3);
}

void test_fifo() {
    OrderBook b;

    b.limit_sell(1, 100, 5);
    b.limit_sell(2, 100, 5);

    auto fills = b.limit_buy(3, 100, 7);

    assert(fills.size() == 2);
    assert(fills[0].resting_id == 1);
    assert(fills[0].quantity == 5);
    assert(fills[1].resting_id == 2);
    assert(fills[1].quantity == 2);
}

void test_cancel_non_best_order() {
    OrderBook b;

    b.limit_sell(1, 100, 5);
    b.limit_sell(2, 105, 5);

    assert(b.cancel(2));
    assert(*b.best_ask() == 100);
    assert(!b.cancel(2));
}

int main() {
    test_partial_resting_fill();
    test_incoming_partial_fill();
    test_multilevel_fill();
    test_fifo();
    test_cancel_non_best_order();

    std::cout << "All order book tests passed.\n";
}