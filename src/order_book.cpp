#include <set>
#include <map>
#include <list>
#include <unordered_map>
#include <iterator>
#include <optional>
#include <cstdint>
#include <vector> 
#include <functional> 
#include <algorithm>
#include <iostream>

#include "order_book.hpp"



OrderBook::OrderBook() = default;

std::vector<OrderBook::Fill> OrderBook::limit_buy(OrderId id, Price price, Quantity quantity) {
    if(orders.contains(id)){
        return {};
    }
    std::vector<Fill> res;
    while(quantity > 0 && !ask.empty()) {
        Order cur = ask.begin()->second.front();
        if (cur.price <= price) {
            OrderBook::Fill record;
            record.resting_id = cur.id;
            record.incoming_id = id;
            record.price = cur.price;
            record.quantity = std::min(cur.quantity, quantity);
            if(cur.quantity <= quantity) {
                ask.begin()->second.pop_front();
                if(ask[cur.price].empty()) {
                    ask.erase(cur.price);
                }
                orders.erase(record.resting_id);
            }
            else {
                ask.begin()->second.front().quantity -= quantity;
            }
            quantity -= record.quantity;
            res.push_back(record);
        }
        else {
            break;
        }
    }
    if(quantity != 0) {
        Order leftover;
        leftover.id = id;
        leftover.price = price;
        leftover.quantity = quantity;
        leftover.side = Side::Buy;
        if(bid.contains(leftover.price)) {
            bid[leftover.price].push_back(leftover);
        }
        else {
            bid[leftover.price] = std::list<Order>{leftover};
        }
        orders[id] = std::prev(bid[leftover.price].end());
    }
    return res;
}

std::vector<OrderBook::Fill> OrderBook::limit_sell(OrderId id, Price price, Quantity quantity) {
      if(orders.contains(id)){
        return {};
    }
    std::vector<Fill> res;
    while(quantity > 0 && !bid.empty()) {
        Order cur = bid.begin()->second.front();
        if (cur.price >= price) {
            OrderBook::Fill record;
            record.resting_id = cur.id;
            record.incoming_id = id;
            record.price = cur.price;
            record.quantity = std::min(cur.quantity, quantity);
            if(cur.quantity <= quantity) {
                bid.begin()->second.pop_front();
                if(bid[cur.price].empty()) {
                    bid.erase(cur.price);
                }
                orders.erase(record.resting_id);
            }
            else {
                bid.begin()->second.front().quantity -= quantity;
            }
            quantity -= record.quantity;
            res.push_back(record);
        }
        else {
            break;
        }
    }
    if(quantity != 0) {
        Order leftover;
        leftover.id = id;
        leftover.price = price;
        leftover.quantity = quantity;
        leftover.side = Side::Sell;
        if(ask.contains(leftover.price)) {
            ask[leftover.price].push_back(leftover);
        }
        else {
            ask[leftover.price] = std::list<Order>{leftover};
        }
        orders[id] = std::prev(ask[leftover.price].end());
    }
    return res;
}

bool OrderBook::cancel(OrderId id) {
    if(orders.contains(id)) {
        std::list<Order>::iterator iter = orders[id];
        Order cur = *iter;
        if(cur.side == Side::Sell) {
            ask[cur.price].erase(iter);
            if(ask[cur.price].empty()) {
                ask.erase(cur.price);
            }
        }
        else {
            bid[cur.price].erase(iter);
            if(bid[cur.price].empty()) {
                bid.erase(cur.price);
            }
        }
        orders.erase(id);
        return true;
    }
    return false;
}

std::optional<OrderBook::Price> OrderBook::best_bid() const {
    if(!bid.empty()) {
        return bid.begin()->first;
    }
    return std::nullopt;

}

std::optional<OrderBook::Price> OrderBook::best_ask() const {
    if(!ask.empty()) {
        return ask.begin()->first;
    }
    return std::nullopt;
}

void OrderBook::reveal() const {
    std::cout << "Ask: ";
    for(const auto& [price, level] : ask) {
        std::cout << price << " - {";
        for(const auto& ord : level) {
            ord.print();
        }
        std::cout << "}" << std::endl;
    }
    std::cout << std::endl;
    std::cout << "Bid: ";
    for(const auto& [price, level] : bid) {
        std::cout << price << " - {";
        for(const auto& ord : level) {
            ord.print();
        }
        std::cout << "}" << std::endl;
    }
    std::cout << std::endl << "-------------------------" << std::endl;
}
 