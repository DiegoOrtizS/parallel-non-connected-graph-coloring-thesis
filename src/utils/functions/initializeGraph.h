#ifndef INITIALIZE_GRAPH_H
#define INITIALIZE_GRAPH_H

void initializeGraph(int n, int **&graph) {
    graph = new int*[n];
    for (int i = 0; i < n; i++) {
        graph[i] = new int[n];
        for (int j = 0; j < n; j++) {
            graph[i][j] = 0;
        }
    }
}

#endif // INITIALIZE_GRAPH_H