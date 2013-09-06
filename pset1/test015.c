#include "m61.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// File name and line number of wild free.

int main() {
    void *ptr = malloc(2001);
    free((char *) ptr + 100);
    m61_printstatistics();
}

//! MEMORY BUG: test015.c:9: invalid free of pointer ???, not allocated
//! ???
