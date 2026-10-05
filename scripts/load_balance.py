#!/usr/bin/env python3
"""Load balance of coloring by components: how much speedup the component granularity allows.

Coloring by components gives whole components to processes. With work w_i per component and
W = sum w_i, the coloring phase on p processes takes at least max(W / p, w_max), so its speedup is
at most min(p, W / w_max): one component with more than W / p of the work caps it below p.

For each graph and cost model this script reports, per number of processes p:
* the ceiling W / w_max, which holds for every p;
* the speedup W / makespan of the contiguous assignment (v1 default: components in order of their
  smallest vertex, in blocks of floor(k/p) or ceil(k/p) components);
* the speedup W / makespan of LPT (largest estimated cost first, to the least loaded process, ties
  to the lowest rank), whose makespan is at most 4/3 - 1/(3p) times the optimum (Graham 1969).

Cost models: "matrix", w_i = n_i^2 (v1 colors blocks of the adjacency matrix, and its LPT estimates
n_i^2); "csr", w_i = n_i + m_i (v2 colors on CSR, and its LPT estimates n_i + m_i). The model
counts operations, not seconds: it bounds how much of the coloring can run in parallel.

Usage: python3 load_balance.py NAME=FILE.edges [NAME=FILE.edges ...] [--p 2,4,8,16,32] [--csv out.csv]
FILE.edges is the binary edge list of the v2 programs: int64 n, m, k, then m uint32 pairs (u, v).
"""

import argparse
import array
import csv
import heapq
import struct
import sys


def read_components(path):
    """(n_i, m_i) of every component, in order of its smallest vertex."""
    with open(path, "rb") as file:
        n, m, _ = struct.unpack("<qqq", file.read(24))
        pairs = array.array("I")
        pairs.frombytes(file.read(8 * m))
    if sys.byteorder != "little":
        pairs.byteswap()

    parent = list(range(n))

    def find(v):
        while parent[v] != v:
            parent[v] = parent[parent[v]]  # path halving
            v = parent[v]
        return v

    for j in range(0, 2 * m, 2):
        a, b = find(pairs[j]), find(pairs[j + 1])
        if a != b:
            parent[max(a, b)] = min(a, b)

    index = {}
    sizes = []
    for v in range(n):  # v ascending, so components are numbered by their smallest vertex
        root = find(v)
        if root not in index:
            index[root] = len(sizes)
            sizes.append([0, 0])
        sizes[index[root]][0] += 1
    for j in range(0, 2 * m, 2):
        sizes[index[find(pairs[j])]][1] += 1
    return [tuple(s) for s in sizes]


def contiguous_makespan(work, p):
    per, extra = divmod(len(work), p)
    makespan, start = 0, 0
    for r in range(p):
        count = per + (1 if r < extra else 0)
        makespan = max(makespan, sum(work[start:start + count]))
        start += count
    return makespan


def lpt_makespan(work, p):
    order = sorted(range(len(work)), key=lambda c: -work[c])  # stable, like std::stable_sort
    loads = [(0, r) for r in range(p)]
    for c in order:
        load, r = heapq.heappop(loads)
        heapq.heappush(loads, (load + work[c], r))
    return max(load for load, _ in loads)


COST = {
    "matrix": lambda n_i, m_i: n_i * n_i,
    "csr": lambda n_i, m_i: n_i + m_i,
}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("graphs", nargs="+", help="NAME=FILE.edges")
    parser.add_argument("--p", default="2,4,8,16,32", help="comma-separated process counts")
    parser.add_argument("--csv", help="write the table to this file")
    args = parser.parse_args()
    processes = [int(p) for p in args.p.split(",")]

    rows = []
    for item in args.graphs:
        name, path = item.split("=", 1)
        components = read_components(path)
        largest = max(components)
        print(f"{name}: k={len(components)}, largest component n_i={largest[0]} m_i={largest[1]}")
        for cost, model in COST.items():
            work = [model(n_i, m_i) for n_i, m_i in components]
            total, heaviest = sum(work), max(work)
            for p in processes:
                rows.append({
                    "graph": name,
                    "cost": cost,
                    "k": len(components),
                    "p": p,
                    "ceiling": round(total / heaviest, 3),
                    "bound": round(min(p, total / heaviest), 3),
                    "speedup_contiguous": round(total / contiguous_makespan(work, p), 3),
                    "speedup_lpt": round(total / lpt_makespan(work, p), 3),
                })

    outputs = [sys.stdout]
    if args.csv:
        outputs.append(open(args.csv, "w", newline=""))
    for output in outputs:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    if args.csv:
        outputs[1].close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
