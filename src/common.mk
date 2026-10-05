# Shared build settings. Each program's Makefile sets ROOT (path to src/) and CXX, then includes this file.
# -O3: the original Makefiles had no optimization flag, so every earlier timing was an unoptimized build.
# -std=c++17: the code uses structured bindings.
CXXFLAGS ?= -std=c++17 -O3 -march=native -Wall -Wextra
LDLIBS   ?= -lGL -lGLU -lglut

COMMON_OBJS = GraphGenerator.o Graph.o InitializeGraph.o

all: a.out

a.out: main.o $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

GraphGenerator.o: $(ROOT)/generator/GraphGenerator.cpp $(ROOT)/generator/GraphGenerator.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

Graph.o: $(ROOT)/utils/graphs/Graph.cpp $(ROOT)/utils/graphs/Graph.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

InitializeGraph.o: $(ROOT)/utils/functions/initializeGraph.cpp $(ROOT)/utils/functions/initializeGraph.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o a.out

.PHONY: all clean run
