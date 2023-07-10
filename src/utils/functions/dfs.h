#ifndef DFS_H
#define DFS_H

void DFS(lli** graph, lli &n, lli &vertex, bool* visited, lli& componentSize, lli& componentEdges) {
    visited[vertex] = true;
    ++componentSize;

    for (lli i = 0; i < n; ++i) {
        if (graph[vertex][i] && !visited[i]) {
            DFS(graph, n, i, visited, componentSize, componentEdges);
            componentEdges += graph[vertex][i];
        }
    }
}

#endif // DFS_H