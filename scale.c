#include <unistd.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>

typedef uint32_t pixel;

static void
nearest_neighbor_H4(pixel *restrict img, float hscale, uint64_t i, uint64_t sw, uint64_t sh, uint64_t dw)
{
    uint64_t xidx;

    // i: row index
    // j: column index

    for (uint64_t j = 0; j < dw; ++j) {
        xidx = hscale * j + 0.5;
        xidx = MAX(0, MIN(xidx, sw));
        img[i * sw + j] = img[i * sw + xidx];
    }
}

static void
nearest_neighbor_V4(pixel *restrict img, float vscale, uint64_t i, uint64_t sw, uint64_t sh, uint64_t dh)
{
    uint64_t yidx;

    // i: column index
    // j: row index

    for (uint64_t j = 0; j < dh; ++j) {
        yidx = vscale * j + 0.5;
        yidx = MAX(0, MIN(yidx, sh));
        img[j * sw + i] = img[yidx * sw + i];
    }
}

static void
trunc_restride_H4(pixel *restrict img, uint64_t i, uint64_t sw, uint64_t dw)
{
    // i: row index
    // j: column index

    for (uint64_t j = 0; j < dw; ++j) {
        img[i * dw + j] = img[i * sw + j];
    }
}

static void
scale_image_4(uint8_t *img, uint64_t sw, uint64_t sh, uint64_t dw, uint64_t dh)
{
    float vscale = (float) ((double) sh / (double) dh);
    float hscale = (float) ((double) sw / (double) dw);

    for (uint64_t i = 0; i < sw; ++i) {
        nearest_neighbor_V4((pixel *) img, vscale, i, sw, sh, dh);
    }

    for (uint64_t i = 0; i < dh; ++i) {
        nearest_neighbor_H4((pixel *) img, hscale, i, sw, sh, dw);
    }

    for (uint64_t i = 0; i < dh; ++i) {
        trunc_restride_H4((pixel *) img, i, sw, dw);
    }
}

int main(int argc, char *argv[])
{
    uint8_t *simg, *timg;
    uint64_t ssz, dsz;
    uint64_t sw, sh, dw, dh;
    int infd, outfd;

    if (argc != 7) {
        fprintf(stderr, "%s <input.rgba> width height newwidth newheight <output.rgba>\n", argv[0]);
        return 1;
    }

    sw    = atoi(argv[2]);
    sh    = atoi(argv[3]);
    dw    = atoi(argv[4]);
    dh    = atoi(argv[5]);
    ssz   = sw * sh * 4;
    dsz   = dw * dh * 4;

    infd  = open(argv[1], O_RDWR);
    outfd = open(argv[6], O_RDWR | O_CREAT | O_TRUNC, 0644);

    if (infd == -1 || outfd == -1) {
        perror("open");
        return 1;
    }

    if (posix_fallocate(outfd, 0, ssz)) {
        perror("fallocate");
        return 1;
    }

    simg = mmap(NULL, ssz, PROT_READ | PROT_WRITE, MAP_SHARED, infd,  0);
    timg = mmap(NULL, ssz, PROT_READ | PROT_WRITE, MAP_SHARED, outfd, 0);

    memcpy(timg, simg, ssz);
    scale_image_4(timg, sw, sh, dw, dh);
    msync(timg, ssz, MS_SYNC);
    munmap(timg, ssz);

    if (ftruncate(outfd, dsz)) {
        perror("ftruncate");
        return 1;
    }

    return 0;
}
