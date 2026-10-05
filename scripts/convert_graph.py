#!/usr/bin/env python3
"""Converts graphs between Matrix Market / SNAP edge lists and the binary .edges format.

  to-edges:  python3 convert_graph.py to-edges <input.mtx|input.txt> <output.edges>
             Reads a Matrix Market coordinate file (any field, general or symmetric) or a SNAP
             edge list ("u v" per line, '#' comments). Removes self-loops, direction and duplicates,
             relabels vertices to 0..n-1 (Matrix Market keeps all n rows, so isolated vertices stay as
             single-vertex components; SNAP keeps only vertices that appear in an edge) and counts the
             connected components k. Prints "n m k".
  to-mtx:    python3 convert_graph.py to-mtx <input.edges> <output.mtx>
             Writes a symmetric pattern Matrix Market file (input for baselines such as ColPack).

.edges format: int64 header (n, m, k), then m sorted pairs of uint32 (u, v) with u < v, little-endian.
"""

import struct
import sys
from array import array


def read_matrix_market(path):
    with open(path) as file:
        banner = file.readline().lower().split()
        if len(banner) < 4 or banner[0] != "%%matrixmarket" or banner[2] != "coordinate":
            raise ValueError("only Matrix Market coordinate files are supported")
        line = file.readline()
        while line.startswith("%"):
            line = file.readline()
        rows, cols, _ = map(int, line.split())
        if rows != cols:
            raise ValueError("the matrix must be square to be read as a graph")
        edges = set()
        for line in file:
            parts = line.split()
            if len(parts) < 2:
                continue
            u, v = int(parts[0]) - 1, int(parts[1]) - 1
            if u != v:
                edges.add((min(u, v), max(u, v)))
    return rows, edges


def read_snap(path):
    ids, edges = {}, set()
    with open(path) as file:
        for line in file:
            if line.startswith("#") or not line.strip():
                continue
            a, b = line.split()[:2]
            u = ids.setdefault(a, len(ids))
            v = ids.setdefault(b, len(ids))
            if u != v:
                edges.add((min(u, v), max(u, v)))
    return len(ids), edges


def count_components(n, edges):
    parent = list(range(n))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    components = n
    for u, v in edges:
        a, b = find(u), find(v)
        if a != b:
            parent[max(a, b)] = min(a, b)
            components -= 1
    return components


def to_edges(source, target):
    with open(source) as file:
        is_matrix_market = file.readline().lower().startswith("%%matrixmarket")
    n, edges = read_matrix_market(source) if is_matrix_market else read_snap(source)
    if n >= 2**32:
        raise ValueError("vertex ids must fit in uint32")
    ordered = sorted(edges)
    k = count_components(n, ordered)
    flat = array("I")
    for u, v in ordered:
        flat.append(u)
        flat.append(v)
    if sys.byteorder != "little":
        flat.byteswap()
    with open(target, "wb") as file:
        file.write(struct.pack("<qqq", n, len(ordered), k))
        flat.tofile(file)
    print(n, len(ordered), k)


def to_mtx(source, target):
    with open(source, "rb") as file:
        n, m, _ = struct.unpack("<qqq", file.read(24))
        flat = array("I")
        flat.fromfile(file, 2 * m)
    with open(target, "w") as file:
        file.write("%%MatrixMarket matrix coordinate pattern symmetric\n")
        file.write(f"{n} {n} {m}\n")
        for i in range(0, 2 * m, 2):
            # Lower triangle, 1-based, as Matrix Market expects for symmetric matrices.
            file.write(f"{flat[i + 1] + 1} {flat[i] + 1}\n")


def main():
    if len(sys.argv) != 4 or sys.argv[1] not in ("to-edges", "to-mtx"):
        print(__doc__)
        return 1
    (to_edges if sys.argv[1] == "to-edges" else to_mtx)(sys.argv[2], sys.argv[3])
    return 0


if __name__ == "__main__":
    sys.exit(main())
