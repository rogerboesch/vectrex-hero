/*
 * image.h — 256x256 QL Mode 8 image data (pure C)
 */
#pragma once

#include <stdint.h>
#include <string.h>

#define IMAGE_SIZE 256
#define IMAGE_PIXELS (IMAGE_SIZE * IMAGE_SIZE)
#define MODE8_BYTES_PER_IMAGE 32768
#define MAX_IMAGES 16

typedef struct {
    char name[64];
    uint8_t pixels[IMAGE_SIZE][IMAGE_SIZE]; /* color index 0-7 */
} QLImage;

static inline void image_init(QLImage *img, const char *name) {
    memset(img, 0, sizeof(*img));
    strncpy(img->name, name, sizeof(img->name) - 1);
}

static inline void image_clear(QLImage *img) {
    memset(img->pixels, 0, sizeof(img->pixels));
}

static inline void image_flip_h(QLImage *img) {
    for (int y = 0; y < IMAGE_SIZE; y++) {
        for (int x = 0; x < IMAGE_SIZE / 2; x++) {
            uint8_t tmp = img->pixels[y][x];
            img->pixels[y][x] = img->pixels[y][IMAGE_SIZE - 1 - x];
            img->pixels[y][IMAGE_SIZE - 1 - x] = tmp;
        }
    }
}

static inline void image_flip_v(QLImage *img) {
    for (int y = 0; y < IMAGE_SIZE / 2; y++) {
        uint8_t tmp[IMAGE_SIZE];
        memcpy(tmp, img->pixels[y], IMAGE_SIZE);
        memcpy(img->pixels[y], img->pixels[IMAGE_SIZE - 1 - y], IMAGE_SIZE);
        memcpy(img->pixels[IMAGE_SIZE - 1 - y], tmp, IMAGE_SIZE);
    }
}

static inline void image_move_up(QLImage *img) {
    uint8_t tmp[IMAGE_SIZE];
    memcpy(tmp, img->pixels[0], IMAGE_SIZE);
    memmove(img->pixels[0], img->pixels[1], (IMAGE_SIZE - 1) * IMAGE_SIZE);
    memcpy(img->pixels[IMAGE_SIZE - 1], tmp, IMAGE_SIZE);
}

static inline void image_move_down(QLImage *img) {
    uint8_t tmp[IMAGE_SIZE];
    memcpy(tmp, img->pixels[IMAGE_SIZE - 1], IMAGE_SIZE);
    memmove(img->pixels[1], img->pixels[0], (IMAGE_SIZE - 1) * IMAGE_SIZE);
    memcpy(img->pixels[0], tmp, IMAGE_SIZE);
}

static inline void image_move_left(QLImage *img) {
    for (int y = 0; y < IMAGE_SIZE; y++) {
        uint8_t first = img->pixels[y][0];
        memmove(&img->pixels[y][0], &img->pixels[y][1], IMAGE_SIZE - 1);
        img->pixels[y][IMAGE_SIZE - 1] = first;
    }
}

static inline void image_move_right(QLImage *img) {
    for (int y = 0; y < IMAGE_SIZE; y++) {
        uint8_t last = img->pixels[y][IMAGE_SIZE - 1];
        memmove(&img->pixels[y][1], &img->pixels[y][0], IMAGE_SIZE - 1);
        img->pixels[y][0] = last;
    }
}

/* Convert to Mode 8 byte pairs (matches Python _image_to_mode8_bytes) */
static inline void image_to_mode8(const QLImage *img, uint8_t *out) {
    static const int col_remap[] = {0, 1, 2, 3, 4, 6, 5, 7};
    int idx = 0;
    for (int y = 0; y < IMAGE_SIZE; y++) {
        for (int x = 0; x < IMAGE_SIZE; x += 4) {
            uint8_t even_byte = 0, odd_byte = 0;
            for (int bit = 0; bit < 4; bit++) {
                int c = col_remap[img->pixels[y][x + bit] & 7];
                int shift = 7 - (bit * 2);
                even_byte |= (((c >> 2) & 1) << shift);
                odd_byte  |= ((c & 1) << shift);
                odd_byte  |= (((c >> 1) & 1) << (shift - 1));
            }
            out[idx++] = even_byte;
            out[idx++] = odd_byte;
        }
    }
}
