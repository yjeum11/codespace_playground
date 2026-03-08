#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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
    unsigned char *image_data = stbi_load(infile, &image_x, &image_y, &image_nchannels, 1);
    if (image_data == NULL) {
        printf("Error in reading file %s. Aborting...\n", infile);
        return -1;
    }

    bayer_4x4(image_data, image_x, image_y, invert);

    stbi_write_png(outfile, image_x, image_y, 1, image_data, image_x);

    stbi_image_free(image_data);
    return 0;
}
