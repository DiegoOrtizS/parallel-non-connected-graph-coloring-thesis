#include <chrono>
#include <set>
#include "../../utils/isWellColored.h"
#include "../../generator/GraphGenerator.h"

using namespace std::chrono;

int main(int argc, char** argv) {
    int numVertices = 1e4;
    GraphGenerator graph(numVertices);
    graph.generateGraph(6245000, 8, 1, 42);
    auto adjMatrix = graph.getGraph();
    graph.validateGraph();

    // sequential coloring
    auto start = high_resolution_clock::now();
    int* colors = new int[numVertices];
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

    isWellColored(colors, adjMatrix, numVertices);

    // get chromatic number
    int chromaticNumber = 0;
    for (int i = 0; i < numVertices; i++) {
        if (colors[i] > chromaticNumber) {
            chromaticNumber = colors[i];
        }
    }

    // measure time
    std::cout << "Chromatic number: " << chromaticNumber << std::endl;
    auto stop = high_resolution_clock::now();
    duration<double> duration = stop - start;
    std::cout << "Time taken by sequential coloring: " << duration.count() << " seconds" << std::endl;
}