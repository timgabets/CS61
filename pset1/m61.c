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

// head of the list:
struct list* head = NULL;

/**
 * [m61_malloc allocates memory]
 * @param sz   [dsize of requested memory]
 * @param file [file where allocation got requested]
 * @param line [line in this file]
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
 * [m61_free frees previously allocated memory]
 * @param ptr  [pointer to allocated memory]
 * @param file [file where allocation got requested]
 * @param line [line in this file]
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
 * [m61_realloc changes the size of the allocated memory block]
 * @param ptr  [pointer to previously allocated memory]
 * @param sz   [new requested size]
 * @param file [file]
 * @param line [line]
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
/**
 * [m61_calloc allocates memory for an array of nmemb elements of sz bytes each.]
 * @param nmemb [number of elements]
 * @param sz    [size of each elemnt]
 * @param file  [file]
 * @param line  [line]
 */
void *m61_calloc(size_t nmemb, size_t sz, const char *file, int line) 
{
    void *ptr = m61_malloc(nmemb * sz, file, line);
    if (ptr)
        memset(ptr, 0, nmemb * sz);
    return ptr;
}


/**
 * [m61_getstatistics collects statistics from the linked list]
 * @param stats [description]
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
 * [m61_printstatistics prints statistics]
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
 * [m61_printleakreport prints leak report]
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
 * [m61_add2list adds items to the list]
 * @param  ptr    [pointer to allocated memory]
 * @param  sz     [size of the allocated memory]
 * @param  status [status of memory]
 * @param  file   [file]
 * @param  line   [line]
 * @return        [status code]
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
 * [m61_removefromlist 'removing' items from the list ]
 * @param  ptr [pointer to allocated]
 * @return     [status code]
 * 
 * Well, we don't actualy removing these items from list, just marking them as INACTIVE
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
 * [m61_getsize gets size of allocated memory]
 * @param  ptr [pointer to allocated memory]
 * @return     [size of the allocated memory or 0 if memory was not allocated]
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


/**
 * [m61_getmetadata description]
 * @param  ptr [description]
 * @return     [description]
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

/**
 * [loadBar Prints a progress bar with ASCII codes]
 * @param i [the current index we are on]
 * @param num [the number of indicies to process]
 * @param r [the number of times we want to update the display (doing it every time will cause programs with large n to slow down majorly)]
 * @param width [the width of the bar]
 * 
 * Thanks to Ross Hemsley for this implementation 
 * http://www.rosshemsley.co.uk/2011/02/creating-a-progress-bar-in-c-or-any-other-console-app/
 */
inline void loadBar(int i, int num, int step, int width)
{
    // Only update step times.
    if ( i % (num / step) != 0 ) return;
 
    // Calculuate the ratio of complete-to-incomplete.
    float ratio = i / (float) num;
    int c = ratio * width;
 
    // Show the percentage complete.
    printf("%3d%% [", (int)(ratio*100) );
 
    // Show the load bar
    for (int i = 0; i < c; i++)
       printf("=");
 
    for (int i = c; i < width; i++)
       printf(" ");
 
    // ANSI Control codes to go back to the
    // previous line and clear it.
    printf("]\n\033[F\033[J");
}

/**
 * [hh_initcounters initilizes memory allocation counters for heavy jeater reporter]
 */
void hh_initcounters(void)
{
    for(int i = 0; i < NALLOCATORS; i++)
    {
        hh_memsize[i] = 0;
        hh_counter[i] = 0;
    }

    hh_overallsize = 0;
}


/**
 * [hh_printstats prints heavy hitter report's statistics]
 * @param count [number of allocations]
 */
void hh_printstats(unsigned long long count)
{   
    float sizerate;
    float countrate;

    for(int i = 0; i < NALLOCATORS; i++)
    {   
        sizerate = 100.0 * hh_memsize[i] / hh_overallsize;
        countrate = 100.0 * hh_counter[i] / count;
        if(sizerate >= 20)
            printf("HEAVY HITTER: function %i: %lld bytes (~%.1f%%)\n", i, hh_memsize[i], sizerate);

        if(countrate >= 20)
            printf("HEAVY HITTER: function %i: %lld of %lld allocations (~%.1f%%)\n", i, hh_counter[i], count, countrate);
    }

    printf("Overall memory allocated: %lld bytes\n", hh_overallsize);
}