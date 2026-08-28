"""Order-book imbalance vs. next midpoint move, on LOBSTER AAPL Level 1 data.

LOBSTER sample files are derived from NASDAQ Historical TotalView-ITCH. This
script reads the reconstructed message/orderbook CSV pair, builds one
observation per constant-midpoint interval, and measures how often the next
midpoint move is up as a function of order-book imbalance.

Usage:  python3 analysis/analyze_lobster.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import pandas as pd

REPO_ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = REPO_ROOT / "data" / "lobster"
RESULTS_DIR = REPO_ROOT / "results"

ORDERBOOK_COLUMNS = ["ask_price", "ask_size", "bid_price", "bid_size"]
MESSAGE_COLUMNS = ["time", "event_type", "order_id", "size", "price", "direction"]

# Left-closed buckets; the final bucket also includes imbalance == 1.0.
BUCKET_EDGES = [-1.0, -0.6, -0.2, 0.2, 0.6, 1.0]
BUCKET_LABELS = [
    "[-1.0, -0.6)",
    "[-0.6, -0.2)",
    "[-0.2, 0.2)",
    "[0.2, 0.6)",
    "[0.6, 1.0]",
]

# dataviz: single series -> one hue, no legend, recessive axes.
SERIES_COLOR = "#2a78d6"
SURFACE = "#fcfcfb"
TEXT_PRIMARY = "#0b0b0b"
TEXT_SECONDARY = "#52514e"

PRICE_SCALE = 10_000  # LOBSTER stores dollar price x 10,000.


class DataError(RuntimeError):
    """Raised when the LOBSTER sample files are missing or malformed."""


def locate_files() -> tuple[Path, Path]:
    """Find the paired AAPL Level 1 message and orderbook CSVs."""
    messages = sorted(DATA_DIR.glob("*message*.csv"))
    orderbooks = sorted(DATA_DIR.glob("*orderbook*.csv"))

    if not messages or not orderbooks:
        raise DataError(
            f"No LOBSTER CSV files found in {DATA_DIR}.\n"
            "Download the LOBSTER AAPL Level 1 sample "
            "(https://php.lobsterdata.com/info/DataSamples.php), extract it, and place "
            "the paired files there, e.g.:\n"
            "  data/lobster/AAPL_2012-06-21_34200000_57600000_message_1.csv\n"
            "  data/lobster/AAPL_2012-06-21_34200000_57600000_orderbook_1.csv"
        )

    # Pair by the filename stem with the message/orderbook tag removed.
    by_key = {p.name.replace("_orderbook_", "_"): p for p in orderbooks}
    for message_path in messages:
        key = message_path.name.replace("_message_", "_")
        if key in by_key:
            return message_path, by_key[key]

    raise DataError(
        f"Found message and orderbook files in {DATA_DIR} but none of them pair up.\n"
        f"  messages:   {[p.name for p in messages]}\n"
        f"  orderbooks: {[p.name for p in orderbooks]}"
    )


def load(message_path: Path, orderbook_path: Path) -> pd.DataFrame:
    """Load and join one LOBSTER message/orderbook pair. LOBSTER CSVs have no header."""
    messages = pd.read_csv(message_path, header=None, names=MESSAGE_COLUMNS)
    book = pd.read_csv(orderbook_path, header=None, names=ORDERBOOK_COLUMNS)

    if len(messages) != len(book):
        raise DataError(
            "Message and orderbook row counts differ "
            f"({len(messages)} vs {len(book)}); the files are not a matched pair."
        )
    assert len(messages) == len(book)

    return pd.concat([messages.reset_index(drop=True), book.reset_index(drop=True)], axis=1)


def derive_features(df: pd.DataFrame) -> pd.DataFrame:
    """Spread, midpoint and imbalance, in raw LOBSTER integer price units."""
    df = df.copy()
    df["spread"] = df["ask_price"] - df["bid_price"]
    df["midpoint"] = (df["ask_price"] + df["bid_price"]) / 2
    total_size = df["bid_size"] + df["ask_size"]
    df["imbalance"] = (df["bid_size"] - df["ask_size"]) / total_size
    return df


def drop_empty_book_rows(df: pd.DataFrame) -> tuple[pd.DataFrame, int]:
    """Drop rows where a side is empty (LOBSTER dummy price/size), which make
    the midpoint and imbalance undefined."""
    valid = (
        (df["bid_size"] > 0)
        & (df["ask_size"] > 0)
        & (df["bid_price"] > 0)
        & (df["ask_price"] > 0)
    )
    return df[valid].reset_index(drop=True), int((~valid).sum())


def build_intervals(df: pd.DataFrame) -> pd.DataFrame:
    """One observation per consecutive constant-midpoint interval.

    Neighbouring rows inside an unchanged-midpoint interval are not independent
    observations, so we keep only the first book state of each interval and label
    it by the direction of the *next* interval's midpoint. The final interval has
    no following move and is dropped.
    """
    interval_id = (df["midpoint"] != df["midpoint"].shift()).cumsum()
    first = df.groupby(interval_id, as_index=False).first()

    first["next_midpoint"] = first["midpoint"].shift(-1)
    first = first.iloc[:-1].copy()  # final interval has no following move
    first["next_move_up"] = (first["next_midpoint"] > first["midpoint"]).astype(int)
    return first.reset_index(drop=True)


def bucket_imbalance(imbalance: pd.Series) -> pd.Series:
    """Left-closed buckets, with the top bucket closed at 1.0 so a fully
    bid-heavy book is not silently dropped."""
    buckets = pd.cut(
        imbalance,
        bins=BUCKET_EDGES,
        labels=BUCKET_LABELS,
        right=False,
        include_lowest=True,
    )
    buckets = buckets.where(imbalance < 1.0, BUCKET_LABELS[-1])
    if buckets.isna().any():
        raise DataError(
            f"{int(buckets.isna().sum())} observations fell outside the imbalance "
            "buckets; imbalance should always lie in [-1, 1]."
        )
    return buckets


def summarize(observations: pd.DataFrame) -> pd.DataFrame:
    """Count and up/down probability per imbalance bucket."""
    observations = observations.copy()
    observations["imbalance_bucket"] = bucket_imbalance(observations["imbalance"])

    grouped = observations.groupby("imbalance_bucket", observed=False)["next_move_up"]
    table = pd.DataFrame(
        {
            "imbalance_bucket": BUCKET_LABELS,
            "observations": grouped.count().reindex(BUCKET_LABELS).astype(int).values,
            "prob_next_move_up": grouped.mean().reindex(BUCKET_LABELS).values,
        }
    )
    # Every interval boundary is a midpoint change by construction, so down = 1 - up.
    table["prob_next_move_down"] = 1.0 - table["prob_next_move_up"]
    return table


def plot(table: pd.DataFrame, base_rate: float, out_path: Path) -> None:
    fig, ax = plt.subplots(figsize=(8.5, 5.0), facecolor=SURFACE)
    ax.set_facecolor(SURFACE)

    bars = ax.bar(
        table["imbalance_bucket"],
        table["prob_next_move_up"],
        width=0.62,
        color=SERIES_COLOR,
    )

    ax.axhline(base_rate, color=TEXT_SECONDARY, linewidth=1, linestyle="--", alpha=0.7)
    ax.annotate(
        f"overall base rate {base_rate:.3f}",
        xy=(-0.48, base_rate),
        xytext=(0, 4),
        textcoords="offset points",
        ha="left",
        va="bottom",
        fontsize=9,
        color=TEXT_SECONDARY,
    )

    for bar, prob, count in zip(bars, table["prob_next_move_up"], table["observations"]):
        if pd.isna(prob):
            continue
        x = bar.get_x() + bar.get_width() / 2
        ax.annotate(
            f"{prob:.3f}",
            xy=(x, prob),
            xytext=(0, 6),
            textcoords="offset points",
            ha="center",
            fontsize=10,
            color=TEXT_PRIMARY,
        )
        ax.annotate(
            f"n={count:,}",
            xy=(x, 0),
            xytext=(0, 8),
            textcoords="offset points",
            ha="center",
            va="bottom",
            fontsize=9,
            color=SURFACE,
        )

    ax.set_title(
        "Probability the next midpoint move is up, by order-book imbalance",
        fontsize=13,
        color=TEXT_PRIMARY,
        pad=14,
        loc="left",
    )
    ax.set_xlabel("imbalance  (bid_size - ask_size) / (bid_size + ask_size)",
                  fontsize=10, color=TEXT_SECONDARY, labelpad=10)
    ax.set_ylabel("P(next midpoint move is up)", fontsize=10, color=TEXT_SECONDARY)
    ax.set_ylim(0, max(0.7, float(table["prob_next_move_up"].max()) + 0.12))
    ax.tick_params(colors=TEXT_SECONDARY, labelsize=9)
    ax.grid(axis="y", color=TEXT_SECONDARY, alpha=0.15, linewidth=0.8)
    ax.set_axisbelow(True)
    for side in ("top", "right", "left"):
        ax.spines[side].set_visible(False)
    ax.spines["bottom"].set_color(TEXT_SECONDARY)
    ax.spines["bottom"].set_alpha(0.4)

    fig.tight_layout()
    fig.savefig(out_path, dpi=160, facecolor=SURFACE)
    plt.close(fig)


def main() -> int:
    try:
        message_path, orderbook_path = locate_files()
        raw = load(message_path, orderbook_path)
        raw = derive_features(raw)
        clean, dropped = drop_empty_book_rows(raw)
        observations = build_intervals(clean)
        if observations.empty:
            raise DataError("No usable constant-midpoint intervals were found.")
        table = summarize(observations)
    except DataError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    base_rate = observations["next_move_up"].mean()

    RESULTS_DIR.mkdir(exist_ok=True)
    csv_path = RESULTS_DIR / "imbalance_next_move.csv"
    png_path = RESULTS_DIR / "imbalance_next_move.png"
    table.to_csv(csv_path, index=False)
    plot(table, base_rate, png_path)

    print(f"message file      : {message_path.name}")
    print(f"orderbook file    : {orderbook_path.name}")
    print(f"total raw rows    : {len(raw):,}")
    if dropped:
        print(f"dropped rows      : {dropped:,} (one side of the book empty)")
    print(f"midpoint intervals: {len(observations):,} (final interval dropped: no next move)")
    print(f"mean spread       : ${raw['spread'].mean() / PRICE_SCALE:.4f}")
    print(f"mean midpoint     : ${raw['midpoint'].mean() / PRICE_SCALE:.2f}")
    print(f"up-move base rate : {base_rate:.4f}")
    print()
    print(
        table.to_string(
            index=False,
            formatters={
                "observations": lambda v: f"{v:,}",
                "prob_next_move_up": lambda v: f"{v:.4f}",
                "prob_next_move_down": lambda v: f"{v:.4f}",
            },
        )
    )
    print()
    print(f"wrote {csv_path.relative_to(REPO_ROOT)}")
    print(f"wrote {png_path.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
