#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void gamma_correct(unsigned char *input, int x, int y) {
    for (int i = 0; i < y; i++) {
        for (int j = 0; j < x; j++) {
            int idx = i * x + j;
            float scaled = input[idx] / 255.0;
            scaled = powf(scaled, 2.2);
            scaled *= 255.0;
            input[idx] = (unsigned char)scaled;
        }
    }
}

unsigned char *bayer_matrix_gen(int k) {
    if (k <= 1) {
        unsigned char *result = malloc(4);
        unsigned char temp[4] = {0, 2, 3, 1};
        memcpy(result, temp, 4);
        return result;
    }
    unsigned char *prev = bayer_matrix_gen(k-1);
    // level 1 is 2x2 
    // level 2 is 4x4
    // ... level n is 2^n x 2^n
    unsigned int size = 1 << k;
    unsigned int prev_size = 1 << (k-1);

    unsigned char *result = malloc(size*size);
    for (unsigned int i = 0; i < size; i++) {
        for (unsigned int j = 0; j < size; j++) {
            unsigned int idx = i * size + j;
            unsigned int prev_idx = (i % prev_size) * prev_size + (j % prev_size);

            if (i < prev_size) {
                if (j < prev_size) {
                    result[idx] = 4 * prev[prev_idx];
                } else {
                    result[idx] = 4 * prev[prev_idx] + 2;
                }
            } else {
                if (j < prev_size) {
                    result[idx] = 4 * prev[prev_idx] + 3;
                } else {
                    result[idx] = 4 * prev[prev_idx] + 1;
                }
            }
        }
    }

    free(prev);
    return result;
}

void bayer(unsigned char *gray_data, int x, int y, int level, int invert) {
    unsigned char *bayer_matrix = bayer_matrix_gen(level);
    int bayer_size = 1 << level;
    printf("bayer_szie = %d\n", bayer_size);

    // Normalize bayer_matrix to 0-255
    for (int i = 0; i < (1 << (level*2)); ++i) {
        bayer_matrix[i] = bayer_matrix[i] * (256 / (1 << (level*2))); 
    }

    for (int i = 0; i < y; ++i) {
        for (int j = 0; j < x; ++j) {
            int pixel_idx = i * x + j;
            int bayer_threshold = bayer_matrix[
                (i % bayer_size) * bayer_size +
                j % bayer_size
            ];
            if (invert)
                bayer_threshold = 256 - bayer_threshold;
            gray_data[pixel_idx] = gray_data[pixel_idx] > bayer_threshold ? 255 : 0;
        }
    }

    free(bayer_matrix);

}

int main(int argc, char *argv[]) {
    char opt;
    char *infile = NULL;
    char *outfile = NULL;
    int level = 1;
    int invert = 0;
    while ((opt = getopt(argc, argv, "i:o:vl:")) != -1) {
        if (opt == 'i') {
            infile = optarg;
        }
        if (opt == 'o') {
            outfile = optarg;
        }
        if (opt == 'v') {
            invert = 1;
        }
        if (opt == 'l') {
            level = atoi(optarg);
        }
    }

    if (infile == NULL) {
        printf("Please specify input files\n");
    }

    char buf[256];
    if (outfile == NULL) {
        strncpy(buf, "output_", 16);
        strncat(buf, infile, 64);
        outfile = buf;
    }

    int image_x, image_y, image_nchannels;
    unsigned char *image_data = stbi_load(infile, &image_x, &image_y, &image_nchannels, 1);
    if (image_data == NULL) {
        printf("Error in reading file %s. Aborting...\n", infile);
        return -1;
    }

    gamma_correct(image_data, image_x, image_y);

    bayer(image_data, image_x, image_y, level, invert);

    stbi_write_png(outfile, image_x, image_y, 1, image_data, image_x);

    stbi_image_free(image_data);
    return 0;
}
