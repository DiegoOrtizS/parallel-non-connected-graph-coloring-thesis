#ifndef COMBINE_COMPONENTS_TO_ADJACENCY_MATRIX_H
#define COMBINE_COMPONENTS_TO_ADJACENCY_MATRIX_H

#include <vector>

void combineComponentsToAdjacencyMatrix(std::vector<int**> components, 
                            std::vector<int> verticesPerComponent, int **&graph) {
    int accumulatedVertices = 0;
    for (int i = 0; i < components.size(); ++i) {
        int **component = components[i];
        int vertexCount = verticesPerComponent[i];
        for (int j = 0; j < vertexCount; j++) {
            for (int k = 0; k < vertexCount; ++k) {
                graph[j+accumulatedVertices][k+accumulatedVertices] = component[j][k];
            }
        }
        accumulatedVertices += vertexCount;
    }
}

#endif // COMBINE_COMPONENTS_TO_ADJACENCY_MATRIX_H