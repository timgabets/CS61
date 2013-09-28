#include "io61.h"

// Usage: ./reverse61 [FILE]
//    Copies the input FILE to standard output one character at a time,
//    reversing the order of characters in the input.

int main(int argc, char** argv) {
    const char* in_filename = argc >= 2 ? argv[1] : NULL;
    io61_profile_begin();
    io61_file* inf = io61_open_check(in_filename, O_RDONLY);
    io61_file* outf = io61_fdopen(STDOUT_FILENO, O_WRONLY);

    ssize_t in_size = io61_filesize(inf);
    if (in_size == -1) {
        fprintf(stderr, "reverse61: can't get size of input file\n");
        exit(1);
    }

    while (in_size != 0) {
        --in_size;
        io61_seek(inf, in_size);
        int ch = io61_readc(inf);
        io61_writec(outf, ch);
    }

    io61_close(inf);
    io61_close(outf);
    io61_profile_end();
}
