#include <stdio.h>
#include <stdlib.h>

void* __malloc(size_t sz, void* wasm_stack_base, void* wasm_stack_top);

int* array;
int* array2;


int main()
{
	void* stack_base;
	void* stack_top;
    /*
	array = (int*)__malloc(sizeof(int)*6, &stack_base, &stack_top);
    for (int i = 0; i < 6; i++) 
		array[i] = i*11;
    */
	//printf("array: %p %d\n", array, array[10]);
    //printf("wasm stack top from user: %p\n", stack_top);    
    //printf("wasm stack base from user: %p\n", stack_base);    

    /*
	array2 = (int*)__malloc(sizeof(int)*4, &stack_base, &stack_top);
    for (int i = 0; i < 4; i++) 
		array2[i] = i*1000;
    
	for (int i = 0; i < 10; i++) 
        printf("array:%p %d\n", array+i, array[i]);
    */

    array = (int*)__malloc(sizeof(int), &stack_base, &stack_top);
    *array = 111;
    
    array2 = (int*)__malloc(sizeof(int), &stack_base, &stack_top);
    *array2 = 222;
    
	return 0;
}
