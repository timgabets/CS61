#include "io61.h"

// Usage: ./blockcat61 [-b BLOCKSIZE] [FILE]
//    Copies the input FILE to standard output one block at a time.
//    Default BLOCKSIZE is 4096.

int main(int argc, char** argv) {
    // Parse arguments
    size_t blocksize = 4096;
    if (argc >= 3 && strcmp(argv[1], "-b") == 0) {
        blocksize = strtoul(argv[2], 0, 0);
        argc -= 2, argv += 2;
    }

    // Allocate buffer, open files
    assert(blocksize > 0);
    char* buf = malloc(blocksize);

    const char* in_filename = argc >= 2 ? argv[1] : NULL;
    io61_profile_begin();
    io61_file* inf = io61_open_check(in_filename, O_RDONLY);
    io61_file* outf = io61_fdopen(STDOUT_FILENO, O_WRONLY);

    // Copy file data
    while (1) {
        ssize_t amount = io61_read(inf, buf, blocksize);
        if (amount <= 0)
            break;
        io61_write(outf, buf, amount);
    }

    io61_close(inf);
    io61_close(outf);
    io61_profile_end();
}
