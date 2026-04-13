import sys
import subprocess
from pathlib import Path

# Follow the paper's experiment settings
OPS_VALUES = [10000, 100000, 1000000]
THREAD_VALUES = [2, 4, 8, 16]
WORKLOADS = [
    (0.5, 0.5, 0.0),
    (0.7, 0.3, 0.0),
    (0.99, 0.01, 0.0),
]
DEFAULT_SEED = 42

def main() -> int:
    f = open("result.txt", "x")
    repo_root = Path(__file__).resolve().parents[1]
    benchmark_path = repo_root/"build"/"benchmark.exe"

    if not benchmark_path.exists():
        print(f"Benchmark executable not found: {benchmark_path}", file=sys.stderr)
        print("Build it first, e.g. make benchmark", file=sys.stderr)
        return 1

    for ops in OPS_VALUES:
        for threads in THREAD_VALUES:
            for insert_ratio, delete_ratio, decrease_ratio in WORKLOADS:
                print(
                    f"Running ops={ops} threads={threads} "
                    f"workload={insert_ratio}/{delete_ratio}/{decrease_ratio}"
                )
                command = [ benchmark_path, "--impl", "coarse", "--threads", str(threads), "--ops", str(ops), "--seed", str(DEFAULT_SEED), "--workload", str(insert_ratio), str(delete_ratio), str(decrease_ratio)]
                completed = subprocess.run(command, capture_output=True, text=True)
                
                if completed.returncode != 0:
                    print("stderr:", completed.stderr)
                else:
                    with open("result.txt", "a") as f:
                        f.write(completed.stdout)

    f.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
