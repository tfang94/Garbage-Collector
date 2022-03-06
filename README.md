[![Open in Visual Studio Code](https://classroom.github.com/assets/open-in-vscode-f059dc9a6f8d3a56e377f745f24479a46679e63a5d9fe6f495e02850cd0d8118.svg)](https://classroom.github.com/online_ide?assignment_repo_id=7240162&assignment_repo_type=AssignmentRepo)
# wasm-gc

A Conservative GC for WebAssembly.

## Installation

All these instructions have been tested on a Linux machine. However, it should be possible to use a Mac.
Install following requirements:

1. Clone this repository.
1. Install `wasmtime` from https://wasmtime.dev/ 
2. Download `wasmtime-linux-c-api` from https://github.com/bytecodealliance/wasmtime/releases/tag/v0.34.1 . Extract the archive in the directory of this repository.
3. Download `wasi-sdk` from https://github.com/WebAssembly/wasi-sdk/releases. Extract the archive in the directory of this repository.

## Build

Make sure you extracted the archives of wasmtime and wasi in this repository.
To build the test example, go to the parent directory.

1. `make test.wasm`
2. `make wasm-gc`
3. `export LD_LIBRARY_PATH=./wasmtime-v0.34.1-x86_64-linux-c-api/lib:$LD_LIBRARY_PATH`
4. `./wasm-gc`

Running last command should display `array 0 10`


