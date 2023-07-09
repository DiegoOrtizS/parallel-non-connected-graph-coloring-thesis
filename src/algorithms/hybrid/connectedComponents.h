#include <vector>
#include <unordered_map>
#include <algorithm>

using namespace std;

int find(int parent[], int x) {
    if (parent[x] == x) {
        return x;
    }
    return parent[x] = find(parent, parent[x]);
}

void unionSet(int parent[], int rank[], int x, int y) {
    int px = find(parent, x);
    int py = find(parent, y);

    if (rank[px] > rank[py]) {
        parent[py] = px;
    } else {
        parent[px] = py;
        if (rank[px] == rank[py]) {
            rank[py]++;
        }
    }
}

vector<vector<int>> dsu(int matrixSize, int** matrix) {
    vector<vector<int>> components;
    int parent[matrixSize];
    int rank[matrixSize];

    #pragma omp parallel for
    for (int i = 0; i < matrixSize; i++) {
        parent[i] = i;
        rank[i] = 0;
    }

    #pragma omp parallel for
    for (int i = 0; i < matrixSize; i++) {
        for (int j = 0; j < matrixSize; j++) {
            if (matrix[i][j]) {
                unionSet(parent, rank, i, j);
            }
        }
    }

    unordered_map<int, vector<int>> componentsMap;
    #pragma omp parallel for shared(componentsMap)
    for (int i = 0; i < matrixSize; i++) {
        #pragma omp critical
        {
            componentsMap[find(parent, i)].push_back(i);
        }
    }

    for (auto& [p, c] : componentsMap) {
        components.push_back(c);
    }

    return components;
}