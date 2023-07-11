#ifndef CONNECTED_COMPONENTS_HYBRID_H
#define CONNECTED_COMPONENTS_HYBRID_H

#include <vector>
#include <unordered_map>
#include <algorithm>

int find(lli parent[], lli x) {
    if (parent[x] == x) {
        return x;
    }
    return parent[x] = find(parent, parent[x]);
}

void unionSet(lli parent[], lli rank[], lli x, lli y) {
    lli px = find(parent, x);
    lli py = find(parent, y);

    if (rank[px] > rank[py]) {
        parent[py] = px;
    } else {
        parent[px] = py;
        if (rank[px] == rank[py]) {
            rank[py]++;
        }
    }
}

std::vector<std::vector<lli>> dsu(lli matrixSize, lli** matrix) {
    std::vector<std::vector<lli>> components;
    lli parent[matrixSize];
    lli rank[matrixSize];

    for (lli i = 0; i < matrixSize; i++) {
        parent[i] = i;
        rank[i] = 0;
    }

    for (lli i = 0; i < matrixSize; i++) {
        for (lli j = 0; j < matrixSize; j++) {
            if (matrix[i][j]) {
                unionSet(parent, rank, i, j);
            }
        }
    }

    std::unordered_map<lli, std::vector<lli>> componentsMap;

    for (lli i = 0; i < matrixSize; i++) {
        componentsMap[find(parent, i)].push_back(i);
    }

    for (auto& [p, c] : componentsMap) {
        components.push_back(c);
    }

    return components;
}

#endif // CONNECTED_COMPONENTS_HYBRID_H