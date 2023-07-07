#include <iostream>
#include <mpi.h>
#include "../../generator/GraphGenerator.h"
#include "../../utils/glutInitialize.h"
#include "../../utils/isWellColored.h"
#include "coloringAlgorithms.h"

using namespace std;

int main(int argc, char** argv) {
    int processId;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);

    GraphGenerator graph;

    if (processId == 0) {
        int numVertices = 1e4;
        graph.setN(numVertices);
        graph.generateGraph(6245000, 8, 1, 42);
        auto adjMatrix = graph.getGraph();
        graph.validateGraph();
    }
    
    // measure time
    auto start = MPI_Wtime();
    graph.coloringMPI(processId, largestDegreeFirst);
    auto stop = MPI_Wtime();
    if (processId == 0) {
        cout << "Time taken by MPI coloring: " << stop - start << " seconds" << endl;
    }
    MPI_Finalize();
    return 0;
}
