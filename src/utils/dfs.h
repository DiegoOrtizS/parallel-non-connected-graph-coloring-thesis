#ifndef DFS_H
#define DFS_H

void DFS(int** graph, int &n, int &vertex, bool* visited, int& componentSize, int& componentEdges) {
    visited[vertex] = true;
    ++componentSize;

    for (int i = 0; i < n; ++i) {
        if (graph[vertex][i] && !visited[i]) {
            DFS(graph, n, i, visited, componentSize, componentEdges);
            componentEdges += graph[vertex][i];
        }
    }
}

#endif // DFS_H