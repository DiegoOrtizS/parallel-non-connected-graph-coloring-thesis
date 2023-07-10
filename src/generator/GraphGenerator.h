#include <GL/glut.h>
#include <vector>
#include <array>

#include "../utils/graphs/Graph.h"

class GraphGenerator : public mesquo::Graph
{
    private:
        static GraphGenerator* currentInstance;
        lli m;
        lli nPrime;
        std::vector<std::array<float, 3>> colors;
        lli *colorIndex;
        lli chromaticNumber;
        
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
        GraphGenerator(lli n);
        GraphGenerator(lli n, lli **graph);
        ~GraphGenerator();
        void setColorIndex(lli *colorIndex, lli *colorLabels = nullptr);
        void setChromaticNumber(lli chromaticNumber);
        void generateGraph(lli m, lli nPrime);
        void drawGraph();
        void validateGraph();
        void saveGraph(std::string dir = "../../data");
        bool loadGraph(std::string name, std::string dir = "../../data");
        void loadIfExistsOrGenerateNewGraph(lli n, lli m, lli nPrime, std::string dir = "../../data");
};