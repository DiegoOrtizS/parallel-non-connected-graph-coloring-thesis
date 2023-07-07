#include <GL/glut.h>

#include "../utils/Graph.h"

class GraphGenerator : public Graph
{
    private:
        static GraphGenerator* currentInstance;
        int m;
        int nPrime;
        float maxDensity;
        int seed;
        float colors[10][3] = {
            {1.0, 1.0, 1.0}, // white
            {1.0, 0.0, 0.0}, // red
            {0.0, 1.0, 0.0}, // green
            {0.0, 0.0, 1.0}, // blue
            {1.0, 1.0, 0.0}, // yellow
            {1.0, 0.0, 1.0}, // magenta
            {0.0, 1.0, 1.0}, // cyan
            {1.0, 0.5, 0.0}, // orange
            {0.5, 0.0, 1.0}, // purple
            {0.0, 0.5, 1.0}  // light blue
        };
        int *colorIndex;
        
        
        static void staticDrawGraph() {
            currentInstance->drawGraph();
        }
        void setupDrawGraph() {
            currentInstance = this;
            glutDisplayFunc(GraphGenerator::staticDrawGraph);
        }

    public:
        GraphGenerator();
        GraphGenerator(int n);
        GraphGenerator(int n, int **graph);
        ~GraphGenerator();
        void setColorIndex(int *colorIndex);
        void generateGraph(int m, int nPrime, float maxDensity, int seed);
        void drawGraph();
        bool validateGraph();
};