#include <array>
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

struct Panel {
    int iterations;
    const char* caption;
};

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        std::cout << "usage: " << argv[0] << " [output.svg]\n";
        std::cout << "render a six-panel Barnsley fern growth study as SVG\n";
        return 0;
    }

    const std::string output = argc > 1 ? argv[1] : "art/barnsley-fern-growth-stages.svg";
    constexpr int width = 1500;
    constexpr int height = 1180;
    constexpr int columns = 3;
    constexpr int rows = 2;
    constexpr double min_x = -3.0;
    constexpr double max_x = 3.0;
    constexpr double min_y = 0.0;
    constexpr double max_y = 10.0;
    constexpr int burn_in = 120;

    const std::array<Panel, 6> panels{{
        {600, "just enough points to reveal the stem and first leaflet hints"},
        {1800, "the midrib stabilizes and the fern stops looking accidental"},
        {6000, "secondary leaflets start reading as structure instead of noise"},
        {18000, "density arrives and the silhouette becomes hard to miss"},
        {54000, "the upper canopy and side detail keep tightening"},
        {120000, "same rules, same seed, just a larger iteration budget"},
    }};

    struct LayerSet {
        std::vector<std::string> layers[3];
    };

    std::array<LayerSet, panels.size()> layer_sets;

    std::mt19937_64 rng(424242);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    Point p{0.0, 0.0};

    std::size_t panel_index = 0;
    for (int i = 0; i < panels.back().iterations; ++i) {
        p = step(p, dist(rng));
        if (i < burn_in) continue;

        while (panel_index < panels.size() && i >= panels[panel_index].iterations) {
            ++panel_index;
        }
        if (panel_index >= panels.size()) break;

        const int row = static_cast<int>(panel_index) / columns;
        const int col = static_cast<int>(panel_index) % columns;
        const double panel_x = 60.0 + col * 460.0;
        const double panel_y = 150.0 + row * 470.0;
        const double panel_w = 400.0;
        const double panel_h = 370.0;

        const double x = panel_x + 24.0 + (p.x - min_x) / (max_x - min_x) * (panel_w - 48.0);
        const double y = panel_y + panel_h - 22.0 - (p.y - min_y) / (max_y - min_y) * (panel_h - 52.0);
        const int layer = p.y > 7.0 ? 2 : (p.y > 3.0 ? 1 : 0);

        std::ostringstream cmd;
        cmd << "M " << std::fixed << std::setprecision(2) << x << ' ' << y << " h 0.01";
        layer_sets[panel_index].layers[layer].push_back(cmd.str());
    }

    std::ofstream out(output);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << width << ' ' << height
        << "\" width=\"" << width << "\" height=\"" << height << "\">\n";
    out << "<defs>\n";
    out << "  <linearGradient id=\"bg\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"1\">\n";
    out << "    <stop offset=\"0%\" stop-color=\"#04110d\"/>\n";
    out << "    <stop offset=\"55%\" stop-color=\"#0a2319\"/>\n";
    out << "    <stop offset=\"100%\" stop-color=\"#03100b\"/>\n";
    out << "  </linearGradient>\n";
    out << "</defs>\n";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"url(#bg)\"/>\n";
    out << "<text x=\"60\" y=\"62\" fill=\"#dcfce7\" font-size=\"36\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Barnsley fern growth study</text>\n";
    out << "<text x=\"60\" y=\"92\" fill=\"#86efac\" font-size=\"18\" font-family=\"Helvetica, Arial, sans-serif\">Same affine rules, same random seed, different iteration budgets.</text>\n";
    out << "<text x=\"60\" y=\"118\" fill=\"#bbf7d0\" font-size=\"16\" font-family=\"Helvetica, Arial, sans-serif\">The point is not just that the fern exists. It is that density and leaflet structure emerge in layers as the orbit keeps landing on the same attractor.</text>\n";

    for (std::size_t idx = 0; idx < panels.size(); ++idx) {
        const int row = static_cast<int>(idx) / columns;
        const int col = static_cast<int>(idx) % columns;
        const double panel_x = 60.0 + col * 460.0;
        const double panel_y = 150.0 + row * 470.0;
        const double panel_w = 400.0;
        const double panel_h = 370.0;

        out << "<rect x=\"" << panel_x << "\" y=\"" << panel_y << "\" width=\"" << panel_w
            << "\" height=\"" << panel_h << "\" rx=\"18\" fill=\"#061711\" fill-opacity=\"0.82\" stroke=\"#1f3b30\" stroke-width=\"1.5\"/>\n";
        out << "<text x=\"" << (panel_x + 22.0) << "\" y=\"" << (panel_y + 34.0)
            << "\" fill=\"#d1fae5\" font-size=\"24\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">"
            << panels[idx].iterations << " iterations</text>\n";
        out << "<text x=\"" << (panel_x + 22.0) << "\" y=\"" << (panel_y + 60.0)
            << "\" fill=\"#a7f3d0\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">"
            << panels[idx].caption << "</text>\n";

        out << "<path d=\"";
        for (const auto& cmd : layer_sets[idx].layers[0]) out << cmd << ' ';
        out << "\" stroke=\"#14532d\" stroke-opacity=\"0.24\" stroke-width=\"0.95\" stroke-linecap=\"round\" fill=\"none\"/>\n";

        out << "<path d=\"";
        for (const auto& cmd : layer_sets[idx].layers[1]) out << cmd << ' ';
        out << "\" stroke=\"#22c55e\" stroke-opacity=\"0.36\" stroke-width=\"0.95\" stroke-linecap=\"round\" fill=\"none\"/>\n";

        out << "<path d=\"";
        for (const auto& cmd : layer_sets[idx].layers[2]) out << cmd << ' ';
        out << "\" stroke=\"#dcfce7\" stroke-opacity=\"0.54\" stroke-width=\"0.95\" stroke-linecap=\"round\" fill=\"none\"/>\n";
    }

    out << "</svg>\n";

    if (!out) {
        std::cerr << "failed to write " << output << '\n';
        return 1;
    }

    std::cout << output << '\n';
    return 0;
}
