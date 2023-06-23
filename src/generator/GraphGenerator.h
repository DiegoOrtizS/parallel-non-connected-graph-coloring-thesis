#include <GL/glut.h>
#include "../utils/Graph.h"

class GraphGenerator : Graph
{
    private:
        static GraphGenerator* currentInstance;
        int nPrime;
        float maxDensity;
        int seed;
        
        static void staticDrawGraph() {
            currentInstance->drawGraph();
        }
        void setupDrawGraph() {
            currentInstance = this;
            glutDisplayFunc(GraphGenerator::staticDrawGraph);
        }

    public:
        GraphGenerator(int n, int m, int nPrime, float maxDensity, int seed);
        ~GraphGenerator();
        void generateGraph();
        void drawGraph();
        bool validateGraph();
};