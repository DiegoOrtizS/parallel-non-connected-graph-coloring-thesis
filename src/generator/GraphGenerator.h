#ifndef GRAPH_GENERATOR_H
#define GRAPH_GENERATOR_H

#include <string>
#include <GL/glut.h>
#include <vector>
#include <array>

#include "../utils/graphs/Graph.h"
#include "../utils/functions/graphVariant.h"
#include "../utils/functions/edgeListIO.h"

// Edges of the instance (n, m, nPrime, variant), sorted, with u < v. Defined in GraphGenerator.cpp.
std::vector<Edge> generateEdgeList(lli n, lli m, lli nPrime, const GraphVariant &variant);

class GraphGenerator : public mesquo::Graph
{
    private:
        static GraphGenerator* currentInstance;
        lli m;
        lli nPrime;
        GraphVariant variant = GraphVariant::fromEnv();
        std::vector<std::array<float, 3>> colors;
        lli *colorIndex;
        lli chromaticNumber;
        
        void initializeColors();
        static void staticDrawGraph() {
            currentInstance->drawGraph();
        }
    public:
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
        std::string graphName(lli m, lli nPrime) const;
        void drawGraph();
        void validateGraph();
        void saveGraph(std::string dir = "../../data");
        void saveEdges(std::string dir = "../../data");
        bool loadGraph(std::string name, std::string dir = "../../data");
        void loadIfExistsOrGenerateNewGraph(lli n, lli m, lli nPrime, std::string dir = "../../data");
};

#endif // GRAPH_GENERATOR_H
