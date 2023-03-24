#include <stdio.h>
#include <stdlib.h>

void *__malloc(size_t sz, void *wasm_stack_base, void *wasm_stack_top);

extern unsigned int __heap_base;
extern unsigned int __data_end;

// This function gets popped off wasm stack after execution so generated memory is garbage.  Use to test __colect_memory()
void generate_garbage(int rep)
{
    int *garb;
    for (int i = 0; i < rep; i++)
    {
        garb = __malloc(2 * (i + 1) * sizeof(int), (void *)&__heap_base, (void *)&__data_end);
        printf("garb instance %d offset: %ld\n", i + 1, (long)garb); // for testing
    }
}

int main()
{
    void *heap_base = (void *)&__heap_base;
    void *data_end = (void *)&__data_end;
    int a = 61; // put some ints on wasm stack
    int b = 63;
    int c = 65;
    int d = 67;
    int e = 69;
    int *p = __malloc(sizeof(int), heap_base, data_end); // put some pointers on wasm stack to test mark function
    int *p1 = __malloc(2 * sizeof(int), heap_base, data_end);
    int *p2 = __malloc(5 * sizeof(int), heap_base, data_end);
    int *p3 = __malloc(10 * sizeof(int), heap_base, data_end);

    generate_garbage(5);

    *p = 17;
    *p2 = 21;
    *p3 = 23;
    int *p4 = __malloc(sizeof(int), heap_base, data_end);

    // // for testing - __mark_memory() prints offset of each item marked
    // printf("p offset: %ld\n", (long)p);
    // printf("p1 offset: %ld\n", (long)p1);
    // printf("p2 offset: %ld\n", (long)p2);
    // printf("p3 offset: %ld\n", (long)p3);
    // printf("p4 offset: %ld\n", (long)p4);

    return 0;
}
