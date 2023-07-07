#include <iostream>
#include "Graph.h"
#include "../algorithms/mpi/connectedComponents.h"


Graph::Graph() {
    n = 0;
    graph = nullptr;
}

Graph::Graph(int n) {
    this->n = n;
    graph = new int*[n];
    for (int i = 0; i < n; i++) {
        graph[i] = new int[n];
    }
}

Graph::Graph(int n, int **graph) {
    this->n = n;
    this->graph = graph;
}

Graph::~Graph() {
    for (int i = 0; i < n; i++) {
        delete[] graph[i];
    }
    delete[] graph;
}

void Graph::setN(int n) {
    this->n = n;
    graph = new int*[n];
    for (int i = 0; i < n; i++) {
        graph[i] = new int[n];
    }
}

int Graph::getN() {
    return n;
}

void Graph::setGraph(int **graph) {
    this->graph = graph;
}

int **Graph::getGraph() {
    return graph;
}

void Graph::coloringMPI(int processId, int (*coloringAlgorithm)(int, int**)) {
    int chromaticNumber;

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

            MPI_Send(&submatrixSize, 1, MPI_INT, nextProcessId, 0, MPI_COMM_WORLD);
            for (int i = 0; i < submatrixSize; i++) {
                MPI_Send(submatrix[i], submatrixSize, MPI_INT, nextProcessId, 0, MPI_COMM_WORLD);
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
        MPI_Recv(&submatrixSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // allocate memory for the submatrix
        int** submatrix = new int*[submatrixSize];
        for (int i = 0; i < submatrixSize; i++) {
            submatrix[i] = new int[submatrixSize];
        }

        for (int i = 0; i < submatrixSize; i++) {
            MPI_Recv(submatrix[i], submatrixSize, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        chromaticNumber = coloringAlgorithm(submatrixSize, submatrix);
    }

    // MPI REDUCE of the chromatic number
    int maxChromaticNumber;
    MPI_Reduce(&chromaticNumber, &maxChromaticNumber, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
    if (processId == 0)
        std::cout << "Graph chromatic number is " << maxChromaticNumber << std::endl;
}