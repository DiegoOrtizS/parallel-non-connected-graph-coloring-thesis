#include <iostream>
#include "../../generator/GraphGenerator.h"
#include "../../utils/functions/glutInitialize.h"
#include "../../utils/functions/isWellColored.h"
#include "coloringOMP.h"

using namespace std;

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
    omp_set_num_threads(omp_get_max_threads());
    float beg = omp_get_wtime();
    int *colors, chromaticNumber;
    std::pair<int*, int> result = coloringOMP(n, adjMatrix);
    colors = result.first;
    chromaticNumber = result.second;
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
