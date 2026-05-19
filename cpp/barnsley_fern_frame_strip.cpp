#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
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

struct FrameSpec {
    int iterations;
    const char* caption;
};

struct FrameSnapshot {
    int iterations = 0;
    std::string caption;
    std::array<std::string, 3> layer_paths;
    int occupied_cells = 0;
    double occupied_fraction = 0.0;
    int upper_cells = 0;
    double upper_fraction = 0.0;
};

static std::string format_percent(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << (100.0 * value) << '%';
    return out.str();
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        std::cout << "usage: " << argv[0] << " [output.svg] [output.csv]\n";
        std::cout << "render an eight-frame Barnsley fern timeline with occupancy metrics\n";
        return 0;
    }

    const std::string output_svg = argc > 1 ? argv[1] : "art/barnsley-fern-frame-strip.svg";
    const std::string output_csv = argc > 2 ? argv[2] : "art/barnsley-fern-frame-strip.csv";

    constexpr double min_x = -3.0;
    constexpr double max_x = 3.0;
    constexpr double min_y = 0.0;
    constexpr double max_y = 10.0;
    constexpr int burn_in = 120;
    constexpr int grid_w = 96;
    constexpr int grid_h = 160;
    constexpr double upper_cut = 6.5;

    const std::array<FrameSpec, 8> frames{{
        {320, "stem first, side hints next"},
        {900, "the midrib stops looking accidental"},
        {2200, "small leaflets start holding shape"},
        {5200, "the main silhouette is already there"},
        {12000, "the upper canopy stops flickering"},
        {26000, "the canopy thickens"},
        {52000, "most coarse cells are already claimed"},
        {90000, "same rules, much longer prefix"},
    }};

    std::array<std::string, 3> current_paths;
    std::vector<char> occupied(grid_w * grid_h, 0);
    std::vector<char> upper_occupied(grid_w * grid_h, 0);
    int occupied_cells = 0;
    int upper_cells = 0;
    const int upper_band_cells = grid_w * std::max(1, static_cast<int>(std::round((max_y - upper_cut) / (max_y - min_y) * grid_h)));

    std::array<FrameSnapshot, frames.size()> snapshots;

    std::mt19937_64 rng(424242);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    Point p{0.0, 0.0};

    auto mark_grid = [&](Point q) {
        if (q.x < min_x || q.x > max_x || q.y < min_y || q.y > max_y) return;
        int gx = static_cast<int>((q.x - min_x) / (max_x - min_x) * grid_w);
        int gy = static_cast<int>((q.y - min_y) / (max_y - min_y) * grid_h);
        gx = std::clamp(gx, 0, grid_w - 1);
        gy = std::clamp(gy, 0, grid_h - 1);
        const int idx = gy * grid_w + gx;
        if (!occupied[idx]) {
            occupied[idx] = 1;
            ++occupied_cells;
        }
        if (q.y >= upper_cut && !upper_occupied[idx]) {
            upper_occupied[idx] = 1;
            ++upper_cells;
        }
    };

    std::size_t next_frame = 0;
    for (int i = 0; i < frames.back().iterations; ++i) {
        p = step(p, dist(rng));
        if (i < burn_in) continue;

        mark_grid(p);

        const int layer = p.y > 7.0 ? 2 : (p.y > 3.0 ? 1 : 0);
        std::ostringstream cmd;
        cmd << "M " << std::fixed << std::setprecision(2) << p.x << ' ' << p.y << " h 0.01 ";
        current_paths[layer] += cmd.str();

        while (next_frame < frames.size() && (i + 1) >= frames[next_frame].iterations) {
            snapshots[next_frame].iterations = frames[next_frame].iterations;
            snapshots[next_frame].caption = frames[next_frame].caption;
            snapshots[next_frame].layer_paths = current_paths;
            snapshots[next_frame].occupied_cells = occupied_cells;
            snapshots[next_frame].occupied_fraction = static_cast<double>(occupied_cells) / (grid_w * grid_h);
            snapshots[next_frame].upper_cells = upper_cells;
            snapshots[next_frame].upper_fraction = static_cast<double>(upper_cells) / upper_band_cells;
            ++next_frame;
        }
    }

    const int width = 1900;
    const int height = 1640;
    const int columns = 3;
    const int rows = 3;
    const double panel_w = 360.0;
    const double panel_h = 320.0;
    const double panel_gap_x = 24.0;
    const double panel_gap_y = 28.0;
    const double left = 70.0;
    const double top = 140.0;
    const double chart_top = top + rows * panel_h + 2.0 * panel_gap_y + 56.0;
    const double chart_h = 240.0;
    const double chart_w = width - 2.0 * left - 36.0;

    auto panel_x = [&](int col) { return left + col * (panel_w + panel_gap_x); };
    auto panel_y = [&](int row) { return top + row * (panel_h + panel_gap_y); };
    auto sx = [&](double x, double x0) { return x0 + 20.0 + (x - min_x) / (max_x - min_x) * (panel_w - 40.0); };
    auto sy = [&](double y, double y0) { return y0 + panel_h - 22.0 - (y - min_y) / (max_y - min_y) * (panel_h - 68.0); };

    const double log_min = std::log10(static_cast<double>(frames.front().iterations));
    const double log_max = std::log10(static_cast<double>(frames.back().iterations));
    auto chart_x = [&](double iterations) {
        return left + 66.0 + (std::log10(iterations) - log_min) / (log_max - log_min) * (chart_w - 112.0);
    };
    auto chart_y = [&](double value) {
        return chart_top + chart_h - 36.0 - value * (chart_h - 76.0);
    };

    std::ofstream out(output_svg);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << width << ' ' << height
        << "\" width=\"" << width << "\" height=\"" << height << "\">\n";
    out << "<defs>\n";
    out << "  <linearGradient id=\"bg\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"1\">\n";
    out << "    <stop offset=\"0%\" stop-color=\"#03140f\"/>\n";
    out << "    <stop offset=\"55%\" stop-color=\"#0a241a\"/>\n";
    out << "    <stop offset=\"100%\" stop-color=\"#04150f\"/>\n";
    out << "  </linearGradient>\n";
    out << "</defs>\n";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"url(#bg)\"/>\n";
    out << "<text x=\"70\" y=\"58\" fill=\"#dcfce7\" font-size=\"34\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Barnsley fern frame strip</text>\n";
    out << "<text x=\"70\" y=\"86\" fill=\"#86efac\" font-size=\"18\" font-family=\"Helvetica, Arial, sans-serif\">Eight prefixes from the same random orbit, plus a coarse occupancy read so the growth is not just decorative.</text>\n";
    out << "<text x=\"70\" y=\"112\" fill=\"#bbf7d0\" font-size=\"15\" font-family=\"Helvetica, Arial, sans-serif\">The useful question is timing: when does the fern become legible, and when does the upper canopy start owning area?</text>\n";

    for (std::size_t idx = 0; idx < snapshots.size(); ++idx) {
        const int row = static_cast<int>(idx) / columns;
        const int col = static_cast<int>(idx) % columns;
        const double x0 = panel_x(col);
        const double y0 = panel_y(row);
        const auto& snap = snapshots[idx];

        out << "<rect x=\"" << x0 << "\" y=\"" << y0 << "\" width=\"" << panel_w
            << "\" height=\"" << panel_h << "\" rx=\"18\" fill=\"#061711\" fill-opacity=\"0.84\" stroke=\"#1f3b30\" stroke-width=\"1.5\"/>\n";
        out << "<text x=\"" << (x0 + 18.0) << "\" y=\"" << (y0 + 30.0)
            << "\" fill=\"#d1fae5\" font-size=\"22\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">"
            << snap.iterations << " iterations</text>\n";
        out << "<text x=\"" << (x0 + 18.0) << "\" y=\"" << (y0 + 52.0)
            << "\" fill=\"#a7f3d0\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">"
            << snap.caption << "</text>\n";
        out << "<text x=\"" << (x0 + 18.0) << "\" y=\"" << (y0 + panel_h - 22.0)
            << "\" fill=\"#bbf7d0\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">grid "
            << format_percent(snap.occupied_fraction) << " | canopy " << format_percent(snap.upper_fraction) << "</text>\n";

        out << "<g transform=\"translate(0 0) scale(1 -1) translate(0 -" << (2.0 * y0 + panel_h) << ")\">\n";
        out << "<path d=\"";
        for (const auto& cmd : snap.layer_paths[0]) out << cmd;
        out << "\" transform=\"translate(" << x0 + 20.0 << ' ' << y0 + 16.0 << ") scale(" << (panel_w - 40.0) / (max_x - min_x) << ' ' << (panel_h - 68.0) / (max_y - min_y) << ") translate(" << (-min_x) << ' ' << (-min_y) << ")\" stroke=\"#14532d\" stroke-opacity=\"0.25\" stroke-width=\"0.02\" stroke-linecap=\"round\" fill=\"none\"/>\n";
        out << "<path d=\"";
        for (const auto& cmd : snap.layer_paths[1]) out << cmd;
        out << "\" transform=\"translate(" << x0 + 20.0 << ' ' << y0 + 16.0 << ") scale(" << (panel_w - 40.0) / (max_x - min_x) << ' ' << (panel_h - 68.0) / (max_y - min_y) << ") translate(" << (-min_x) << ' ' << (-min_y) << ")\" stroke=\"#22c55e\" stroke-opacity=\"0.38\" stroke-width=\"0.02\" stroke-linecap=\"round\" fill=\"none\"/>\n";
        out << "<path d=\"";
        for (const auto& cmd : snap.layer_paths[2]) out << cmd;
        out << "\" transform=\"translate(" << x0 + 20.0 << ' ' << y0 + 16.0 << ") scale(" << (panel_w - 40.0) / (max_x - min_x) << ' ' << (panel_h - 68.0) / (max_y - min_y) << ") translate(" << (-min_x) << ' ' << (-min_y) << ")\" stroke=\"#dcfce7\" stroke-opacity=\"0.58\" stroke-width=\"0.02\" stroke-linecap=\"round\" fill=\"none\"/>\n";
        out << "</g>\n";
    }

    out << "<rect x=\"" << left << "\" y=\"" << chart_top << "\" width=\"" << chart_w << "\" height=\"" << chart_h
        << "\" rx=\"18\" fill=\"#061711\" fill-opacity=\"0.84\" stroke=\"#1f3b30\" stroke-width=\"1.5\"/>\n";
    out << "<text x=\"" << left + 18.0 << "\" y=\"" << chart_top + 30.0
        << "\" fill=\"#d1fae5\" font-size=\"20\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Coarse occupancy curve</text>\n";
    out << "<text x=\"" << left + 18.0 << "\" y=\"" << chart_top + 56.0
        << "\" fill=\"#a7f3d0\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">Blue: occupied cells anywhere in the fern box. Mint: occupied cells in the upper canopy band.</text>\n";

    for (int tick = 0; tick <= 4; ++tick) {
        const double frac = 0.25 * tick;
        const double y = chart_y(frac);
        out << "<line x1=\"" << left + 66.0 << "\" y1=\"" << y << "\" x2=\"" << left + chart_w - 46.0
            << "\" y2=\"" << y << "\" stroke=\"#183126\" stroke-width=\"1\"/>\n";
        out << "<text x=\"" << left + 56.0 << "\" y=\"" << y + 4.0
            << "\" fill=\"#bbf7d0\" font-size=\"11\" text-anchor=\"end\" font-family=\"Helvetica, Arial, sans-serif\">"
            << std::fixed << std::setprecision(0) << frac * 100.0 << "%</text>\n";
    }

    for (const auto& frame : snapshots) {
        const double x = chart_x(frame.iterations);
        out << "<line x1=\"" << x << "\" y1=\"" << chart_top + 60.0 << "\" x2=\"" << x << "\" y2=\"" << chart_top + chart_h - 36.0
            << "\" stroke=\"#163126\" stroke-width=\"1\"/>\n";
        out << "<text x=\"" << x << "\" y=\"" << chart_top + chart_h - 14.0
            << "\" fill=\"#bbf7d0\" font-size=\"11\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">"
            << frame.iterations << "</text>\n";
    }

    out << "<polyline fill=\"none\" stroke=\"#60a5fa\" stroke-width=\"3\" points=\"";
    for (const auto& frame : snapshots) out << chart_x(frame.iterations) << ',' << chart_y(frame.occupied_fraction) << ' ';
    out << "\"/>\n";
    out << "<polyline fill=\"none\" stroke=\"#6ee7b7\" stroke-width=\"3\" points=\"";
    for (const auto& frame : snapshots) out << chart_x(frame.iterations) << ',' << chart_y(frame.upper_fraction) << ' ';
    out << "\"/>\n";

    for (const auto& frame : snapshots) {
        out << "<circle cx=\"" << chart_x(frame.iterations) << "\" cy=\"" << chart_y(frame.occupied_fraction)
            << "\" r=\"4.5\" fill=\"#dbeafe\" stroke=\"#60a5fa\" stroke-width=\"2\"/>\n";
        out << "<circle cx=\"" << chart_x(frame.iterations) << "\" cy=\"" << chart_y(frame.upper_fraction)
            << "\" r=\"4.5\" fill=\"#dcfce7\" stroke=\"#6ee7b7\" stroke-width=\"2\"/>\n";
    }

    out << "<line x1=\"" << left + 18.0 << "\" y1=\"" << chart_top + chart_h - 58.0 << "\" x2=\"" << left + 46.0 << "\" y2=\"" << chart_top + chart_h - 58.0 << "\" stroke=\"#60a5fa\" stroke-width=\"3\"/>\n";
    out << "<text x=\"" << left + 54.0 << "\" y=\"" << chart_top + chart_h - 54.0 << "\" fill=\"#dbeafe\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">whole fern occupancy</text>\n";
    out << "<line x1=\"" << left + 220.0 << "\" y1=\"" << chart_top + chart_h - 58.0 << "\" x2=\"" << left + 248.0 << "\" y2=\"" << chart_top + chart_h - 58.0 << "\" stroke=\"#6ee7b7\" stroke-width=\"3\"/>\n";
    out << "<text x=\"" << left + 256.0 << "\" y=\"" << chart_top + chart_h - 54.0 << "\" fill=\"#dcfce7\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">upper-canopy occupancy</text>\n";

    out << "</svg>\n";
    if (!out) {
        std::cerr << "failed to write " << output_svg << '\n';
        return 1;
    }

    std::ofstream csv(output_csv);
    csv << "frame,iterations,occupied_cells,occupied_fraction,upper_cells,upper_fraction\n";
    for (std::size_t i = 0; i < snapshots.size(); ++i) {
        const auto& frame = snapshots[i];
        csv << i << ',' << frame.iterations << ',' << frame.occupied_cells << ','
            << std::fixed << std::setprecision(6) << frame.occupied_fraction << ','
            << frame.upper_cells << ',' << frame.upper_fraction << '\n';
    }
    if (!csv) {
        std::cerr << "failed to write " << output_csv << '\n';
        return 1;
    }

    std::cout << output_svg << '\n' << output_csv << '\n';
    return 0;
}
