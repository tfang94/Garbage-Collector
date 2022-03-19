#include <stdio.h>
#include <stdlib.h>

void *__malloc(size_t sz, void *wasm_stack_base, void *wasm_stack_top);

void *stack_base;
void *stack_top;

int *array;
int *array2;

int helper()
{
    printf("helper function\n");
    int a = 27; // trying to find a=27 when scanning C stack
    // Allocate memory to trigger __malloc_callback which scans the stack.  a=27 should still be on the stack at this point.
    array2 = (int *)__malloc(2 * sizeof(int), &stack_base, &stack_top);
    *array2 = 333;
    return a;
}

int main()
{
    // void *stack_base;
    // void *stack_top;

    int a = helper();
    printf("a: %d\n", a); // just to make sure int a is relevant and stays on stack

    /* array = (int*)__malloc(sizeof(int)*6, &stack_base, &stack_top); */
    /* for (int i = 0; i < 6; i++)  */
    /* 	array[i] = i*11; */

    // printf("array: %p %d\n", array, array[10]);
    // printf("wasm stack top from user: %p\n", stack_top);
    // printf("wasm stack base from user: %p\n", stack_base);

    /* array2 = (int*)__malloc(sizeof(int)*4, &stack_base, &stack_top); */
    /* for (int i = 0; i < 4; i++)  */
    /* 	array2[i] = i*10; */

    array = (int *)__malloc(3 * sizeof(int), &stack_base, &stack_top);
    *array = 111;

    array2 = (int *)__malloc(2 * sizeof(int), &stack_base, &stack_top);
    *array2 = 222;

    printf("wasm memory (user): \n");
    for (int i = 0; i < 10; i++)
        printf("array:%p %d\n", array + i, array[i]);

    return 0;
}
