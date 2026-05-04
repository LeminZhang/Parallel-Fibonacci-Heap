#!/usr/bin/env python3
"""
Generate P-core vs E-core comparison figures.
"""

import argparse
import os
import tempfile
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "parallel_fib_heap_matplotlib"))
Path(os.environ["MPLCONFIGDIR"]).mkdir(parents=True, exist_ok=True)

import matplotlib.pyplot as plt
import numpy as np

from batched_insert import build_heatmap, parse_file as parse_insert_results
from decrease_key import parse_decrease_key_results
from extract_min import parse_extract_min_results


def non_baseline_threads(data):
    return sorted(thread for thread in set(data["threads"]) if thread != 1)


def speedup_for_record(data, size_key, size_value, thread):
    baseline = None
    current = None
    for i, (size, current_thread) in enumerate(zip(data[size_key], data["threads"])):
        if size != size_value:
            continue
        if current_thread == 1:
            baseline = data["time_ms"][i]
        if current_thread == thread:
            current = i

    if current is None:
        return np.nan
    if data["speedup"][current] is not None:
        return data["speedup"][current]
    if baseline:
        return baseline / data["time_ms"][current]
    return np.nan


def draw_insert_heatmap_comparison(e_insert, p_insert, output_dir):
    output_dir.mkdir(parents=True, exist_ok=True)

    datasets = [
        ("E-core", e_insert, [2, 4, 6, 8]),
        ("P-core", p_insert, [3, 6, 9, 12]),
    ]
    heatmaps = []
    batch_sizes = []
    ops_list = []

    for label, data, threads in datasets:
        for thread in threads:
            heatmap, current_batch_sizes, current_ops_list = build_heatmap(data, thread)
            heatmaps.append((label, thread, heatmap))
            batch_sizes = current_batch_sizes
            ops_list = current_ops_list

    valid_values = [
        heatmap[~np.isnan(heatmap)]
        for _, _, heatmap in heatmaps
        if not np.all(np.isnan(heatmap))
    ]
    if not valid_values:
        print("No insert heatmap data for P/E comparison")
        return

    all_values = np.concatenate(valid_values)
    vmin = np.min(all_values)
    vmax = np.max(all_values)

    fig, axes = plt.subplots(2, 4, figsize=(24, 12), squeeze=False)
    cmap = plt.cm.viridis.copy()
    cmap.set_bad(color="lightgray")
    image = None

    for idx, (label, thread, heatmap) in enumerate(heatmaps):
        row = idx // 4
        col = idx % 4
        ax = axes[row][col]
        image = ax.imshow(heatmap, cmap=cmap, aspect="auto", origin="lower", vmin=vmin, vmax=vmax)
        ax.set_title(f"{label}, threads = {thread}", fontsize=13, fontweight="bold")
        ax.set_xticks(range(len(ops_list)))
        ax.set_xticklabels(ops_list, rotation=45, ha="right", fontsize=7)
        ax.set_yticks(range(len(batch_sizes)))
        ax.set_yticklabels(batch_sizes, fontsize=7)

        if row == 1:
            ax.set_xlabel("Number of Operations", fontsize=11, fontweight="bold")
        if col == 0:
            ax.set_ylabel("Batch Size", fontsize=11, fontweight="bold")

    fig.suptitle("Insert Speedup Heatmap: E-core vs P-core", fontsize=18, fontweight="bold")
    fig.tight_layout(rect=(0, 0, 0.94, 0.96))
    cbar_ax = fig.add_axes((0.955, 0.12, 0.015, 0.76))
    fig.colorbar(image, cax=cbar_ax, label="Speedup")

    output_path = output_dir / "insert_heatmap_p_vs_e.png"
    fig.savefig(output_path, dpi=150)
    plt.close(fig)
    print(f"Saved: {output_path}")


