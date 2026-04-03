#!/usr/bin/env python3
"""Plot latency percentiles from observer CSV (utc_s,samples,p50_ns,...) vs time."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import pandas as pd


def LoadLatencyCsv(path: Path) -> pd.DataFrame:
    df = pd.read_csv(path)
    if df.empty:
        return df

    for col in ("utc_s", "samples", "p50_ns", "p90_ns", "p95_ns", "p99_ns"):
        if col not in df.columns:
            raise SystemExit(f"Missing column {col!r} in {path}")

    df["utc_s"] = pd.to_numeric(df["utc_s"], errors="coerce")
    df = df.dropna(subset=["utc_s"]).copy()
    for col in ("samples", "p50_ns", "p90_ns", "p95_ns", "p99_ns"):
        df[col] = pd.to_numeric(df[col], errors="coerce")
    df = df.dropna()

    df["time_utc"] = pd.to_datetime(df["utc_s"], unit="s", utc=True)
    for name in ("p50", "p90", "p95", "p99"):
        df[f"{name}_us"] = df[f"{name}_ns"] / 1000.0
    return df


def BuildFigure(df: pd.DataFrame) -> plt.Figure:
    if df.empty:
        raise SystemExit("No numeric rows to plot.")

    fig, ax = plt.subplots(1, 1, figsize=(11, 5))

    ax.plot(df["time_utc"], df["p50_us"], label="p50", linewidth=1.0, alpha=0.9)
    ax.plot(df["time_utc"], df["p90_us"], label="p90", linewidth=1.0, alpha=0.9)
    ax.plot(df["time_utc"], df["p95_us"], label="p95", linewidth=1.0, alpha=0.9)
    ax.plot(df["time_utc"], df["p99_us"], label="p99", linewidth=1.0, alpha=0.9)
    ax.set_ylabel("Latency (µs)")
    ax.set_title("Latency percentiles over time (per batch)")
    _, ymax = ax.get_ylim()
    ax.set_ylim(0, ymax)
    ax.grid(True, alpha=0.35)
    ax.legend(loc="upper right", framealpha=0.9)
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%H:%M:%S"))
    ax.tick_params(axis="x", rotation=20)

    fig.autofmt_xdate()
    return fig


def ParseArgs(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__.strip())
    p.add_argument(
        "csv",
        nargs="?",
        type=Path,
        default=Path("/var/log/hft/latency_percentiles.csv"),
        help="Path to latency_percentiles.csv",
    )
    p.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help="Save figure to this path (png/pdf/svg). If omitted, show interactively.",
    )
    return p.parse_args(argv)


def Main(argv: list[str]) -> int:
    args = ParseArgs(argv)
    if not args.csv.is_file():
        print(f"File not found: {args.csv}", file=sys.stderr)
        return 1

    df = LoadLatencyCsv(args.csv)
    fig = BuildFigure(df)

    if args.output is not None:
        fig.savefig(args.output, dpi=150, bbox_inches="tight")
        print(f"Wrote {args.output}")
    else:
        plt.show()
    plt.close(fig)
    return 0


if __name__ == "__main__":
    raise SystemExit(Main(sys.argv[1:]))
