#include <iostream>
#include "Graph.h"
#include "../functions/initializeGraph.h"

mesquo::Graph::Graph() {
    n = 0;
    graph = nullptr;
}

mesquo::Graph::Graph(lli n) {
    this->n = n;
    initializeGraph(n, graph);
}

mesquo::Graph::Graph(lli n, lli **graph) {
    this->n = n;
    this->graph = graph;
}

mesquo::Graph::~Graph() {
    for (lli i = 0; i < n; i++) {
        delete[] graph[i];
    }
    delete[] graph;
}

void mesquo::Graph::setN(lli n) {
    this->n = n;
    initializeGraph(n, graph);
}

lli mesquo::Graph::getN() {
    return n;
}

void mesquo::Graph::setGraph(lli **graph) {
    this->graph = graph;
}

lli **mesquo::Graph::getGraph() {
    return graph;
}