def draw_insert_speedup_comparison(e_insert, p_insert, output_dir, batch_size, operations):
    output_dir.mkdir(parents=True, exist_ok=True)

    fig, ax = plt.subplots(figsize=(10, 6))
    for label, data, marker in [("E-core", e_insert, "o"), ("P-core", p_insert, "s")]:
        if batch_size not in data or operations not in data[batch_size]:
            print(f"No {label} insert data for batch_size={batch_size}, operations={operations}")
            continue

        row = data[batch_size][operations]
        threads = sorted(row)
        baseline = row.get(1, {}).get("time_ms")
        speedups = []
        mops_values = []
        for thread in threads:
            item = row[thread]
            speedup = item["speedup"]
            if speedup is None and baseline:
                speedup = baseline / item["time_ms"]
            speedups.append(speedup)
            mops_values.append(item["mmops"])

        ax.plot(threads, speedups, marker=marker, linewidth=2, label=label)
        for thread, speedup, mops in zip(threads, speedups, mops_values):
            ax.annotate(
                f"{mops:.1f} Mops",
                xy=(thread, speedup),
                xytext=(0, 7),
                textcoords="offset points",
                ha="center",
                va="bottom",
                fontsize=8,
            )

    ax.axhline(y=1, color="black", linestyle="--", linewidth=1.5, alpha=0.6)
    ax.set_xlabel("Number of Threads", fontsize=13, fontweight="bold")
    ax.set_ylabel("Speedup", fontsize=13, fontweight="bold")
    ax.set_title(
        f"Insert Speedup: E-core vs P-core\nbatch size = {batch_size}, operations = {operations}",
        fontsize=14,
        fontweight="bold",
    )
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=11)
    ax.margins(y=0.18)
    plt.tight_layout()

    output_path = output_dir / f"insert_speedup_p_vs_e_batch_{batch_size}_ops_{operations}.png"
    fig.savefig(output_path, dpi=150)
    plt.close(fig)
    print(f"Saved: {output_path}")


def draw_operation_speedup_comparison(e_data, p_data, output_dir, name, size_key, title, output_name):
    output_dir.mkdir(parents=True, exist_ok=True)

    sizes = sorted(set(e_data[size_key]) & set(p_data[size_key]))
    if not sizes:
        print(f"No common sizes for {name} comparison")
        return

    n_cols = min(3, len(sizes))
    n_rows = int(np.ceil(len(sizes) / n_cols))
    fig, axes = plt.subplots(n_rows, n_cols, figsize=(6 * n_cols, 4.8 * n_rows), squeeze=False)

    for idx, size in enumerate(sizes):
        ax = axes[idx // n_cols][idx % n_cols]
        for label, data, marker in [("E-core", e_data, "o"), ("P-core", p_data, "s")]:
            threads = non_baseline_threads(data)
            speedups = [speedup_for_record(data, size_key, size, thread) for thread in threads]
            ax.plot(threads, speedups, marker=marker, linewidth=2, label=label)

        ax.axhline(y=1, color="black", linestyle="--", linewidth=1.2, alpha=0.6)
        ax.set_title(f"{size} nodes", fontsize=12, fontweight="bold")
        ax.set_xlabel("Number of Threads")
        ax.set_ylabel("Speedup")
        ax.grid(True, alpha=0.3)
        ax.legend(fontsize=9)

    for idx in range(len(sizes), n_rows * n_cols):
        axes[idx // n_cols][idx % n_cols].axis("off")

    fig.suptitle(title, fontsize=16, fontweight="bold")
    fig.tight_layout(rect=(0, 0, 1, 0.95))

    output_path = output_dir / output_name
    fig.savefig(output_path, dpi=150)
    plt.close(fig)
    print(f"Saved: {output_path}")


def main():
    parser = argparse.ArgumentParser(description="Generate P-core vs E-core comparison figures.")
    parser.add_argument("--e-dir", default="benchmark_result/E_core_final", help="E-core benchmark result directory")
    parser.add_argument("--p-dir", default="benchmark_result/P_core_final", help="P-core benchmark result directory")
    parser.add_argument("--output-dir", default="visualization/figures/P_E_comparison", help="Output figure directory")
    parser.add_argument("--insert-batch-size", type=int, default=256, help="Batch size for insert speedup comparison")
    parser.add_argument("--insert-operations", type=int, default=65536, help="Operations for insert speedup comparison")
    args = parser.parse_args()

    e_dir = Path(args.e_dir)
    p_dir = Path(args.p_dir)
    output_dir = Path(args.output_dir)

    e_insert = parse_insert_results(e_dir / "insert.txt")
    p_insert = parse_insert_results(p_dir / "insert.txt")
    e_extract = parse_extract_min_results(e_dir / "extract_min.txt")
    p_extract = parse_extract_min_results(p_dir / "extract_min.txt")
    e_decrease = parse_decrease_key_results(e_dir / "decress_key.txt")
    p_decrease = parse_decrease_key_results(p_dir / "decress_key.txt")

    draw_insert_heatmap_comparison(e_insert, p_insert, output_dir)
    draw_insert_speedup_comparison(e_insert, p_insert, output_dir, args.insert_batch_size, args.insert_operations)
    draw_operation_speedup_comparison(
        e_extract,
        p_extract,
        output_dir,
        "deleteMin",
        "nodes",
        "DeleteMin Speedup: E-core vs P-core",
        "delete_min_speedup_p_vs_e.png",
    )
    draw_operation_speedup_comparison(
        e_decrease,
        p_decrease,
        output_dir,
        "decreaseKey",
        "nodes",
        "DecreaseKey Speedup: E-core vs P-core",
        "decrease_key_speedup_p_vs_e.png",
    )


if __name__ == "__main__":
    main()
