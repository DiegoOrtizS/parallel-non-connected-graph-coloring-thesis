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

## Next

1. Milestone B1c: `--scatterv`, and bitmap blocks in the root-based path.
2. Milestone C (needs Khipu): timing campaigns for v1, replicate, v2; ping-pong for alpha and beta; validation of the time model.
3. Out of scope until Khipu or a newer MPI is available: MPI 4 partitioned communication (`MPI_Psend_init` is missing in the Open MPI 4.1 of ubuntu-latest), Kokkos/GPU.
