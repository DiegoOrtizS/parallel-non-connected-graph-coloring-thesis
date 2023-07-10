#ifndef IS_WELL_COLORED_H
#define IS_WELL_COLORED_H

#include <stdexcept>
#include <iostream>

void isWellColored(lli* colors, lli** adjacencyMatrix, lli numVertices, lli *labels = nullptr) {
    for (lli vertex = 0; vertex < numVertices; ++vertex) {
        if (colors[vertex] == 0) {
            throw std::logic_error("Error: Vertex " + std::to_string(vertex) + "is not colored.");
        }
    }

    for (lli vertex = 0; vertex < numVertices; ++vertex) {
        lli vertexColor = labels == nullptr ? colors[vertex] : colors[labels[vertex]];
        for (lli adjacentVertex = 0; adjacentVertex < numVertices; ++adjacentVertex) {
            lli adjacentColor = labels == nullptr ? colors[adjacentVertex] : colors[labels[adjacentVertex]];
            if (adjacencyMatrix[vertex][adjacentVertex] == 1 && adjacentColor == vertexColor) {
                throw std::logic_error("Error: Vertex " + std::to_string(vertex) + " and vertex " + std::to_string(adjacentVertex) + " have the same color.");
            }
        }
    }    

    std::cout << "The graph is well colored." << std::endl;
}

# endif // IS_WELL_COLORED_H