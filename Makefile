WASI_SDK=wasi-sdk-14.0/
CLANG=$(WASI_SDK)/bin/clang 
WASM_TIME_C_API=$(wildcard wasmtime-v0.34.1-*-c-api/)
ifeq ($(shell uname -s),Darwin)
   LDPATH=DYLD_LIBRARY_PATH
else
   LDPATH=LD_LIBRARY_PATH
endif

CXX=g++

all: test.wasm wasm-gc
	$(LDPATH)=$(WASM_TIME_C_API)lib ./wasm-gc

clean:
	- rm -f test.wasm wasm-gc

test.wasm: example/test.c
	$(CLANG) $< -o $@ -Wl,--allow-undefined -Wl,--export=array -O0

wasm-gc: src/wasm-gc.cpp
	$(CXX) $< -I$(WASM_TIME_C_API)/include/ -L$(WASM_TIME_C_API)/lib -lwasmtime -o $@
