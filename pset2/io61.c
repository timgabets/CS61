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
#include <sys/mman.h>

#define STANDALONE      -1
#define NUMBEROFSLOTS   8192
#define BUFSIZE         8192
#define SUCCESS         0
#define FAIL            -1
#define TRUE            1
#define FALSE           0

struct io61_file {
    int     fd;
    int     mode;
    int     seq;
    off_t   pos;
};

typedef struct cacheslot{
    io61_file*  address;
    //char        data[BUFSIZE];
    io61_file*  inf;
    io61_file*  outf;
    char*       data;
    size_t      offset;
    size_t      bufsize;
    off_t       pos;        // file position
}cacheslot;

typedef struct filerelation{
    io61_file* inf;     // reading from this file
    io61_file* outf;    // writing to this file
}filerelation;

filerelation rw;       

struct cacheslot cache[NUMBEROFSLOTS];

int cacheready = 0;
int toevict = 0;        // index of the cache slot, that will be evicted next

int io61_getslot(io61_file*);
void io61_cacheinit(void);
int io61_evict(void);
ssize_t io61_write_mmap(io61_file*, const char*, size_t);
ssize_t io61_write_seq(io61_file*, const char*, size_t);
ssize_t io61_read_mmap(io61_file*, char*, size_t);
ssize_t io61_read_seq(io61_file*, char*, size_t);
int io61_sortcache(io61_file*);
void quicksort(int, int);


/**
 * [io61_fdopen return a new io61_file that reads from and/or writes to the given file descriptor `fd`.
 *              We need not support read/write files.]
 * @param  fd   [file descriptor]
 * @param  mode [is either O_RDONLY for a read-only file or O_WRONLY for a write-only file.]
 * @return      [description]
 */
io61_file* io61_fdopen(int fd, int mode) {

    assert(fd >= 0);
    io61_file* f = (io61_file*) malloc(sizeof(io61_file));
    f -> fd = fd;
    f -> mode = mode;
    f -> seq = TRUE;        // file is sequential by default

    if(mode == O_RDONLY)
        rw.inf = f;
    else
        rw.outf = f;        

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
 * [io61_writec writes a single character `ch` to `f`. By design, there is only one cache page is associated with each file]
 * @param  f  [file]
 * @param  ch [character to write]
 * @return    [Returns 0 on success or -1 on error]
 */
int io61_writec(io61_file* f, int ch) 
{

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
}

/**
 * [io61_read description]
 * @param  f   [description]
 * @param  buf [description]
 * @param  sz  [description]
 * @return     [description]
 */
ssize_t io61_read(io61_file* f, char* buf, size_t sz){
    
    if(f -> seq == TRUE)   // sequential file
        return io61_read_seq(f, buf, sz);
    else        // random access file
        return io61_read_mmap(f, buf, sz);
}

/**
 * [io61_read Read up to `sz` characters from `f` into `buf`.]
 * @param  f   [description]
 * @param  buf [description]
 * @param  sz  [description]
 * @return     [Returns the number of characters read on success; normally this is `sz`. 
 *              Returns a short count if the file ended before `sz` characters could be read. 
 *              Returns -1 an error occurred before any characters were read.]
 */
ssize_t io61_read_seq(io61_file* f, char* buf, size_t sz) {

    ssize_t nread;

    for(int i = 0; i < NUMBEROFSLOTS; i++)
    {
        if(cache[i].address == f && cache[i].offset < cache[i].bufsize)
        {   
            if(sz > cache[i].bufsize - cache[i].offset)
                sz = cache[i].bufsize - cache[i].offset;

            memcpy(buf, &cache[i].data[ cache[i].offset ], sz);
            cache[i].offset += sz;

            return sz;
        }
    }

    // nothing found in a cache:
    int i = io61_getslot(f); 
    nread = read(f -> fd, cache[i].data, BUFSIZE);
    cache[i].bufsize = nread;

    // returning the first chunk of read data:
    memcpy(buf, cache[i].data, sz);
    cache[i].offset = sz;

    if (nread == 0 && sz != 0)
        return -1;
    else
        return sz;
}

/**
 * [io61_read_mmap description]
 * @param  f   [description]
 * @param  buf [calling programm expects that this buffer will be used for data interchange, 
 *              but we use it i a different way - buf is storing the pointer to read file structure. 
 *              ]
 * @param  sz  [description]
 * @return     [description]
 */
ssize_t io61_read_mmap(io61_file* f, char* buf, size_t sz) {
    (void) buf;
    (void) f;

    int i = io61_getslot(f);
    cache[i].pos = f -> pos;
    cache[i].data = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, f -> fd, cache[i].pos);
    cache[i].bufsize = sz;

    return sz;
}


/**
 * [io61_write write function wrapper]
 * @param  f   [description]
 * @param  buf [description]
 * @param  sz  [description]
 * @return     [description]
 */ 
ssize_t io61_write(io61_file* f, const char* buf, size_t sz) {

    if(f -> seq == TRUE)   // sequential file
        return io61_write_seq(f, buf, sz);
    else        // random access file
        return io61_write_mmap(f, buf, sz);
}

/**
 * [io61_write_seq writes `sz` characters from `buf` to `f`. Used for sequential files.]
 * @param  f   [description]
 * @param  buf [description]
 * @param  sz  [description]
 * @return     [Returns the number of characters written on success; normally this is `sz`. 
 *              Returns -1 if an error occurred before any characters were written.]
 */
