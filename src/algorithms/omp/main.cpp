#include <iostream>
#include <vector>
#include <set>
#include <omp.h>
#include "../../generator/GraphGenerator.h"
#include "../../utils/functions/glutInitialize.h"
#include "../../utils/functions/isWellColored.h"

using namespace std;

int coloringOMP(int numThreads, int n, int **adjMatrix, int *&colors) {
    omp_set_num_threads(numThreads);
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

    return chromaticNumber;
}

int main(int argc, char** argv) {
    // int n = 1e4;
    // int n = 9;
    // GraphGenerator *graph = new GraphGenerator(n);
    // graph->generateGraph(7, 3);

    GraphGenerator* graph = new GraphGenerator();
    graph->loadGraph("10000 6245000 8");
    auto adjMatrix = graph->getGraph();
    graph->validateGraph();
    int n = graph->getN();
    int *colors = new int[n];
    float beg = omp_get_wtime();
    int chromaticNumber = coloringOMP(omp_get_max_threads(), n, adjMatrix, colors);
    float end = omp_get_wtime();
    std::cout << "Time: " << end - beg << std::endl;
    isWellColored(colors, adjMatrix, n);
    std::cout << "Chromatic number: " << chromaticNumber << std::endl;

    glutInitialize(argc, argv);
    graph->setColorIndex(colors);
    graph->setChromaticNumber(chromaticNumber);
    graph->drawGraph();
    glutMainLoop();

    for (int i = 0; i < n; i++) {
        delete[] adjMatrix[i];
    }
    delete[] colors;
    delete graph;

    return 0;
}
