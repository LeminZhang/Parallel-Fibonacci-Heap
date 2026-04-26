import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path

# Follow the paper's experiment settings
OPS_VALUES = [10000, 100000, 1000000]
THREAD_VALUES = [1, 2, 4, 8, 16]
INSERT_WORKLOADS = [
    (1.0, 0.0, 0.0),
    (0.9995, 0.0005, 0.0),
    (0.9999, 0.0001, 0.0),
    (0.99, 0.01, 0.0),
    (0.95, 0.05, 0.0),
    (0.98, 0.01, 0.01),
]
DECREASE_WORKLOAD = [
    (0.95, 0.01, 0.04),
    (0.90, 0.01, 0.09),
    (0.80, 0.01, 0.19),
]
WORKLOADS = INSERT_WORKLOADS + DECREASE_WORKLOAD
DEFAULT_SEED = 42
PERF_EVENTS = "cache-references,cache-misses,cycles,instructions"


def parse_perf_stat(stderr_text: str) -> dict[str, str]:
    metrics: dict[str, str] = {}
    for raw_line in stderr_text.splitlines():
        line = raw_line.strip()
        if not line:
            continue

        match = re.match(r"^([\d,]+)\s+([A-Za-z0-9_.-]+)(?:\s+#.*)?$", line)
        if match is None:
            continue

        value, event = match.groups()
        metrics[event] = value.replace(",", "")

    return metrics


def format_perf_metrics(metrics: dict[str, str]) -> str:
    references = metrics.get("cache-references")
    misses = metrics.get("cache-misses")

    if references is None or misses is None:
        return "cache_miss_rate_pct=NA"

    reference_count = int(references)
    miss_count = int(misses)
    if reference_count == 0:
        return "cache_miss_rate_pct=NA"

    miss_rate_pct = 100.0 * miss_count / reference_count
    return f"cache_miss_rate_pct={miss_rate_pct:.3f}"


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    benchmark_path = repo_root / "build" / "benchmark_binary.exe"
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_path = repo_root / f"result_binary_{timestamp}.txt"
    use_perf = "--use-perf" in sys.argv

    if not benchmark_path.exists():
        print(f"Benchmark executable not found: {benchmark_path}", file=sys.stderr)
        print("Build it first, e.g. make benchmark", file=sys.stderr)
        return 1

    with open(output_path, "x", encoding="utf-8"):
        pass

    for ops in OPS_VALUES:
        for threads in THREAD_VALUES:
            for insert_ratio, delete_ratio, decrease_ratio in WORKLOADS:
                print(
                    f"Running ops={ops} threads={threads} "
                    f"workload={insert_ratio}/{delete_ratio}/{decrease_ratio}"
                )

                benchmark_command = [
                    str(benchmark_path),
                    "--threads",
                    str(threads),
                    "--ops",
                    str(ops),
                    "--seed",
                    str(DEFAULT_SEED),
                    "--workload",
                    str(insert_ratio),
                    str(delete_ratio),
                    str(decrease_ratio),
                ]

                command = benchmark_command
                if use_perf:
                    command = [
                        "perf",
                        "stat",
                        "-e",
                        PERF_EVENTS,
                        *benchmark_command,
                    ]

                completed = subprocess.run(command, capture_output=True, text=True)

                if completed.returncode != 0:
                    print("stderr:", completed.stderr, file=sys.stderr)
                    continue

                line = completed.stdout.strip()
                if use_perf:
                    perf_metrics = parse_perf_stat(completed.stderr)
                    line = f"{line} {format_perf_metrics(perf_metrics)}"

                with open(output_path, "a", encoding="utf-8") as f:
                    f.write(line + "\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
