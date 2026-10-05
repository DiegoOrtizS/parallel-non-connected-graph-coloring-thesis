#include <iostream>
#include <string>
#include "../../generator/GraphGenerator.h"
#include "../../utils/functions/isWellColored.h"
#include "../../utils/functions/report.h"
#include "coloringOMP.h"
#include "coloringOMPComponents.h"

// Usage: ./a.out n m nPrime threads [rsoc|components]
int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " n m nPrime threads [rsoc|components]" << std::endl;
        return 1;
    }
    lli n = std::stoll(argv[1]);
    lli m = std::stoll(argv[2]);
    lli nPrime = std::stoll(argv[3]);
    int threads = std::stoi(argv[4]);
    std::string algorithm = argc > 5 ? argv[5] : "rsoc";

    GraphGenerator* graphGenerator = new GraphGenerator();
    graphGenerator->loadIfExistsOrGenerateNewGraph(n, m, nPrime);
    omp_set_num_threads(threads);
    lli **graph = graphGenerator->getGraph();

    // double, not float: omp_get_wtime() counts from an arbitrary origin (often system uptime),
    // and a float only keeps ~7 significant digits, i.e. millisecond-scale timings are lost.
    PhaseTimes phases;
    double start = omp_get_wtime();
    ColoringResult result = algorithm == "components"
        ? coloringOMPComponents(n, graph, &phases)
        : coloringOMP(n, graph);
    double stop = omp_get_wtime();
    if (algorithm != "components") {
        phases.coloring = stop - start;
    }

    isWellColored(result.colors, n, graph);
    std::cout << "Number of colors: " << result.chromaticNumber << std::endl;
    std::cout << "Total time: " << stop - start << " seconds" << std::endl;
    printCsvLine(algorithm == "components" ? "omp-components" : "omp-rsoc",
                 n, m, nPrime, 1, threads, stop - start, phases, result.chromaticNumber);

    delete[] result.colors;
    return 0;
}
