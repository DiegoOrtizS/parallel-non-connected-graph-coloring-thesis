#include <GL/glut.h>
#include <iostream>
#include "GraphGenerator.h"
#include <stdexcept>

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
    glutInitWindowSize(500, 500);
    glutCreateWindow("Graph Visualization");
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 1.0, 1.0, 10.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0.0, 0.0, 3.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

    GraphGenerator generator(4, 2, 2, 0.9, 42);
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
