#include "m61.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// More boundary write error checks #2.

int main() {
    int *array = (int *) malloc(10); // oops, forgot "* sizeof(int)"
    for (int i = 0; i < 10; ++i)
        array[i] = 0;
    free(array);
    m61_printstatistics();
}

//! MEMORY BUG???: detected wild write during free of pointer ???
//! ???
