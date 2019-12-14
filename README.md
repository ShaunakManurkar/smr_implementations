# smr_implementations

The folder contains multiple safe memory reclamation algorithms for different lock-free data structures. The implementation is done is C++.

## Building
To build the project navigate to any folder. Each foldeer contains a Makefile and do:
> make

## Test
For macOS/linux:
>    ./benchmark (data structure)

For Windows:
 >   ./benchmark.exe (data structure)

You must provide the data structure as an input. The following data structure can be given as input:

    - linkedlist
    - queue
    - stack

Therefore, the command will be:
>    ./benchmark linkedlist

## Output
A sample output will be:
>    numThreads=4,Ops/sec = 2568161, Total unreclaimed nodes = 0

The numThreads will vary from 4-40.