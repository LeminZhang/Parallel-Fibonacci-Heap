# Parallel-Fibonacci-Heap
CMU 15618 Course Project (Spring 26)

## Experimental Setup

The current benchmarked implementation targets the `insert` / `deleteMin` version of the data structure. `decreaseKey` is excluded from the main experiment matrix.

### Experiment Matrix

The benchmark uses the following settings:

- operation counts:
  - `10,000`
  - `100,000`
  - `1,000,000`
- thread counts:
  - `1`
  - `2`
  - `4`
  - `8`
  - `16`
- operation mixes (`insert / deleteMin / decreaseKey`):
  - `1.0 / 0.0 / 0.0`
  - `0.9999 / 0.0001 / 0.0`
  - `0.9995 / 0.0005 / 0.0`
  - `0.99 / 0.01 / 0.0`
  - `0.95 / 0.05 / 0.0`

### Workload Generation

For each run:

- the operation trace is generated from a fixed random seed;
- keys are generated as random positive integers;
- the same seed and workload mix are used across compared implementations;
- a warmup phase inserts an initial set of keys before timed execution begins.

### Execution Method

The pre-generated trace is executed in parallel with static chunking across worker threads. This avoids adding benchmark-side dynamic scheduling overhead to the measured heap operations.

### Measured Metrics

Each run records the following metrics:

- `time_ms`
- `throughput_ops_per_sec`
- `executed_ops`
- `insert_ops`
- `delete_ops`

To track the quality of relaxed `deleteMin`, the benchmark also records:

- `delete_value_sum`
- `avg_deleted_value`

### Compared Implementations

The main experiments compare:

- coarse-grained synchronized Fibonacci heap
- fine-grained synchronized Fibonacci heap
- binary heap baseline

All three implementations are evaluated under the same operation counts, thread counts, seeds, and workload mixes.
