# C++23 Limit Order Book & Matching Engine

A limit order book with price-time priority matching, plus two separate
measurement components:

1. a **synthetic throughput benchmark** of the engine, and
2. a **historical microstructure analysis** of real NASDAQ-derived market data.

They're independent. The benchmark tells you nothing about the historical data,
and the historical analysis never touches the C++ engine. Keeping them apart is
deliberate, so neither borrows credibility from the other.

## Build & run

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
./build-release/orderbook_tests    # correctness
./build-release/orderbook_bench    # synthetic throughput benchmark
```

## Synthetic matching-engine benchmark

`bench/benchmark.cpp` runs a generated mixed workload through the engine:
1,000,000 commands, built from 100,000 blocks of 7 limit submissions and 3
cancellations that are guaranteed to hit resting orders, with two deliberate
crossing pairs per block. Five timed passes after a warm-up, median reported.

Measured on an Apple M4 Pro, Release (`-O3`):

| metric | value |
|---|---|
| median throughput | **18.3 M commands/sec** |
| commands per run | 1,000,000 |
| fills | 200,000 |
| executed volume | 10,109,700 |
| cancellations | 300,000 |

The workload is synthetic and deterministic. It stresses the engine's data
structures under a fixed command mix. It isn't a replay of real market activity,
and it says nothing about how the engine would behave under any exchange's actual
message flow.

## LOBSTER historical analysis

### Data

`data/lobster/` is expected to contain the free
[LOBSTER AAPL Level 1 sample](https://php.lobsterdata.com/info/DataSamples.php).
LOBSTER reconstructs these files from **NASDAQ Historical TotalView-ITCH**. The
sample used here is AAPL on 2012-06-21, 09:30 to 16:00.

This project reads those reconstructed CSVs and nothing more. It does not parse
raw ITCH, does not rebuild the Nasdaq book itself, and does not reproduce
Nasdaq's matching behaviour.

Each sample is a message file (`time, event_type, order_id, size, price,
direction`) paired with an orderbook file (`ask_price, ask_size, bid_price,
bid_size`), one orderbook row per message row. LOBSTER stores prices as dollar
price × 10,000. The analysis stays in those integer units and converts to dollars
only for output a human reads.

If the CSVs aren't there, the script exits and tells you what to download and
where to put it.

### Method

```bash
pip install -r analysis/requirements.txt
python3 analysis/analyze_lobster.py
```

For every book state the script computes

- `spread = ask_price - bid_price`
- `midpoint = (ask_price + bid_price) / 2`
- `imbalance = (bid_size - ask_size) / (bid_size + ask_size)`

The midpoint sits still across long runs of consecutive messages. Treating every
row as an observation would count thousands of near-identical states from one
unchanged-midpoint stretch as independent evidence, which would inflate the
sample without adding information. So the series is split into consecutive
**constant-midpoint intervals**, and:

- each interval contributes one observation, the **first** book state in it;
- that observation is labelled `next_move_up = 1` when the **next** interval's
  midpoint is higher, otherwise `0`;
- the last interval is dropped, since nothing follows it.

Observations are then bucketed by imbalance into `[-1.0, -0.6)`, `[-0.6, -0.2)`,
`[-0.2, 0.2)`, `[0.2, 0.6)`, `[0.6, 1.0]`, and the up-move rate is reported per
bucket. Every interval boundary is a midpoint change by construction, so
`P(down) = 1 - P(up)`.

### Result

118,497 raw rows yield 64,350 constant-midpoint intervals (no row had an empty
side of the book). The overall up-move base rate is 0.4978.

![Probability the next midpoint move is up, by order-book imbalance](results/imbalance_next_move.png)

| imbalance bucket | observations | P(next move up) | P(next move down) |
|---|---:|---:|---:|
| [-1.0, -0.6) | 11,068 | 0.4448 | 0.5552 |
| [-0.6, -0.2) | 9,212 | 0.4659 | 0.5341 |
| [-0.2, 0.2) | 19,504 | 0.4933 | 0.5067 |
| [0.2, 0.6) | 8,844 | 0.5219 | 0.4781 |
| [0.6, 1.0] | 15,722 | 0.5457 | 0.4543 |

Full output: [`results/imbalance_next_move.csv`](results/imbalance_next_move.csv).

The up-move rate rises monotonically across the buckets. A bid-heavy book
(imbalance ≥ 0.6) is followed by an upward midpoint move 54.6% of the time,
against 44.5% for an ask-heavy book, roughly 10 percentage points apart around a
49.8% base rate.

### Limitations

- **One sample period, one ticker.** A single AAPL trading day in 2012. That's
  no basis for saying the relationship holds for other names, other days, or
  today's market.
- **Descriptive association, not a strategy.** This is a conditional frequency,
  not a backtest. Because the analysis ignores transaction costs, spread, queue
  position, fill probability, and adverse selection, it does not establish
  profitability.
- **Level 1 only.** Imbalance comes from top-of-book sizes. Depth behind the
  touch is not used.
- **Not causal.** The same underlying order flow can drive both the imbalance
  and the move that follows it.
