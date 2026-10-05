#!/usr/bin/env python3
"""One summary row of the density sweep: colors of each method against the chromatic number.

Reads the CSV lines of the mpi_v2 runs (--root --blocks=complement --color=ldf|matching|cliques)
and the per-component output of coloring_quality.py --exact for one instance, and prints one CSV
row (with header when --header is given). Color counts are graph-level: the maximum over
components, as in the rest of the thesis. chi is exact when every component is solved to
optimality; otherwise chi_lower and chi_upper are its bounds.

Usage: python3 density_summary.py --density D --log runs.log --quality quality.csv [--header]
"""

import argparse
import csv
import statistics
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--density", required=True, help="target density of every component")
    parser.add_argument("--log", required=True, help="log with the CSV lines of the mpi_v2 runs")
    parser.add_argument("--quality", required=True, help="per-component CSV of coloring_quality.py")
    parser.add_argument("--header", action="store_true")
    args = parser.parse_args()

    colors = {}
    with open(args.log) as file:
        for line in file:
            fields = line.strip().split(",")
            if fields[0] == "CSV" and fields[1].startswith("mpi-v2r-complement-"):
                colors[fields[1].rsplit("-", 1)[1]] = int(fields[13])

    with open(args.quality) as file:
        rows = list(csv.DictReader(file))
    if any(not r["complement_omega"] or not r["ilp_status"] for r in rows):
        sys.exit("every component of the sweep must be dense and go through the integer program")
    sizes = [int(r["n_i"]) for r in rows]
    complement_degree = [int(r["n_i"]) - 1 - 2 * int(r["m_i"]) / int(r["n_i"]) for r in rows]

    summary = {
        "density": args.density,
        "k": len(rows),
        "n_i": max(sizes),
        "complement_avg_degree": round(statistics.mean(complement_degree), 2),
        "complement_omega": max(int(r["complement_omega"]) for r in rows),
        "complement_triangles": round(statistics.mean(int(r["complement_triangles"]) for r in rows), 1),
        "n_minus_nu": max(int(r["n_i"]) - int(r["complement_matching"]) for r in rows),
        "chi_lower": max(int(r["chi_lower"]) for r in rows),
        "chi_upper": max(int(r["chi_upper"]) for r in rows),
        "optimal_components": sum(r["ilp_status"] == "OPTIMAL" for r in rows),
        "large_classes": round(statistics.mean(int(r["ilp_large_classes"]) for r in rows), 1),
        "colors_ldf": colors["ldf"],
        "colors_dsatur": max(int(r["colors_dsatur"]) for r in rows),
        "colors_matching": colors["matching"],
        "colors_cliques": colors["cliques"],
    }
    writer = csv.DictWriter(sys.stdout, fieldnames=list(summary))
    if args.header:
        writer.writeheader()
    writer.writerow(summary)
    return 0


if __name__ == "__main__":
    sys.exit(main())
