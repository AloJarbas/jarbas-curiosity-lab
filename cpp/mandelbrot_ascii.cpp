#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        std::cout << "usage: " << argv[0] << " [width] [height]\n";
        std::cout << "render the Mandelbrot set as ASCII\n";
        return 0;
    }

    const int width = argc > 1 ? std::atoi(argv[1]) : 100;
    const int height = argc > 2 ? std::atoi(argv[2]) : 36;
    if (width < 2 || height < 2) {
        std::cerr << "width and height must both be at least 2\n";
        return 1;
    }

    const std::string palette = " .,:;ox%#@";

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double cr = -2.2 + 3.0 * x / (width - 1.0);
            const double ci = -1.2 + 2.4 * y / (height - 1.0);
            double zr = 0.0;
            double zi = 0.0;
            int iter = 0;
            const int max_iter = 80;

            while (zr * zr + zi * zi <= 4.0 && iter < max_iter) {
                const double next_zr = zr * zr - zi * zi + cr;
                zi = 2.0 * zr * zi + ci;
                zr = next_zr;
                ++iter;
            }

            const int idx = iter == max_iter ? (int)palette.size() - 1
                                             : iter * ((int)palette.size() - 1) / max_iter;
            std::cout << palette[idx];
        }
        std::cout << '\n';
    }

    return 0;
}
