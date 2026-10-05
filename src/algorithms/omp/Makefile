ROOT = ../..
CXX = g++
CXXFLAGS = -std=c++17 -O3 -march=native -Wall -Wextra -fopenmp
include $(ROOT)/common.mk

# algorithm: rsoc (default) or components
run:
	./a.out ${n} ${m} ${nPrime} ${threads} ${algorithm}
