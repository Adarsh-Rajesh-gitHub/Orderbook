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