#ifndef IO61_H
#define IO61_H
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct io61_file io61_file;

io61_file* io61_fdopen(int fd, int mode);
io61_file* io61_open_check(const char* filename, int mode);
int io61_close(io61_file* f);

ssize_t io61_filesize(io61_file* f);

int io61_seek(io61_file* f, size_t pos);

int io61_readc(io61_file* f);
int io61_writec(io61_file* f, int ch);

ssize_t io61_read(io61_file* f, char* buf, size_t sz);
ssize_t io61_write(io61_file* f, const char* buf, size_t sz);

int io61_flush(io61_file* f);

void io61_profile_begin(void);
void io61_profile_end(void);

#endif
