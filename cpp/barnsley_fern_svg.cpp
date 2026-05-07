#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

struct Point {
    double x;
    double y;
};

static Point step(Point p, double r) {
    if (r < 0.01) return {0.0, 0.16 * p.y};
    if (r < 0.86) return {0.85 * p.x + 0.04 * p.y, -0.04 * p.x + 0.85 * p.y + 1.6};
    if (r < 0.93) return {0.20 * p.x - 0.26 * p.y, 0.23 * p.x + 0.22 * p.y + 1.6};
    return {-0.15 * p.x + 0.28 * p.y, 0.26 * p.x + 0.24 * p.y + 0.44};
}

int main(int argc, char** argv) {
    const std::string output = argc > 1 ? argv[1] : "art/barnsley-fern.svg";
    const int iterations = argc > 2 ? std::atoi(argv[2]) : 90000;
    const int width = 1200;
    const int height = 1600;
    const double min_x = -3.0, max_x = 3.0, min_y = 0.0, max_y = 10.0;

    std::mt19937_64 rng(424242);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    auto sx = [&](double x) {
        return 90.0 + (x - min_x) / (max_x - min_x) * (width - 180.0);
    };
    auto sy = [&](double y) {
        return height - 80.0 - (y - min_y) / (max_y - min_y) * (height - 160.0);
    };

    std::vector<std::string> layers[3];
    Point p{0.0, 0.0};
    for (int i = 0; i < iterations; ++i) {
        p = step(p, dist(rng));
        if (i < 120) continue;
        const double x = sx(p.x);
        const double y = sy(p.y);
        const int layer = p.y > 7.0 ? 2 : (p.y > 3.0 ? 1 : 0);
        std::ostringstream cmd;
        cmd << "M " << std::fixed << std::setprecision(2) << x << ' ' << y << " h 0.01";
        layers[layer].push_back(cmd.str());
    }

    std::ofstream out(output);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << width << ' ' << height
        << "\" width=\"" << width << "\" height=\"" << height << "\">\n";
    out << "<defs>\n";
    out << "  <linearGradient id=\"bg\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"1\">\n";
    out << "    <stop offset=\"0%\" stop-color=\"#02120c\"/>\n";
    out << "    <stop offset=\"60%\" stop-color=\"#08261b\"/>\n";
    out << "    <stop offset=\"100%\" stop-color=\"#03140e\"/>\n";
    out << "  </linearGradient>\n";
    out << "</defs>\n";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"url(#bg)\"/>\n";
    out << "<text x=\"70\" y=\"54\" fill=\"#d1fae5\" font-size=\"34\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Barnsley Fern</text>\n";
    out << "<text x=\"70\" y=\"82\" fill=\"#86efac\" font-size=\"18\" font-family=\"Helvetica, Arial, sans-serif\">A fern-shaped attractor from four affine rules and a weighted dice roll</text>\n";
    out << "<path d=\"";
    for (const auto& cmd : layers[0]) out << cmd << ' ';
    out << "\" stroke=\"#14532d\" stroke-opacity=\"0.22\" stroke-width=\"0.9\" stroke-linecap=\"round\" fill=\"none\"/>\n";
    out << "<path d=\"";
    for (const auto& cmd : layers[1]) out << cmd << ' ';
    out << "\" stroke=\"#22c55e\" stroke-opacity=\"0.35\" stroke-width=\"0.9\" stroke-linecap=\"round\" fill=\"none\"/>\n";
    out << "<path d=\"";
    for (const auto& cmd : layers[2]) out << cmd << ' ';
    out << "\" stroke=\"#bbf7d0\" stroke-opacity=\"0.55\" stroke-width=\"0.9\" stroke-linecap=\"round\" fill=\"none\"/>\n";
    out << "</svg>\n";

    if (!out) {
        std::cerr << "failed to write " << output << '\n';
        return 1;
    }

    std::cout << output << '\n';
    return 0;
}
