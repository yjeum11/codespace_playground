#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void normalize(float *output, unsigned char *input, int x, int y) {
    // float *output = malloc(sizeof(float) * x * y);
    for (int i = 0; i < y; i++) {
        for (int j = 0; j < x; j++) {
            int idx = i * x + j;
            output[idx] = input[idx] / 255.0;
        }
    }
}

void denormalize(unsigned char *output, float *input, int x, int y) {
    for (int i = 0; i < y; i++) {
        for (int j = 0; j < x; j++) {
            int idx = i * x + j;
            output[idx] = (unsigned char) (input[idx] * 255);
        }
    }
}

void gamma_correct(float *input, int x, int y) {
    for (int i = 0; i < y; i++) {
        for (int j = 0; j < x; j++) {
            int idx = i * x + j;
            input[idx] = powf(input[idx], 2.2);
        }
    }
}

float *bayer_matrix_gen(int k) {
    if (k <= 1) {
        float *result = malloc(4 * sizeof(float));
        float temp[4] = {0.0, 2.0, 3.0, 1.0};
        memcpy(result, temp, 4 * sizeof(float));
        return result;
    }
    float *prev = bayer_matrix_gen(k-1);
    // level 1 is 2x2 
    // level 2 is 4x4
    // ... level n is 2^n x 2^n
    unsigned int size = 1 << k;
    unsigned int prev_size = 1 << (k-1);

    float *result = malloc(size*size * sizeof(float));
    for (unsigned int i = 0; i < size; i++) {
        for (unsigned int j = 0; j < size; j++) {
            unsigned int idx = i * size + j;
            unsigned int prev_idx = (i % prev_size) * prev_size + (j % prev_size);

            if (i < prev_size) {
                if (j < prev_size) {
                    result[idx] = 4 * prev[prev_idx];
                } else {
                    result[idx] = 4 * prev[prev_idx] + 2.0f;
                }
            } else {
                if (j < prev_size) {
                    result[idx] = 4 * prev[prev_idx] + 3.0f;
                } else {
                    result[idx] = 4 * prev[prev_idx] + 1.0f;
                }
            }
        }
    }

    free(prev);
    return result;
}

void bayer(float *gray_data, int x, int y, int level, int invert) {
    float *bayer_matrix = bayer_matrix_gen(level);
    int bayer_size = 1 << level;
    float max_value = 1 - 1/(bayer_size * bayer_size);
    printf("bayer max: %f\n", max_value);

    // Normalize bayer_matrix to 0.0-1.0
    for (int i = 0; i < (1 << (level*2)); ++i) {
        bayer_matrix[i] = bayer_matrix[i] / (1 << (level*2)); 
        bayer_matrix[i] -= 0.5f;
    }

    printf("%f %f\n%f %f\n", bayer_matrix[0], bayer_matrix[1], bayer_matrix[2], bayer_matrix[3]);

    for (int i = 0; i < y; ++i) {
        for (int j = 0; j < x; ++j) {
            int pixel_idx = i * x + j;
            float bayer_threshold = bayer_matrix[
                (i % bayer_size) * bayer_size +
                j % bayer_size
            ];
            gray_data[pixel_idx] = gray_data[pixel_idx] + bayer_threshold > 0.5 ? 1.0 : 0.0;
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
    unsigned char *image_data_raw = stbi_load(infile, &image_x, &image_y, &image_nchannels, 1);
    if (image_data_raw == NULL) {
        printf("Error in reading file %s. Aborting...\n", infile);
        return -1;
    }

    float *image_data = malloc(sizeof(float) * image_x * image_y);

    normalize(image_data, image_data_raw, image_x, image_y);

    gamma_correct(image_data, image_x, image_y);

    bayer(image_data, image_x, image_y, level, invert);

    denormalize(image_data_raw, image_data, image_x, image_y);

    stbi_write_png(outfile, image_x, image_y, 1, image_data_raw, image_x);
    free(image_data);
    stbi_image_free(image_data_raw);
    return 0;
}
