#include "io61.h"

// Usage: ./reordercat61 [-b BLOCKSIZE] [-S SEED] [FILE]
//    Copies the input FILE to standard output in blocks. The blocks
//    are transferred in random order, but the resulting output file
//    should be the same as the input. Default BLOCKSIZE is 4096.

int main(int argc, char** argv) {
    // Parse arguments
    size_t blocksize = 4096;
    srandom(83419);
    while (argc >= 3) {
        if (strcmp(argv[1], "-b") == 0) {
            blocksize = strtoul(argv[2], 0, 0);
            argc -= 2, argv += 2;
        } else if (strcmp(argv[1], "-S") == 0) {
            srandom(strtoul(argv[2], 0, 0));
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
        fprintf(stderr, "reordercat61: input file is not seekable\n");
        exit(1);
    }

    io61_file* outf = io61_fdopen(STDOUT_FILENO, O_WRONLY);
    if (io61_seek(outf, 0) < 0) {
        fprintf(stderr, "reordercat61: output file is not seekable\n");
        exit(1);
    }

    // Calculate random permutation of file's blocks
    size_t nblocks = inf_size / blocksize;
    if (nblocks > (30 << 20)) {
        fprintf(stderr, "reordercat61: file too large\n");
        exit(1);
    } else if (nblocks * blocksize != inf_size) {
        fprintf(stderr, "reordercat61: input file size not a multiple of block size\n");
        exit(1);
    }

    size_t* blockpos = (size_t*) malloc(sizeof(size_t) * nblocks);
    for (size_t i = 0; i < nblocks; ++i)
        blockpos[i] = i;

    // Copy file data
    while (nblocks != 0) {
        // Choose block to read
        size_t index = random() % nblocks;
        size_t pos = blockpos[index] * blocksize;
        blockpos[index] = blockpos[nblocks - 1];
        --nblocks;

        // Transfer that block
        io61_seek(inf, pos);
        ssize_t amount = io61_read(inf, buf, blocksize);
        if (amount <= 0)
            break;
        io61_seek(outf, pos);
        io61_write(outf, buf, amount);
    }

    io61_close(inf);
    io61_close(outf);
    io61_profile_end();
}
