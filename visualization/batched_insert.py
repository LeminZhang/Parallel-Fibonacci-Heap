import argparse
import os
import re
import tempfile
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "parallel_fib_heap_matplotlib"))
Path(os.environ["MPLCONFIGDIR"]).mkdir(parents=True, exist_ok=True)

import numpy as np
import matplotlib.pyplot as plt


def parse_file(filename):
    pattern = re.compile(
        r"number of operations=(\d+)\s+batch_size=(\d+)\s+threads=(\d+)\s+time_ms=([\d.]+)\s+MMops=([\d.]+)(?:\s+speedup=([\d.]+))?"
    )
    data = {}

    with open(filename, "r") as f:
        for line in f:
            m = pattern.search(line)
            if not m:
                continue
            ops = int(m.group(1))
            batch = int(m.group(2))
            threads = int(m.group(3))
            time_ms = float(m.group(4))
            mmops = float(m.group(5))
            speedup = float(m.group(6)) if m.group(6) else None

            data.setdefault(batch, {})
            data[batch].setdefault(ops, {})
            data[batch][ops][threads] = {
                "time_ms": time_ms,
                "mmops": mmops,
                "speedup": speedup,
            }

    return data


def build_heatmap(data, n_threads):
    batch_sizes = sorted(data.keys())
    ops_list = sorted({ops for batch in data for ops in data[batch]})
    heatmap = np.full((len(batch_sizes), len(ops_list)), np.nan)

    for i, batch in enumerate(batch_sizes):
        for j, ops in enumerate(ops_list):
            if ops not in data[batch]:
                continue
            if 1 not in data[batch][ops] or n_threads not in data[batch][ops]:
                continue

            t1 = data[batch][ops][1]["time_ms"]
            tn = data[batch][ops][n_threads]["time_ms"]
            speedup = t1 / tn

            heatmap[i, j] = speedup

    return heatmap, batch_sizes, ops_list


def draw_heatmap(data, n_threads, output_dir):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    heatmap, batch_sizes, ops_list = build_heatmap(data, n_threads)

    if np.all(np.isnan(heatmap)):
        print(f"No usable insert data for threads={n_threads}")
        return

    plt.figure(figsize=(12, 7))
    cmap = plt.cm.viridis.copy()
    cmap.set_bad(color="lightgray")
    plt.imshow(heatmap, cmap=cmap, aspect="auto", origin="lower")
    plt.colorbar(label="Speedup")

    plt.xticks(range(len(ops_list)), ops_list, rotation=45)
    plt.yticks(range(len(batch_sizes)), batch_sizes)

    plt.xlabel("Number of Operations")
    plt.ylabel("Batch Size")
    plt.title(f"Batched Insert Speedup Heatmap (threads = {n_threads})")

    plt.tight_layout()
    output_path = output_dir / f"insert_speedup_heatmap_threads_{n_threads}.png"
    plt.savefig(output_path, dpi=150)
    plt.close()
    print(f"Saved: {output_path}")


