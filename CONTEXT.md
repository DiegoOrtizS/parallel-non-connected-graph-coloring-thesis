# Domain language

| Term | Meaning |
|------|---------|
| component ($V_i$, $n_i$, $m_i$) | connected component of the input graph: vertex set, vertex count, edge count |
| $k$ | number of components |
| diagonal block ($A_i$) | adjacency submatrix of one component; after reordering vertices by component the matrix is block diagonal |
| labels | original vertex ids of a block, sent with it so colors can be mapped back |
| root | MPI rank 0: holds the whole graph, runs DSU, packs and sends blocks, gathers colors |
| $\mathcal{C}_r$ | components assigned to rank $r$ (contiguous blocks in v1, LPT in v2) |
| $w_r$ | words sent to rank $r$: $\sum_{i \in \mathcal{C}_r} (1 + n_i + n_i^2)$ in v1 |
| phases | `dsu`, `pack`, `send`, `color`, `gather`: the terms of the thesis time model, separated by barriers |
| $p$, $h$, $c$ | MPI processes, OpenMP threads per process, total cores $c = p \cdot h$ |
| $T_1^*$ | sequential reference time: `seq-ldf-components` (same work as the parallel versions) |
| RSOC | Reduced Synchronization Optimistic Coloring (Rokos, Gorman, Kelly 2015): the OpenMP baseline |
| LDF | greedy coloring in Largest-Degree-First order: the local coloring of the MPI version |
| colors ($\hat{c}$) | colors used by a heuristic; an upper bound of the chromatic number $\chi$ |
| v1 / v2 | thesis version as analyzed / redesigned version from `docs/EXTENSIONS.md` |
| provisional data | values read off the old plots; must never be presented as measurements |

## Phases in v2 (`algorithms/mpi_v2`)

The CSV keeps the v1 columns; in v2 they mean:

| Column | v2 work |
|--------|---------|
| `t_dsu` | local DSU on the edge slice + sparse binomial-tree merge of (v, root) pairs + final labels at the root |
| `t_pack` | broadcast of component ids, `MPI_Allreduce` of edges per component, replicated LPT, bucketing of edges |
| `t_send` | `MPI_Alltoallv` of edges to component owners |
| `t_color` | CSR build + greedy LDF |
| `t_gather` | max-color reduction + `MPI_Gatherv` of vertices and colors |

Reading the edge slice and verifying the coloring are not timed.

| Term | Meaning |
|------|---------|
| `.edges` file | binary edge list: int64 header (n, m, k) + m sorted uint32 pairs (u < v) |
| VOLUME line | bytes delivered to other processes per phase, summed over ranks; deterministic, valid thesis data |
