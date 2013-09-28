#include "io61.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>

// profile61.c
//    These profile functions measure how much time and memory are used
//    by your code. The io61_profile_end() function prints a simple
//    report to standard error.

static struct timeval tv_begin;

void io61_profile_begin(void) {
    int r = gettimeofday(&tv_begin, 0);
    assert(r >= 0);
}

void io61_profile_end(void) {
    struct timeval tv_end;
    struct rusage usage, cusage;

    int r = gettimeofday(&tv_end, 0);
    assert(r >= 0);
    r = getrusage(RUSAGE_SELF, &usage);
    assert(r >= 0);
    r = getrusage(RUSAGE_CHILDREN, &cusage);
    assert(r >= 0);

    timersub(&tv_end, &tv_begin, &tv_end);
    timeradd(&usage.ru_utime, &cusage.ru_utime, &usage.ru_utime);
    timeradd(&usage.ru_stime, &cusage.ru_stime, &usage.ru_stime);

    char buf[1000];
    int len = sprintf(buf, "{\"time\":%ld.%06ld, \"utime\":%ld.%06ld, \"stime\":%ld.%06ld, \"maxrss\":%ld}\n",
                      tv_end.tv_sec, (long) tv_end.tv_usec,
                      usage.ru_utime.tv_sec, (long) usage.ru_utime.tv_usec,
                      usage.ru_stime.tv_sec, (long) usage.ru_stime.tv_usec,
                      usage.ru_maxrss + cusage.ru_maxrss);

    // Print the report to file descriptor 100 if it's available. Our
    // `check.pl` test harness uses this file descriptor.
    off_t off = lseek(100, 0, SEEK_CUR);
    int fd = (off != (off_t) -1 || errno == ESPIPE ? 100 : STDERR_FILENO);
    if (fd == STDERR_FILENO)
        fflush(stderr);
    ssize_t nwritten = write(fd, buf, len);
    assert(nwritten == len);
}
