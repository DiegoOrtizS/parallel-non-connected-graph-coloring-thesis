#ifndef COLORING_OMP_H
#define COLORING_OMP_H

#include <omp.h>
#include <vector>
#include <set>

#include <omp.h>
#include <vector>
#include <set>
#include "../../utils/structs/ColoringResult.h"

ColoringResult coloringOMP(lli n, lli **adjMatrix) {
    lli *colors = new lli[n];
    #pragma omp parallel for
    for (lli i = 0; i < n; i++) {
        std::set<lli> C;
        for (lli j = 0; j < n; j++) {
            if (adjMatrix[i][j] && colors[j] != 0) {
                C.insert(colors[j]);
            }
        }

        lli smallestColor = 1;
        while (C.count(smallestColor) > 0) {
            smallestColor++;
        }

        colors[i] = smallestColor;
    }

    #pragma omp barrier

    std::vector<lli> U;
    for (lli i = 0; i < n; i++) {
        U.push_back(i);
    }

    lli iteration = 1;
    while (!U.empty()) {
        std::vector<lli> L;

        #pragma omp parallel for
        for (lli i = 0; i < U.size(); i++) {
            lli vertex = U[i];
            bool shouldUpdateColor = false;

            for (lli j = 0; j < n; j++) {
                if (adjMatrix[vertex][j] && j > vertex && colors[j] == colors[vertex]) {
                    shouldUpdateColor = true;
                    break;
                }
            }

            if (shouldUpdateColor) {
                std::set<lli> C;
                for (lli j = 0; j < n; j++) {
                    if (adjMatrix[vertex][j] && colors[j] != 0) {
                        C.insert(colors[j]);
                    }
                }

                lli smallestColor = 1;
                while (C.count(smallestColor) > 0) {
                    smallestColor++;
                }

                colors[vertex] = smallestColor;

                #pragma omp critical
                {
                    L.push_back(vertex);
                }
            }
        }

        #pragma omp barrier

        U = L;

        iteration++;
    }

    lli chromaticNumber = 0;
    #pragma omp parallel for reduction(max : chromaticNumber)
    for (lli i = 0; i < n; i++) {
        chromaticNumber = std::max(chromaticNumber, colors[i]);
    }

    return ColoringResult(colors, chromaticNumber);
}

#endif // COLORING_ALGORITHMS_H