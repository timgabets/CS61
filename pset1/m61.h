#ifndef M61_H
#define M61_H 1
#include <stdlib.h>
#define NALLOCATORS 40

void *m61_malloc(size_t sz, const char *file, int line);
void m61_free(void *ptr, const char *file, int line);
void *m61_realloc(void *ptr, size_t sz, const char *file, int line);
void *m61_calloc(size_t nmemb, size_t sz, const char *file, int line);

struct m61_statistics {
    unsigned long long nactive;         // # active allocations
    unsigned long long active_size;     // # bytes in active allocations
    unsigned long long ntotal;          // # total allocations
    unsigned long long total_size;      // # bytes in total allocations
    unsigned long long nfail;           // # failed allocation attempts
    unsigned long long fail_size;       // # bytes in failed alloc attempts
};

void m61_getstatistics(struct m61_statistics *stats);
void m61_printstatistics(void);
void m61_printleakreport(void);

/**
 * m61-related globals:
 * 
 * We use linked list to save pointer metadata, which is not really a great idea,
 * because memory for this linked list is allocating from the heap, like the 
 * usual user data. It means that data and metadata are mixed up in the heap. 
 * It works fine in 99% of cases, but sometimes user can smartly overwrite 
 * this metadata (like in test026 ), and then a really bad thing can happen.
 */
struct list 
{
    void*           address;    // pointer to allocated memory 
    int             status;     // 0 is inactive, 1 is active, 2 is failed
    size_t          size;       // size of allocated memory
    char            file[32];   // name of the file, from where allocation requested
    int             line;       // line in the file
    struct list*    next;       // next item in the list
};


/**
 * m61 functions declarations:
 */
int m61_add2list(void* ptr, size_t sz, int status, const char* file, int line);
int m61_removefromlist(void* ptr);
size_t m61_getsize(void* ptr);
struct list* m61_getmetadata(void* ptr);

/**
 * hhtest functions declarations:
 */
void loadBar(int i, int num, int step, int width);
void hh_initcounters(void);
void hh_printstats(unsigned long long count);

/**
 * heavy heater report-related globals:
 */
unsigned long long hh_overallsize;             // size of all allocations
unsigned long long hh_memsize[NALLOCATORS];    // size of memory, allocated by every function
unsigned long long hh_counter[NALLOCATORS];    // number of allocations requested by every function


#if !M61_DISABLE
#define malloc(sz)              m61_malloc((sz), __FILE__, __LINE__)
#define free(ptr)               m61_free((ptr), __FILE__, __LINE__)
#define realloc(ptr, sz)        m61_realloc((ptr), (sz), __FILE__, __LINE__)
#define calloc(nmemb, sz)       m61_calloc((nmemb), (sz), __FILE__, __LINE__)
#endif

#endif
