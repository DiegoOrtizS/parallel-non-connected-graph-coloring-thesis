#include <omp.h>
#include <vector>
#include <set>

std::pair<int*, int> coloringOMP(int n, int **adjMatrix) {
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

    int iteration = 1;
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

    int chromaticNumber = 0;
    #pragma omp parallel for reduction(max : chromaticNumber) schedule(dynamic)
    for (int i = 0; i < n; i++) {
        chromaticNumber = std::max(chromaticNumber, colors[i]);
    }

    return std::make_pair(colors, chromaticNumber);
}