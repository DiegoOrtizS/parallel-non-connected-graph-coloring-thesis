#include <chrono>
#include <set>
#include "../../generator/GraphGenerator.h"
#include "../../utils/functions/isWellColored.h"

using namespace std::chrono;

int main(int argc, char** argv) {
    GraphGenerator* graphGenerator = new GraphGenerator();
    if (argc < 4) {
        std::cerr << "Insufficient arguments!" << std::endl;
        return 1;
    }
    lli n = std::stoll(argv[1]);
    lli m = std::stoll(argv[2]);
    lli nPrime = std::stoll(argv[3]);
    graphGenerator->loadIfExistsOrGenerateNewGraph(n, m, nPrime);

    graphGenerator->validateGraph();
    auto adjMatrix = graphGenerator->getGraph();

    // sequential coloring
    auto start = high_resolution_clock::now();
    lli* colors = new lli[n];
    for (int i = 0; i < n; i++) {
        // Get the colors of adjacent vertices
        std::set<int> C;
        for (int j = 0; j < n; j++) {
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

    isWellColored(colors, n, adjMatrix);

    // get chromatic number
    int chromaticNumber = 0;
    for (int i = 0; i < n; i++) {
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