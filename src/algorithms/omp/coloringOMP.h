#ifndef COLORING_OMP_H
#define COLORING_OMP_H

#include <omp.h>
#include <vector>
#include <algorithm>
#include "../../utils/types.h"
#include "../../utils/structs/ColoringResult.h"

// Smallest color not used by the colored neighbors of `vertex`.
// `mark` is a per-thread scratch array of size n + 2; mark[c] == stamp means color c is taken.
// Using a stamp avoids clearing the array (and the std::set allocations of the previous version).
inline lli smallestFreeColor(lli n, lli **adjMatrix, const lli *colors, lli vertex,
                             std::vector<lli> &mark, lli stamp) {
    for (lli j = 0; j < n; j++) {
        lli c = colors[j];
        if (adjMatrix[vertex][j] && c > 0 && c <= n) {
            mark[c] = stamp;
        }
    }
    lli color = 1;
    while (mark[color] == stamp) {
        color++;
    }
    return color;
}

// Reduced Synchronization Optimistic Coloring (Rokos, Gorman and Kelly, Euro-Par 2015, Algorithm 3).
// Reads of colors[] race with writes by design (optimistic coloring); conflicts are detected
// and repaired in the following rounds, and the final coloring is verified by the caller.
ColoringResult coloringOMP(lli n, lli **adjMatrix) {
    lli *colors = new lli[n]();  // value-initialized: 0 means "uncolored"

    // Round 0: tentative coloring of every vertex.
    #pragma omp parallel
    {
        std::vector<lli> mark(n + 2, -1);
        #pragma omp for schedule(static)
        for (lli i = 0; i < n; i++) {
            colors[i] = smallestFreeColor(n, adjMatrix, colors, i, mark, i);
        }
    }

    std::vector<lli> U(n);
    for (lli i = 0; i < n; i++) {
        U[i] = i;
    }

    // Detect-and-recolor rounds, a single implicit barrier each.
    lli round = 1;
    while (!U.empty()) {
        std::vector<lli> L;

        #pragma omp parallel
        {
            std::vector<lli> mark(n + 2, -1);
            std::vector<lli> localL;
            #pragma omp for schedule(dynamic, 64) nowait
            for (size_t idx = 0; idx < U.size(); idx++) {
                lli vertex = U[idx];
                bool defective = false;
                for (lli j = vertex + 1; j < n; j++) {
                    if (adjMatrix[vertex][j] && colors[j] == colors[vertex]) {
                        defective = true;
                        break;
                    }
                }
                if (defective) {
                    colors[vertex] = smallestFreeColor(n, adjMatrix, colors, vertex, mark, round * n + vertex);
                    localL.push_back(vertex);
                }
            }
            #pragma omp critical
            L.insert(L.end(), localL.begin(), localL.end());
        }

        U.swap(L);
        round++;
    }

    lli chromaticNumber = 0;
    #pragma omp parallel for reduction(max : chromaticNumber)
    for (lli i = 0; i < n; i++) {
        chromaticNumber = std::max(chromaticNumber, colors[i]);
    }

    return ColoringResult(colors, chromaticNumber);
}

#endif // COLORING_OMP_H
