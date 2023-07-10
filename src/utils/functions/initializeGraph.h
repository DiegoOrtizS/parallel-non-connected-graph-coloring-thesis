#ifndef INITIALIZE_GRAPH_H
#define INITIALIZE_GRAPH_H

void initializeGraph(lli n, lli **&graph) {
    graph = new lli*[n];
    for (lli i = 0; i < n; i++) {
        graph[i] = new lli[n];
        for (lli j = 0; j < n; j++) {
            graph[i][j] = 0;
        }
    }
}

#endif // INITIALIZE_GRAPH_H