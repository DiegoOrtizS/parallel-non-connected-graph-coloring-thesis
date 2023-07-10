#include <iostream>
#include "../../generator/GraphGenerator.h"
#include "../../utils/functions/glutInitialize.h"
#include "../../utils/functions/isWellColored.h"
#include "coloringOMP.h"

using namespace std;

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
    omp_set_num_threads(omp_get_max_threads());
    auto graph = graphGenerator->getGraph();

    float start = omp_get_wtime();
    ColoringResult result = coloringOMP(n, graph);
    float stop = omp_get_wtime();

    isWellColored(result.colors, n, graph);
    std::cout << "Chromatic number:  " << result.chromaticNumber << std::endl;
    std::cout << "Total Time: " << stop - start << " seconds" << std::endl;

    glutInitialize(argc, argv);
    graphGenerator->setColorIndex(result.colors);
    graphGenerator->setChromaticNumber(result.chromaticNumber);
    graphGenerator->drawGraph();
    glutMainLoop();

    return 0;
}
