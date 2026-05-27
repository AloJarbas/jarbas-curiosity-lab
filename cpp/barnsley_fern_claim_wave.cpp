#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

struct Point {
    double x;
    double y;
};

struct FrameSpec {
    int iterations;
    const char* caption;
};

struct IntervalStats {
    int start = 0;
    int end = 0;
    std::string caption;
    int new_cells = 0;
    int cumulative_cells = 0;
    double new_fraction = 0.0;
    double cumulative_fraction = 0.0;
    double mean_height = 0.0;
    double canopy_share = 0.0;
};

static Point step(Point p, double r) {
    if (r < 0.01) return {0.0, 0.16 * p.y};
    if (r < 0.86) return {0.85 * p.x + 0.04 * p.y, -0.04 * p.x + 0.85 * p.y + 1.6};
    if (r < 0.93) return {0.20 * p.x - 0.26 * p.y, 0.23 * p.x + 0.22 * p.y + 1.6};
    return {-0.15 * p.x + 0.28 * p.y, 0.26 * p.x + 0.24 * p.y + 0.44};
}

static std::string hex_color(const std::array<int, 3>& rgb) {
    std::ostringstream out;
    out << '#'
        << std::hex << std::setfill('0')
        << std::setw(2) << rgb[0]
        << std::setw(2) << rgb[1]
        << std::setw(2) << rgb[2];
    return out.str();
}

static std::array<int, 3> mix_rgb(const std::array<int, 3>& a, const std::array<int, 3>& b, double t) {
    t = std::clamp(t, 0.0, 1.0);
    std::array<int, 3> mixed{};
    for (int i = 0; i < 3; ++i) {
        mixed[i] = static_cast<int>(std::round(a[i] + (b[i] - a[i]) * t));
    }
    return mixed;
}

static std::string mix_color(const std::array<int, 3>& a, const std::array<int, 3>& b, double t) {
    return hex_color(mix_rgb(a, b, t));
}

static std::string format_percent(double value, int places = 1) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(places) << value * 100.0 << '%';
    return out.str();
}

