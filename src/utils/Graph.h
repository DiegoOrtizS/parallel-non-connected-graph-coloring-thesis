#ifndef GRAPH_H
#define GRAPH_H

#include <mpi.h>

class Graph
{
    protected:
        int n;
        int **graph;
    
    public:
        Graph();
        Graph(int n);
        Graph(int n, int **graph);
        ~Graph();
        void setN(int n);
        int getN();
        void setGraph(int **graph);
        int** getGraph();
        void coloringMPI(int processId, int (*coloringAlgorithm)(int, int**));
};

#endif // GRAPH_H