#ifndef COLORING_OMP_COMPONENTS_H
#define COLORING_OMP_COMPONENTS_H

#include <omp.h>
#include <vector>
#include "../../utils/types.h"
#include "../../utils/structs/ColoringResult.h"
#include "../../utils/structs/PhaseTimes.h"
#include "../mpi/connectedComponents.h"
#include "../mpi/coloringAlgorithms.h"

// Shared-memory counterpart of the component-based proposal: same DSU and LDF as the MPI version,
// but components are colored by threads of one process, so there is no packing or communication.
// It isolates the effect of the memory paradigm from the n^2 vs sum(n_i^2) work difference
// between RSOC on the whole graph and the component-based algorithms.
ColoringResult coloringOMPComponents(lli n, lli **adjMatrix, PhaseTimes *phaseTimes = nullptr) {
    double t = omp_get_wtime();
    std::vector<std::vector<lli>> components = dsu(n, adjMatrix);
    double tDsu = omp_get_wtime() - t;

    lli *colors = new lli[n]();
    lli chromaticNumber = 0;
    lli totalComponents = components.size();

    t = omp_get_wtime();
    // Dynamic schedule: component sizes may differ (they do not in the balanced generator).
    #pragma omp parallel for schedule(dynamic, 1) reduction(max : chromaticNumber)
    for (lli idx = 0; idx < totalComponents; idx++) {
        const std::vector<lli> &c = components[idx];
        lli size = c.size();
        lli **submatrix = new lli*[size];
        for (lli i = 0; i < size; i++) {
            submatrix[i] = new lli[size];
            for (lli j = 0; j < size; j++) {
                submatrix[i][j] = adjMatrix[c[i]][c[j]];
            }
        }

        ColoringResult result = largestDegreeFirst(size, submatrix);
        for (lli i = 0; i < size; i++) {
            colors[c[i]] = result.colors[i];  // components are disjoint: no write conflicts
        }
        chromaticNumber = std::max(chromaticNumber, result.chromaticNumber);

        delete[] result.colors;
        for (lli i = 0; i < size; i++) {
            delete[] submatrix[i];
        }
        delete[] submatrix;
    }
    double tColoring = omp_get_wtime() - t;

    if (phaseTimes != nullptr) {
        phaseTimes->dsu = tDsu;
        phaseTimes->coloring = tColoring;
    }

    return ColoringResult(colors, chromaticNumber);
}

#endif // COLORING_OMP_COMPONENTS_H
