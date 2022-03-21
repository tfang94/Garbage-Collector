#include <stdio.h>
#include <stdlib.h>

void *__malloc(size_t sz, void *wasm_stack_base, void *wasm_stack_top);

extern unsigned int __heap_base;
extern unsigned int __data_end;

int main()
{
    void *heap_base = (void *)&__heap_base;
    void *data_end = (void *)&__data_end;
    int a = 61;
    int b = 63;
    int c = 65;
    int d = 67;
    int *p = __malloc(sizeof(int), heap_base, data_end);
    *p = 17;
    int *p1 = __malloc(sizeof(int), heap_base, data_end);
    *p1 = 19;
    int *p2 = __malloc(sizeof(int), heap_base, data_end);
    *p2 = 21;
    int *p3 = __malloc(sizeof(int), heap_base, data_end);
    *p3 = 23;
    int *p4 = __malloc(sizeof(int), heap_base, data_end);
    *p4 = 25;
    // printf("p: %d\n", p);
    // printf("p1: %d\n", p1);
    // printf("p2: %d\n", p2);
    // printf("p3: %d\n", p3);
    // printf("p4: %d\n", p4);

    return 0;
}
