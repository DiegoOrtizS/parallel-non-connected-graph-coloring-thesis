CXX = mpic++
CFLAGS = -std=c++11 -Wall -Wextra
LDFLAGS = -lGL -lGLU -lglut
UTILS_DIR = ../../utils
GENERATOR_DIR = ../../generator

all: a.out

a.out: main.o GraphGenerator.o $(UTILS_DIR)/Graph.o
	$(CXX) $(CFLAGS) -o $@ $^ $(LDFLAGS)

main.o: main.cpp
	$(CXX) $(CFLAGS) -c $<

GraphGenerator.o: $(GENERATOR_DIR)/GraphGenerator.cpp $(GENERATOR_DIR)/GraphGenerator.h
	$(CXX) $(CFLAGS) -c $<

Graph.o: $(UTILS_DIR)/Graph.cpp $(UTILS_DIR)/Graph.h
	$(CXX) $(CFLAGS) -c $<

run:
	mpirun -np ${np} ./a.out

clean:
	rm -f *.o a.out
	rm -f $(UTILS_DIR)/*.o
	rm -f $(GENERATOR_DIR)/*.o