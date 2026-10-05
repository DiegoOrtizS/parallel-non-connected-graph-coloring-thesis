#include <iostream>
#include <string>
#include "ColPackHeaders.h"

// Distance-1 coloring of a Matrix Market graph with ColPack, for each ordering heuristic.
// Prints COLPACK,<ordering>,<colors>. Used as an external baseline for coloring quality.
// Usage: ./driver <graph.mtx>
int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " graph.mtx" << std::endl;
        return 1;
    }
    const std::string orderings[] = {"NATURAL", "LARGEST_FIRST", "SMALLEST_LAST", "DYNAMIC_LARGEST_FIRST", "INCIDENCE_DEGREE"};
    for (const std::string &ordering : orderings) {
        ColPack::GraphColoringInterface graph(SRC_FILE, argv[1], "AUTO_DETECTED");
        graph.Coloring(ordering, "DISTANCE_ONE");
        std::cout << "COLPACK," << ordering << "," << graph.GetVertexColorCount() << std::endl;
    }
    return 0;
}
