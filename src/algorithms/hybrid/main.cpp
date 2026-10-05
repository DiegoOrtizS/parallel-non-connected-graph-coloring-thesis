#include <iostream>
#include "../../generator/GraphGenerator.h"
#include "../../utils/functions/isWellColored.h"
#include "../../utils/functions/report.h"
#include "coloringHybrid.h"

// Usage: mpirun -np P ./a.out n m nPrime threads [--lpt] [--replicate]
int main(int argc, char** argv) {
    int processId, processSize;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);
    MPI_Comm_size(MPI_COMM_WORLD, &processSize);

    if (argc < 5) {
        if (processId == 0) {
            std::cerr << "Usage: mpirun -np P " << argv[0] << " n m nPrime threads" << std::endl;
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    lli n = std::stoll(argv[1]);
    lli m = std::stoll(argv[2]);
    lli nPrime = std::stoll(argv[3]);
    int threads = std::stoi(argv[4]);
    std::string suffix;
    DistributionOptions options = parseDistributionOptions(argc, argv, 5, suffix);

    // Every process colors with OpenMP, so every process must limit its threads.
    // Setting this only on the root left the others with one thread per core (oversubscription).
    omp_set_num_threads(threads);

    GraphGenerator *graphGenerator = new GraphGenerator();
    if (processId == 0) {
        graphGenerator->loadIfExistsOrGenerateNewGraph(n, m, nPrime);
    }

    PhaseTimes phases;
    CommVolume volume;
    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();
    ColoringResult result = coloringHybrid(processId, n, graphGenerator->getGraph(), coloringOMP, &phases, options, &volume);
    double stop = MPI_Wtime();

    if (processId == 0) {
        // Colors come back in component order: the labels map them to the original vertices.
        isWellColored(result.colors, n, graphGenerator->getGraph(), result.labels);
        std::cout << "Number of colors: " << result.chromaticNumber << std::endl;
        std::cout << "Total time: " << stop - start << " seconds" << std::endl;
        printCsvLine("hybrid-rsoc" + suffix, n, m, nPrime, processSize, threads, stop - start, phases, result.chromaticNumber);
        printVolumeLine("hybrid-rsoc" + suffix, n, m, nPrime, processSize, threads, volume);
        delete[] result.colors;
        delete[] result.labels;
    }

    MPI_Finalize();
    return 0;
}
