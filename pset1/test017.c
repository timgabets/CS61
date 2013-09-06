#include "m61.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>
// Check alignment of returned data.

int main() {
    double *ptr = (double *) malloc(sizeof(double));
    assert((uintptr_t) ptr % __alignof__(double) == 0);
    assert((uintptr_t) ptr % __alignof__(unsigned long long) == 0);
}
