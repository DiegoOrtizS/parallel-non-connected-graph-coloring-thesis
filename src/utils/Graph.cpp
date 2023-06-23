#include "Graph.h"

Graph::Graph(int n, int m) {
    this->n = n;
    this->m = m;
    graph = new int*[n];
    for (int i = 0; i < n; i++) {
        graph[i] = new int[n];
        for (int j = 0; j < n; j++) {
            graph[i][j] = 0;
        }
    }
}

Graph::~Graph() {
    for (int i = 0; i < n; i++) {
        delete[] graph[i];
    }
    delete[] graph;
}