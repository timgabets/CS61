/**
 * CS61 Problem Set 2.
 * 
 * Tim Gabets <gabets@g.harvard.edu> 
 * October 2013
 */


#include "io61.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

#define STANDALONE      -1
#define NUMBEROFSLOTS   4
#define BUFSIZE         4096
#define SUCCESS         0
#define FAIL            -1
#define TRUE            1
#define FALSE           0

struct io61_file {
    int     fd;
    int     mode;
    int     head;      // STANDALONE if it is the only page, associated with the file
};

typedef struct cacheslot{
    io61_file*  address;
    char        data[BUFSIZE];
    ssize_t     offset;
    int         next;
}cacheslot;

struct cacheslot cache[NUMBEROFSLOTS];

int cacheready = 0;
int toevict = 0;        // index of the cache slot, that will be evicted next

int io61_getslot(io61_file*);
void io61_cacheinit(void);
int io61_evict(io61_file*);

// io61_fdopen(fd, mode)
//    Return a new io61_file that reads from and/or writes to the given
//    file descriptor `fd`. `mode` is either O_RDONLY for a read-only file
//    or O_WRONLY for a write-only file. You need not support read/write
//    files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = (io61_file*) malloc(sizeof(io61_file));
    f -> fd = fd;
    f -> mode = mode;

    f -> head = STANDALONE;

    return f;
}


/**
 * [io61_close close the io61_file `f`]
 * @param  f [file]
 * @return   [description]
 */
int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    free(f);
    return r;
}


/**
 * [io61_readc Read a single (unsigned) character from `f` and return it. 
 *             By design, there is only one cache page is associated with each file]
 * @param  f [file]
 * @return   [EOF (which is -1) on error or end-of-file.]
 */
int io61_readc(io61_file* f) {

    for(int i = 0; i < NUMBEROFSLOTS; i++)
    {
        if(cache[i].address == f)
        {
            if(cache[i].offset < BUFSIZE)
            {
                return cache[i].data[ cache[i].offset++ ];
            }else
            {
                // finished with this slot.
                cache[i].address = NULL;                
                break;
            }
        }
    }

    // nothing found in a cache:
    int i = io61_getslot(f);   
    int bytesread = read(f -> fd, cache[i].data, BUFSIZE);

    if(bytesread == BUFSIZE)
        return cache[i].data[ cache[i].offset++ ];

    // FIXME:
    return EOF;
}


/**
 * [io61_getslot description]
 * @param  f [file]
 * @return   [index of cache slot]
 */
int io61_getslot(io61_file* f)
{
    if(cacheready == 0)
    {
        io61_cacheinit();
        cacheready = 1;
    }

    // looking for unused slots:
    for(int i = 0; i < NUMBEROFSLOTS; i++)
    {
        if(cache[i].address == NULL)
        {    
            cache[i].address = f;
            cache[i].offset = 0;
            
            if(f -> head != STANDALONE)
            {
                // f -> head is storing the head cach slot. 
                // Searching for the tail slot:
                int temp = f -> head;
                while(cache[temp].next != -1)
                    temp = cache[temp].next;
                
                // now 'temp' is the index of the tail slot
                cache[temp].next = i;
            }else
                cache[i].next = STANDALONE;

            return i;
        }
    }

    // no free slots. Evicting:
    int i = io61_evict(f);

    // FIXME:
    // the last rites:   
    cache[i].address = f;
    cache[i].offset = 0;
   
    // ready to leave now
    return i;
}

/**
 * [io61_evict applying evicting policy to cache slots]
 * @return  [index of freed cache slot]
 */
int io61_evict(io61_file* f)
{
    if(f -> mode == O_WRONLY)
        io61_flush(cache[toevict].address);
        
    // no flush
    int index = toevict;

    toevict++;
    toevict %= NUMBEROFSLOTS;

    return index;


}

void io61_cacheinit(void)
{
    for(int i = 0; i < NUMBEROFSLOTS; i++)
    {
        cache[i].address = NULL;
        cache[i].next = STANDALONE;
    }
}

/**
 * [io61_writec writes a single character `ch` to `f`. By design, there is only one cache page is associated with each file]
 * @param  f  [file]
 * @param  ch [character to write]
 * @return    [Returns 0 on success or -1 on error]
 */
