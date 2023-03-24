[![Open in Visual Studio Code](https://classroom.github.com/assets/open-in-vscode-f059dc9a6f8d3a56e377f745f24479a46679e63a5d9fe6f495e02850cd0d8118.svg)](https://classroom.github.com/online_ide?assignment_repo_id=7240162&assignment_repo_type=AssignmentRepo)
# wasm-gc

I implemented a conservative mark-sweep garbage collector as part of a partner project during my Masters studies.  Written in C & C++ and tested in Linux, the garbage collector works for programs that compile C to WebAssembly.  The programs need to use __malloc instead of malloc to allocate memory as that will trigger a callback to our garbage collector with its custom memory allocation.

Garbage collection is a memory management process which automatically frees up unused memory.  Modern systems have their own garbage collectors.  For instance, users don't need to worry about memory allocation or freeing memory in Java.  Java's garbage collector would handle that in the background.

In C, however, users can allocate and free memory themselves.  This can be both a blessing and a potential curse.  The increased cababilities also come with security concerns and potential forgetfulness that can result in unused allocated memory (garbage).

We implemented a conservative mark-sweep GC which runs when allocated memory crosses the threshhold set by environment variable WASM_GC_THRESH (default 0.8).  It keeps track of a used list and a free list organized by different size classes.  For memory allocation it will first check the free list for available space, and if there is none, it will perform bump-pointer allocation. For garbage collection, it uses the mark-sweep approach.  During the mark phase it scans through all memory (stack, heap, registers) for anything that is referenced by a pointer and marks it.  During the sweep phase, it reclaims all unmarked memory (garbage) to the free list.

Additional background on WebAssembly if interested: 
https://people.mpi-sws.org/~rossberg/papers/Haas,%20Rossberg,%20Schuff,%20Titzer,%20Gohman,%20Wagner,%20Zakai,%20Bastien,%20Holman%20-%20Bringing%20the%20Web%20up%20to%20Speed%20with%20WebAssembly.pdf 

To build the project, follow the instructions below. Note: our garbage collector uses the global exports __heap_base and __data_end, so it's important that these stay exported (defined in Makefile).

Work Breakdown:
Thomas: scanning roots and mark function, handle environment and exported variables, documentation, testing
Matt: allocation and sweeping, high level planning, lead documentation and workflow organization, testing

## Instructions

All these instructions have been tested on a Linux machine. However, it should be possible to use a Mac.
Install following requirements:

1. Clone this repository.
1. Install `wasmtime` from https://wasmtime.dev/ 
2. Download `wasmtime-linux-c-api` from https://github.com/bytecodealliance/wasmtime/releases/tag/v0.34.1 . Extract the archive in the directory of this repository.
3. Download `wasi-sdk` from https://github.com/WebAssembly/wasi-sdk/releases. Extract the archive in the directory of this repository.
4. The Makefile will compile and run test.c and wasm-gc.cpp.  Simply enter "make".  Note that the garbage collector will only reclaim memory when used memory crosses a threshhold set by environmental variable WASM_GC_THRESH (default 0.8).  To visualize and test the process, uncomment lines 392-393 under __malloc_callback in wasm-gc.cpp.  This will force garbage collection to run everytime memory is allocated.  In test.c, I tested by writing a function that essentially generates garbage.  It allocates memory to the heap, but will get popped off the stack after execution leaving behind memory with nothing pointing to it (garbage).



