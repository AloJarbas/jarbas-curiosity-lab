#!/usr/bin/env python3
from __future__ import annotations

import argparse
from statistics import mean

PALETTE = " .:-=+*#%@"


def iterate_logistic(r: float, x0: float, warmup: int, keep: int) -> list[float]:
    x = x0
    for _ in range(warmup):
        x = r * x * (1.0 - x)
    values = []
    for _ in range(keep):
        x = r * x * (1.0 - x)
        values.append(x)
    return values


def sparkline(values: list[float], width: int) -> str:
    buckets = [0] * width
    for value in values:
        idx = min(width - 1, int(value * width))
        buckets[idx] += 1
    peak = max(buckets) or 1
    return "".join(PALETTE[min(len(PALETTE) - 1, int(count / peak * (len(PALETTE) - 1)))] for count in buckets)


def main() -> None:
    parser = argparse.ArgumentParser(description="ASCII logistic-map explorer")
    parser.add_argument("--r-min", type=float, default=3.5)
    parser.add_argument("--r-max", type=float, default=4.0)
    parser.add_argument("--rows", type=int, default=18)
    parser.add_argument("--cols", type=int, default=72)
    parser.add_argument("--warmup", type=int, default=400)
    parser.add_argument("--keep", type=int, default=120)
    parser.add_argument("--x0", type=float, default=0.217)
    args = parser.parse_args()

    print("r        mean(x)  spread   density")
    print("-" * (26 + args.cols))
    for row in range(args.rows):
        r = args.r_min + (args.r_max - args.r_min) * row / max(1, args.rows - 1)
        values = iterate_logistic(r, args.x0, args.warmup, args.keep)
        avg = mean(values)
        spread = max(values) - min(values)
        density = sparkline(values, args.cols)
        print(f"{r:0.5f}  {avg:0.5f}  {spread:0.5f}  {density}")


if __name__ == "__main__":
    main()
