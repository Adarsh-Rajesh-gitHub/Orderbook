cmake_minimum_required(VERSION 3.20)
project(Orderbook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

add_executable(orderbook src/main.cpp src/order_book.cpp)
target_include_directories(orderbook PRIVATE include)

add_executable(orderbook_tests tests/order_book_test.cpp src/order_book.cpp)
target_include_directories(orderbook_tests PRIVATE include)

add_executable(orderbook_bench bench/benchmark.cpp src/order_book.cpp)
target_include_directories(orderbook_bench PRIVATE include)