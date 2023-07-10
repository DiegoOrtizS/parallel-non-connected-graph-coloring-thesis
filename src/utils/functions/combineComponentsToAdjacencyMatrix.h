#ifndef COMBINE_COMPONENTS_TO_ADJACENCY_MATRIX_H
#define COMBINE_COMPONENTS_TO_ADJACENCY_MATRIX_H

#include <vector>

void combineComponentsToAdjacencyMatrix(std::vector<lli**> components, 
                            std::vector<lli> verticesPerComponent, lli **&graph) {
    lli accumulatedVertices = 0;
    for (lli i = 0; i < components.size(); ++i) {
        lli **component = components[i];
        lli vertexCount = verticesPerComponent[i];
        for (lli j = 0; j < vertexCount; j++) {
            for (lli k = 0; k < vertexCount; ++k) {
                graph[j+accumulatedVertices][k+accumulatedVertices] = component[j][k];
            }
        }
        accumulatedVertices += vertexCount;
    }
}

#endif // COMBINE_COMPONENTS_TO_ADJACENCY_MATRIX_H