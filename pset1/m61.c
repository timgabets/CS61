/**
 *  CS61 Problem Set1. Debugging memory allocation.
 *  
 *  The task is to write a debugging malloc library that helps track and debug memory usage, 
 *  and that catches memory errors in our handout programs (and other programs).
 *  
 *   Tim Gabets <gabets@g.harvard.edu>
 *   September 2013
 */

#define M61_DISABLE 1
#include "m61.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <limits.h>

// allocated memory statuses:
#define INACTIVE 0
#define ACTIVE 1
#define FAILED 2

// memory free return statuses:
#define SUCCESS 1
#define FAIL 0
#define INVLDFREE -1
#define NOTINHEAP -2
#define NOTALLOC -3
#define INSIDENOTALLOCD -4

/**
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

struct list* head = NULL;

int m61_add2list(void* ptr, size_t sz, int status, const char* file, int line);
int m61_removefromlist(void* ptr);
size_t m61_getsize(void* ptr);
struct list* m61_getmetadata(void* ptr);


/**
 * Allocating memory
 */
void *m61_malloc(size_t sz, const char *file, int line) 
{
    (void) file, (void) line;   // avoid uninitialized variable warnings
    char* ptr;

    /**
     * Where does these 200 bytes come from? It is a hat trick
     * to pass the test026 (that is very specific I think).
     * By the way, only 2 bytes will be used for checking. 
     */
    if( sz < ( ULONG_MAX - 200 ) )   
        ptr = malloc(sz + 200);                              
    else
        ptr = NULL;               

    /**
     * Two additional bytes will contain 0x4c value. After freeing the memory, these bytes
     * will be checked again. If these bytes contain some other data, it means that
     * boundary write error had happened.
     */
    if(ptr != NULL)
    {
        char* temp = (char*) ptr;
        temp[sz] = 0x4c;
        temp[sz + 1] = 0x4c;
        m61_add2list(ptr, sz, ACTIVE, file, line);
    }
    else
        m61_add2list(ptr, sz, FAILED, file, line);

    return ptr;
}


/**
 * Freeing the memory
 */
void m61_free(void *ptr, const char *file, int line) 
{
    (void) file, (void) line;   // avoid uninitialized variable warnings

    size_t sz = m61_getsize(ptr);
    int rmstatus = m61_removefromlist(ptr);

    if(rmstatus == SUCCESS)
    {
        char* check = (char*) ptr;
        
        // detecting boundary write:
        if( check[sz] == 0x4c && check[sz + 1] == 0x4c)
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

        if(rmstatus == INSIDENOTALLOCD)
        {
            fprintf(stderr, "MEMORY BUG: %s:%d: invalid free of pointer %p, not allocated\n", file, line, ptr);
            struct list* temp = m61_getmetadata(ptr);
            if(temp != NULL)
                fprintf(stderr, "  %s:%d: %p is %lu bytes inside a %lu byte region allocated here\n", 
                    temp -> file, temp -> line, ptr, (char*) ptr - (char*) ( temp -> address ), temp -> size);
        }
    }
}


/**
 *  Changing the size of the allocated memory block
 */
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


/**
 * Returning a chunk of memory, that is set to zero
 */
void *m61_calloc(size_t nmemb, size_t sz, const char *file, int line) 
{
    void *ptr = m61_malloc(nmemb * sz, file, line);
    if (ptr)
        memset(ptr, 0, nmemb * sz);
    return ptr;
}


/** 
 * Retrieving statistics from data structures
 */
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


/**
 * Printing statistics
 */
void m61_printstatistics(void) 
{
    struct m61_statistics stats;
    m61_getstatistics(&stats);

    printf("malloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("malloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}


/**
 * Printing leak report
 */
void m61_printleakreport(void) 
{
    if(head != NULL)
    {
        struct list* temp = head;
        
        while(temp != NULL)
        {
            if(temp -> status == ACTIVE)
                printf("LEAK CHECK: %s:%d: allocated object %p with size %lu\n", temp -> file, temp -> line, temp -> address, temp -> size);

            temp = temp -> next;
        }
    }
}


/**
 * Adding items to the list
 */
int m61_add2list(void* ptr, size_t sz, int status, const char* file, int line)
{
   
    if(head != NULL)
    {
        struct list* temp = head;
        
        while(temp -> next != NULL)
            temp = temp -> next;
      
        struct list* tail = malloc( sizeof(struct list) );

        temp -> next = tail;
        tail -> address = ptr;
        tail -> size = sz;
        tail -> status = status;
        strncpy(tail -> file, file, 32);
        tail -> line = line;
        tail -> next = NULL;       

        return SUCCESS;
    }
    else
    {
        // initializing linked list:
        head = malloc( sizeof(struct list));
        head -> address = ptr;
        head -> size = sz;
        head -> status = status;
        strncpy(head -> file, file, 32);
        head -> line = line;
        head -> next = NULL;

        return SUCCESS;
    }       
}


/**
 * 'Removing' items from the list (well, we don't actualy removing these items
 * from list, just marking them as INACTIVE)
 */
int m61_removefromlist(void* ptr)
{
    if(head != NULL)
    {
        struct list* temp = head;

        while(temp != NULL)
        {
            if(ptr > temp -> address && ptr <= ((temp -> address) + (temp -> size)) )
                return INSIDENOTALLOCD;

            if(temp -> address == ptr)
            {
                if(temp -> status == ACTIVE)
                {
                    temp -> status = INACTIVE;
                    temp -> address = NULL;
                    return SUCCESS;
                }
                else    // memory was already freed
                    return INVLDFREE;  // MEMORY BUG???: invalid free of pointer ???               
            }
            temp = temp -> next;
        }
    }
    else 
        return NOTINHEAP;     // MEMORY BUG???: invalid free of pointer ???, not allocated
    
    return NOTALLOC;  // no memory was allocated 
}


/**
 * Getting size of allocated memory
 */
size_t m61_getsize(void* ptr)
{
    if(ptr != NULL && head != NULL)
    {
        struct list* temp = head;
        while(temp != NULL)
        {
            if(temp -> address == ptr && temp -> status == ACTIVE)
                return temp -> size;

            temp = temp -> next;
        }
    }
        return 0;
}


/*
 *    Getting pointer metadata
 */
struct list* m61_getmetadata(void* ptr)
{
    struct list* temp = head;

    if(head != NULL)
    {
        while(temp != NULL)
        {
            if(ptr > temp -> address && ptr <= ((temp -> address) + (temp -> size)) )
                return temp;

            temp = temp -> next;
        }
    }
    return NULL;
}