#include <iostream>
#include <numeric>
#include <string>
#include <stdexcept>
#include "GraphGenerator.h"
#include "../utils/functions/glutInitialize.h"

// Number of connected components of an edge list (DSU with path halving, linking by index).
static lli countComponents(lli n, const std::vector<Edge> &edges) {
    std::vector<uint32_t> parent(n);
    std::iota(parent.begin(), parent.end(), 0);
    auto find = [&](uint32_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    lli components = n;
    for (const Edge &edge : edges) {
        uint32_t a = find(edge.first), b = find(edge.second);
        if (a != b) {
            parent[std::max(a, b)] = std::min(a, b);
            components--;
        }
    }
    return components;
}

static void validateEdges(lli n, lli m, lli nPrime, const std::vector<Edge> &edges) {
    if (static_cast<lli>(edges.size()) != m) {
        throw std::length_error("Edge list has " + std::to_string(edges.size()) + " edges, expected " + std::to_string(m));
    }
    lli components = countComponents(n, edges);
    if (components != nPrime) {
        throw std::length_error("Edge list has " + std::to_string(components) + " components, expected " + std::to_string(nPrime));
    }
}

// Usage: ./a.out n m nPrime [--draw | --edges-only]
// Generates (or loads) the graph in src/data and validates it: exactly m edges and nPrime components.
//   default       writes the adjacency matrix "<name>.txt" (v1 programs) and the edge list "<name>.edges" (v2)
//   --edges-only  writes only the edge list, without the n x n matrix, for graphs too large for it
//   --draw        opens an OpenGL window (small graphs only)
int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " n m nPrime [--draw | --edges-only]" << std::endl;
        return 1;
    }
    lli n = std::stoll(argv[1]);
    lli m = std::stoll(argv[2]);
    lli nPrime = std::stoll(argv[3]);
    std::string option = argc > 4 ? argv[4] : "";
    const std::string dir = "../data";

    try {
        if (option == "--edges-only") {
            GraphVariant variant = GraphVariant::fromEnv();
            std::vector<Edge> edges = generateEdgeList(n, m, nPrime, variant);
            validateEdges(n, m, nPrime, edges);
            writeEdgeList(dir + "/" + graphFileName(n, m, nPrime, variant) + ".edges", EdgeFileHeader{n, m, nPrime}, edges);
            std::cout << "Graph is valid." << std::endl;
            return 0;
        }

        GraphGenerator generator;
        generator.loadIfExistsOrGenerateNewGraph(n, m, nPrime, dir);
        generator.validateGraph();
        validateEdges(n, m, nPrime, edgesFromMatrix(n, generator.getGraph()));
        generator.saveEdges(dir);

        if (option == "--draw") {
            glutInitialize(argc, argv);
            generator.setupDrawGraph();
            glutMainLoop();
        }
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
