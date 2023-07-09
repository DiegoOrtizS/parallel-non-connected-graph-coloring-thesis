#include <iostream>
#include <mpi.h>
#include "../../generator/GraphGenerator.h"
#include "../../utils/functions/glutInitialize.h"
#include "../../utils/functions/isWellColored.h"
#include "connectedComponents.h"
#include "coloringAlgorithms.h"

using namespace std;

int main(int argc, char** argv) {    
    int processId;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);
    omp_set_num_threads(4);

    GraphGenerator *graphGenerator = new GraphGenerator();

    if (processId == 0) {
        graphGenerator->loadGraph("10000 6245000 8");
        graphGenerator->validateGraph();
    }
    
    auto start = MPI_Wtime();
    int chromaticNumber = coloringMPI(processId, graphGenerator->getN(), graphGenerator->getGraph(), coloringOMP);
    auto stop = MPI_Wtime();
    if (processId == 0) {
        std::cout << "Graph chromatic number is " << chromaticNumber << std::endl;
        cout << "Time taken by MPI coloring: " << stop - start << " seconds" << endl;
    }
    MPI_Finalize();
    return 0;
}
