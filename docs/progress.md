# Progress

## State (2026-10-05)

- PR #2 (`fix/experiments`, draft): measurement fixes, per-phase instrumentation, CI green.
- Milestone A done in code, CI green:
  - generator variants (`GRAPH_VARIANT`: Zipf sizes, permuted labels),
  - `--lpt` and `--replicate` in MPI and hybrid,
  - ping-pong benchmark,
  - coloring quality with exact chi via complement matching (CI job `coloring-quality` on G1, G2, G3),
  - top-level `Makefile` (`build`, `smoke`, `quality`, `results`).
- First real result: chi(G1) = 53 exactly and LDF is optimal on G1; chi(G2) in [105, 150].

## Next

1. Diego requests Khipu access; run the v1 campaign plus `--replicate` (H1) and ping-pong (alpha, beta).
2. Milestone B: edge-list format + CSR, density-based block representation, root-free distributed DSU (review SiskinCC/RobinCC first), MPI 4 partitioned communication behind a flag.
3. After the campaign: validate the cost model and write the "Redesign" subsection of the thesis.
