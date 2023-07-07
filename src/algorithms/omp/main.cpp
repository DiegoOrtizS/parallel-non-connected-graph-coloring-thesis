#include <iostream>
#include <vector>
#include <set>
#include <omp.h>
#include "../../generator/GraphGenerator.h"
#include "../../utils/glutInitialize.h"
#include "../../utils/isWellColored.h"

using namespace std;

int main(int argc, char** argv) {
    int numVertices = 1e4;
    GraphGenerator graph(numVertices);
    graph.generateGraph(6245000, 8, 1, 42);
    auto adjMatrix = graph.getGraph();
    graph.validateGraph();


    // Array to store the colors of each vertex
    int* colors = new int[numVertices];

    // Set the number of threads for parallel execution
    omp_set_num_threads(omp_get_max_threads());

    float beg = omp_get_wtime();

    // Step 1: Assign colors to initial vertices
    #pragma omp parallel for
    for (int i = 0; i < numVertices; i++) {
        // Get the colors of adjacent vertices
        std::set<int> C;
        for (int j = 0; j < numVertices; j++) {
            if (adjMatrix[i][j] && colors[j] != 0) {
                C.insert(colors[j]);
            }
        }

        // Find the smallest color not in C
        int smallestColor = 1;
        while (C.count(smallestColor) > 0) {
            smallestColor++;
        }

        // Assign the smallest color to the vertex
        colors[i] = smallestColor;
    }

    // Synchronization barrier
    #pragma omp barrier

    // Step 2: Iteratively update colors until convergence
    std::vector<int> U;
    for (int i = 0; i < numVertices; i++) {
        U.push_back(i);
    }

    int iteration = 1;
    while (!U.empty()) {
        std::vector<int> L;

        #pragma omp parallel for schedule(dynamic)
        for (int i = 0; i < U.size(); i++) {
            int vertex = U[i];
            bool shouldUpdateColor = false;

            // Check if any adjacent vertex has a greater value and the same color
            for (int j = 0; j < numVertices; j++) {
                if (adjMatrix[vertex][j] && j > vertex && colors[j] == colors[vertex]) {
                    shouldUpdateColor = true;
                    break;
                }
            }

            if (shouldUpdateColor) {
                // Get the colors of adjacent vertices
                std::set<int> C;
                for (int j = 0; j < numVertices; j++) {
                    if (adjMatrix[vertex][j] && colors[j] != 0) {
                        C.insert(colors[j]);
                    }
                }

                // Find the smallest color not in C
                int smallestColor = 1;
                while (C.count(smallestColor) > 0) {
                    smallestColor++;
                }

                // Assign the smallest color to the vertex
                colors[vertex] = smallestColor;

                // Add the vertex to L
                #pragma omp critical
                {
                    L.push_back(vertex);
                }
            }
        }

        // Synchronization barrier
        #pragma omp barrier

        // Update U for the next iteration
        U = L;

        iteration++;
    }

    float end = omp_get_wtime();
    std::cout << "Time: " << end - beg << std::endl;
    isWellColored(colors, adjMatrix, numVertices);

    glutInitialize(argc, argv);
    graph.setColorIndex(colors);
    graph.drawGraph();
    glutMainLoop();

    for (int i = 0; i < numVertices; i++) {
        delete[] adjMatrix[i];
    }
    delete[] colors;

    return 0;
}
