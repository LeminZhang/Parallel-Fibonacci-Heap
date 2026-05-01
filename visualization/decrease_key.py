#!/usr/bin/env python3
"""
Plot decrease_key benchmark results
Speedup vs Threads (Bar Chart)
"""

import re
import os
import tempfile
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "parallel_fib_heap_matplotlib"))
Path(os.environ["MPLCONFIGDIR"]).mkdir(parents=True, exist_ok=True)

import matplotlib.pyplot as plt
import numpy as np

def parse_decrease_key_results(filepath):
    """Parse decrease_key benchmark output file"""
    data = {
        'nodes': [],
        'threads': [],
        'time_ms': [],
        'mops': [],
        'speedup': []
    }
    
    pattern = re.compile(
        r"number of nodes=(\d+)\s+batch_size=(\d+)\s+threads=(\d+)\s+time_ms=([\d.]+)\s+Mops=([\d.]+)(?:\s+speedup=([\d.]+))?"
    )
    
    with open(filepath, 'r') as f:
        for line in f:
            match = pattern.search(line)
            if match:
                nodes = int(match.group(1))
                batch = int(match.group(2))
                threads = int(match.group(3))
                time_ms = float(match.group(4))
                mops = float(match.group(5))
                speedup = float(match.group(6)) if match.group(6) else None
                
                data['nodes'].append(nodes)
                data['threads'].append(threads)
                data['time_ms'].append(time_ms)
                data['mops'].append(mops)
                data['speedup'].append(speedup)
    
    return data

def plot_decrease_key(data, output_dir='.'):
    """Create visualization: Speedup vs Threads (Bar Chart)"""
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Group data by node size
    nodes_values = sorted(set(data['nodes']))
    thread_values = sorted(set(data['threads']))[1:]  # Exclude thread=1 (baseline)
    
    # ===== Speedup vs Threads (Bar Chart) =====
    fig, ax = plt.subplots(figsize=(12, 6))
    
    width = 0.15  # Width of each bar
    x = np.arange(len(thread_values))  # Label locations
    
    # Plot bars for each node size
    for idx, nodes in enumerate(nodes_values):
        speedups = []
        mops_values = []
        for t in thread_values:
            value = np.nan
            mops = np.nan
            for i, (n, thr) in enumerate(zip(data['nodes'], data['threads'])):
                if n == nodes and thr == t and data['speedup'][i] is not None:
                    value = data['speedup'][i]
                    mops = data['mops'][i]
                    break
            speedups.append(value)
            mops_values.append(mops)
        
        offset = width * (idx - len(nodes_values) / 2 + 0.5)
        bars = ax.bar(x + offset, speedups, width, label=f'{nodes} nodes')
        for bar, mops in zip(bars, mops_values):
            if np.isnan(mops):
                continue
            height = bar.get_height()
            ax.annotate(
                f'{mops:.1f} Mops',
                xy=(bar.get_x() + bar.get_width() / 2, height),
                xytext=(0, 3),
                textcoords='offset points',
                ha='center',
                va='bottom',
                rotation=90,
                fontsize=7,
            )
    
    # Add baseline line (speedup = 1)
    ax.axhline(y=1, color='black', linestyle='--', linewidth=2, alpha=0.6, label='Baseline (1x speedup)')
    
    ax.set_xlabel('Number of Threads', fontsize=13, fontweight='bold')
    ax.set_ylabel('Speedup', fontsize=13, fontweight='bold')
    ax.set_title('Decrease Key - Speedup vs Threads', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(thread_values)
    ax.legend(fontsize=11, loc='best')
    ax.grid(True, alpha=0.3, axis='y')
    ax.margins(y=0.2)
    plt.tight_layout()
    plt.savefig(output_dir / 'decrease_key_speedup_vs_threads.png', dpi=150)
    print(f"Saved: {output_dir / 'decrease_key_speedup_vs_threads.png'}")
    plt.close()

if __name__ == '__main__':
    import sys
    
    # Use input file from command line or default
    input_file = sys.argv[1] if len(sys.argv) > 1 else 'benchmark_result/GHC_final/decress_key.txt'
    output_dir = sys.argv[2] if len(sys.argv) > 2 else 'visualization/figures/GHC_final'
    
    if not Path(input_file).exists():
        print(f"Error: {input_file} not found")
        sys.exit(1)
    
    print(f"Parsing: {input_file}")
    data = parse_decrease_key_results(input_file)
    
    print(f"Found {len(data['nodes'])} data points")
    print(f"Nodes: {sorted(set(data['nodes']))}")
    print(f"Threads: {sorted(set(data['threads']))}")
    
    plot_decrease_key(data, output_dir)
    print("\nAll plots generated successfully!")
