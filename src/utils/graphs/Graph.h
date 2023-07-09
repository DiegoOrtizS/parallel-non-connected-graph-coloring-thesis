#ifndef GRAPH_H
#define GRAPH_H

#include <iostream>

namespace mesquo {
    class Graph
    {
        protected:
            int n;
            int **graph;
        
        public:
            Graph();
            Graph(int n);
            Graph(int n, int **graph);
            ~Graph();
            void setN(int n);
            int getN();
            void setGraph(int **graph);
            int** getGraph();
            friend std::ostream &operator<<(std::ostream &os, const Graph &graph) {
                for (int i = 0; i < graph.n; i++) {
                    for (int j = 0; j < graph.n; j++) {
                        os << graph.graph[i][j] << " ";
                    }
                    os << std::endl;
                }
                return os;
            };
    };
}

#endif // GRAPH_H