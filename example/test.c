#include<stdio.h>
#include<stdlib.h>

void* __malloc(size_t sz);

int* array;

int main()
{
	array = (int*)__malloc(sizeof(int)*100);

	for (int i = 0; i < 100; i++) {
		array[i] = i;
	}
	printf("array: %p %d\n", array, array[10]);

	return 0;
}
