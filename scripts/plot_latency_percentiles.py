#!/usr/bin/env python3
"""Plot latency percentiles from observer CSV vs time"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib.dates as mdates
import matplotlib.pyplot as plt
import pandas as pd

METRICS: list[tuple[str, str]] = [
    ("total", "Total (feeder read → executor read)"),
    ("feeder", "Δ Feeder (socket read → MD write)"),
    ("md_queue", "Δ MD queue (MD write → trader read)"),
    ("trader", "Δ Trader (trader read → order write)"),
    ("order_queue", "Δ Order queue (order write → executor read)"),
]

PERCS = (50, 90, 95, 99)


def _RequireColumns(df: pd.DataFrame, path: Path, names: list[str]) -> None:
    for col in names:
        if col not in df.columns:
            raise SystemExit(f"Missing column {col!r} in {path}")


def LoadLatencyCsvLegacy(path: Path, df: pd.DataFrame) -> pd.DataFrame:
    for col in ("utc_s", "samples", "p50_ns", "p90_ns", "p95_ns", "p99_ns"):
        _RequireColumns(df, path, [col])
    df["utc_s"] = pd.to_numeric(df["utc_s"], errors="coerce")
    df = df.dropna(subset=["utc_s"]).copy()
    for col in ("samples", "p50_ns", "p90_ns", "p95_ns", "p99_ns"):
        df[col] = pd.to_numeric(df[col], errors="coerce")
    df = df.dropna()
    df["time_utc"] = pd.to_datetime(df["utc_s"], unit="s", utc=True)
    for name in ("p50", "p90", "p95", "p99"):
        df[f"total_{name}_us"] = df[f"{name}_ns"] / 1000.0
    return df


def LoadLatencyCsvExtended(path: Path, df: pd.DataFrame) -> pd.DataFrame:
    cols = ["utc_s", "samples"]
    for prefix, _ in METRICS:
        for p in PERCS:
            cols.append(f"{prefix}_p{p}_ns")
    _RequireColumns(df, path, cols)
    df["utc_s"] = pd.to_numeric(df["utc_s"], errors="coerce")
    df = df.dropna(subset=["utc_s"]).copy()
    for col in cols[1:]:
        df[col] = pd.to_numeric(df[col], errors="coerce")
    df = df.dropna()
    df["time_utc"] = pd.to_datetime(df["utc_s"], unit="s", utc=True)
    for prefix, _ in METRICS:
        for p in PERCS:
            ns_col = f"{prefix}_p{p}_ns"
            df[f"{prefix}_p{p}_us"] = df[ns_col] / 1000.0
    return df


def LoadLatencyCsv(path: Path) -> tuple[pd.DataFrame, bool]:
    df = pd.read_csv(path)
    if df.empty:
        return df, False
    if "total_p50_ns" in df.columns:
        return LoadLatencyCsvExtended(path, df), True
    if "p50_ns" in df.columns:
        return LoadLatencyCsvLegacy(path, df), False
    raise SystemExit(
        f"Unrecognized CSV schema in {path}: expected total_p50_ns or p50_ns column."
    )


def PlotPercentilesOnAx(ax: plt.Axes, df: pd.DataFrame, prefix: str, title: str) -> None:
    ax.plot(
        df["time_utc"],
        df[f"{prefix}_p50_us"],
        label="p50",
        linewidth=1.0,
        alpha=0.9,
    )
    ax.plot(
        df["time_utc"],
        df[f"{prefix}_p90_us"],
        label="p90",
        linewidth=1.0,
        alpha=0.9,
    )
    ax.plot(
        df["time_utc"],
        df[f"{prefix}_p95_us"],
        label="p95",
        linewidth=1.0,
        alpha=0.9,
    )
    ax.plot(
        df["time_utc"],
        df[f"{prefix}_p99_us"],
        label="p99",
        linewidth=1.0,
        alpha=0.9,
    )
    ax.set_ylabel("Latency (µs)")
    ax.set_title(title)
    _, ymax = ax.get_ylim()
    ax.set_ylim(0, ymax)
    ax.grid(True, alpha=0.35)
    ax.legend(loc="upper right", framealpha=0.9)
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%H:%M:%S"))
    ax.tick_params(axis="x", rotation=20)


def BuildFigureExtended(df: pd.DataFrame) -> plt.Figure:
    n = len(METRICS)
    fig, axes = plt.subplots(n, 1, figsize=(11, 2.8 * n), sharex=True)
    if n == 1:
        axes = [axes]
    for ax, (prefix, title) in zip(axes, METRICS, strict=True):
        PlotPercentilesOnAx(ax, df, prefix, title)
    axes[-1].set_xlabel("Time (UTC)")
    fig.suptitle("Latency percentiles over time (per batch)", y=1.002, fontsize=12)
    fig.tight_layout()
    fig.autofmt_xdate()
    return fig


def BuildFigureLegacy(df: pd.DataFrame) -> plt.Figure:
    fig, ax = plt.subplots(1, 1, figsize=(11, 5))
    PlotPercentilesOnAx(ax, df, "total", "Latency percentiles over time (per batch)")
    fig.autofmt_xdate()
    return fig


def BuildFigure(df: pd.DataFrame, extended: bool) -> plt.Figure:
    if df.empty:
        raise SystemExit("No numeric rows to plot.")
    if extended:
        return BuildFigureExtended(df)
    return BuildFigureLegacy(df)


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

    df, extended = LoadLatencyCsv(args.csv)
    fig = BuildFigure(df, extended)

    if args.output is not None:
        fig.savefig(args.output, dpi=150, bbox_inches="tight")
        print(f"Wrote {args.output}")
    else:
        plt.show()
    plt.close(fig)
    return 0


if __name__ == "__main__":
    raise SystemExit(Main(sys.argv[1:]))
