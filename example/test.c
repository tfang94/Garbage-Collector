#include <stdio.h>
#include <stdlib.h>

void* __malloc(size_t sz);
char* char_array;
int* int_array;

int main()
{
    /*
      allocates space for array. test code version: all __malloc does 
      is return 0. to wasm, addresses are just integers starting at zero
     */
	int_array = (int*)__malloc(sizeof(int)*4);
    char_array = (char*)__malloc(sizeof(char)*5);
    
    //fill in the data starting at zero
	for (int i = 0; i < 5; i++) {
		int_array[i] = 120+i;
	}

	for (int i = 6; i < 10; i++) {
		int_array[i] = 127+i;
	}
    //print out the pointers and the data.
    //note from addresses that these integers take up 4 bytes 
	for (int i = 0; i < 15; i++) {
        printf("array:%p %d\n", int_array+i, int_array[i]);
	}

	return 0;
}
