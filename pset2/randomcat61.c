#include "io61.h"

// Usage: ./randomcat61 [-b MAXBLOCKSIZE] [-S SEED] [FILE]
//    Copies the input FILE to standard output in blocks. Each block has a
//    random size between 1 and MAXBLOCKSIZE (which defaults to 4096).

int main(int argc, char** argv) {
    // Parse arguments
    size_t max_blocksize = 4096;
    srandom(83419);
    while (argc >= 3) {
        if (strcmp(argv[1], "-b") == 0) {
            max_blocksize = strtoul(argv[2], 0, 0);
            argc -= 2, argv += 2;
        } else if (strcmp(argv[1], "-S") == 0) {
            srandom(strtoul(argv[2], 0, 0));
            argc -= 2, argv += 2;
        } else
            break;
    }

    // Allocate buffer, open files
    assert(max_blocksize > 0);
    char* buf = malloc(max_blocksize);

    const char* in_filename = argc >= 2 ? argv[1] : NULL;
    io61_profile_begin();
    io61_file* inf = io61_open_check(in_filename, O_RDONLY);
    io61_file* outf = io61_fdopen(STDOUT_FILENO, O_WRONLY);

    // Copy file data
    while (1) {
        size_t m = (random() % max_blocksize) + 1;
        ssize_t amount = io61_read(inf, buf, m);
        if (amount <= 0)
            break;
        io61_write(outf, buf, amount);
    }

    io61_close(inf);
    io61_close(outf);
    io61_profile_end();
}
