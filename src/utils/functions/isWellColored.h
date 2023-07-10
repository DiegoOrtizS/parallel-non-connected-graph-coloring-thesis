#ifndef IS_WELL_COLORED_H
#define IS_WELL_COLORED_H

#include <stdexcept>
#include <iostream>

void isWellColored(lli* colors, lli n, lli** graph, lli *labels = nullptr) {
    for (lli vertex = 0; vertex < n; ++vertex) {
        if (colors[vertex] == 0) {
            throw std::logic_error("Error: Vertex " + std::to_string(vertex) + "is not colored.");
        }
    }

    for (lli vertex = 0; vertex < n; ++vertex) {
        lli vertexLabel = vertex;
        if (labels != nullptr) {
            vertexLabel = labels[vertex];
        }
        for (lli neighbor = 0; neighbor < n; ++neighbor) {
            lli neighborLabel = neighbor;
            if (labels != nullptr) {
                neighborLabel = labels[neighbor];
            }
            if (graph[vertexLabel][neighborLabel] == 1 && colors[vertex] == colors[neighbor]) {
                throw std::logic_error("Error: Vertex " + std::to_string(vertex) + " and vertex " + std::to_string(neighbor) + " have the same color.");
            }
        }
    }

    std::cout << "The graph is well colored." << std::endl;
}

# endif // IS_WELL_COLORED_H