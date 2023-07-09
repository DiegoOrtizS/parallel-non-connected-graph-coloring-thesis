#include <GL/glut.h>
#include <vector>
#include <array>

#include "../utils/graphs/Graph.h"

class GraphGenerator : public mesquo::Graph
{
    private:
        static GraphGenerator* currentInstance;
        int m;
        int nPrime;
        float maxDensity;
        int seed;
        std::vector<std::array<float, 3>> colors;
        int *colorIndex;
        int chromaticNumber;
        
        void intializeColors();
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
        void setChromaticNumber(int chromaticNumber);
        void generateGraph(int m, int nPrime);
        void drawGraph();
        bool validateGraph();
        void saveGraph(std::string dir = "../../data");
        void loadGraph(std::string name, std::string dir = "../../data");
};