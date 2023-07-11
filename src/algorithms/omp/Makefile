CC = g++
CFLAGS = -std=c++11 -Wall -Wextra -fopenmp
LDFLAGS = -lGL -lGLU -lglut
UTILS_GRAPHS_DIR = ../../utils/graphs
UTILS_FUNCTIONS_DIR = ../../utils/functions
GENERATOR_DIR = ../../generator

all: a.out

a.out: main.o GraphGenerator.o $(UTILS_GRAPHS_DIR)/Graph.o InitializeGraph.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

main.o: main.cpp
	$(CC) $(CFLAGS) -c $<

GraphGenerator.o: $(GENERATOR_DIR)/GraphGenerator.cpp $(GENERATOR_DIR)/GraphGenerator.h
	$(CC) $(CFLAGS) -c $<

Graph.o: $(UTILS_GRAPHS_DIR)/Graph.cpp $(UTILS_GRAPHS_DIR)/Graph.h
	$(CC) $(CFLAGS) -c $< -o $@

InitializeGraph.o: $(UTILS_FUNCTIONS_DIR)/initializeGraph.cpp $(UTILS_FUNCTIONS_DIR)/initializeGraph.h
	$(CXX) $(CFLAGS) -c $<

run:
	./a.out ${n} ${m} ${nPrime} ${threads}

clean:
	rm -f *.o a.out
	rm -f $(UTILS_GRAPHS_DIR)/*.o
	rm -f $(GENERATOR_DIR)/*.o