#include <iostream>
#include "GraphGenerator.h"
#include "../utils/glutInitialize.h"
#include <stdexcept>

int main(int argc, char** argv) {
    glutInitialize(argc, argv);

    GraphGenerator generator(1e4, 995000, 50, 1, 42);
    generator.generateGraph();
    try {
        generator.validateGraph();
    } 
    catch (const std::length_error& e) {
        std::cout << e.what() << std::endl;
    }

    generator.drawGraph();
    glutMainLoop();

    return 0;
}
