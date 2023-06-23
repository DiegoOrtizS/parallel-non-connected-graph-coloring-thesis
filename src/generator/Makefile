CC = g++
CFLAGS = -std=c++11 -Wall -Wextra
LDFLAGS = -lGL -lGLU -lglut
UTILS_DIR = ../utils

all: a.out

a.out: main.o GraphGenerator.o $(UTILS_DIR)/Graph.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

main.o: main.cpp
	$(CC) $(CFLAGS) -c $<

GraphGenerator.o: GraphGenerator.cpp GraphGenerator.h
	$(CC) $(CFLAGS) -c $<

Graph.o: $(UTILS_DIR)/Graph.cpp $(UTILS_DIR)/Graph.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o a.out
