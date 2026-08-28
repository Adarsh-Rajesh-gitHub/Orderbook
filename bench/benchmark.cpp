#include "order_book.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

enum class Type { Buy, Sell, Cancel };

struct Command {
    Type type;
    OrderBook::OrderId id;
    OrderBook::Price price;
    OrderBook::Quantity quantity;
};

static std::uint64_t rng(std::uint64_t& x) {
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return x;
}

int main() {
    constexpr std::size_t BLOCKS = 100'000;
    constexpr std::size_t N = BLOCKS * 10;
    constexpr int RUNS = 5;

    std::vector<Command> commands;
    commands.reserve(N);

    std::uint64_t price_rng = 123456789;
    std::uint64_t qty_rng   = 987654321;
    OrderBook::OrderId next_id = 1;

    // Each block:
    // 7 limit submissions + 3 guaranteed-valid cancellations.
    // Two pairs deliberately match; three orders rest and are cancelled.
    for (std::size_t b = 0; b < BLOCKS; ++b) {
        auto base = static_cast<OrderBook::Price>(
            10000 + static_cast<int>(rng(price_rng) % 21) - 10);

        auto q1 = static_cast<OrderBook::Quantity>(rng(qty_rng) % 100 + 1);
        auto q2 = static_cast<OrderBook::Quantity>(rng(qty_rng) % 100 + 1);
        auto q3 = static_cast<OrderBook::Quantity>(rng(qty_rng) % 100 + 1);
        auto q4 = static_cast<OrderBook::Quantity>(rng(qty_rng) % 100 + 1);
        auto q5 = static_cast<OrderBook::Quantity>(rng(qty_rng) % 100 + 1);

        auto a = next_id++, b_id = next_id++, c = next_id++;
        auto d = next_id++, e = next_id++, f = next_id++, g = next_id++;

        commands.push_back({Type::Buy,  a, base - 100, q1});
        commands.push_back({Type::Sell, b_id, base + 100, q2});
        commands.push_back({Type::Buy,  c, base - 101, q3});

        // Match pair 1.
        commands.push_back({Type::Sell, d, base + 1, q4});
        commands.push_back({Type::Buy,  e, base + 1, q4});

        // Match pair 2.
        commands.push_back({Type::Buy,  f, base, q5});
        commands.push_back({Type::Sell, g, base, q5});

        // All three are guaranteed to still be resting.
        commands.push_back({Type::Cancel, a, 0, 0});
        commands.push_back({Type::Cancel, b_id, 0, 0});
        commands.push_back({Type::Cancel, c, 0, 0});
    }

    auto execute = [&](OrderBook& book, const Command& c,
                       std::uint64_t& fills,
                       std::uint64_t& volume,
                       std::uint64_t& cancels) {
        if (c.type == Type::Cancel) {
            cancels += book.cancel(c.id);
            return;
        }

        auto result = (c.type == Type::Buy)
            ? book.limit_buy(c.id, c.price, c.quantity)
            : book.limit_sell(c.id, c.price, c.quantity);

        fills += result.size();
        for (const auto& f : result)
            volume += f.quantity;
    };

    // Warm-up.
    {
        OrderBook book;
        std::uint64_t f = 0, v = 0, c = 0;
        for (std::size_t i = 0; i < 100'000; ++i)
            execute(book, commands[i], f, v, c);
    }

    std::vector<double> throughputs;

    for (int run = 0; run < RUNS; ++run) {
        OrderBook book;
        std::uint64_t fill_count = 0;
        std::uint64_t executed_volume = 0;
        std::uint64_t cancel_count = 0;

        auto start = std::chrono::steady_clock::now();

        for (const auto& cmd : commands)
            execute(book, cmd, fill_count, executed_volume, cancel_count);

        auto end = std::chrono::steady_clock::now();

        double seconds =
            std::chrono::duration<double>(end - start).count();

        double throughput = N / seconds;
        throughputs.push_back(throughput);

        std::cout << "Run " << run + 1 << ": "
                  << throughput / 1e6 << " M commands/sec"
                  << " | fills=" << fill_count
                  << " volume=" << executed_volume
                  << " cancels=" << cancel_count << '\n';
    }

    std::sort(throughputs.begin(), throughputs.end());

    std::cout << "Median: "
              << throughputs[RUNS / 2] / 1e6
              << " M commands/sec\n";
}