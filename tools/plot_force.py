"""Plot the Rankine force and depth response for every soil preset."""

import argparse
import json
import math
from pathlib import Path

import matplotlib.pyplot as plt


def force(soil, depth, width):
    kp = math.tan(math.pi / 4.0 + math.radians(soil["friction_angle_deg"]) / 2.0) ** 2
    return width * (
        0.5 * soil["unit_weight_N_m3"] * depth * depth * kp
        + 2.0 * soil["cohesion_kPa"] * 1000.0 * depth * math.sqrt(kp)
    )


def solve_depth(soil, applied_force, width, max_depth=2.0):
    lower, upper = 0.0, max_depth
    if force(soil, upper, width) <= applied_force:
        return upper
    for _ in range(80):
        middle = (lower + upper) / 2.0
        if force(soil, middle, width) < applied_force:
            lower = middle
        else:
            upper = middle
    return (lower + upper) / 2.0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--soils", default="data/soils.json")
    parser.add_argument("--out", default="docs")
    parser.add_argument("--force", type=float, default=5000.0)
    args = parser.parse_args()

    with Path(args.soils).open(encoding="utf-8") as stream:
        soils = json.load(stream)["soils"]
    output = Path(args.out)
    output.mkdir(parents=True, exist_ok=True)

    depths = [index * 0.01 for index in range(201)]
    plt.figure()
    for soil in soils:
        plt.plot(depths, [force(soil, depth, 0.5) for depth in depths], label=soil["name"])
    plt.xlabel("Depth (m)")
    plt.ylabel("Cutting force (N)")
    plt.title("Rankine cutting force vs depth")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(output / "force_vs_depth.png", dpi=150)
    plt.close()

    names = [soil["name"] for soil in soils]
    reached = [solve_depth(soil, args.force, 0.5) for soil in soils]
    plt.figure()
    plt.bar(names, reached)
    plt.xlabel("Soil preset")
    plt.ylabel("Depth reached (m)")
    plt.title(f"Depth reached at {args.force:g} N")
    plt.xticks(rotation=25, ha="right")
    plt.tight_layout()
    plt.savefig(output / "depth_at_fixed_force.png", dpi=150)
    plt.close()


if __name__ == "__main__":
    main()
