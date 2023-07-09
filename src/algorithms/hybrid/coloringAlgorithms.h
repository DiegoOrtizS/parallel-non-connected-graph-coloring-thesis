#ifndef COLORING_ALGORITHMS_H
#define COLORING_ALGORITHMS_H

#include <vector>
#include <set>
#include <algorithm>

using namespace std;

int coloringOMP(int n, int **adjMatrix) {
    int *colors = new int[n];
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        std::set<int> C;
        for (int j = 0; j < n; j++) {
            if (adjMatrix[i][j] && colors[j] != 0) {
                C.insert(colors[j]);
            }
        }

        int smallestColor = 1;
        while (C.count(smallestColor) > 0) {
            smallestColor++;
        }

        colors[i] = smallestColor;
    }

    #pragma omp barrier

    std::vector<int> U;
    for (int i = 0; i < n; i++) {
        U.push_back(i);
    }

    int iteration = 1, chromaticNumber = 0;
    while (!U.empty()) {
        std::vector<int> L;

        #pragma omp parallel for
        for (int i = 0; i < U.size(); i++) {
            int vertex = U[i];
            bool shouldUpdateColor = false;

            for (int j = 0; j < n; j++) {
                if (adjMatrix[vertex][j] && j > vertex && colors[j] == colors[vertex]) {
                    shouldUpdateColor = true;
                    break;
                }
            }

            if (shouldUpdateColor) {
                std::set<int> C;
                for (int j = 0; j < n; j++) {
                    if (adjMatrix[vertex][j] && colors[j] != 0) {
                        C.insert(colors[j]);
                    }
                }

                int smallestColor = 1;
                while (C.count(smallestColor) > 0) {
                    smallestColor++;
                }

                colors[vertex] = smallestColor;
                if (smallestColor > chromaticNumber) {
                    chromaticNumber = smallestColor;
                }

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

    return chromaticNumber;
}

#endif // COLORING_ALGORITHMS_H