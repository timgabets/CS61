#include "io61.h"

// Usage: ./ostridecat61 [-b BLOCKSIZE] [-s STRIDE] [FILE]
//    Copies the input FILE to standard output in blocks, shuffling its
//    contents. Reads FILE sequentially, but writes to standard output in a
//    strided access pattern. Default BLOCKSIZE is 1 and default STRIDE is
//    1024. This means the output file's bytes are written in the sequence
//    0, 1024, 2048, ..., 1, 1025, 2049, ..., etc.

int main(int argc, char** argv) {
    // Parse arguments
    size_t blocksize = 1;
    size_t stride = 1024;
    while (argc >= 3) {
        if (strcmp(argv[1], "-b") == 0) {
            blocksize = strtoul(argv[2], 0, 0);
            argc -= 2, argv += 2;
        } else if (strcmp(argv[1], "-s") == 0) {
            stride = strtoul(argv[2], 0, 0);
            argc -= 2, argv += 2;
        } else
            break;
    }

    // Allocate buffer, open files, measure file sizes
    assert(blocksize > 0);
    char* buf = malloc(blocksize);

    const char* in_filename = argc >= 2 ? argv[1] : NULL;
    io61_profile_begin();
    io61_file* inf = io61_open_check(in_filename, O_RDONLY);

    size_t inf_size = io61_filesize(inf);
    if ((ssize_t) inf_size < 0) {
        fprintf(stderr, "ostridecat61: input file is not seekable\n");
        exit(1);
    }

    io61_file* outf = io61_fdopen(STDOUT_FILENO, O_WRONLY);
    if (io61_seek(outf, 0) < 0) {
        fprintf(stderr, "ostridecat61: output file is not seekable\n");
        exit(1);
    }

    // Copy file data
    size_t pos = 0, written = 0;
    while (written < inf_size) {
        // Copy a block
        ssize_t amount = io61_read(inf, buf, blocksize);
        if (amount <= 0)
            break;
        io61_write(outf, buf, amount);
        written += amount;

        // Move `outf` file position to next stride
        pos += stride;
        if (pos >= inf_size) {
            pos = (pos % stride) + blocksize;
            if (pos + blocksize > stride)
                blocksize = stride - pos;
        }
        int r = io61_seek(outf, pos);
        assert(r >= 0);
    }

    io61_close(inf);
    io61_close(outf);
    io61_profile_end();
}
