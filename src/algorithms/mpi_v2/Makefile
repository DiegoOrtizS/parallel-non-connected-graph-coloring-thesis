# v2 does not use the matrix generator nor OpenGL: it reads the binary edge list written by
# src/generator (run the generator first).
CXX = mpic++
CXXFLAGS = -std=c++17 -O3 -march=native -Wall -Wextra

all: a.out

a.out: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

run:
	mpirun -np ${np} ./a.out ${n} ${m} ${nPrime}

clean:
	rm -f a.out

.PHONY: all clean run
