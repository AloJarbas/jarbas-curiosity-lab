#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
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

struct Cell {
    int first_hit = -1;
    int hits = 0;
};

static Point step(Point p, double r) {
    if (r < 0.01) return {0.0, 0.16 * p.y};
    if (r < 0.86) return {0.85 * p.x + 0.04 * p.y, -0.04 * p.x + 0.85 * p.y + 1.6};
    if (r < 0.93) return {0.20 * p.x - 0.26 * p.y, 0.23 * p.x + 0.22 * p.y + 1.6};
    return {-0.15 * p.x + 0.28 * p.y, 0.26 * p.x + 0.24 * p.y + 0.44};
}

static std::string color_for(double t) {
    t = std::clamp(t, 0.0, 1.0);
    const std::array<std::array<int, 3>, 5> stops{{
        {20, 83, 45},
        {14, 116, 144},
        {37, 99, 235},
        {234, 179, 8},
        {249, 115, 22},
    }};
    const double scaled = t * (stops.size() - 1);
    const int idx = static_cast<int>(std::floor(scaled));
    const int next = std::min<int>(stops.size() - 1, idx + 1);
    const double frac = scaled - idx;
    const auto mix = [&](int a, int b) {
        return static_cast<int>(std::round(a + (b - a) * frac));
    };
    std::ostringstream out;
    out << '#'
        << std::hex << std::setfill('0')
        << std::setw(2) << mix(stops[idx][0], stops[next][0])
        << std::setw(2) << mix(stops[idx][1], stops[next][1])
        << std::setw(2) << mix(stops[idx][2], stops[next][2]);
    return out.str();
}

