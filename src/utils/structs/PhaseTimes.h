#ifndef PHASE_TIMES_H
#define PHASE_TIMES_H

// Wall-clock time (seconds) of each phase of Eq. (tp) in the thesis, reduced with MPI_MAX
// over all processes. Phases are separated by barriers, so their sum is the coloring time.
// Shared-memory runs only fill `coloring` (and `dsu` for the per-component variant).
struct PhaseTimes {
    double dsu = 0;      // connected components on the root (serial)
    double pack = 0;     // root copies each diagonal block into its message buffer (serial)
    double send = 0;     // MPI_Send / MPI_Recv of the buffers: pure communication
    double coloring = 0; // local coloring, slowest process
    double gather = 0;   // MPI_Reduce + MPI_Gather + MPI_Gatherv of colors and labels
};

#endif // PHASE_TIMES_H
