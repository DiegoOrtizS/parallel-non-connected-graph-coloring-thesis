#include <iostream>
#include "Graph.h"
#include "../functions/initializeGraph.h"


mesquo::Graph::Graph() {
    n = 0;
    graph = nullptr;
}

mesquo::Graph::Graph(int n) {
    this->n = n;
    initializeGraph(n, graph);
}

mesquo::Graph::Graph(int n, int **graph) {
    this->n = n;
    this->graph = graph;
}

mesquo::Graph::~Graph() {
    for (int i = 0; i < n; i++) {
        delete[] graph[i];
    }
    delete[] graph;
}

void mesquo::Graph::setN(int n) {
    this->n = n;
    initializeGraph(n, graph);
}

int mesquo::Graph::getN() {
    return n;
}

void mesquo::Graph::setGraph(int **graph) {
    this->graph = graph;
}

int **mesquo::Graph::getGraph() {
    return graph;
}