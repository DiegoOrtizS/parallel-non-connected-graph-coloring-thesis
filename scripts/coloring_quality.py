#!/usr/bin/env python3
"""Coloring quality per connected component: heuristic colors versus the chromatic number.

For each component G_i the color classes are cliques of the complement H_i. Hence:

* If H_i is triangle-free, chi(G_i) = n_i - nu(H_i), where nu is the maximum matching size (exact).
* If omega(H_i) = 3, a clique cover uses a triangles and b edges with a + b <= nu(H_i), so
  chi(G_i) >= n_i - nu(H_i) - t(H_i), with t the number of triangles.
* Always chi(G_i) >= ceil(n_i / omega(H_i)) and chi(G_i) >= omega(G_i).

Complement bounds are used for dense components (density >= 0.5), where H_i is sparse; sparse
components use the clique number of G_i. chi is reported exact when lower and upper bounds meet.

Upper bounds come from greedy Largest-First and DSatur (networkx implementations).

Usage: python3 coloring_quality.py <graph.txt> [--csv out.csv]
The graph file is the generator's format: a header line "n m k" followed by n rows of n 0/1 values.
"""

import argparse
import csv
import math
import sys

import networkx as nx


def read_graph(path):
    graph = nx.Graph()
    with open(path) as file:
        n, _, _ = map(int, file.readline().split())
        graph.add_nodes_from(range(n))
        for i in range(n):
            row = file.readline().split()
            graph.add_edges_from((i, j) for j in range(i + 1, n) if row[j] == "1")
    return graph


def clique_number(graph):
    return max((len(c) for c in nx.find_cliques(graph)), default=1)


def component_quality(component):
    n = component.number_of_nodes()
    m = component.number_of_edges()
    largest_first = nx.greedy_color(component, strategy="largest_first")
    dsatur = nx.greedy_color(component, strategy="DSATUR")
    colors_lf = max(largest_first.values()) + 1
    colors_dsatur = max(dsatur.values()) + 1

    upper = min(colors_lf, colors_dsatur)
    density = 2 * m / (n * (n - 1)) if n > 1 else 0.0
    triangles = matching = omega_h = None
    if density >= 0.5:
        # Dense component: its complement is sparse, so cliques and matchings of H are cheap.
        complement = nx.complement(component)
        triangles = sum(nx.triangles(complement).values()) // 3
        matching = len(nx.max_weight_matching(complement, maxcardinality=True))
        omega_h = clique_number(complement) if complement.number_of_edges() else 1
        lower = math.ceil(n / omega_h)
        if omega_h <= 2:
            lower = max(lower, n - matching)
            upper = min(upper, n - matching)
        elif omega_h == 3:
            lower = max(lower, n - matching - triangles)
    else:
        # Sparse component: the complement is dense; use the clique lower bound of G itself.
        lower = clique_number(component)
    exact = lower == upper

    return {
        "n_i": n,
        "m_i": m,
        "density": round(density, 4),
        "colors_lf": colors_lf,
        "colors_dsatur": colors_dsatur,
        "complement_triangles": triangles,
        "complement_matching": matching,
        "complement_omega": omega_h,
        "chi_lower": lower,
        "chi_upper": upper,
        "chi_exact": exact,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("graph")
    parser.add_argument("--csv", help="write one row per component to this file")
    args = parser.parse_args()

    graph = read_graph(args.graph)
    rows = []
    for index, nodes in enumerate(sorted(nx.connected_components(graph), key=min)):
        row = {"component": index, **component_quality(graph.subgraph(nodes).copy())}
        rows.append(row)

    if args.csv:
        with open(args.csv, "w", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)

    chi_lower = max(r["chi_lower"] for r in rows)
    chi_upper = max(r["chi_upper"] for r in rows)
    exact = sum(r["chi_exact"] for r in rows)
    print(f"components: {len(rows)}, exact chi in {exact}")
    print(f"colors LF: {max(r['colors_lf'] for r in rows)}, DSatur: {max(r['colors_dsatur'] for r in rows)}")
    print(f"chi(G) in [{chi_lower}, {chi_upper}]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
