#ifndef ADJACENCY_LIST_TO_MATRIX_H
#define ADJACENCY_LIST_TO_MATRIX_H

#include <vector>
#include <iostream>
#include "../../generator/jngen.h"

int **adjacencyListToMatrix(jngen::Graph graph) {
    int n = graph.n();
    int **adjMatrix = new int*[n];
    for (int i = 0; i < n; ++i) {
        adjMatrix[i] = new int[n];
        for (int j = 0; j < n; ++j) {
            adjMatrix[i][j] = 0;
        }
        for (int j : graph.edges(i)) {
            adjMatrix[i][j] = 1;
        }
    }

    return adjMatrix;
}

#endif // ADJACENCY_LIST_TO_MATRIX_H