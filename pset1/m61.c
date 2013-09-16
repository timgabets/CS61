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
#include <limits.h>

// memory status:
#define INACTIVE 0
#define ACTIVE 1
#define FAILED 2

// memory free return statuses:
#define SUCCESS 1
#define FAIL 0
#define INVLDFREE -1
#define NOTINHEAP -2
#define NOTALLOC -3

struct list 
{
    void*           address;    // pointer to allocated memory 
    int             status;     // 0 is inactive, 1 is active, 2 is failed
    size_t          size;       // size of allocated memory
    char            file[32];   // file
    int             line;       // line in a file
    struct list*    next;       // next item in the list
};

static struct list* head = NULL;

int m61_add2list(void* ptr, size_t sz, int status, const char* file, int line);
int m61_removefromlist(void* ptr);
size_t m61_getsize(void* ptr);

void *m61_malloc(size_t sz, const char *file, int line) 
{
    (void) file, (void) line;   // avoid uninitialized variable warnings
    char* ptr;

    // we suppose that size_t is unsigned long
    if( sz < ( ULONG_MAX - 2 ) ) 
        ptr = malloc(sz + 30);      // allocating some more butes for boundary access detection
                                    // by the way, only 2 bytes will be used for checking.                  
    else
        ptr = NULL;               

    // two additional bytes will contain 0x4c value. After freeing the memory, these bytes
    // will be checked again. If these bytes contain some other data, it means thah
    //  boundary write error had happened.
    if(ptr != NULL)
    {
        char* temp = (char*) ptr;
        temp[sz] = 0x4c;
        temp[sz + 1] = 0x4c;
    }

    if(ptr != NULL)
        m61_add2list(ptr, sz, ACTIVE, file, line);
    else
        m61_add2list(ptr, sz, FAILED, file, line);

    return ptr;
}

void m61_free(void *ptr, const char *file, int line) 
{
    (void) file, (void) line;   // avoid uninitialized variable warnings

    size_t sz = m61_getsize(ptr);
    int rmstatus = m61_removefromlist(ptr);

    if(rmstatus == SUCCESS)
    {
        char* temp = (char*) ptr;
        // detecting boundary write:
        if( temp[sz] == 0x4c && temp[sz + 1] == 0x4c)
        {
            free(ptr);
        }
        else    // memory was written beyond the actual dimensions of an allocated memory block.
            fprintf( stderr, "MEMORY BUG: %s:%d: detected wild write during free of pointer %p\n", file, line, ptr);
    }
    else    
    {
        if(rmstatus == NOTINHEAP) 
            fprintf( stderr, "MEMORY BUG: %s:%d: invalid free of pointer %p, not in heap\n", file, line, ptr);
    
        if(rmstatus == INVLDFREE)
            fprintf(stderr, "MEMORY BUG: %s:%d: invalid free of pointer %p\n", file, line, ptr);
    
        if(rmstatus == NOTALLOC)
            fprintf(stderr, "MEMORY BUG: %s:%d: invalid free of pointer %p, not allocated\n", file, line, ptr);
    }

}

void *m61_realloc(void *ptr, size_t sz, const char *file, int line) 
{
    void *new_ptr = NULL;
    
    if (sz)
        new_ptr = m61_malloc(sz, file, line);
    else
    {
        // reallocating zero bytes of memory? Okay!
        m61_free(ptr, file, line);
        return NULL;
    }

    if(head != NULL)
    {
        struct list* temp = head;
        while(temp -> next != NULL)
        {
            if(temp -> address == ptr)
            {
                memcpy(new_ptr, ptr, temp -> size);
                m61_free(ptr, file, line);
            }


            temp = temp -> next;
        }
    }   
    return new_ptr;
}

void *m61_calloc(size_t nmemb, size_t sz, const char *file, int line) 
{
    // Your code here (to fix test010).
    void *ptr = m61_malloc(nmemb * sz, file, line);
    if (ptr)
        memset(ptr, 0, nmemb * sz);
    return ptr;
}

void m61_getstatistics(struct m61_statistics* stats)
{
    
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

void m61_printleakreport(void) 
{
    // TODO: Your code here.

    if(head != NULL)
    {
        struct list* temp = head;
        
        // running through the linked list
        while(temp -> next != NULL)
        {
            if(temp -> status == ACTIVE)
                printf("LEAK CHECK: %s:%d: allocated object %p with size %lu\n", temp -> file, temp -> line, temp -> address, temp -> size);

            temp = temp -> next;
        }
    }
}

// adding items to the list
int m61_add2list(void* ptr, size_t sz, int status, const char* file, int line)
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
        strncpy(tail -> file, file, 32);
        tail -> line = line;
        tail -> next = NULL;       

        return SUCCESS;   // success
    }
    else
    {
        head = malloc( sizeof(struct list) );
        head -> address = ptr;
        head -> size = sz;
        head -> status = status;
        strncpy(head -> file, file, 32);
        head -> line = line;
        head -> next = NULL;

        return SUCCESS; // success
    }       
}

// we don't actualy removing items from list, just marking them as INACTIVE
int m61_removefromlist(void* ptr)
{
    // running through the linked list to find needed pointer
    if(head != NULL)
    {
        struct list* temp = head;
        while(temp -> next != NULL)
        {
            if(temp -> address == ptr)
                break;
            temp = temp -> next;
        }
    
        // at this point we either at the tail or at the needed item:
        if(temp -> address == ptr)
        {
            if(temp -> status == ACTIVE)
            {
                temp -> status = INACTIVE;
                temp -> address = NULL;
                return SUCCESS;
            }
            else    // memory was already freed
            {
                return INVLDFREE;  // MEMORY BUG???: invalid free of pointer ???
            }
        }else
        {
            return NOTALLOC;
        }
        
    }
    else // no memory was allocated 
    {
        // TODO: checking the type of the poiner - is it from heap or from stack.
        return NOTINHEAP;     // MEMORY BUG???: invalid free of pointer ???, not allocated
    }
}

size_t m61_getsize(void* ptr)
{
    if(ptr != NULL && head != NULL)
    {
        struct list* temp = head;
        while(temp -> next != NULL)
        {
            if(temp -> address == ptr)
                break;
            temp = temp -> next;
        }
    
        return temp -> size;
    }
    else 
        return 0;
}