def draw_combined_heatmaps(data, threads, output_dir):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    heatmaps = []
    batch_sizes = []
    ops_list = []
    for thread in threads:
        heatmap, current_batch_sizes, current_ops_list = build_heatmap(data, thread)
        if np.all(np.isnan(heatmap)):
            print(f"No usable insert data for threads={thread}")
            continue
        heatmaps.append((thread, heatmap))
        batch_sizes = current_batch_sizes
        ops_list = current_ops_list

    if not heatmaps:
        print("No usable insert heatmap data for combined figure")
        return

    n_cols = 2
    n_rows = int(np.ceil(len(heatmaps) / n_cols))
    fig, axes = plt.subplots(n_rows, n_cols, figsize=(18, 7 * n_rows), squeeze=False)
    cmap = plt.cm.viridis.copy()
    cmap.set_bad(color="lightgray")

    all_values = np.concatenate([heatmap[~np.isnan(heatmap)] for _, heatmap in heatmaps])
    vmin = np.min(all_values)
    vmax = np.max(all_values)
    image = None

    for idx, (thread, heatmap) in enumerate(heatmaps):
        row = idx // n_cols
        col = idx % n_cols
        ax = axes[row][col]
        image = ax.imshow(heatmap, cmap=cmap, aspect="auto", origin="lower", vmin=vmin, vmax=vmax)
        ax.set_title(f"threads = {thread}", fontsize=14, fontweight="bold")
        ax.set_xticks(range(len(ops_list)))
        ax.set_xticklabels(ops_list, rotation=45, ha="right", fontsize=8)
        ax.set_yticks(range(len(batch_sizes)))
        ax.set_yticklabels(batch_sizes, fontsize=8)

        if row == n_rows - 1:
            ax.set_xlabel("Number of Operations", fontsize=12, fontweight="bold")
        if col == 0:
            ax.set_ylabel("Batch Size", fontsize=12, fontweight="bold")

    for idx in range(len(heatmaps), n_rows * n_cols):
        axes[idx // n_cols][idx % n_cols].axis("off")

    fig.suptitle("Batched Insert Speedup Heatmaps", fontsize=18, fontweight="bold")
    fig.tight_layout(rect=(0, 0, 0.94, 0.96))
    cbar_ax = fig.add_axes((0.955, 0.12, 0.015, 0.76))
    fig.colorbar(image, cax=cbar_ax, label="Speedup")

    output_path = output_dir / "insert_speedup_heatmaps_combined.png"
    fig.savefig(output_path, dpi=150)
    plt.close(fig)
    print(f"Saved: {output_path}")


def draw_speedup_bar(data, batch_size, operations, output_dir):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    if batch_size not in data or operations not in data[batch_size]:
        print(f"No insert data for batch_size={batch_size}, operations={operations}")
        return

    row = data[batch_size][operations]
    threads = sorted(row)
    speedups = []
    mmops_values = []

    baseline = row.get(1, {}).get("time_ms")
    for thread in threads:
        item = row[thread]
        speedup = item["speedup"]
        if speedup is None and baseline:
            speedup = baseline / item["time_ms"]
        speedups.append(speedup)
        mmops_values.append(item["mmops"])

    fig, ax = plt.subplots(figsize=(9, 6))
    bars = ax.bar([str(thread) for thread in threads], speedups, width=0.6)

    ax.axhline(y=1, color="black", linestyle="--", linewidth=2, alpha=0.6, label="Baseline (1x speedup)")
    ax.set_xlabel("Number of Threads", fontsize=13, fontweight="bold")
    ax.set_ylabel("Speedup", fontsize=13, fontweight="bold")
    ax.set_title(
        f"Insert Speedup (batch size = {batch_size}, operations = {operations})",
        fontsize=14,
        fontweight="bold",
    )
    ax.grid(True, alpha=0.3, axis="y")
    ax.legend(fontsize=10, loc="best")

    for bar, mmops in zip(bars, mmops_values):
        height = bar.get_height()
        ax.annotate(
            f"{mmops:.2f} Mops",
            xy=(bar.get_x() + bar.get_width() / 2, height),
            xytext=(0, 4),
            textcoords="offset points",
            ha="center",
            va="bottom",
            fontsize=9,
        )

    ax.margins(y=0.15)
    plt.tight_layout()
    output_path = output_dir / f"insert_speedup_bar_batch_{batch_size}_ops_{operations}.png"
    plt.savefig(output_path, dpi=150)
    plt.close()
    print(f"Saved: {output_path}")


def main():
    parser = argparse.ArgumentParser(description="Draw speedup heatmap from input file.")
    parser.add_argument(
        "--input",
        default="benchmark_result/GHC_final/insert.txt",
        help="Path to input data file",
    )
    parser.add_argument(
        "--output-dir",
        default="visualization/figures/GHC_final",
        help="Directory for generated figures",
    )
    parser.add_argument(
        "--threads",
        type=int,
        nargs="*",
        help="Thread counts to plot. Defaults to all non-baseline thread counts in the file.",
    )
    parser.add_argument(
        "--bar-batch-size",
        type=int,
        default=256,
        help="Batch size for the insert speedup bar chart.",
    )
    parser.add_argument(
        "--bar-operations",
        type=int,
        default=65536,
        help="Number of operations for the insert speedup bar chart.",
    )

    args = parser.parse_args()

    if not Path(args.input).exists():
        raise FileNotFoundError(args.input)

    data = parse_file(args.input)
    available_threads = sorted(
        {thread for batch in data.values() for ops in batch.values() for thread in ops}
    )
    threads = args.threads or [thread for thread in available_threads if thread != 1]

    for thread in threads:
        draw_heatmap(data, thread, args.output_dir)

    draw_combined_heatmaps(data, threads, args.output_dir)
    draw_speedup_bar(data, args.bar_batch_size, args.bar_operations, args.output_dir)

if __name__ == "__main__":
    main()
