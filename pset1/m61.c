/*
    CS61 Problem Set1. Debugging memory allocation.

    The task is to write a debugging malloc library that helps track and debug memory usage, 
    and that catches memory errors in our handout programs (and other programs).

    Tim Gabets <gabets@g.harvard.edu>
    September 2013
*/

#define M61_DISABLE 1
#include "m61.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

struct m61_statistics stats;
int statsinit = 0;      // inidicates does statistics initialized (!0) or not 0.

void *m61_malloc(size_t sz, const char *file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings

    struct metadata ptrdata; 
    ptrdata.size = sz;

    void* chunk = malloc(sizeof(struct metadata) + sz);
    memcpy(&chunk, &ptrdata, sizeof(struct metadata));
    
    // TODO: update statistics
    if(statsinit == 0)
        m61_initstatistics();

    stats.nactive++;
    stats.active_size += sz;
    stats.ntotal++;    
    stats.total_size += sz;

    return chunk + sizeof(struct metadata);
}

void m61_free(void *ptr, const char *file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.
    free(ptr);
}

void *m61_realloc(void *ptr, size_t sz, const char *file, int line) {
    void *new_ptr = NULL;
    if (sz)
        new_ptr = m61_malloc(sz, file, line);
    // Oops! In order to copy the data from `ptr` into `new_ptr`, we need
    // to know how much data there was in `ptr`. That requires work.
    // Your code here (to fix test008).
    m61_free(ptr, file, line);
    return new_ptr;
}

void *m61_calloc(size_t nmemb, size_t sz, const char *file, int line) {
    // Your code here (to fix test010).
    void *ptr = m61_malloc(nmemb * sz, file, line);
    if (ptr)
        memset(ptr, 0, nmemb * sz);
    return ptr;
}

void m61_getstatistics() {
    // Stub: set all statistics to enormous numbers
    //memset(stats, 255, sizeof(struct m61_statistics));

    
}

void m61_printstatistics(void) {
    //m61_getstatistics(&stats);

    printf("malloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("malloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}

void m61_printleakreport(void) {
    // Your code here.
}

void m61_initstatistics()
{
    stats.nactive = 0;
    stats.active_size = 0;
    stats.ntotal = 0;
    stats.total_size = 0;
    stats.nfail = 0;
    stats.fail_size = 0;

    statsinit = 1;
}
