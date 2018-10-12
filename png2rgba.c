#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <unistd.h>
#include <png.h>

int main(int argc, char *argv[])
{
    size_t width, height, outsize, filesize;
    int out;
    FILE *in;
    png_structp png;
    png_infop info;
    png_bytep rows, *row_ptrs;
    png_byte color_type;
    png_byte bit_depth;

    in  = fopen(argv[1], "rb");
    out = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0644);

    png  = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png_create_info_struct(png);

    if (!png || !info) {
        fprintf(stderr, "Unknown error initializing libpng\n");
        return 1;
    }

    if (!in || out == -1) {
        fprintf(stderr, "File not found\n");
        return 1;
    }

    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "Unknown error processing image\n");
        return 1;
    }

    png_init_io(png, in);
    png_read_info(png, info);

    width      = png_get_image_width(png, info);
    height     = png_get_image_height(png, info);
    color_type = png_get_color_type(png, info);
    bit_depth  = png_get_bit_depth(png, info);
    filesize   = width * height * 4 + height * sizeof(png_bytep);
    outsize    = width * height * 4;

    fprintf(stderr, "PNG %zux%zu\n", width, height);

    if (bit_depth == 16)
        png_set_strip_16(png);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    if (posix_fallocate(out, 0, filesize)) {
        fprintf(stderr, "Failed to allocate output buffer\n");
        return 1;
    }

    rows = (png_bytep) mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, out, 0);
    row_ptrs = (png_bytep *) &rows[outsize];

    for (int i = 0; i < height; ++i) {
        row_ptrs[i] = &rows[i * width * 4];
    }

    png_read_image(png, row_ptrs);

    png_destroy_read_struct(&png, &info, NULL);

    msync(rows, filesize, MS_SYNC);
    munmap(rows, filesize);

    if (ftruncate(out, outsize)) {
        perror("ftruncate");
        return 1;
    }

    fclose(in);
    close(out);
}

