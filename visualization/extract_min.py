#!/usr/bin/env python3
"""
Plot extract_min benchmark results
"""

import re
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

def parse_extract_min_results(filepath):
    """Parse extract_min benchmark output file"""
    data = {
        'ops': [],
        'threads': [],
        'time_ms': [],
        'speedup': []
    }
    
    pattern = re.compile(
        r"operations=(\d+)\s+batch_size=(\d+)\s+threads=(\d+)\s+time_ms=([\d\.]+)(?:\s+speedup=([\d\.]+))?"
    )
    
    with open(filepath, 'r') as f:
        for line in f:
            match = pattern.search(line)
            if match:
                ops = int(match.group(1))
                batch = int(match.group(2))
                threads = int(match.group(3))
                time_ms = float(match.group(4))
                speedup = float(match.group(5)) if match.group(5) else None
                
                data['ops'].append(ops)
                data['threads'].append(threads)
                data['time_ms'].append(time_ms)
                data['speedup'].append(speedup)
    
    return data

def plot_extract_min(data, output_dir='.'):
    """Create visualization: Speedup vs Threads"""
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True)
    
    # Group data by operation size
    ops_values = sorted(set(data['ops']))
    thread_values = sorted(set(data['threads']))
    
    # ===== Speedup vs Threads =====
    fig, ax = plt.subplots(figsize=(10, 6))
    
    # Get speedup for each operation size
    for op in ops_values:
        speedups_for_op = []
        threads_for_op = []
        for i, (o, t) in enumerate(zip(data['ops'], data['threads'])):
            if o == op and data['speedup'][i] is not None:
                threads_for_op.append(t)
                speedups_for_op.append(data['speedup'][i])
        
        if speedups_for_op:
            ax.plot(threads_for_op, speedups_for_op, marker='o', label=f'{op} ops', linewidth=2.5, markersize=8)
    
    # Add ideal speedup line
    ideal_threads = sorted(set(data['threads']))
    ax.plot(ideal_threads, ideal_threads, '--', label='Ideal speedup', color='black', linewidth=2.5, alpha=0.6)
    
    ax.set_xlabel('Number of Threads', fontsize=13, fontweight='bold')
    ax.set_ylabel('Speedup', fontsize=13, fontweight='bold')
    ax.set_title('Extract Min - Speedup vs Threads', fontsize=14, fontweight='bold')
    ax.set_xticks(thread_values)
    ax.legend(fontsize=11, loc='best')
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_dir / 'extract_min_speedup_vs_threads.png', dpi=150)
    print(f"✓ Saved: {output_dir / 'extract_min_speedup_vs_threads.png'}")
    plt.close()

if __name__ == '__main__':
    import sys
    
    # Use input file from command line or default
    input_file = sys.argv[1] if len(sys.argv) > 1 else 'benchmark_result/extract_min.txt'
    output_dir = sys.argv[2] if len(sys.argv) > 2 else 'visualization/'
    
    if not Path(input_file).exists():
        print(f"Error: {input_file} not found")
        sys.exit(1)
    
    print(f"Parsing: {input_file}")
    data = parse_extract_min_results(input_file)
    
    print(f"Found {len(data['ops'])} data points")
    print(f"Operations: {sorted(set(data['ops']))}")
    print(f"Threads: {sorted(set(data['threads']))}")
    
    plot_extract_min(data, output_dir)
    print("\n✓ All plots generated successfully!")
