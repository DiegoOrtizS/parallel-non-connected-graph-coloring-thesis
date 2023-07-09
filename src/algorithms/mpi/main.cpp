#include <iostream>
#include "../../generator/GraphGenerator.h"
#include "../../utils/functions/glutInitialize.h"
#include "../../utils/functions/isWellColored.h"
#include "coloringMPI.h"

using namespace std;

int main(int argc, char** argv) {    
    int processId;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);

    GraphGenerator *graphGenerator = new GraphGenerator();

    if (processId == 0) {
        // graphGenerator->setN(1e4);
        // graphGenerator->generateGraph(6245000, 8);
        graphGenerator->loadGraph("10000 6245000 8");
        // graphGenerator->loadGraph("9 7 3");
        graphGenerator->validateGraph();
    }
    
    auto start = MPI_Wtime();
    std::pair<int*, int> result = coloringMPI(processId, graphGenerator->getN(), graphGenerator->getGraph(), largestDegreeFirst);
    auto stop = MPI_Wtime();
    int *colors = result.first;
    int chromaticNumber = result.second;
    if (processId == 0) {
        isWellColored(colors, graphGenerator->getGraph(), graphGenerator->getN());
        std::cout << "Graph chromatic number is " << chromaticNumber << std::endl;
        std::cout << "Time taken by MPI coloring: " << stop - start << " seconds" << std::endl;
    }
    MPI_Finalize();
    return 0;
}
