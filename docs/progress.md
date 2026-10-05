# Progress

## State (2026-10-05)

Stacked draft PRs, all CI green:

1. #2 `fix/experiments` → `main`: measurement fixes, instrumentation, milestone A (variants, LPT, replicate, ping-pong, coloring quality).
2. #3 `feat/edge-list` → #2: binary edge-list format; deterministic jngen seed (instances are now reproducible); both formats byte-identical.
3. #4 `feat/root-free` → #3: root-free v2 (`algorithms/mpi_v2`), communication-volume accounting, CI job `communication-volume`.

Results already valid for the thesis (deterministic, from CI):

- chi(G1) = 52 exactly, LDF optimal; chi(G2) in [105, 150].
- Communication volume, p = 4: replication / components = 63.4 (G1) and 127.7 (G2), matching the model p*k.
- v2 sends about half of v1 on dense components.

## Next (after B1c, PR #6)

1. Milestone B1c: `--scatterv`, and bitmap blocks in the root-based path.
2. Milestone C (needs Khipu): timing campaigns for v1, replicate, v2; ping-pong for alpha and beta; validation of the time model.
3. Out of scope until Khipu or a newer MPI is available: MPI 4 partitioned communication (`MPI_Psend_init` is missing in the Open MPI 4.1 of ubuntu-latest), Kokkos/GPU.

## B1c done (PR #6, CI green)

- `mpi_v2 --root` (bitmap / edge blocks, `--scatterv`), `scripts/convert_graph.py`, ColPack baseline.
- ca-GrQc (SNAP): n = 5242, m = 14484, k = 355; chi = omega = 44, LDF optimal; ColPack agrees.
- Remaining before Khipu: ConnectIt or FastSV as connectivity baseline.

## Complement representation (PR #7, CI green)

- Blocks: edges / bitmap / complement with 16-bit indices; auto by exact bytes (thresholds 1/16 and 15/16).
- LDF on the complement (same colors as on G), matching coloring (Karp-Sipser), best of both.
- G1 / G2, p = 4: complement sends 5.4 kB / 203 kB vs 379 kB / 18.8 MB for the v1 matrix.

## Exact chromatic number, clique coloring and load balance (PRs #8, #9, CI green)

- coloring_quality.py --exact: clique packing on the complement with CP-SAT; chi(G1) = 52, chi(G2) = 128 (32/32 optimal, < 1.4 s each).
- --color=cliques: triangles of the complement, then Karp-Sipser; G2: 132 colors vs LDF 154.
- load_balance.py: speedup ceiling W / w_max; Zipf s = 1: 1.60, ca-GrQc: 1.12 (CSR); LPT reaches min(p, W / w_max).

## Density sweep (PR #10, CI green, run 37347320522)

- quality-vs-density: 8 components of 312 vertices per density, 0.85 to 0.995, against chi from CP-SAT (300 s per component).
- LDF overhead grows with the complement average degree: +4.8 % (3.1), +29 % (15.6), up to +43 % (46.7, against the lower bound).
- --color=cliques now packs cliques of any size, larger first: within 1.5 % up to degree 9.3, at most 14 % at 46.7; the triangle-only version fell to 108 colors (worse than LDF) at 46.7.
- CP-SAT proves optimality up to average degree ~15 (n_i = 312).

## Local search, complement methods on bitmap blocks, component baselines (PRs #11, #12, CI green)

- --color=cliques: local search on the packing (accepted only when the color count falls; deterministic work cap). Sweep run 37356235196: within 1 % up to complement degree 9.3, 5.5 % at 15.6, at most 13 % at 62.2 (D = 0.80); LDF up to 47 %.
- matching, cliques and best on bitmap blocks of density >= 1/2 (complement built at the owner); best never worse than LDF. LDF colors identical to the forced-complement run.
- components-baselines (run 37357917571): FastSV, LACC, ConnectIt and SDB14 find the same k as mpi_v2 on G2, Zipf G2, a 10^6-vertex permuted graph and ca-GrQc. No timings.
