#ifndef CONNECTED_COMPONENTS_MPI_H
#define CONNECTED_COMPONENTS_MPI_H

#include <vector>
#include <unordered_map>
#include <algorithm>
#include "../../utils/types.h"

lli find(std::vector<lli> &parent, lli x) {
    if (parent[x] == x) {
        return x;
    }
    return parent[x] = find(parent, parent[x]);
}

void unionSet(std::vector<lli> &parent, std::vector<lli> &rank, lli x, lli y) {
    lli px = find(parent, x);
    lli py = find(parent, y);
    if (px == py) {
        return;
    }

    if (rank[px] > rank[py]) {
        parent[py] = px;
    } else {
        parent[px] = py;
        if (rank[px] == rank[py]) {
            rank[py]++;
        }
    }
}

// Connected components with union by rank and path compression.
// Scanning the upper triangle of the adjacency matrix costs Theta(n^2).
std::vector<std::vector<lli>> dsu(lli matrixSize, lli** matrix) {
    std::vector<std::vector<lli>> components;
    // std::vector instead of a VLA: large n would overflow the stack.
    std::vector<lli> parent(matrixSize), rank(matrixSize, 0);

    for (lli i = 0; i < matrixSize; i++) {
        parent[i] = i;
    }

    for (lli i = 0; i < matrixSize; i++) {
        for (lli j = i + 1; j < matrixSize; j++) {
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

    // Deterministic order (by smallest vertex) so every run assigns the same components to each process.
    std::sort(components.begin(), components.end());

    return components;
}

#endif // CONNECTED_COMPONENTS_MPI_H
