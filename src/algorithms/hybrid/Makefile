ROOT = ../..
CXX = mpic++
CXXFLAGS = -std=c++17 -O3 -march=native -Wall -Wextra -fopenmp
include $(ROOT)/common.mk

run:
	OMP_NUM_THREADS=${threads} mpirun -np ${np} ./a.out ${n} ${m} ${nPrime} ${threads}
