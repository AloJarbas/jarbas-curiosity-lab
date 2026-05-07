#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int next_cell(int left, int center, int right) {
    int pattern = (left << 2) | (center << 1) | right;
    return (30 >> pattern) & 1;
}

int main(int argc, char **argv) {
    int width = argc > 1 ? atoi(argv[1]) : 61;
    int steps = argc > 2 ? atoi(argv[2]) : 24;
    if (width < 3 || steps < 1) {
        fprintf(stderr, "usage: %s [width>=3] [steps>=1]\n", argv[0]);
        return 1;
    }

    unsigned char *current = calloc((size_t)width, 1);
    unsigned char *next = calloc((size_t)width, 1);
    if (!current || !next) {
        fprintf(stderr, "allocation failed\n");
        free(current);
        free(next);
        return 1;
    }

    current[width / 2] = 1;
    for (int step = 0; step < steps; ++step) {
        for (int i = 0; i < width; ++i) {
            putchar(current[i] ? '#' : ' ');
        }
        putchar('\n');

        memset(next, 0, (size_t)width);
        for (int i = 1; i < width - 1; ++i) {
            next[i] = (unsigned char)next_cell(current[i - 1], current[i], current[i + 1]);
        }

        unsigned char *tmp = current;
        current = next;
        next = tmp;
    }

    free(current);
    free(next);
    return 0;
}
