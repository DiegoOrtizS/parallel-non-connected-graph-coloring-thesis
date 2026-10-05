#!/usr/bin/env python3
"""Coloring quality per connected component: heuristic colors versus the chromatic number.

For each component G_i the color classes are cliques of the complement H_i. Hence:

* chi(G_i) = n_i - max sum(|Q| - 1) over vertex-disjoint cliques Q of H_i with |Q| >= 2: a
  partition of V_i into cliques of H_i is a proper coloring of G_i, and every vertex outside the
  chosen cliques is a class of its own. With --exact this packing is solved as an integer program
  (OR-Tools CP-SAT) on dense components, whose complement is sparse and has few cliques. The
  problem is NP-hard in general (it contains Partition into Triangles), so the solver has a time
  limit: chi is reported exact only when CP-SAT proves optimality, and otherwise its bounds are used.
  The upper bound does not rely on the solver: the chosen cliques are checked to be disjoint
  independent sets of G_i.
* If H_i is triangle-free, the packing is a matching: chi(G_i) = n_i - nu(H_i), where nu is the
  maximum matching size.
* In general chi(G_i) >= n_i - nu(H_i) - T(H_i), with T the number of triangles of H_i: a cover by
  cliques of sizes s_j saves sum(s_j - 1) vertices; one edge per clique gives a matching, and the
  remaining sum(s_j - 2) is at most the number of triangles inside the (disjoint) cliques.
* Always chi(G_i) >= ceil(n_i / omega(H_i)) and chi(G_i) >= omega(G_i).

Complement bounds are used for dense components (density >= 0.5), where H_i is sparse; sparse
components use the clique number of G_i. chi is reported exact when lower and upper bounds meet.

Upper bounds come from greedy Largest-First and DSatur (networkx implementations).

Usage: python3 coloring_quality.py <graph.txt> [--csv out.csv] [--exact [--time-limit seconds]]
The graph file is the generator's format: a header line "n m k" followed by n rows of n 0/1 values.
"""

import argparse
import csv
import itertools
import math
import sys

import networkx as nx

# The integer program is built only when the complement has at most this many cliques of size >= 2.
MAX_CLIQUES = 100_000


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


def exact_packing(component, complement, time_limit):
    """Maximum sum(|Q| - 1) over vertex-disjoint cliques Q of the complement, solved with CP-SAT.

    Returns the bounds it gives on chi(G_i) and the solver statistics, or None when the complement
    has more than MAX_CLIQUES cliques.
    """
    from ortools.sat.python import cp_model

    cliques = []
    for clique in nx.enumerate_all_cliques(complement):
        if len(clique) < 2:
            continue
        if len(cliques) == MAX_CLIQUES:
            return None
        cliques.append(clique)

    n = component.number_of_nodes()
    model = cp_model.CpModel()
    chosen = [model.new_bool_var(f"q{j}") for j in range(len(cliques))]
    containing = {v: [] for v in component}
    for clique, variable in zip(cliques, chosen):
        for v in clique:
            containing[v].append(variable)
    for variables in containing.values():
        if len(variables) > 1:
            model.add_at_most_one(variables)
    model.maximize(sum((len(clique) - 1) * variable for clique, variable in zip(cliques, chosen)))

    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = time_limit
    solver.parameters.random_seed = 1
    solver.parameters.num_workers = 4
    status = solver.solve(model)

    # Certificate of the upper bound: disjoint cliques of H_i are disjoint independent sets of G_i.
    saved = large = 0
    covered = set()
    if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        for clique, variable in zip(cliques, chosen):
            if solver.boolean_value(variable):
                assert covered.isdisjoint(clique), "chosen cliques overlap"
                assert not any(component.has_edge(u, v) for u, v in itertools.combinations(clique, 2)), \
                    "a chosen class is not independent in G_i"
                covered.update(clique)
                saved += len(clique) - 1
                large += len(clique) >= 3
    return {
        "lower": n - math.floor(solver.best_objective_bound + 1e-9),
        "upper": n - saved,
        "status": solver.status_name(status),
        "cliques": len(cliques),
        "large_classes": large,
        "seconds": round(solver.wall_time, 3),
    }


def component_quality(component, exact=False, time_limit=60.0):
    n = component.number_of_nodes()
    m = component.number_of_edges()
    largest_first = nx.greedy_color(component, strategy="largest_first")
    dsatur = nx.greedy_color(component, strategy="DSATUR")
    colors_lf = max(largest_first.values()) + 1
    colors_dsatur = max(dsatur.values()) + 1

    upper = min(colors_lf, colors_dsatur)
    density = 2 * m / (n * (n - 1)) if n > 1 else 0.0
    triangles = matching = omega_h = packing = None
    if density >= 0.5:
        # Dense component: its complement is sparse, so cliques and matchings of H are cheap.
        complement = nx.complement(component)
        triangles = sum(nx.triangles(complement).values()) // 3
        matching = len(nx.max_weight_matching(complement, maxcardinality=True))
        omega_h = clique_number(complement) if complement.number_of_edges() else 1
        # Any clique cover saves sum(s - 1) <= nu(H) + T(H) vertices (one edge per clique forms a
        # matching; the extra s - 2 of a clique are bounded by its own triangles), for any omega(H).
        lower = max(math.ceil(n / omega_h), n - matching - triangles)
        if omega_h <= 2:
            upper = min(upper, n - matching)
        if exact:
            packing = exact_packing(component, complement, time_limit)
    else:
        # Sparse component: the complement is dense; use the clique lower bound of G itself.
        lower = clique_number(component)
    bound_lower = lower
    if packing is not None:
        lower = max(lower, packing["lower"])
        upper = min(upper, packing["upper"])
    assert lower <= upper, f"inconsistent bounds: {lower} > {upper}"

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
        "chi_exact": lower == upper,
        "bound_lower": bound_lower,
        "ilp_status": packing["status"] if packing else None,
        "ilp_cliques": packing["cliques"] if packing else None,
        "ilp_large_classes": packing["large_classes"] if packing else None,
        "ilp_seconds": packing["seconds"] if packing else None,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("graph")
    parser.add_argument("--csv", help="write one row per component to this file")
    parser.add_argument("--exact", action="store_true", help="solve the clique packing with CP-SAT")
    parser.add_argument("--time-limit", type=float, default=60.0, help="CP-SAT seconds per component")
    args = parser.parse_args()

    graph = read_graph(args.graph)
    rows = []
    for index, nodes in enumerate(sorted(nx.connected_components(graph), key=min)):
        quality = component_quality(graph.subgraph(nodes).copy(), args.exact, args.time_limit)
        rows.append({"component": index, **quality})

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
    if args.exact:
        solved = [r for r in rows if r["ilp_status"] is not None]
        optimal = sum(r["ilp_status"] == "OPTIMAL" for r in solved)
        seconds = max((r["ilp_seconds"] for r in solved), default=0)
        print(f"clique packing (CP-SAT): optimal in {optimal}/{len(solved)} dense components, at most {seconds} s each")
    print(f"chi(G) in [{chi_lower}, {chi_upper}]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
