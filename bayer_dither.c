#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void image_to_gray(unsigned char *output, unsigned char *data, int x, int y, int N) {
    // first try averaging RGB data
    for (int i = 0; i < y; ++i) {
        for (int j = 0; j < x; ++j) {
            int pixel_index = (i*x + j) * N;
            int output_index = i*x + j;
            unsigned char average;
            if (N == 2) {
                // already grayscale
                average = data[pixel_index];
            } else {
                // weights from photoshop or GIMP. doesn't matter too much.
                average = 0.30 * data[pixel_index] + 0.59 * data[pixel_index + 1] + 0.11 * data[pixel_index + 2];
            }
            output[output_index] = average;
        }
    }
}

void gray_to_image(unsigned char *output, unsigned char *data, int x, int y, int N) {
    for (int i = 0; i < y; ++i) {
        for (int j = 0; j < x; ++j) {
            int pixel_index = (i*x + j) * N;
            int gray_index = i*x + j;

            for (int k = 0; k < N-1; ++k) {
                output[pixel_index+k] = data[gray_index];
            }
            output[pixel_index+N-1] = 255;
        }
    }
}

void bayer_4x4(unsigned char *gray_data, int x, int y, int invert) {
    unsigned char bayer_matrix[16] = {
        0, 8, 2, 10,
        12, 4, 14, 6,
        3, 11, 1, 9,
        15, 7, 13, 5
    };

    // Normalize bayer_matrix to 0-255
    for (int i = 0; i < 16; ++i) {
        bayer_matrix[i] = bayer_matrix[i] * (256 / 16); 
    }

    for (int i = 0; i < y; ++i) {
        for (int j = 0; j < x; ++j) {
            int pixel_idx = i * x + j;
            int bayer_threshold = bayer_matrix[
                (j % 4) * 4 +
                i % 4
            ];
            if (invert)
                bayer_threshold = 256 - bayer_threshold;
            gray_data[pixel_idx] = gray_data[pixel_idx] > bayer_threshold ? 255 : 0;
        }
    }

}

int main(int argc, char *argv[]) {
    char opt;
    char *infile = NULL;
    char *outfile = NULL;
    int invert = 0;
    while ((opt = getopt(argc, argv, "i:o:v")) != -1) {
        if (opt == 'i') {
            infile = optarg;
        }
        if (opt == 'o') {
            outfile = optarg;
        }
        if (opt == 'v') {
            invert = 1;
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
    unsigned char *image_data = stbi_load(infile, &image_x, &image_y, &image_nchannels, 0);
    if (image_data == NULL) {
        printf("Error in reading file %s. Aborting...\n", infile);
        return -1;
    }
    // just store 1 byte per pixel for grayscale
    unsigned char *gray_data = malloc(sizeof(unsigned char) * image_x * image_y);

    // Convert to grayscale (and compress data)
    image_to_gray(gray_data, image_data, image_x, image_y, image_nchannels);

    bayer_4x4(gray_data, image_x, image_y, invert);

    // Convert back to raw image data from my compressed data
    gray_to_image(image_data, gray_data, image_x, image_y, image_nchannels);
    stbi_write_png(outfile, image_x, image_y, image_nchannels, image_data, image_x * image_nchannels);

    stbi_image_free(image_data);
    free(gray_data);
    return 0;
}
