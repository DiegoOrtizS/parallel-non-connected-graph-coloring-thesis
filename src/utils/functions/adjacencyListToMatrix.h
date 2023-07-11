#ifndef ADJACENCY_LIST_TO_MATRIX_H
#define ADJACENCY_LIST_TO_MATRIX_H

#include <vector>
#include <iostream>
#include "../../generator/jngen.h"

lli **adjacencyListToMatrix(jngen::Graph graph) {
    lli n = graph.n();
    lli **adjMatrix = new lli*[n];
    for (lli i = 0; i < n; ++i) {
        adjMatrix[i] = new lli[n];
        for (lli j = 0; j < n; ++j) {
            adjMatrix[i][j] = 0;
        }
        for (lli j : graph.edges(i)) {
            adjMatrix[i][j] = 1;
        }
    }

    return adjMatrix;
}

#endif // ADJACENCY_LIST_TO_MATRIX_H