#define _GNU_SOURCE // O_TMPFILE
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <unistd.h>
#include <png.h>

int main(int argc, char *argv[])
{
    size_t width, height, filesize, tablesize;
    int in, tbl;
    FILE *out;
    png_structp png;
    png_infop info;
    png_bytep rows, *row_ptrs;

    if (argc != 5) {
        fprintf(stderr, "Usage: %s <input.rgba> width height <output.png>", argv[0]);
    }

    in   = open(argv[1], O_RDONLY, 0644);
    tbl  = open(dirname(argv[1]), O_RDWR | O_TMPFILE, 0644);
    out  = fopen(argv[4], "wb");
    png  = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png_create_info_struct(png);

    width  = atoi(argv[2]);
    height = atoi(argv[3]);

    if (!png || !info) {
        fprintf(stderr, "Unknown error initializing libpng\n");
        return 1;
    }

    if (!out || in == -1 || tbl == -1) {
        fprintf(stderr, "File not found\n");
        return 1;
    }

    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "Unknown error processing image\n");
        return 1;
    }

    filesize  = width * height * 4;
    tablesize = height * sizeof(png_bytep);

    png_init_io(png, out);
    png_set_IHDR(
        png,
        info,
        width, height,
        8,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png, info);

    if (posix_fallocate(tbl, 0, tablesize)) {
        perror("fallocate");
        return 1;
    }

    rows = (png_bytep) mmap(NULL, filesize, PROT_READ, MAP_SHARED, in, 0);
    row_ptrs = (png_bytep *) mmap(NULL, tablesize, PROT_READ | PROT_WRITE, MAP_SHARED, tbl, 0);

    for (size_t i = 0; i < height; ++i) {
        row_ptrs[i] = &rows[i * width * 4];
    }

    png_write_image(png, row_ptrs);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);

    munmap(rows, filesize);
    munmap(row_ptrs, tablesize);

    fclose(out);
    close(in);
    close(tbl);
}

