#include <stdio.h>
#include <stdlib.h>

void *__malloc(size_t sz, void *wasm_stack_base, void *wasm_stack_top);

// void *stack_base;
// void *stack_top;
extern unsigned int __heap_base;
extern unsigned int __data_end;
int *array;
int *array2;

int main()
{
    int s = 33;
    int r = 22;
    int q = 11;
    int w = 99;
    int z = 420;
    int y = 69;
    int x = 42;
    int a = 27;
    void *stack_base;
    stack_base = &a;
    int b = 52;
    void *stack_top;

    stack_top = &b;
    array = (int *)__malloc(3 * sizeof(int), &stack_base, &stack_top);
    *array = 111;

    array2 = (int *)__malloc(2 * sizeof(int), &stack_base, &stack_top);
    *array2 = 222;

    // printf("wasm memory (user): \n");
    // for (int i = 0; i < 10; i++)
    //     printf("array:%p %d\n", array + i, array[i]);

    int *x1 = (int *)&__heap_base;
    int *x2 = (int *)&__data_end;
    // printf("Cfile __heap_base: %p\n", x1);
    // printf("C file __data_end: %p\n", x2);
    // printf("hello\n");

    return 0;
}