static std::string format_fixed(double value, int places = 2) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(places) << value;
    return out.str();
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        std::cout << "usage: " << argv[0] << " [output.svg] [output.csv]\n";
        std::cout << "render a Barnsley fern claim-wave card\n";
        return 0;
    }

    const std::string output_svg = argc > 1 ? argv[1] : "art/barnsley-fern-claim-wave.svg";
    const std::string output_csv = argc > 2 ? argv[2] : "art/barnsley-fern-claim-wave.csv";

    constexpr double min_x = -3.0;
    constexpr double max_x = 3.0;
    constexpr double min_y = 0.0;
    constexpr double max_y = 10.0;
    constexpr int burn_in = 120;
    constexpr int grid_w = 96;
    constexpr int grid_h = 160;
    constexpr double canopy_cut = 6.5;

    const std::array<FrameSpec, 8> frames{{
        {320, "stem and lower spine claim first"},
        {900, "the first side leaves stop looking accidental"},
        {2200, "mid-height fronds start filling in"},
        {5200, "the silhouette is already readable"},
        {12000, "upper canopy keeps doing the real claiming"},
        {26000, "novelty slides to the leaflet fringe"},
        {52000, "late claims mostly live on outer lace"},
        {90000, "the last gains are edge polish, not new bulk"},
    }};

    const std::array<int, 3> background{4, 14, 11};
    const std::array<int, 3> old_cell{24, 88, 58};
    const std::array<int, 3> old_cell_bright{50, 140, 90};
    const std::array<int, 3> new_low{34, 197, 94};
    const std::array<int, 3> new_high{250, 204, 21};
    const std::array<int, 3> panel_fill{6, 24, 17};
    const std::array<int, 3> frame_color{31, 59, 48};

    std::vector<int> first_hit(grid_w * grid_h, -1);
    std::mt19937_64 rng(424242);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    Point p{0.0, 0.0};

    const int max_iterations = frames.back().iterations;
    for (int raw_step = 0; raw_step < max_iterations + burn_in; ++raw_step) {
        p = step(p, dist(rng));
        if (raw_step < burn_in) continue;
        if (p.x < min_x || p.x > max_x || p.y < min_y || p.y > max_y) continue;

        int gx = static_cast<int>((p.x - min_x) / (max_x - min_x) * grid_w);
        int gy = static_cast<int>((p.y - min_y) / (max_y - min_y) * grid_h);
        gx = std::clamp(gx, 0, grid_w - 1);
        gy = std::clamp(gy, 0, grid_h - 1);
        const int idx = gy * grid_w + gx;
        if (first_hit[idx] < 0) {
            first_hit[idx] = raw_step - burn_in + 1;
        }
    }

    std::array<IntervalStats, frames.size()> stats{};
    const int total_cells = grid_w * grid_h;
    int cumulative_cells = 0;
    int peak_new_index = 0;
    int peak_height_index = 0;

    for (std::size_t i = 0; i < frames.size(); ++i) {
        const int start = i == 0 ? 0 : frames[i - 1].iterations;
        const int end = frames[i].iterations;
        int new_cells = 0;
        double height_sum = 0.0;
        int canopy_cells = 0;

        for (int gy = 0; gy < grid_h; ++gy) {
            const double cell_y = min_y + (gy + 0.5) / grid_h * (max_y - min_y);
            for (int gx = 0; gx < grid_w; ++gx) {
                const int hit = first_hit[gy * grid_w + gx];
                if (hit <= 0 || hit > end) continue;
                if (hit > start) {
                    ++new_cells;
                    height_sum += cell_y;
                    if (cell_y >= canopy_cut) ++canopy_cells;
                }
            }
        }

        cumulative_cells += new_cells;
        stats[i].start = start;
        stats[i].end = end;
        stats[i].caption = frames[i].caption;
        stats[i].new_cells = new_cells;
        stats[i].cumulative_cells = cumulative_cells;
        stats[i].new_fraction = static_cast<double>(new_cells) / total_cells;
        stats[i].cumulative_fraction = static_cast<double>(cumulative_cells) / total_cells;
        stats[i].mean_height = new_cells > 0 ? height_sum / new_cells : 0.0;
        stats[i].canopy_share = new_cells > 0 ? static_cast<double>(canopy_cells) / new_cells : 0.0;

        if (stats[i].new_cells > stats[peak_new_index].new_cells) peak_new_index = static_cast<int>(i);
        if (stats[i].mean_height > stats[peak_height_index].mean_height) peak_height_index = static_cast<int>(i);
    }

    const double max_new_fraction = std::max_element(
        stats.begin(),
        stats.end(),
        [](const IntervalStats& a, const IntervalStats& b) { return a.new_fraction < b.new_fraction; }
    )->new_fraction;

    const int width = 1840;
    const int height = 1180;
    const int columns = 4;
    const double panel_w = 394.0;
    const double panel_h = 248.0;
    const double panel_gap_x = 22.0;
    const double panel_gap_y = 24.0;
    const double left = 70.0;
    const double top = 154.0;
    const double chart_top = top + 2.0 * panel_h + panel_gap_y + 44.0;
    const double chart_h = 300.0;
    const double chart_gap = 28.0;
    const double chart_w = (width - 2.0 * left - chart_gap) / 2.0;
    const double plot_top = chart_top + 86.0;
    const double plot_bottom = chart_top + chart_h - 36.0;
    const double plot_h = plot_bottom - plot_top;

    auto panel_x = [&](int col) { return left + col * (panel_w + panel_gap_x); };
    auto panel_y = [&](int row) { return top + row * (panel_h + panel_gap_y); };

    auto draw_x = [&](int gx, double x0) {
        return x0 + 16.0 + static_cast<double>(gx) / grid_w * (panel_w - 32.0);
    };
    auto draw_y = [&](int gy, double y0) {
        return y0 + 58.0 + (grid_h - gy - 1.0) / grid_h * (panel_h - 92.0);
    };

    auto bar_x = [&](int index, double x0) {
        return x0 + 54.0 + (index + 0.5) / stats.size() * (chart_w - 92.0);
    };
    auto new_bar_y = [&](double value) {
        return plot_bottom - value / (max_new_fraction * 1.12) * plot_h;
    };
    auto line_y = [&](double value) {
        return plot_bottom - value * plot_h;
    };

    std::ofstream out(output_svg);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << width << ' ' << height
        << "\" width=\"" << width << "\" height=\"" << height << "\">\n";
    out << "<defs>\n";
    out << "  <linearGradient id=\"bg\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"1\">\n";
    out << "    <stop offset=\"0%\" stop-color=\"#03140f\"/>\n";
    out << "    <stop offset=\"55%\" stop-color=\"#0a241a\"/>\n";
    out << "    <stop offset=\"100%\" stop-color=\"#091017\"/>\n";
    out << "  </linearGradient>\n";
    out << "</defs>\n";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"url(#bg)\"/>\n";
    out << "<text x=\"70\" y=\"58\" fill=\"#dcfce7\" font-size=\"34\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Barnsley fern claim wave</text>\n";
    out << "<text x=\"70\" y=\"86\" fill=\"#86efac\" font-size=\"18\" font-family=\"Helvetica, Arial, sans-serif\">A frame strip shows growth. This one asks where the fern is still claiming genuinely new coarse area.</text>\n";
    out << "<text x=\"70\" y=\"112\" fill=\"#bbf7d0\" font-size=\"15\" font-family=\"Helvetica, Arial, sans-serif\">Bright cells are new in that interval. Dark green cells were already claimed earlier. The claim front climbs upward, then thins into leaflet edges.</text>\n";

    for (std::size_t i = 0; i < stats.size(); ++i) {
        const int row = static_cast<int>(i) / columns;
        const int col = static_cast<int>(i) % columns;
        const double x0 = panel_x(col);
        const double y0 = panel_y(row);
        const int start = stats[i].start;
        const int end = stats[i].end;

        out << "<rect x=\"" << x0 << "\" y=\"" << y0 << "\" width=\"" << panel_w
            << "\" height=\"" << panel_h << "\" rx=\"18\" fill=\"" << hex_color(panel_fill)
            << "\" fill-opacity=\"0.9\" stroke=\"" << hex_color(frame_color) << "\" stroke-width=\"1.5\"/>\n";
        out << "<text x=\"" << (x0 + 16.0) << "\" y=\"" << (y0 + 28.0)
            << "\" fill=\"#d1fae5\" font-size=\"20\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">"
            << end << " iterations</text>\n";
        out << "<text x=\"" << (x0 + 16.0) << "\" y=\"" << (y0 + 48.0)
            << "\" fill=\"#a7f3d0\" font-size=\"11\" font-family=\"Helvetica, Arial, sans-serif\">"
            << stats[i].caption << "</text>\n";

        for (int gy = 0; gy < grid_h; ++gy) {
            const double cell_y = min_y + (gy + 0.5) / grid_h * (max_y - min_y);
            const double y = draw_y(gy, y0);
            const double h = (panel_h - 92.0) / grid_h + 0.18;
            for (int gx = 0; gx < grid_w; ++gx) {
                const int hit = first_hit[gy * grid_w + gx];
                if (hit <= 0 || hit > end) continue;
                const double x = draw_x(gx, x0);
                const double w = (panel_w - 32.0) / grid_w + 0.18;
                if (hit <= start) {
                    const double old_mix = 0.35 + 0.45 * (cell_y / max_y);
                    out << "<rect x=\"" << std::fixed << std::setprecision(2) << x << "\" y=\"" << y
                        << "\" width=\"" << w << "\" height=\"" << h << "\" fill=\""
                        << mix_color(old_cell, old_cell_bright, old_mix) << "\"/>\n";
                } else {
                    const double t = cell_y / max_y;
                    out << "<rect x=\"" << std::fixed << std::setprecision(2) << x << "\" y=\"" << y
                        << "\" width=\"" << w << "\" height=\"" << h << "\" fill=\""
                        << mix_color(new_low, new_high, t) << "\"/>\n";
                }
            }
        }

        out << "<text x=\"" << (x0 + 16.0) << "\" y=\"" << (y0 + panel_h - 18.0)
            << "\" fill=\"#bbf7d0\" font-size=\"11\" font-family=\"Helvetica, Arial, sans-serif\">new "
            << format_percent(stats[i].new_fraction) << " · cumulative " << format_percent(stats[i].cumulative_fraction)
            << " · mean claim height " << format_fixed(stats[i].mean_height, 2) << "</text>\n";
    }

    out << "<rect x=\"" << left << "\" y=\"" << chart_top << "\" width=\"" << chart_w
        << "\" height=\"" << chart_h << "\" rx=\"20\" fill=\"#061711\" fill-opacity=\"0.88\" stroke=\"#1f3b30\" stroke-width=\"1.5\"/>\n";
    out << "<text x=\"" << (left + 18.0) << "\" y=\"" << (chart_top + 30.0)
        << "\" fill=\"#d1fae5\" font-size=\"20\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Newly claimed cell share</text>\n";
    out << "<text x=\"" << (left + 18.0) << "\" y=\"" << (chart_top + 54.0)
        << "\" fill=\"#a7f3d0\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">Each bar is the new coarse area claimed in that interval, not the full fern already on screen.</text>\n";

    for (int tick = 0; tick <= 4; ++tick) {
        const double frac = tick / 4.0;
        const double y = new_bar_y(frac * max_new_fraction * 1.12);
        out << "<line x1=\"" << (left + 54.0) << "\" y1=\"" << y << "\" x2=\"" << (left + chart_w - 38.0)
            << "\" y2=\"" << y << "\" stroke=\"#183126\" stroke-width=\"1\"/>\n";
        out << "<text x=\"" << (left + 46.0) << "\" y=\"" << (y + 4.0)
            << "\" fill=\"#bbf7d0\" font-size=\"11\" text-anchor=\"end\" font-family=\"Helvetica, Arial, sans-serif\">"
            << format_percent(frac * max_new_fraction * 1.12, 0) << "</text>\n";
    }

    for (std::size_t i = 0; i < stats.size(); ++i) {
        const double x = bar_x(static_cast<int>(i), left);
        const double y = new_bar_y(stats[i].new_fraction);
        const double bar_w = (chart_w - 140.0) / stats.size() * 0.56;
        out << "<rect x=\"" << (x - bar_w / 2.0) << "\" y=\"" << y << "\" width=\"" << bar_w
            << "\" height=\"" << (plot_bottom - y) << "\" rx=\"8\" fill=\"#facc15\" fill-opacity=\"0.92\"/>\n";
        out << "<text x=\"" << x << "\" y=\"" << (chart_top + chart_h - 14.0)
            << "\" fill=\"#bbf7d0\" font-size=\"10\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">"
            << stats[i].end << "</text>\n";
    }

    const double right_chart_left = left + chart_w + chart_gap;
    out << "<rect x=\"" << right_chart_left << "\" y=\"" << chart_top << "\" width=\"" << chart_w
        << "\" height=\"" << chart_h << "\" rx=\"20\" fill=\"#08111f\" fill-opacity=\"0.88\" stroke=\"#334155\" stroke-width=\"1.5\"/>\n";
    out << "<text x=\"" << (right_chart_left + 18.0) << "\" y=\"" << (chart_top + 30.0)
        << "\" fill=\"#e0f2fe\" font-size=\"20\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Where the novelty lives</text>\n";
    out << "<text x=\"" << (right_chart_left + 18.0) << "\" y=\"" << (chart_top + 54.0)
        << "\" fill=\"#bae6fd\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">Blue = mean height of newly claimed cells. Mint = share of those new cells already in the upper canopy.</text>\n";

    for (int tick = 0; tick <= 4; ++tick) {
        const double frac = tick / 4.0;
        const double y = line_y(frac);
        out << "<line x1=\"" << (right_chart_left + 54.0) << "\" y1=\"" << y << "\" x2=\"" << (right_chart_left + chart_w - 38.0)
            << "\" y2=\"" << y << "\" stroke=\"#183126\" stroke-width=\"1\"/>\n";
        out << "<text x=\"" << (right_chart_left + 46.0) << "\" y=\"" << (y + 4.0)
            << "\" fill=\"#cbd5e1\" font-size=\"11\" text-anchor=\"end\" font-family=\"Helvetica, Arial, sans-serif\">"
            << format_percent(frac, 0) << "</text>\n";
    }

    out << "<polyline fill=\"none\" stroke=\"#60a5fa\" stroke-width=\"3\" points=\"";
    for (std::size_t i = 0; i < stats.size(); ++i) {
        out << bar_x(static_cast<int>(i), right_chart_left) << ',' << line_y(stats[i].mean_height / max_y) << ' ';
    }
    out << "\"/>\n";
    out << "<polyline fill=\"none\" stroke=\"#6ee7b7\" stroke-width=\"3\" points=\"";
    for (std::size_t i = 0; i < stats.size(); ++i) {
        out << bar_x(static_cast<int>(i), right_chart_left) << ',' << line_y(stats[i].canopy_share) << ' ';
    }
    out << "\"/>\n";

    for (std::size_t i = 0; i < stats.size(); ++i) {
        const double x = bar_x(static_cast<int>(i), right_chart_left);
        out << "<circle cx=\"" << x << "\" cy=\"" << line_y(stats[i].mean_height / max_y)
            << "\" r=\"4.5\" fill=\"#dbeafe\" stroke=\"#60a5fa\" stroke-width=\"2\"/>\n";
        out << "<circle cx=\"" << x << "\" cy=\"" << line_y(stats[i].canopy_share)
            << "\" r=\"4.5\" fill=\"#dcfce7\" stroke=\"#6ee7b7\" stroke-width=\"2\"/>\n";
        out << "<text x=\"" << x << "\" y=\"" << (chart_top + chart_h - 14.0)
            << "\" fill=\"#cbd5e1\" font-size=\"10\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">"
            << stats[i].end << "</text>\n";
    }

    out << "<line x1=\"" << (right_chart_left + 18.0) << "\" y1=\"" << (chart_top + chart_h - 54.0)
        << "\" x2=\"" << (right_chart_left + 44.0) << "\" y2=\"" << (chart_top + chart_h - 54.0)
        << "\" stroke=\"#60a5fa\" stroke-width=\"3\"/>\n";
    out << "<text x=\"" << (right_chart_left + 52.0) << "\" y=\"" << (chart_top + chart_h - 50.0)
        << "\" fill=\"#dbeafe\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">mean claim height</text>\n";
    out << "<line x1=\"" << (right_chart_left + 224.0) << "\" y1=\"" << (chart_top + chart_h - 54.0)
        << "\" x2=\"" << (right_chart_left + 250.0) << "\" y2=\"" << (chart_top + chart_h - 54.0)
        << "\" stroke=\"#6ee7b7\" stroke-width=\"3\"/>\n";
    out << "<text x=\"" << (right_chart_left + 258.0) << "\" y=\"" << (chart_top + chart_h - 50.0)
        << "\" fill=\"#dcfce7\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">canopy share of new cells</text>\n";

    out << "<text x=\"70\" y=\"" << (height - 38.0)
        << "\" fill=\"#bbf7d0\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">Peak new-area interval: "
        << stats[peak_new_index].start << "→" << stats[peak_new_index].end << " ("
        << format_percent(stats[peak_new_index].new_fraction) << " of all coarse cells). Highest claim front: "
        << stats[peak_height_index].start << "→" << stats[peak_height_index].end << " at mean height "
        << format_fixed(stats[peak_height_index].mean_height, 2) << ". Final cumulative occupancy: "
        << format_percent(stats.back().cumulative_fraction) << ".</text>\n";
    out << "</svg>\n";

    if (!out) {
        std::cerr << "failed to write " << output_svg << '\n';
        return 1;
    }

    std::ofstream csv(output_csv);
    csv << "interval_start,interval_end,new_cells,new_fraction,cumulative_cells,cumulative_fraction,mean_claim_height,canopy_share\n";
    for (const auto& row : stats) {
        csv << row.start << ',' << row.end << ',' << row.new_cells << ',' << row.new_fraction << ','
            << row.cumulative_cells << ',' << row.cumulative_fraction << ',' << row.mean_height << ','
            << row.canopy_share << '\n';
    }
    if (!csv) {
        std::cerr << "failed to write " << output_csv << '\n';
        return 1;
    }

    std::cout << "wrote " << output_svg << " and " << output_csv << '\n';
    return 0;
}
