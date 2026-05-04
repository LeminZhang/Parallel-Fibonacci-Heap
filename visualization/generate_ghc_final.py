#!/usr/bin/env python3
"""
Generate all plots for benchmark_result/GHC_final.
"""

import argparse
import subprocess
import sys
from pathlib import Path


def run(script, input_file, output_dir):
    subprocess.run(
        [sys.executable, str(script), str(input_file), str(output_dir)],
        check=True,
    )


def main():
    parser = argparse.ArgumentParser(description="Generate figures for GHC_final benchmark results.")
    parser.add_argument(
        "--results-dir",
        default="benchmark_result/E_core_final",
        help="Directory containing insert.txt, decress_key.txt, extract_min.txt, and dijkstra.txt",
    )
    parser.add_argument(
        "--output-dir",
        default="visualization/figures/E_core_final",
        help="Directory for generated figures",
    )
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    results_dir = Path(args.results_dir)
    output_dir = Path(args.output_dir)

    run(script_dir / "decrease_key.py", results_dir / "decress_key.txt", output_dir)
    run(script_dir / "extract_min.py", results_dir / "extract_min.txt", output_dir)
    run(script_dir / "dijkstra.py", results_dir / "dijkstra.txt", output_dir)

    subprocess.run(
        [
            sys.executable,
            str(script_dir / "batched_insert.py"),
            "--input",
            str(results_dir / "insert.txt"),
            "--output-dir",
            str(output_dir),
        ],
        check=True,
    )


if __name__ == "__main__":
    main()
