CXX = mpic++
CXXFLAGS = -std=c++17 -O3 -march=native -Wall -Wextra

all: a.out

a.out: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

run:
	mpirun -np 2 ./a.out

clean:
	rm -f a.out

.PHONY: all clean run
