#ifndef COLORING_HYBRID_H
#define COLORING_HYBRID_H

#include "coloringAlgorithms.h"
#include "../mpi/coloringMPI.h"

// The hybrid algorithm is the MPI component distribution with RSOC (OpenMP) as local coloring.
// It reuses the MPI implementation instead of keeping a copy of it.
inline ColoringResult coloringHybrid(const int &processId, const lli &n, lli **graph, ColoringResult (*coloringAlgorithm)(lli, lli**), PhaseTimes *phaseTimes = nullptr, const DistributionOptions &options = DistributionOptions(), CommVolume *volume = nullptr) {
    return coloringMPI(processId, n, graph, coloringAlgorithm, phaseTimes, options, volume);
}

#endif // COLORING_HYBRID_H
