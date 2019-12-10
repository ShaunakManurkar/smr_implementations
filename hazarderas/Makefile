
MYDEPS = \
	hazarderas.hpp \
	LinkedListHazardEras.hpp \
	QueueHazardEras.hpp \
	StackHazardEras.hpp \
#	hazardpointer.hpp \
	LinkedListHazardPointer.hpp \
	QueueHazardPointer.hpp \
	StackHazardPointer.hpp \

bench: $(MYDEPS) Benchmark.cpp
	g++ -g -O3 -std=c++14 Benchmark.cpp -o benchmark -lstdc++ -lpthread


bench-asan: $(MYDEPS) Benchmark.cpp
	g++-7 -fuse-ld=gold -fsanitize=address -g -O3 -std=c++14 Benchmark.cpp -o benchmark-asan -lstdc++ -lpthread


all: benchmark