ssize_t io61_write_seq(io61_file* f, const char* buf, size_t sz) {

    for(int i = 0; i < NUMBEROFSLOTS; i++)
    {
        if(cache[i].address == f)
        {
            if(cache[i].offset < cache[i].bufsize)
            {
                if(sz > cache[i].bufsize - cache[i].offset)
                    sz = cache[i].bufsize - cache[i].offset;

                memcpy(&cache[i].data[ cache[i].offset ], buf, sz);
                cache[i].offset += sz;
                return sz;

            }else // flushing needed
            {
                io61_flush(f);
                break;
            }
        }
    }

    int i = io61_getslot(f);
    memcpy(cache[i].data, buf, sz);
    cache[i].offset = sz;

    return sz;
}

/**
 * [io61_write_mmap writes `sz` characters from `buf` to `f`, using mmap IO.
 *                          Used for random access files.]
 * @param  f   [file]
 * @param  buf [buffer to write]
 * @param  sz  [size of buffer]
 * @return     [Returns the number of characters written on success; normally this is `sz`. 
 *              Returns -1 if an error occurred before any characters were written.]
 */ 
ssize_t io61_write_mmap(io61_file* f, const char* buf, size_t sz) {
    (void) buf;
    (void) f;
    return sz;
}

/**
 * [io61_seek change the file pointer for file `f` to `pos` bytes into the file.]
 * @param  f   [description]
 * @param  pos [description]
 * @return     [returns 0 on success and -1 on failure.]
 */
int io61_seek(io61_file* f, size_t pos) {

    off_t r = lseek(f->fd, (off_t) pos, SEEK_SET);
    f -> seq = FALSE;

    if (r == (off_t) pos)
    {
        f -> pos = r;
        return 0;
    }
    else
        return -1;
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

            if(f -> seq == FALSE && f -> mode == O_WRONLY)
                cache[i].data = malloc( sizeof(void*) );
            else
                cache[i].data = malloc(BUFSIZE);

            cache[i].offset = 0;
            cache[i].bufsize = BUFSIZE;
            
            return i;
        }
    }

    // no free slots. Evicting:
    int i = io61_evict();

    // the last rites:   
    cache[i].address = f;
    if(f -> seq == FALSE && f -> mode == O_WRONLY)
        cache[i].data = malloc( sizeof(void*) );
    else
        cache[i].data = malloc(BUFSIZE);
    cache[i].offset = 0;
    cache[i].bufsize = BUFSIZE;
   
    // ready to leave now
    return i;
}

/**
 * [io61_evict applying evicting policy to cache slots]
 * @return  [index of freed cache slot]
 */
int io61_evict(void)
{
    int i = toevict;
    if( (cache[i].address) -> fd == O_WRONLY)
        io61_flush(cache[toevict].address);

    free(cache[i].data);

    toevict++;
    toevict %= NUMBEROFSLOTS;

    return i;
}


/**
 * [io61_cacheinit initializng cache slots]
 */
void io61_cacheinit(void)
{
    for(int i = 0; i < NUMBEROFSLOTS; i++)
    {
        cache[i].address = NULL;
        cache[i].pos = INT_MAX;
    }
}


/**
 * [io61_flush forces a write of any `f` buffers that contain data.]
 * @param  f [description]
 * @return   [description]
 */
int io61_flush(io61_file* f) {
    //(void) f;
    if(f -> mode == O_RDONLY)
        return 0;


    if( f -> seq == TRUE)
    {
        // searching for a slot:
        for(int i = 0; i < NUMBEROFSLOTS; i++)
            if(cache[i].address == f)
            {
                if( write(f -> fd, cache[i].data, cache[i].offset) != -1)
                {
                    cache[i].address = NULL;           
                    free(cache[i].address);
                    return SUCCESS;
                }else
                    return FAIL;
            }
    }else    
    {
        /**
         * file is non-sequential, and the cache slots may contain data in random order.
         * We need to sort cache slots by 'pos' field in increasing order. 
         */
        if( io61_sortcache(f) == SUCCESS )

        lseek(f -> fd, 0, SEEK_SET);
        for(int i = 0; i < NUMBEROFSLOTS; i++)
        {
            if(cache[i].address == rw.inf)
            {
                write(f -> fd, cache[i].data, cache[i].bufsize);
                munmap(cache[i].data, cache[i].bufsize);
                //free(cache[i].data);
            }
        }

        return SUCCESS;
    }

    return FAIL;
}

/**
 * [io61_cachesort sorts the cache array by 'pos' field in increasing order. ]
 * @return [0 on success, -1 on fail]
 */
int io61_sortcache(io61_file* f)
{
    (void) f;
    // sorting cache array:
    quicksort(0, NUMBEROFSLOTS);

    return SUCCESS;
}


/**
 * [quicksort classic quick sort algorithm]
 * @param first [index of the first element in a scope]
 * @param last  [index of the last element inthe scope]
 */
void quicksort(int first, int last)
{
    cacheslot temp;
    int i = first, 
    j = last, 
    x = cache[(first + last) / 2].pos;
 
    do {
        while (cache[i].pos < x) i++;
        while (cache[j].pos > x) j--;
 
        if(i <= j) {
            if (i < j)
            {
                temp = cache[i];
                cache[i] = cache[j];
                cache[j] = temp;
            } 
            i++;
            j--;
        }
    } while (i <= j);
 
    if (i < last)
        quicksort(i, last);
    if (first < j)
        quicksort(first,j);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
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