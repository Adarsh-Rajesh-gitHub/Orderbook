



//I assume the order book will be built here

//a basic order book to my understand all it does is it takes in incoming bids and offers and sorts two lists or rather optimizes so that the highest bid and lowest offer at anytime can be called upon anytime in as less time as possible
//to this end this we need insertion and the least in for offers and most for bids easily queryable and removable
//a sorted set in c++ implements insertion, deletion in log(n) and querying the first element is O(1)
//however beyond just those deleting orders by order id is needed so an ordered map will be used

//allowing an orderbook to be created with differnt ordering and adding quotes in and getting what is inside it next and also being able to remove what is next is all the funcitonality that should be needed

#ifndef ORDERBOOK
#define ORDERBOOK
#include <set>
#include <map>
#include <list>
#include <unordered_map>
#include <iterator>
#include <optional>
#include <cstdint>
#include <vector> 
#include <functional> 

class OrderBook {

using OrderId = std::uint64_t;
using Price =std::int64_t;
using Quantity = std::uint32_t;

struct Fill {
    OrderId resting_id;
    OrderId incoming_id;
    Price price;
    Quantity quantity;
};
private:
    std::map<int, std::list<int>, std::greater<int>> bid;
    std::map<int, std::list<int>> ask;
    std::unordered_map<int, std::list<int>::iterator> orders;
public:
    OrderBook();
    std::vector<Fill> limit_buy(OrderId id, Price price, Quantity);
    std::vector<Fill> limit_sell(OrderId id, Price price, Quantity);
    bool cancel(OrderId id);
    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;

};
#endif