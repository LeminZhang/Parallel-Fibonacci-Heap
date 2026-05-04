#!/usr/bin/env python3
"""
Plot Dijkstra benchmark results.
"""

import re
import sys
import os
import tempfile
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "parallel_fib_heap_matplotlib"))
Path(os.environ["MPLCONFIGDIR"]).mkdir(parents=True, exist_ok=True)

import matplotlib.pyplot as plt
import numpy as np


def read_result_lines(filepath):
    with open(filepath, "rb") as f:
        raw = f.read()

    if raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff"):
        text = raw.decode("utf-16")
    elif raw.startswith(b"\xef\xbb\xbf"):
        text = raw.decode("utf-8-sig")
    else:
        text = raw.decode("utf-8")

    return text.splitlines()


def parse_dijkstra_results(filepath):
    pattern = re.compile(
        r"Dijkstra's algorithm with (\d+) nodes and (\d+) thread(?:s)? took ([\d.]+) ms\.(?:speedup:\s*([\d.]+))?"
    )
    data = {
        "nodes": [],
        "threads": [],
        "time_ms": [],
        "speedup": [],
    }

    for line in read_result_lines(filepath):
        match = pattern.search(line)
        if not match:
            continue

        data["nodes"].append(int(match.group(1)))
        data["threads"].append(int(match.group(2)))
        data["time_ms"].append(float(match.group(3)))
        data["speedup"].append(float(match.group(4)) if match.group(4) else None)

    return data


def plot_dijkstra(data, output_dir):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    nodes_values = sorted(set(data["nodes"]))
    thread_values = sorted(set(data["threads"]))
    parallel_threads = [thread for thread in thread_values if thread != 1]
    x = np.arange(len(parallel_threads))
    width = 0.15

    fig, ax = plt.subplots(figsize=(12, 6))

    for idx, nodes in enumerate(nodes_values):
        speedups = []
        baseline = None
        for i, (n, thread) in enumerate(zip(data["nodes"], data["threads"])):
            if n == nodes and thread == 1:
                baseline = data["time_ms"][i]
                break

        for thread in parallel_threads:
            value = np.nan
            for i, (n, current_thread) in enumerate(zip(data["nodes"], data["threads"])):
                if n != nodes or current_thread != thread:
                    continue
                value = data["speedup"][i]
                if value is None and baseline:
                    value = baseline / data["time_ms"][i]
                break
            speedups.append(value)

        offset = width * (idx - len(nodes_values) / 2 + 0.5)
        ax.bar(x + offset, speedups, width, label=f"{nodes} nodes")

    ax.axhline(y=1, color="black", linestyle="--", linewidth=2, alpha=0.6, label="Baseline (1x speedup)")
    ax.set_xlabel("Number of Threads", fontsize=13, fontweight="bold")
    ax.set_ylabel("Speedup", fontsize=13, fontweight="bold")
    ax.set_title("Dijkstra - Speedup vs Threads (E-core)", fontsize=14, fontweight="bold")
    ax.set_xticks(x)
    ax.set_xticklabels(parallel_threads)
    ax.legend(fontsize=10, loc="best")
    ax.grid(True, alpha=0.3, axis="y")
    plt.tight_layout()

    output_path = output_dir / "dijkstra_speedup_vs_threads.png"
    plt.savefig(output_path, dpi=150)
    plt.close()
    print(f"Saved: {output_path}")


if __name__ == "__main__":
    input_file = sys.argv[1] if len(sys.argv) > 1 else "benchmark_result/GHC_final/dijkstra.txt"
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "visualization/figures/GHC_final"

    if not Path(input_file).exists():
        print(f"Error: {input_file} not found")
        sys.exit(1)

    data = parse_dijkstra_results(input_file)
    print(f"Found {len(data['nodes'])} data points")
    print(f"Nodes: {sorted(set(data['nodes']))}")
    print(f"Threads: {sorted(set(data['threads']))}")
    plot_dijkstra(data, output_dir)
    print("\nAll plots generated successfully!")
