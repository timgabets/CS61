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

struct list* head = NULL;

void *m61_malloc(size_t sz, const char *file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings

    void* ptr = malloc(sz);

    if(ptr != NULL)
        m61_add2list(ptr, sz, ACTIVE);
    else
        m61_add2list(ptr, sz, FAILED);

    return ptr;
}

void m61_free(void *ptr, const char *file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings

    m61_removefromlist(ptr);

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

void m61_getstatistics(struct m61_statistics* stats){
    
    stats -> nactive = 0;        
    stats -> active_size = 0;    
    stats -> ntotal = 0;         
    stats -> total_size = 0;     
    stats -> nfail = 0;          
    stats -> fail_size = 0;
    

    if(head != NULL)
    {
        struct list* temp = head;

        do{
            /*
            struct m61_statistics {
                unsigned long long nactive;         // # active allocations
                unsigned long long active_size;     // # bytes in active allocations
                unsigned long long ntotal;          // # total allocations
                unsigned long long total_size;      // # bytes in total allocations
                unsigned long long nfail;           // # failed allocation attempts
                unsigned long long fail_size;       // # bytes in failed alloc attempts
                };
            */
            if(temp -> status == ACTIVE)
            {
                stats -> nactive++;    
                stats -> total_size += temp -> size;
                stats -> active_size += temp -> size;    
                stats -> ntotal++;
            }

            if(temp -> status == INACTIVE)
            {
                stats -> total_size += temp -> size;
                stats -> ntotal++;
            }

            if(temp -> status == FAILED)
            {
                stats -> nfail++;
                stats -> fail_size += temp -> size;
            }
            
            temp = temp -> next;

        }while(temp != NULL);
    }    
}

void m61_printstatistics(void) 
{
    struct m61_statistics stats;
    m61_getstatistics(&stats);

    printf("malloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("malloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}

void m61_printleakreport(void) {
    // Your code here.
}

// adding items to the list
 void m61_add2list(void* ptr, size_t sz, int status)
 {
   
    if(head != NULL)
    {
        struct list* temp = head;
        
        while(temp -> next != NULL)
            temp = temp -> next;
      
        // at this point temp is tail (the last item in the list)
        struct list* tail = malloc( sizeof(struct list) );

        temp -> next = tail;
        tail -> address = ptr;
        tail -> size = sz;
        tail -> status = status;
        tail -> next = NULL;       

    }
    else
    {
        head = malloc( sizeof(struct list) );
        head -> address = ptr;
        head -> size = sz;
        head -> status = status;
        head -> next = NULL;

    }       
}

// we don't actualy removing items from list, just marking them as INACTIVE
void m61_removefromlist(void* ptr)
{
    // running through the linked list to find needed pointer
    struct list* temp = head;

    while(temp -> address != ptr)
        if(temp -> next != NULL)
            temp = temp -> next;
    
    // at this point we either at the tail or at the needed item:
    if(temp -> next != NULL)
        temp -> status = INACTIVE;

}