#include <iostream>
#include "../../generator/GraphGenerator.h"
#include "../../utils/functions/glutInitialize.h"
#include "../../utils/functions/isWellColored.h"
#include "coloringHybrid.h"

int main(int argc, char** argv) {    
    int processId;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);

    GraphGenerator *graphGenerator = new GraphGenerator();

    if (processId == 0) {
        if (argc < 5) {
            std::cerr << "Insufficient arguments!" << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        lli n = std::stoll(argv[1]);
        lli m = std::stoll(argv[2]);
        lli nPrime = std::stoll(argv[3]);
        int threads = std::stoi(argv[4]);
        omp_set_num_threads(threads);
        graphGenerator->loadIfExistsOrGenerateNewGraph(n, m, nPrime);
    }
    
    auto start = MPI_Wtime();
    ColoringResult result = coloringHybrid(processId, graphGenerator->getN(), graphGenerator->getGraph(), coloringOMP);
    auto stop = MPI_Wtime();

    if (processId == 0) {
        isWellColored(result.colors, graphGenerator->getN(), graphGenerator->getGraph());
        std::cout << "Chromatic number: " << result.chromaticNumber << std::endl;
        std::cout << "Total time: " << stop - start << " seconds" << std::endl;
        
        // glutInitialize(argc, argv);
        // graphGenerator->setColorIndex(result.colors, result.labels);
        // graphGenerator->setChromaticNumber(result.chromaticNumber);
        // graphGenerator->drawGraph();
        // glutMainLoop();
    }
    
    MPI_Finalize();
    return 0;
}
