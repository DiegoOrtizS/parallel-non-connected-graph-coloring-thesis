#include <iostream>
#include "../../generator/GraphGenerator.h"
#include "../../utils/functions/isWellColored.h"
#include "../../utils/functions/report.h"
#include "coloringMPI.h"

// Usage: mpirun -np P ./a.out n m nPrime
int main(int argc, char** argv) {
    int processId, processSize;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);
    MPI_Comm_size(MPI_COMM_WORLD, &processSize);

    if (argc < 4) {
        if (processId == 0) {
            std::cerr << "Usage: mpirun -np P " << argv[0] << " n m nPrime" << std::endl;
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    lli n = std::stoll(argv[1]);
    lli m = std::stoll(argv[2]);
    lli nPrime = std::stoll(argv[3]);

    // Only the root holds the graph; the other processes receive their components.
    GraphGenerator *graphGenerator = new GraphGenerator();
    if (processId == 0) {
        graphGenerator->loadIfExistsOrGenerateNewGraph(n, m, nPrime);
    }

    PhaseTimes phases;
    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();
    ColoringResult result = coloringMPI(processId, n, graphGenerator->getGraph(), largestDegreeFirst, &phases);
    double stop = MPI_Wtime();

    if (processId == 0) {
        isWellColored(result.colors, n, graphGenerator->getGraph(), result.labels);
        std::cout << "Number of colors: " << result.chromaticNumber << std::endl;
        std::cout << "Total time: " << stop - start << " seconds" << std::endl;
        printCsvLine("mpi-ldf", n, m, nPrime, processSize, 1, stop - start, phases, result.chromaticNumber);
        delete[] result.colors;
        delete[] result.labels;
    }

    MPI_Finalize();
    return 0;
}
