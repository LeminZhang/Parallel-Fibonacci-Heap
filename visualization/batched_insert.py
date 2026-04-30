import argparse
import re
import numpy as np
import matplotlib.pyplot as plt

def parse_file(filename):
    pattern = re.compile(
        r"operations=(\d+)\s+batch_size=(\d+)\s+threads=(\d+)\s+time_ms=([\d\.]+)"
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

            data.setdefault(batch, {})
            data[batch].setdefault(ops, {})
            data[batch][ops][threads] = time_ms

    return data

import numpy as np
import matplotlib.pyplot as plt

def draw_heatmap(data, n_threads):
    batch_sizes = sorted(data.keys())
    ops_list = sorted({ops for batch in data for ops in data[batch]})

    # 原始点坐标与 speedup 值
    points = []
    values = []

    for i, batch in enumerate(batch_sizes):
        for j, ops in enumerate(ops_list):
            if ops not in data[batch]:
                continue
            if 1 not in data[batch][ops] or n_threads not in data[batch][ops]:
                continue

            t1 = data[batch][ops][1]
            tn = data[batch][ops][n_threads]
            speedup = t1 / tn

            if speedup > 5:  # 过滤掉异常值
                continue

            points.append([i, j])
            values.append(speedup)

    points = np.array(points)
    values = np.array(values)

    # 构建完整网格
    grid_x, grid_y = np.meshgrid(
        np.arange(len(batch_sizes)),
        np.arange(len(ops_list)),
        indexing="ij"
    )

    # -----------------------------
    # 关键：二维插值（最近邻 + 平滑）
    # -----------------------------
    from scipy.interpolate import griddata

    heatmap = griddata(
        points,
        values,
        (grid_x, grid_y),
        method="linear"
    )

    # -----------------------------
    # 绘图
    # -----------------------------
    plt.figure(figsize=(10, 6))
    plt.imshow(heatmap, cmap="viridis", aspect="auto")
    plt.colorbar(label="Speedup")

    plt.xticks(range(len(ops_list)), ops_list, rotation=45)
    plt.yticks(range(len(batch_sizes)), batch_sizes)

    plt.xlabel("Number of Operations")
    plt.ylabel("Batch Size")
    plt.title(f"Speedup Heatmap (threads = {n_threads})")

    plt.tight_layout()
    plt.savefig(f"heatmap_threads_{n_threads}.png")


def main():
    parser = argparse.ArgumentParser(description="Draw speedup heatmap from input file.")
    parser.add_argument("--input", required=True, help="Path to input data file")
    parser.add_argument("--threads", type=int, choices=[2,4,8], required=True,
                        help="Which n_threads heatmap to draw")

    args = parser.parse_args()

    data = parse_file(args.input)
    draw_heatmap(data, args.threads)

if __name__ == "__main__":
    main()
