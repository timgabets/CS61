/**
 * CS61 Problem Set 2. IO61 implementation.
 * 
 * Tim Gabets <gabets@g.harvard.edu>
 * October 2013
 */

#include "io61.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

#define SLOTSNUMBER 8     // the number of cache slots
#define DATASIZE    32   // the size of each data slot in bytes
#define TRUE        1
#define FALSE       0

int CURRENT_CACHE_SLOT = 0;
time_t expiry_time = 10; 

typedef struct cacheslot{
    void*       address;            // address on primary storage
    unsigned    offset;
    char        data[DATASIZE];     // 
    int         dirty;              // is 1 if data in the cache slot is not written on disk
    time_t      timestamp;          // time of last modification of the slot
}cacheslot;

cacheslot* io61_evicting(void);

// our cache is an array of cacheslots:
cacheslot cache[SLOTSNUMBER];

struct io61_file {
    int         fd;
};


// io61_fdopen(fd, mode)
//    Return a new io61_file that reads from and/or writes to the given
//    file descriptor `fd`. `mode` is either O_RDONLY for a read-only file
//    or O_WRONLY for a write-only file. You need not support read/write
//    files.

io61_file* io61_fdopen(int fd, int mode) {
    
    assert(fd >= 0);
    io61_file* f = (io61_file*) malloc(sizeof(io61_file));

    f -> fd = fd;

    (void) mode;
    return f;
}


// io61_close(f)
//    Close the io61_file `f`.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    free(f);
    return r;
}


/**
 * [io61_readc reading one character from a given file]
 * @param  f [file]
 * @return   [EOF ]
 *
 * For effective one-character reading, we use prefetching policy.
 */
int io61_readc(io61_file* f) {



   unsigned char buf[1];

    if (read(f -> fd, buf, 1) == 1)
        return buf[0];
    else
        return EOF;
}


// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success or
//    -1 on error.

int io61_writec(io61_file* f, int ch) {
    
    // checking is there needed page in a cache:
    for(int i = 0; i < SLOTSNUMBER; i++)
    {
        if(cache[i].address == f)
        {
            if(cache[i].offset < DATASIZE - 1)  // if there is a place in a slot
            {
                cache[i].data[ ++cache[i].offset ] = ch;
                return 0;

            }else        // no space left in the slot. Flushing
            {
                io61_flush(cache[i].address);
                io61_writec(f, ch);
                return 0;
            }

        }
    }

    // nothing found in a cache. Requesting a new cache slot:
    cacheslot* s = io61_evicting();
    
    // filling the cache slot:
    s -> address = f;
    s -> offset = 0;
    s -> dirty = 1;
    s -> data[0] = ch;

    return 0;
 /*   
    unsigned char buf[1];
    buf[0] = ch;

    if (write(f -> fd, buf, 1) == 1)
        return 0;
    else
        return -1;
*/
}


// io61_flush(f)
//    Forces a write of any `f` buffers that contain data.

int io61_flush(io61_file* f) {
    (void) f;

    for(int i = 0; i < SLOTSNUMBER; i++)
    if(cache[i].address == f)
    {
        if( write(f -> fd, cache[i].data, cache[i].offset + 1) == -1)
            return -1;
        else
        {
            cache[i].address = NULL;
            return 0;
        }
    }

    // f wasn't found in the cache
    return -1;
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

/**
 * [io61_evicting description]
 * @return []
 *
 * Round-robin evicting policy for cache pages
 */
cacheslot* io61_evicting()
{
    CURRENT_CACHE_SLOT++;
    CURRENT_CACHE_SLOT %= SLOTSNUMBER;

    if( cache[CURRENT_CACHE_SLOT].dirty == 1 
        && cache[CURRENT_CACHE_SLOT].address != NULL)    
        // this slot contains data that is not written to disk.
        // need to flush first
        io61_flush(cache[CURRENT_CACHE_SLOT].address);

        return &cache[CURRENT_CACHE_SLOT];
}