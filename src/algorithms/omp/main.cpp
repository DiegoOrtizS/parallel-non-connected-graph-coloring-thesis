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

    GraphGenerator* graphGenerator = new GraphGenerator();
    // graphGenerator->loadGraph("3 3 1");
    graphGenerator->loadGraph("9 7 3");
    // graph->loadGraph("10000 6245000 8");
    auto graph = graphGenerator->getGraph();
    graphGenerator->validateGraph();
    int n = graphGenerator->getN();
    omp_set_num_threads(omp_get_max_threads());
    float beg = omp_get_wtime();
    int *colors, chromaticNumber;
    ColoringResult result = coloringOMP(n, graph);
    float end = omp_get_wtime();
    std::cout << "Time: " << end - beg << std::endl;

    isWellColored(result.colors, graph, n);
    std::cout << "Chromatic number: " << result.chromaticNumber << std::endl;

    glutInitialize(argc, argv);
    graphGenerator->setColorIndex(result.colors);
    graphGenerator->setChromaticNumber(result.chromaticNumber);
    graphGenerator->drawGraph();
    glutMainLoop();

    delete[] colors;
    delete graph;

    return 0;
}
