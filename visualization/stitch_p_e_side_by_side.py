#!/usr/bin/env python3
"""
Place existing E-core and P-core figures side by side without replotting them.
"""

import argparse
from pathlib import Path

from PIL import Image


FIGURES = {
    "delete_min_side_by_side.png": "extract_min_speedup_vs_threads.png",
    "insert_side_by_side.png": "insert_speedup_bar_batch_256_ops_65536.png",
    "decrease_key_side_by_side.png": "decrease_key_speedup_vs_threads.png",
}

GIANT_FIGURE_ORDER = [
    "insert_side_by_side.png",
    "delete_min_side_by_side.png",
    "decrease_key_side_by_side.png",
]


def stitch(left_path, right_path, output_path):
    left = Image.open(left_path).convert("RGBA")
    right = Image.open(right_path).convert("RGBA")

    height = max(left.height, right.height)
    width = left.width + right.width
    canvas = Image.new("RGBA", (width, height), "white")
    canvas.paste(left, (0, (height - left.height) // 2))
    canvas.paste(right, (left.width, (height - right.height) // 2))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(output_path)
    print(f"Saved: {output_path}")


def stack_vertically(input_paths, output_path):
    images = [Image.open(path).convert("RGBA") for path in input_paths]
    width = max(image.width for image in images)
    height = sum(image.height for image in images)
    canvas = Image.new("RGBA", (width, height), "white")

    y = 0
    for image in images:
        canvas.paste(image, ((width - image.width) // 2, y))
        y += image.height

    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(output_path)
    print(f"Saved: {output_path}")


def main():
    parser = argparse.ArgumentParser(description="Stitch existing E/P core figures side by side.")
    parser.add_argument("--e-dir", default="visualization/figures/E_core_final")
    parser.add_argument("--p-dir", default="visualization/figures/P_core_final")
    parser.add_argument("--output-dir", default="visualization/figures/P_E_side_by_side")
    args = parser.parse_args()

    e_dir = Path(args.e_dir)
    p_dir = Path(args.p_dir)
    output_dir = Path(args.output_dir)

    for output_name, figure_name in FIGURES.items():
        e_path = e_dir / figure_name
        p_path = p_dir / figure_name
        if not e_path.exists():
            raise FileNotFoundError(e_path)
        if not p_path.exists():
            raise FileNotFoundError(p_path)

        stitch(e_path, p_path, output_dir / output_name)

    stack_vertically(
        [output_dir / name for name in GIANT_FIGURE_ORDER],
        output_dir / "insert_delete_min_decrease_key_side_by_side.png",
    )


if __name__ == "__main__":
    main()
