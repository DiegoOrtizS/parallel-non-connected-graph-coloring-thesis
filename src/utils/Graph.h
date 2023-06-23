#pragma once

class Graph
{
    protected:
        int n;
        int m;
        int **graph;
    
    public:
        Graph(int n, int m);
        ~Graph();
};