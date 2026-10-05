#include <iostream>
#include <string>
#include <stdexcept>
#include "GraphGenerator.h"
#include "../utils/functions/glutInitialize.h"

// Usage: ./a.out n m nPrime [--draw]
// Generates (or loads, if it already exists) the graph in src/data and validates it:
// exactly m edges and nPrime connected components. --draw opens an OpenGL window (small graphs only).
int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " n m nPrime [--draw]" << std::endl;
        return 1;
    }
    lli n = std::stoll(argv[1]);
    lli m = std::stoll(argv[2]);
    lli nPrime = std::stoll(argv[3]);
    bool draw = argc > 4 && std::string(argv[4]) == "--draw";

    GraphGenerator generator;
    try {
        generator.loadIfExistsOrGenerateNewGraph(n, m, nPrime, "../data");
        generator.validateGraph();
    }
    catch (const std::length_error& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    if (draw) {
        glutInitialize(argc, argv);
        generator.setupDrawGraph();
        glutMainLoop();
    }
    return 0;
}
