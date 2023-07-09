#include <iostream>
#include <mpi.h>
#include "../../generator/GraphGenerator.h"
#include "../../utils/functions/glutInitialize.h"
#include "../../utils/functions/isWellColored.h"
#include "connectedComponents.h"
#include "coloringAlgorithms.h"

using namespace std;

int coloringMPI(const int &processId, const int &n, int **graph, int (*coloringAlgorithm)(int, int**)) {
    int chromaticNumber;
    MPI_Request request;
    MPI_Status status;

    if (processId == 0) {
        auto components = dsu(n, graph);
        
        int nextProcessId = 1;

        for (int component = 1; component < components.size(); component++) {
            auto c = components[component];
            int submatrixSize = c.size();
            int **submatrix = new int*[submatrixSize];
            for (int i = 0; i < submatrixSize; i++) {
                submatrix[i] = new int[submatrixSize];
                for (int j = 0; j < submatrixSize; j++) {
                    submatrix[i][j] = graph[c[i]][c[j]];
                }
            }

            MPI_Isend(&submatrixSize, 1, MPI_INT, nextProcessId, 0, MPI_COMM_WORLD, &request);

            for (int i = 0; i < submatrixSize; i++) {
                MPI_Isend(submatrix[i], submatrixSize, MPI_INT, nextProcessId, 0, MPI_COMM_WORLD, &request);
            }
            
            nextProcessId++;
        }

        auto c = components[0];
        int submatrixSize = c.size();
        int **submatrix = new int*[submatrixSize];
        for (int i = 0; i < submatrixSize; i++) {
            submatrix[i] = new int[submatrixSize];
            for (int j = 0; j < submatrixSize; j++) {
                submatrix[i][j] = graph[c[i]][c[j]];
            }
        }

        chromaticNumber = coloringAlgorithm(components[0].size(), submatrix);
    }
    else {
        int submatrixSize;
        MPI_Irecv(&submatrixSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);

        // allocate memory for the submatrix
        int** submatrix = new int*[submatrixSize];
        for (int i = 0; i < submatrixSize; i++) {
            submatrix[i] = new int[submatrixSize];
        }

        for (int i = 0; i < submatrixSize; i++) {
            MPI_Irecv(submatrix[i], submatrixSize, MPI_INT, 0, 0, MPI_COMM_WORLD, &request);
        }

        // print submatrix
        // for (int i = 0; i < submatrixSize; i++) {
        //     for (int j = 0; j < submatrixSize; j++) {
        //         cout << submatrix[i][j] << " ";
        //     }
        //     cout << endl;
        // }
        // cout << endl;

        chromaticNumber = coloringAlgorithm(submatrixSize, submatrix);
    }

    // MPI REDUCE of the chromatic number
    int maxChromaticNumber;
    MPI_Reduce(&chromaticNumber, &maxChromaticNumber, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);

    return maxChromaticNumber;
}

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
    int chromaticNumber = coloringMPI(processId, graphGenerator->getN(), graphGenerator->getGraph(), largestDegreeFirst);
    auto stop = MPI_Wtime();
    if (processId == 0) {
        std::cout << "Graph chromatic number is " << chromaticNumber << std::endl;
        cout << "Time taken by MPI coloring: " << stop - start << " seconds" << endl;
    }
    MPI_Finalize();
    return 0;
}
