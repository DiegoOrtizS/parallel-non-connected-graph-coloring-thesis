#include <iostream>
#include "../../generator/GraphGenerator.h"
#include "../../utils/functions/glutInitialize.h"
#include "../../utils/functions/isWellColored.h"
#include "coloringMPI.h"
#include <fstream>

using namespace std;

int main(int argc, char** argv) {    
    int processId;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);

    GraphGenerator *graphGenerator = new GraphGenerator();

    if (processId == 0) {
        // graphGenerator->setN(1e4);
        // graphGenerator->generateGraph(6245000, 8);
        // graphGenerator->loadGraph("10000 6245000 8");
        graphGenerator->loadGraph("9 7 3");
        // graphGenerator->setN(10);
        // graphGenerator->generateGraph(7, 3);
        graphGenerator->validateGraph();
        graphGenerator->saveGraph();
    }
    
    auto start = MPI_Wtime();
    ColoringResult result = coloringMPI(processId, graphGenerator->getN(), graphGenerator->getGraph(), largestDegreeFirst);
    auto stop = MPI_Wtime();
    if (processId == 0) {        
        // save labels on a txt
        ofstream myfile;
        myfile.open("labels.txt");
        for (int i = 0; i < graphGenerator->getN(); i++) {
            myfile << result.labels[i] << "\n";
        }
        myfile.close();
        myfile.open("colors.txt");
        for (int i = 0; i < graphGenerator->getN(); i++) {
            myfile << result.colors[i] << "\n";
        }
        // isWellColored(result.colors, graphGenerator->getGraph(), graphGenerator->getN(), result.labels);
        std::cout << "Chromatic number:  " << result.chromaticNumber << std::endl;
        std::cout << "Total Time: " << stop - start << " seconds" << std::endl;

        glutInitialize(argc, argv);
        graphGenerator->setColorIndex(result.colors, result.labels);
        graphGenerator->setChromaticNumber(result.chromaticNumber);
        graphGenerator->drawGraph();
        glutMainLoop();
    }
    MPI_Finalize();
    return 0;
}