int io61_writec(io61_file* f, int ch) {
/*
    for(int i = 0; i < NUMBEROFSLOTS; i++)
    {
        if(cache[i].address == f)
        {
            if(cache[i].offset < BUFSIZE)
            {
                cache[i].data[ cache[i].offset++ ] = ch;
                return SUCCESS;

            }else   // not enought space in a buffer. Requesting the icreasing of a buffer:
            {
                // breaking from the cycle in order to request new slot
                if(f -> head == STANDALONE)
                    f -> head = i;
                
                break;
            }
        }    
    }

    int i = io61_getslot(f);
    cache[i].data[ cache[i].offset++ ] = ch;

    return SUCCESS;
*/

    for(int i = 0; i < NUMBEROFSLOTS; i++)
    {
        if(cache[i].address == f)
        {
            if(cache[i].offset < BUFSIZE)
            {
                cache[i].data[ cache[i].offset++ ] = ch;
                return SUCCESS;
            }else
                break;
        }
    }

    io61_flush(f);
    int i = io61_getslot(f);
    cache[i].data[ cache[i].offset++ ] = ch;

    return SUCCESS;

/*
    unsigned char buf[1];
    buf[0] = ch;
    if (write(f->fd, buf, 1) == 1)
        return 0;
    else
        return -1;
*/
}


// io61_flush(f)
//    Forces a write of any `f` buffers that contain data.

int io61_flush(io61_file* f) {
    (void) f;

    if( f -> head != STANDALONE )
    {
        /*
            // f -> head is storing the head cache slot. 
            int temp = f -> head;
            while(temp != -1)
            {
                write(f -> fd, cache[temp].data, cache[temp].offset);
                    
                cache[temp].address = NULL;
                int i = temp;
                temp = cache[temp].next;   
                cache[i].next = STANDALONE;
            }

            f -> head = STANDALONE;
           */
            return SUCCESS;
 
    }
    else   // there is only one cache slot, associated with this file
    {
        // searching for a slot:
        for(int i = 0; i < NUMBEROFSLOTS; i++)
            if(cache[i].address == f)
            {
                write(f -> fd, cache[i].data, cache[i].offset);
                
                cache[i].address = NULL;
                cache[i].next = -1;             
            }
        return FAIL;
    }
}


// io61_read(f, buf, sz)
//    Read up to `sz` characters from `f` into `buf`. Returns the number of
//    characters read on success; normally this is `sz`. Returns a short
//    count if the file ended before `sz` characters could be read. Returns
//    -1 an error occurred before any characters were read.

ssize_t io61_read(io61_file* f, char* buf, size_t sz) {
    size_t nread = 0;
    while (nread != sz) {
        int ch = io61_readc(f);
        if (ch == EOF)
            break;
        buf[nread] = ch;
        ++nread;
    }
    if (nread == 0 && sz != 0)
        return -1;
    else
        return nread;
}


// io61_write(f, buf, sz)
//    Write `sz` characters from `buf` to `f`. Returns the number of
//    characters written on success; normally this is `sz`. Returns -1 if
//    an error occurred before any characters were written.

ssize_t io61_write(io61_file* f, const char* buf, size_t sz) {
    size_t nwritten = 0;
    while (nwritten != sz) {
        if (io61_writec(f, buf[nwritten]) == -1)
            break;
        ++nwritten;
    }
    if (nwritten == 0 && sz != 0)
        return -1;
    else
        return nwritten;
}


// io61_seek(f, pos)
//    Change the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, size_t pos) {
    off_t r = lseek(f->fd, (off_t) pos, SEEK_SET);
    if (r == (off_t) pos)
        return 0;
    else
        return -1;
}


// You should not need to change either of these functions.

// io61_open_check(filename, mode)
//    Open the file corresponding to `filename` and return its io61_file.
//    If `filename == NULL`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != NULL` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename)
        fd = open(filename, mode);
    else if (mode == O_RDONLY)
        fd = STDIN_FILENO;
    else
        fd = STDOUT_FILENO;
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode);
}


// io61_filesize(f)
//    Return the number of bytes in `f`. Returns -1 if `f` is not a seekable
//    file (for instance, if it is a pipe).

ssize_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode) && s.st_size <= SSIZE_MAX)
        return s.st_size;
    else
        return -1;
}