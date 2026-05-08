#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double clamp(double x, double lo, double hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

int main(int argc, char **argv) {
    const char *output = argc > 1 ? argv[1] : "art/phyllotaxis-sunflower.svg";
    const int seeds = argc > 2 ? atoi(argv[2]) : 1800;
    const int width = 1200;
    const int height = 1200;
    const double cx = width / 2.0;
    const double cy = height / 2.0 + 30.0;
    const double golden_angle = M_PI * (3.0 - sqrt(5.0));
    const double max_radius = 470.0;
    const double scale = max_radius / sqrt(seeds > 0 ? (double)seeds : 1.0);

    if (seeds < 50) {
        fprintf(stderr, "usage: %s [output.svg] [seeds>=50]\n", argv[0]);
        return 1;
    }

    FILE *out = fopen(output, "w");
    if (!out) {
        perror(output);
        return 1;
    }

    fprintf(out, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %d %d\" width=\"%d\" height=\"%d\">\n", width, height, width, height);
    fprintf(out, "<defs>\n");
    fprintf(out, "  <radialGradient id=\"bg\" cx=\"50%%\" cy=\"46%%\" r=\"70%%\">\n");
    fprintf(out, "    <stop offset=\"0%%\" stop-color=\"#24110c\"/>\n");
    fprintf(out, "    <stop offset=\"55%%\" stop-color=\"#120804\"/>\n");
    fprintf(out, "    <stop offset=\"100%%\" stop-color=\"#050302\"/>\n");
    fprintf(out, "  </radialGradient>\n");
    fprintf(out, "  <radialGradient id=\"halo\" cx=\"50%%\" cy=\"56%%\" r=\"48%%\">\n");
    fprintf(out, "    <stop offset=\"0%%\" stop-color=\"#facc15\" stop-opacity=\"0.24\"/>\n");
    fprintf(out, "    <stop offset=\"70%%\" stop-color=\"#000000\" stop-opacity=\"0\"/>\n");
    fprintf(out, "  </radialGradient>\n");
    fprintf(out, "</defs>\n");
    fprintf(out, "<rect width=\"100%%\" height=\"100%%\" fill=\"url(#bg)\"/>\n");
    fprintf(out, "<rect width=\"100%%\" height=\"100%%\" fill=\"url(#halo)\"/>\n");
    fprintf(out, "<text x=\"70\" y=\"66\" fill=\"#fde68a\" font-size=\"40\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Golden-Angle Phyllotaxis</text>\n");
    fprintf(out, "<text x=\"70\" y=\"96\" fill=\"#fcd34d\" font-size=\"20\" font-family=\"Helvetica, Arial, sans-serif\">One irrational turn angle, a square-root radius, and the seed head packs itself.</text>\n");

    for (int i = 1; i <= seeds; ++i) {
        const double radius = scale * sqrt((double)i);
        const double theta = i * golden_angle;
        const double x = cx + radius * cos(theta);
        const double y = cy + radius * sin(theta);
        const double t = (double)i / seeds;
        const double hue = 46.0 - 18.0 * t;
        const double sat = 88.0 - 20.0 * t;
        const double light = 69.0 - 43.0 * pow(t, 0.9);
        const double dot = clamp(5.2 - 3.4 * t, 1.15, 5.2);
        fprintf(out,
                "<circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" fill=\"hsl(%.2f %.2f%% %.2f%%)\" fill-opacity=\"0.96\"/>\n",
                x, y, dot, hue, sat, light);
    }

    fprintf(out, "</svg>\n");
    fclose(out);
    printf("%s\n", output);
    return 0;
}
