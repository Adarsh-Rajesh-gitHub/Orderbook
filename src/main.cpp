#include "order_book.hpp"

int main() {
    OrderBook book;

    book.reveal(); //nothing
    book.limit_sell(0, 100, 10);
    book.reveal(); //ask: 100 - 10 bid: nothing
    book.limit_buy(1, 100, 10);
    book.reveal(); //nothing
    book.limit_sell(2, 100, 5);
    book.reveal(); //ask: 100 - 5 bid: nothing
    book.limit_buy(3, 101, 5);
    book.limit_buy(4, 100, 5);
    book.reveal(); //ask: 100 - 3 bid: nothing
    book.limit_buy(4, 100, 5);
    book.reveal(); //same id should reject and be prev




    return 0;
}