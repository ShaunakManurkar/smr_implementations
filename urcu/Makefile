
MYDEPS = \
	URCU.hpp \
	LinkedListURCU.hpp \
	QueueURCU.hpp\
	StackURCU.hpp \

bench: $(MYDEPS) bench.cpp Benchmark.hpp
	g++ -g -O3 -std=c++14 bench.cpp -o bench -lstdc++ -lpthread


bench-asan: $(MYDEPS) bench.cpp Benchmark.hpp
	g++-7 -fuse-ld=gold -fsanitize=address -g -O3 -std=c++14 bench.cpp -o bench-asan -lstdc++ -lpthread


all: bench


	
