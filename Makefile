# Top-level entry points. Each program also builds on its own from its folder.
#   make build            build every program
#   make smoke            build + run every program on small graphs (what CI runs)
#   make quality GRAPH="src/data/1000 30000 16.txt"   coloring quality and chi bounds (needs networkx)
#   make results LOGS="scripts/results/strong-*.log" N=10000 M=1500000 K=32 OUT=../Tesis_Coloreo_Grafos/data/graph2.csv
#                         aggregate campaign logs into the CSV format read by the thesis

PROGRAMS = src/generator src/algorithms/sequential src/algorithms/omp src/algorithms/mpi src/algorithms/hybrid src/benchmarks/pingpong
PYTHON ?= python3

.PHONY: build smoke quality results clean

build:
	@for dir in $(PROGRAMS); do $(MAKE) -s -C $$dir || exit 1; done

smoke:
	bash scripts/smoke.sh

quality:
	@test -n "$(GRAPH)" || { echo "usage: make quality GRAPH=<graph file>"; exit 1; }
	$(PYTHON) scripts/coloring_quality.py "$(GRAPH)" --csv scripts/results/quality.csv

results:
	@test -n "$(LOGS)" -a -n "$(N)" -a -n "$(M)" -a -n "$(K)" -a -n "$(OUT)" || { echo "usage: make results LOGS=... N=... M=... K=... OUT=..."; exit 1; }
	bash scripts/aggregate.sh $(LOGS) > scripts/results/summary.csv
	bash scripts/aggregate.sh --thesis $(N) $(M) $(K) $(LOGS) > "$(OUT)"
	@echo "summary: scripts/results/summary.csv, thesis data: $(OUT)"

clean:
	@for dir in $(PROGRAMS); do $(MAKE) -s -C $$dir clean; done
