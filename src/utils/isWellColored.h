#include <stdexcept>
#include <iostream>

bool isWellColored(int* colors, int** adjacencyMatrix, int numVertices) {
    // Step 1: Verify the number of color vertices
    for (int vertex = 0; vertex < numVertices; ++vertex) {
        if (colors[vertex] == 0) {
            return false;
        }
    }

    // Step 2: Check color conflicts for adjacent vertices
    for (int vertex = 0; vertex < numVertices; ++vertex) {
        int vertexColor = colors[vertex];
        for (int adjacentVertex = 0; adjacentVertex < numVertices; ++adjacentVertex) {
            if (adjacencyMatrix[vertex][adjacentVertex] == 1 && colors[adjacentVertex] == vertexColor) {
                throw std::logic_error("Error: Vertex " + std::to_string(vertex) + " and vertex " + std::to_string(adjacentVertex) + " have the same color.");
            }
        }
    }

    std::cout << "The graph is well colored." << std::endl;
    return true; // The graph is well colored
}