static double median(std::vector<int> values) {
    if (values.empty()) return -1.0;
    std::sort(values.begin(), values.end());
    const std::size_t mid = values.size() / 2;
    if (values.size() % 2 == 1) return static_cast<double>(values[mid]);
    return 0.5 * (values[mid - 1] + values[mid]);
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        std::cout << "usage: " << argv[0] << " [output.svg] [output.csv]\n";
        std::cout << "render a Barnsley fern first-hit map with log-time summary charts\n";
        return 0;
    }

    const std::string output_svg = argc > 1 ? argv[1] : "art/barnsley-fern-first-hit-map.svg";
    const std::string output_csv = argc > 2 ? argv[2] : "art/barnsley-fern-first-hit-map.csv";

    constexpr double min_x = -3.0;
    constexpr double max_x = 3.0;
    constexpr double min_y = 0.0;
    constexpr double max_y = 10.0;
    constexpr int burn_in = 120;
    constexpr int total_iterations = 90000;
    constexpr int grid_w = 120;
    constexpr int grid_h = 200;
    constexpr int histogram_bins = 20;
    constexpr int height_bands = 16;

    std::vector<Cell> grid(grid_w * grid_h);
    std::vector<int> occupancy_prefixes;
    std::vector<int> prefix_steps{320, 900, 2200, 5200, 12000, 26000, 52000, 90000};
    occupancy_prefixes.reserve(prefix_steps.size());

    std::mt19937_64 rng(424242);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    Point p{0.0, 0.0};
    int claimed = 0;

    for (int raw_step = 0; raw_step < total_iterations + burn_in; ++raw_step) {
        p = step(p, dist(rng));
        if (raw_step < burn_in) continue;
        const int step_index = raw_step - burn_in + 1;
        if (p.x < min_x || p.x > max_x || p.y < min_y || p.y > max_y) {
            if (!prefix_steps.empty() && step_index == prefix_steps[occupancy_prefixes.size()]) {
                occupancy_prefixes.push_back(claimed);
            }
            continue;
        }
        int gx = static_cast<int>((p.x - min_x) / (max_x - min_x) * grid_w);
        int gy = static_cast<int>((p.y - min_y) / (max_y - min_y) * grid_h);
        gx = std::clamp(gx, 0, grid_w - 1);
        gy = std::clamp(gy, 0, grid_h - 1);
        Cell& cell = grid[gy * grid_w + gx];
        if (cell.first_hit < 0) {
            cell.first_hit = step_index;
            ++claimed;
        }
        ++cell.hits;
        if (occupancy_prefixes.size() < prefix_steps.size() && step_index == prefix_steps[occupancy_prefixes.size()]) {
            occupancy_prefixes.push_back(claimed);
        }
    }

    while (occupancy_prefixes.size() < prefix_steps.size()) occupancy_prefixes.push_back(claimed);

    std::vector<int> first_hits;
    first_hits.reserve(claimed);
    for (const auto& cell : grid) {
        if (cell.first_hit > 0) first_hits.push_back(cell.first_hit);
    }
    if (first_hits.empty()) {
        std::cerr << "no visited cells recorded\n";
        return 1;
    }

    const double log_min = std::log10(1.0);
    const double log_max = std::log10(static_cast<double>(total_iterations));
    std::vector<int> histogram(histogram_bins, 0);
    for (int first_hit : first_hits) {
        const double t = (std::log10(static_cast<double>(first_hit)) - log_min) / (log_max - log_min);
        int bin = static_cast<int>(std::floor(t * histogram_bins));
        bin = std::clamp(bin, 0, histogram_bins - 1);
        histogram[bin] += 1;
    }

    std::vector<double> band_medians(height_bands, -1.0);
    std::vector<double> band_coverage(height_bands, 0.0);
    for (int band = 0; band < height_bands; ++band) {
        const int y0 = band * grid_h / height_bands;
        const int y1 = (band + 1) * grid_h / height_bands;
        std::vector<int> hits;
        hits.reserve((y1 - y0) * grid_w);
        int covered = 0;
        for (int gy = y0; gy < y1; ++gy) {
            for (int gx = 0; gx < grid_w; ++gx) {
                const Cell& cell = grid[gy * grid_w + gx];
                if (cell.first_hit > 0) {
                    hits.push_back(cell.first_hit);
                    ++covered;
                }
            }
        }
        band_medians[band] = median(hits);
        band_coverage[band] = static_cast<double>(covered) / ((y1 - y0) * grid_w);
    }

    const double overall_median = median(first_hits);
    std::vector<int> upper_hits;
    std::vector<int> lower_hits;
    for (int gy = 0; gy < grid_h; ++gy) {
        for (int gx = 0; gx < grid_w; ++gx) {
            const Cell& cell = grid[gy * grid_w + gx];
            if (cell.first_hit < 0) continue;
            const double y_center = min_y + (gy + 0.5) / grid_h * (max_y - min_y);
            if (y_center >= 6.5) upper_hits.push_back(cell.first_hit);
            if (y_center <= 3.0) lower_hits.push_back(cell.first_hit);
        }
    }
    const double upper_median = median(upper_hits);
    const double lower_median = median(lower_hits);

    const int width = 1600;
    const int height = 1160;
    const double left = 62.0;
    const double top = 148.0;
    const double map_w = 780.0;
    const double map_h = 930.0;
    const double right_x = 876.0;
    const double chart_w = 666.0;
    const double chart_h = 312.0;
    const double lower_chart_y = 628.0;
    const double upper_chart_y = 210.0;

    auto cell_x = [&](int gx) {
        return left + (static_cast<double>(gx) / grid_w) * map_w;
    };
    auto cell_y = [&](int gy) {
        return top + map_h - (static_cast<double>(gy + 1) / grid_h) * map_h;
    };
    auto hist_x = [&](int bin) {
        return right_x + 64.0 + (static_cast<double>(bin) / histogram_bins) * (chart_w - 118.0);
    };
    const int max_hist = *std::max_element(histogram.begin(), histogram.end());
    auto hist_y = [&](double value) {
        return upper_chart_y + chart_h - 46.0 - (value / max_hist) * (chart_h - 96.0);
    };
    auto median_x = [&](double step_value) {
        if (step_value <= 0.0) return right_x + 64.0;
        const double t = (std::log10(step_value) - log_min) / (log_max - log_min);
        return right_x + 64.0 + t * (chart_w - 118.0);
    };
    auto band_y = [&](int band) {
        return lower_chart_y + chart_h - 52.0 - (static_cast<double>(band) + 0.5) / height_bands * (chart_h - 98.0);
    };

    std::ofstream out(output_svg);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << width << ' ' << height
        << "\" width=\"" << width << "\" height=\"" << height << "\">\n";
    out << "<defs>\n";
    out << "  <linearGradient id=\"bg\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"1\">\n";
    out << "    <stop offset=\"0%\" stop-color=\"#03140f\"/>\n";
    out << "    <stop offset=\"50%\" stop-color=\"#081f18\"/>\n";
    out << "    <stop offset=\"100%\" stop-color=\"#020b08\"/>\n";
    out << "  </linearGradient>\n";
    out << "</defs>\n";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"url(#bg)\"/>\n";
    out << "<text x=\"62\" y=\"58\" fill=\"#dcfce7\" font-size=\"34\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Barnsley fern first-hit map</text>\n";
    out << "<text x=\"62\" y=\"87\" fill=\"#86efac\" font-size=\"18\" font-family=\"Helvetica, Arial, sans-serif\">One orbit, one coarse grid, and a narrower question: which parts of the fern get claimed early and which ones only settle much later?</text>\n";
    out << "<text x=\"62\" y=\"115\" fill=\"#bbf7d0\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">Cells are colored by the first post-burn-in iteration that touched them, on a log scale so early stem growth and slow canopy fill can share the same card honestly.</text>\n";

    out << "<rect x=\"" << left << "\" y=\"" << top << "\" width=\"" << map_w << "\" height=\"" << map_h
        << "\" rx=\"22\" fill=\"#061711\" fill-opacity=\"0.88\" stroke=\"#1f3b30\" stroke-width=\"1.5\"/>\n";
    out << "<text x=\"84\" y=\"182\" fill=\"#d1fae5\" font-size=\"22\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Coarse first-hit field</text>\n";
    out << "<text x=\"84\" y=\"206\" fill=\"#a7f3d0\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">cool = early, warm = late</text>\n";

    const double draw_left = left + 26.0;
    const double draw_top = top + 70.0;
    const double draw_w = map_w - 52.0;
    const double draw_h = map_h - 118.0;
    for (int gy = 0; gy < grid_h; ++gy) {
        for (int gx = 0; gx < grid_w; ++gx) {
            const Cell& cell = grid[gy * grid_w + gx];
            const double x = draw_left + (static_cast<double>(gx) / grid_w) * draw_w;
            const double y = draw_top + draw_h - (static_cast<double>(gy + 1) / grid_h) * draw_h;
            const double w = draw_w / grid_w + 0.2;
            const double h = draw_h / grid_h + 0.2;
            if (cell.first_hit < 0) {
                out << "<rect x=\"" << std::fixed << std::setprecision(2) << x << "\" y=\"" << y << "\" width=\"" << w
                    << "\" height=\"" << h << "\" fill=\"#04110c\"/>\n";
            } else {
                const double t = (std::log10(static_cast<double>(cell.first_hit)) - log_min) / (log_max - log_min);
                out << "<rect x=\"" << std::fixed << std::setprecision(2) << x << "\" y=\"" << y << "\" width=\"" << w
                    << "\" height=\"" << h << "\" fill=\"" << color_for(t) << "\"/>\n";
            }
        }
    }

    const double legend_x = left + 40.0;
    const double legend_y = top + map_h - 30.0;
    for (int i = 0; i < 220; ++i) {
        const double t = static_cast<double>(i) / 219.0;
        out << "<rect x=\"" << (legend_x + i * 2.0) << "\" y=\"" << legend_y << "\" width=\"2.1\" height=\"10\" fill=\"" << color_for(t) << "\"/>\n";
    }
    out << "<text x=\"" << legend_x << "\" y=\"" << (legend_y - 6.0) << "\" fill=\"#bbf7d0\" font-size=\"11\" font-family=\"Helvetica, Arial, sans-serif\">1</text>\n";
    out << "<text x=\"" << (legend_x + 110.0) << "\" y=\"" << (legend_y - 6.0) << "\" fill=\"#bbf7d0\" font-size=\"11\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">first-hit iteration (log scale)</text>\n";
    out << "<text x=\"" << (legend_x + 220.0) << "\" y=\"" << (legend_y - 6.0) << "\" fill=\"#bbf7d0\" font-size=\"11\" text-anchor=\"end\" font-family=\"Helvetica, Arial, sans-serif\">" << total_iterations << "</text>\n";

    out << "<rect x=\"" << right_x << "\" y=\"" << upper_chart_y << "\" width=\"" << chart_w << "\" height=\"" << chart_h
        << "\" rx=\"22\" fill=\"#061711\" fill-opacity=\"0.88\" stroke=\"#1f3b30\" stroke-width=\"1.5\"/>\n";
    out << "<text x=\"" << (right_x + 20.0) << "\" y=\"" << (upper_chart_y + 30.0)
        << "\" fill=\"#d1fae5\" font-size=\"18\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Occupancy curve</text>\n";
    out << "<text x=\"" << (right_x + 20.0) << "\" y=\"" << (upper_chart_y + 54.0)
        << "\" fill=\"#a7f3d0\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">same orbit, same grid</text>\n";

    for (int tick = 0; tick <= 4; ++tick) {
        const double frac = 0.25 * tick;
        const double y = upper_chart_y + chart_h - 46.0 - frac * (chart_h - 96.0);
        out << "<line x1=\"" << (right_x + 64.0) << "\" y1=\"" << y << "\" x2=\"" << (right_x + chart_w - 54.0)
            << "\" y2=\"" << y << "\" stroke=\"#183126\" stroke-width=\"1\"/>\n";
        out << "<text x=\"" << (right_x + 54.0) << "\" y=\"" << (y + 4.0) << "\" fill=\"#bbf7d0\" font-size=\"11\" text-anchor=\"end\" font-family=\"Helvetica, Arial, sans-serif\">"
            << std::fixed << std::setprecision(0) << frac * 100.0 << "%</text>\n";
    }
    for (int tick = 0; tick <= 4; ++tick) {
        const double frac = 0.25 * tick;
        const double x = right_x + 64.0 + frac * (chart_w - 118.0);
        const double step_value = std::pow(10.0, log_min + frac * (log_max - log_min));
        out << "<line x1=\"" << x << "\" y1=\"" << (upper_chart_y + 82.0) << "\" x2=\"" << x << "\" y2=\"" << (upper_chart_y + chart_h - 46.0)
            << "\" stroke=\"#183126\" stroke-width=\"1\"/>\n";
        out << "<text x=\"" << x << "\" y=\"" << (upper_chart_y + chart_h - 18.0) << "\" fill=\"#bbf7d0\" font-size=\"11\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">"
            << static_cast<int>(std::round(step_value)) << "</text>\n";
    }
    out << "<polyline fill=\"none\" stroke=\"#93c5fd\" stroke-width=\"3.2\" points=\"";
    for (std::size_t i = 0; i < prefix_steps.size(); ++i) {
        const double x = median_x(prefix_steps[i]);
        const double frac = occupancy_prefixes[i] / static_cast<double>(grid_w * grid_h);
        const double y = upper_chart_y + chart_h - 46.0 - frac * (chart_h - 96.0);
        out << x << ',' << y << ' ';
    }
    out << "\"/>\n";
    for (std::size_t i = 0; i < prefix_steps.size(); ++i) {
        const double x = median_x(prefix_steps[i]);
        const double frac = occupancy_prefixes[i] / static_cast<double>(grid_w * grid_h);
        const double y = upper_chart_y + chart_h - 46.0 - frac * (chart_h - 96.0);
        out << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"5.4\" fill=\"#dbeafe\" stroke=\"#2563eb\" stroke-width=\"2\"/>\n";
    }

    out << "<rect x=\"" << right_x << "\" y=\"" << lower_chart_y << "\" width=\"" << chart_w << "\" height=\"" << chart_h
        << "\" rx=\"22\" fill=\"#061711\" fill-opacity=\"0.88\" stroke=\"#1f3b30\" stroke-width=\"1.5\"/>\n";
    out << "<text x=\"" << (right_x + 20.0) << "\" y=\"" << (lower_chart_y + 30.0)
        << "\" fill=\"#d1fae5\" font-size=\"18\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Arrival vs height</text>\n";
    out << "<text x=\"" << (right_x + 20.0) << "\" y=\"" << (lower_chart_y + 54.0)
        << "\" fill=\"#a7f3d0\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">stem first, canopy later</text>\n";

    for (int tick = 0; tick <= 4; ++tick) {
        const double frac = 0.25 * tick;
        const double x = right_x + 64.0 + frac * (chart_w - 118.0);
        const double step_value = std::pow(10.0, log_min + frac * (log_max - log_min));
        out << "<line x1=\"" << x << "\" y1=\"" << (lower_chart_y + 88.0) << "\" x2=\"" << x << "\" y2=\"" << (lower_chart_y + chart_h - 52.0)
            << "\" stroke=\"#183126\" stroke-width=\"1\"/>\n";
        out << "<text x=\"" << x << "\" y=\"" << (lower_chart_y + chart_h - 18.0) << "\" fill=\"#bbf7d0\" font-size=\"11\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">"
            << static_cast<int>(std::round(step_value)) << "</text>\n";
    }

    std::vector<std::pair<double, double>> band_points;
    for (int band = 0; band < height_bands; ++band) {
        const double y = band_y(band);
        const double y_level = min_y + (static_cast<double>(band) + 0.5) / height_bands * (max_y - min_y);
        out << "<line x1=\"" << (right_x + 64.0) << "\" y1=\"" << y << "\" x2=\"" << (right_x + chart_w - 54.0)
            << "\" y2=\"" << y << "\" stroke=\"#11241c\" stroke-width=\"1\"/>\n";
        out << "<text x=\"" << (right_x + 50.0) << "\" y=\"" << (y + 4.0) << "\" fill=\"#bbf7d0\" font-size=\"11\" text-anchor=\"end\" font-family=\"Helvetica, Arial, sans-serif\">"
            << std::fixed << std::setprecision(1) << y_level << "</text>\n";
        if (band_medians[band] > 0.0) {
            const double x = median_x(band_medians[band]);
            const double radius = 4.0 + 10.0 * band_coverage[band];
            const double t = (std::log10(band_medians[band]) - log_min) / (log_max - log_min);
            band_points.push_back({x, y});
            out << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"" << radius
                << "\" fill=\"" << color_for(t) << "\" stroke=\"#f8fafc\" stroke-width=\"1.2\" fill-opacity=\"0.9\"/>\n";
        }
    }
    if (!band_points.empty()) {
        out << "<polyline fill=\"none\" stroke=\"#e2e8f0\" stroke-width=\"2.0\" stroke-opacity=\"0.65\" points=\"";
        for (const auto& point : band_points) out << point.first << ',' << point.second << ' ';
        out << "\"/>\n";
    }

    out << "<text x=\"" << (right_x + 64.0) << "\" y=\"" << (lower_chart_y + chart_h - 18.0)
        << "\" fill=\"#94a3b8\" font-size=\"11\" font-family=\"Helvetica, Arial, sans-serif\">earlier</text>\n";
    out << "<text x=\"" << (right_x + chart_w - 54.0) << "\" y=\"" << (lower_chart_y + chart_h - 18.0)
        << "\" fill=\"#94a3b8\" font-size=\"11\" text-anchor=\"end\" font-family=\"Helvetica, Arial, sans-serif\">later</text>\n";

    out << "<text x=\"" << (right_x + 20.0) << "\" y=\"1032\" fill=\"#d1fae5\" font-size=\"17\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Quick read</text>\n";
    out << "<text x=\"" << (right_x + 20.0) << "\" y=\"1058\" fill=\"#bbf7d0\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">grid median: " << static_cast<int>(std::round(overall_median)) << " iters</text>\n";
    out << "<text x=\"" << (right_x + 20.0) << "\" y=\"1080\" fill=\"#bbf7d0\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">lower stem median: " << static_cast<int>(std::round(lower_median)) << "</text>\n";
    out << "<text x=\"" << (right_x + 20.0) << "\" y=\"1102\" fill=\"#bbf7d0\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">upper canopy median: " << static_cast<int>(std::round(upper_median)) << "</text>\n";
    out << "<text x=\"" << (right_x + 338.0) << "\" y=\"1058\" fill=\"#bbf7d0\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">claimed by 5,200 iters: "
        << std::fixed << std::setprecision(1) << (100.0 * occupancy_prefixes[3] / static_cast<double>(grid_w * grid_h)) << "%</text>\n";
    out << "<text x=\"" << (right_x + 338.0) << "\" y=\"1080\" fill=\"#bbf7d0\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">claimed by 90,000 iters: "
        << std::fixed << std::setprecision(1) << (100.0 * claimed / static_cast<double>(grid_w * grid_h)) << "%</text>\n";
    out << "<text x=\"" << (right_x + 338.0) << "\" y=\"1102\" fill=\"#bbf7d0\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">same rules, very different arrival times</text>\n";

    out << "</svg>\n";
    if (!out) {
        std::cerr << "failed to write " << output_svg << '\n';
        return 1;
    }

    std::ofstream csv(output_csv);
    csv << "gx,gy,x_center,y_center,first_hit,hits\n";
    for (int gy = 0; gy < grid_h; ++gy) {
        for (int gx = 0; gx < grid_w; ++gx) {
            const Cell& cell = grid[gy * grid_w + gx];
            const double x_center = min_x + (gx + 0.5) / grid_w * (max_x - min_x);
            const double y_center = min_y + (gy + 0.5) / grid_h * (max_y - min_y);
            csv << gx << ',' << gy << ',' << x_center << ',' << y_center << ',' << cell.first_hit << ',' << cell.hits << '\n';
        }
    }
    if (!csv) {
        std::cerr << "failed to write " << output_csv << '\n';
        return 1;
    }

    std::cout << output_svg << '\n' << output_csv << '\n';
    return 0;
}
