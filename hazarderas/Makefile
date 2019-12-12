
MYDEPS = \
	hazarderas.hpp \
	LinkedListHazardEras.hpp \
	QueueHazardEras.hpp \
	StackHazardEras.hpp \

bench: $(MYDEPS) Benchmark.cpp
	g++ -g -O3 -std=c++14 Benchmark.cpp -o benchmark -lstdc++ -lpthread

all: benchmark