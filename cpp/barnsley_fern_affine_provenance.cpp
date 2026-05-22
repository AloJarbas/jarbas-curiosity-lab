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

struct StepResult {
    Point point;
    int transform_index;
};

struct Cell {
    std::array<int, 4> transform_hits{};
    int total_hits = 0;
};

static StepResult step(Point p, double r) {
    if (r < 0.01) return {{0.0, 0.16 * p.y}, 0};
    if (r < 0.86) return {{0.85 * p.x + 0.04 * p.y, -0.04 * p.x + 0.85 * p.y + 1.6}, 1};
    if (r < 0.93) return {{0.20 * p.x - 0.26 * p.y, 0.23 * p.x + 0.22 * p.y + 1.6}, 2};
    return {{-0.15 * p.x + 0.28 * p.y, 0.26 * p.x + 0.24 * p.y + 0.44}, 3};
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

static std::string mix_color(const std::array<int, 3>& a, const std::array<int, 3>& b, double t) {
    t = std::clamp(t, 0.0, 1.0);
    std::array<int, 3> mixed{};
    for (int i = 0; i < 3; ++i) {
        mixed[i] = static_cast<int>(std::round(a[i] + (b[i] - a[i]) * t));
    }
    return hex_color(mixed);
}

static std::string format_percent(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << value * 100.0 << '%';
    return out.str();
}

static std::string format_ratio(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    return out.str();
}

static double entropy_bits(const std::array<int, 4>& counts, int total) {
    if (total <= 0) return 0.0;
    double entropy = 0.0;
    for (int count : counts) {
        if (count <= 0) continue;
        const double p = static_cast<double>(count) / total;
        entropy -= p * std::log2(p);
    }
    return entropy;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        std::cout << "usage: " << argv[0] << " [output.svg] [output.csv]\n";
        std::cout << "render a Barnsley fern affine-rule provenance card\n";
        return 0;
    }

    const std::string output_svg = argc > 1 ? argv[1] : "art/barnsley-fern-affine-provenance.svg";
    const std::string output_csv = argc > 2 ? argv[2] : "art/barnsley-fern-affine-provenance.csv";

    constexpr double min_x = -3.0;
    constexpr double max_x = 3.0;
    constexpr double min_y = 0.0;
    constexpr double max_y = 10.0;
    constexpr int burn_in = 120;
    constexpr int total_iterations = 120000;
    constexpr int grid_w = 120;
    constexpr int grid_h = 200;
    constexpr int height_bands = 16;

    const std::array<std::string, 4> transform_names{{
        "stem",
        "bulk frond",
        "left leaflet",
        "right leaflet",
    }};
    const std::array<std::array<int, 3>, 4> transform_colors{{
        std::array<int, 3>{16, 185, 129},
        std::array<int, 3>{34, 197, 94},
        std::array<int, 3>{56, 189, 248},
        std::array<int, 3>{250, 204, 21},
    }};
    const std::array<int, 3> empty_color{5, 18, 14};
    const std::array<int, 3> frame_color{31, 59, 48};

    std::vector<Cell> grid(grid_w * grid_h);
    std::array<long long, 4> global_hits{};
    std::array<int, 4> dominant_cells{};
    std::array<std::array<long long, 4>, height_bands> band_hits{};
    std::array<int, height_bands> band_visited{};
    std::array<double, height_bands> band_entropy_sum{};
    std::array<double, height_bands> band_purity_sum{};

    std::mt19937_64 rng(424242);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    Point p{0.0, 0.0};

    for (int raw_step = 0; raw_step < total_iterations + burn_in; ++raw_step) {
        const StepResult next = step(p, dist(rng));
        p = next.point;
        if (raw_step < burn_in) continue;
        if (p.x < min_x || p.x > max_x || p.y < min_y || p.y > max_y) continue;

        int gx = static_cast<int>((p.x - min_x) / (max_x - min_x) * grid_w);
        int gy = static_cast<int>((p.y - min_y) / (max_y - min_y) * grid_h);
        gx = std::clamp(gx, 0, grid_w - 1);
        gy = std::clamp(gy, 0, grid_h - 1);
        Cell& cell = grid[gy * grid_w + gx];
        cell.transform_hits[next.transform_index] += 1;
        cell.total_hits += 1;
        global_hits[next.transform_index] += 1;
    }

    int visited_cells = 0;
    double entropy_sum = 0.0;
    double purity_sum = 0.0;
    int strongest_cells = 0;

    for (int gy = 0; gy < grid_h; ++gy) {
        const int band = std::clamp(gy * height_bands / grid_h, 0, height_bands - 1);
        for (int gx = 0; gx < grid_w; ++gx) {
            const Cell& cell = grid[gy * grid_w + gx];
            if (cell.total_hits <= 0) continue;
            ++visited_cells;
            const auto max_it = std::max_element(cell.transform_hits.begin(), cell.transform_hits.end());
            const int dominant = static_cast<int>(std::distance(cell.transform_hits.begin(), max_it));
            const double purity = static_cast<double>(*max_it) / cell.total_hits;
            const double entropy = entropy_bits(cell.transform_hits, cell.total_hits);
            dominant_cells[dominant] += 1;
            entropy_sum += entropy;
            purity_sum += purity;
            if (purity >= 0.75) ++strongest_cells;
            band_visited[band] += 1;
            band_entropy_sum[band] += entropy;
            band_purity_sum[band] += purity;
            for (int t = 0; t < 4; ++t) band_hits[band][t] += cell.transform_hits[t];
        }
    }

    if (visited_cells == 0) {
        std::cerr << "no visited cells recorded\n";
        return 1;
    }

    const long long total_hit_count = std::accumulate(global_hits.begin(), global_hits.end(), 0LL);
    const double mean_entropy = entropy_sum / visited_cells;
    const double mean_purity = purity_sum / visited_cells;
    const double strong_cell_share = static_cast<double>(strongest_cells) / visited_cells;

    const int width = 1600;
    const int height = 1600;
    const double map_left = 60.0;
    const double map_top = 172.0;
    const double map_width = 860.0;
    const double map_height = 1320.0;
    const double chart_left = 954.0;
    const double top_chart_top = 210.0;
    const double chart_width = 586.0;
    const double top_chart_height = 610.0;
    const double bottom_chart_top = 864.0;
    const double bottom_chart_height = 516.0;

    const double draw_left = map_left + 28.0;
    const double draw_top = map_top + 82.0;
    const double draw_width = map_width - 56.0;
    const double draw_height = map_height - 168.0;

    std::ofstream out(output_svg);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << width << ' ' << height
        << "\" width=\"" << width << "\" height=\"" << height << "\">\n";
    out << "<defs>\n";
    out << "  <linearGradient id=\"bg\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"1\">\n";
    out << "    <stop offset=\"0%\" stop-color=\"#03140f\"/>\n";
    out << "    <stop offset=\"55%\" stop-color=\"#0a2219\"/>\n";
    out << "    <stop offset=\"100%\" stop-color=\"#04120d\"/>\n";
    out << "  </linearGradient>\n";
    out << "</defs>\n";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"url(#bg)\"/>\n";
    out << "<text x=\"60\" y=\"58\" fill=\"#dcfce7\" font-size=\"34\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Barnsley fern affine-rule provenance</text>\n";
    out << "<text x=\"60\" y=\"86\" fill=\"#86efac\" font-size=\"18\" font-family=\"Helvetica, Arial, sans-serif\">Not every part of the fern is built the same way. This card asks which affine rule actually owns each coarse cell.</text>\n";
    out << "<text x=\"60\" y=\"112\" fill=\"#bbf7d0\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">Cell color = dominant affine rule. Brighter cells mean cleaner ownership; darker cells are mixed-rule neighborhoods.</text>\n";

    out << "<rect x=\"" << map_left << "\" y=\"" << map_top << "\" width=\"" << map_width
        << "\" height=\"" << map_height << "\" rx=\"24\" fill=\"#061711\" fill-opacity=\"0.88\" stroke=\""
        << hex_color(frame_color) << "\" stroke-width=\"1.5\"/>\n";
    out << "<text x=\"84\" y=\"208\" fill=\"#d1fae5\" font-size=\"22\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Dominant rule field</text>\n";
    out << "<text x=\"84\" y=\"232\" fill=\"#a7f3d0\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">stem = mint, bulk frond = green, left leaflet = blue, right leaflet = gold</text>\n";

    for (int gy = 0; gy < grid_h; ++gy) {
        for (int gx = 0; gx < grid_w; ++gx) {
            const Cell& cell = grid[gy * grid_w + gx];
            const double x = draw_left + (static_cast<double>(gx) / grid_w) * draw_width;
            const double y = draw_top + draw_height - (static_cast<double>(gy + 1) / grid_h) * draw_height;
            const double w = draw_width / grid_w + 0.20;
            const double h = draw_height / grid_h + 0.20;
            if (cell.total_hits <= 0) {
                out << "<rect x=\"" << std::fixed << std::setprecision(2) << x << "\" y=\"" << y
                    << "\" width=\"" << w << "\" height=\"" << h << "\" fill=\"" << hex_color(empty_color) << "\"/>\n";
                continue;
            }
            const auto max_it = std::max_element(cell.transform_hits.begin(), cell.transform_hits.end());
            const int dominant = static_cast<int>(std::distance(cell.transform_hits.begin(), max_it));
            const double purity = static_cast<double>(*max_it) / cell.total_hits;
            const double t = 0.22 + 0.78 * ((purity - 0.25) / 0.75);
            out << "<rect x=\"" << std::fixed << std::setprecision(2) << x << "\" y=\"" << y
                << "\" width=\"" << w << "\" height=\"" << h << "\" fill=\""
                << mix_color(empty_color, transform_colors[dominant], t) << "\"/>\n";
        }
    }

    const double legend_y = map_top + map_height - 58.0;
    for (int t = 0; t < 4; ++t) {
        const double lx = map_left + 34.0 + t * 195.0;
        out << "<rect x=\"" << lx << "\" y=\"" << legend_y << "\" width=\"20\" height=\"20\" rx=\"4\" fill=\""
            << hex_color(transform_colors[t]) << "\"/>\n";
        out << "<text x=\"" << (lx + 30.0) << "\" y=\"" << (legend_y + 15.0)
            << "\" fill=\"#d1fae5\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">"
            << transform_names[t] << "</text>\n";
    }
    out << "<text x=\"84\" y=\"" << (legend_y - 26.0)
        << "\" fill=\"#bbf7d0\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">visited cells "
        << visited_cells << " / " << (grid_w * grid_h) << " · mean purity " << format_percent(mean_purity)
        << "</text>\n";
    out << "<text x=\"84\" y=\"" << (legend_y - 8.0)
        << "\" fill=\"#bbf7d0\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">mean entropy "
        << format_ratio(mean_entropy) << " bits</text>\n";

    out << "<rect x=\"" << chart_left << "\" y=\"" << top_chart_top << "\" width=\"" << chart_width
        << "\" height=\"" << top_chart_height << "\" rx=\"22\" fill=\"#061711\" fill-opacity=\"0.88\" stroke=\""
        << hex_color(frame_color) << "\" stroke-width=\"1.5\"/>\n";
    out << "<text x=\"972\" y=\"246\" fill=\"#d1fae5\" font-size=\"22\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Rule mix by height band</text>\n";
    out << "<text x=\"972\" y=\"270\" fill=\"#a7f3d0\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">Height changes the rule mix.</text>\n";

    const double band_left = chart_left + 112.0;
    const double band_top = top_chart_top + 100.0;
    const double band_width = chart_width - 152.0;
    const double band_height = top_chart_height - 152.0;
    for (int tick = 0; tick <= 4; ++tick) {
        const double frac = tick / 4.0;
        const double x = band_left + frac * band_width;
        out << "<line x1=\"" << x << "\" y1=\"" << band_top << "\" x2=\"" << x << "\" y2=\""
            << (band_top + band_height) << "\" stroke=\"#173328\" stroke-width=\"1\"/>\n";
        out << "<text x=\"" << x << "\" y=\"" << (band_top + band_height + 22.0)
            << "\" fill=\"#bbf7d0\" font-size=\"11\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">"
            << static_cast<int>(frac * 100.0) << "%</text>\n";
    }
    for (int band = 0; band < height_bands; ++band) {
        const double y = band_top + band_height - (band + 1.0) / height_bands * band_height;
        const double h = band_height / height_bands - 4.0;
        out << "<text x=\"" << (band_left - 12.0) << "\" y=\"" << (y + h * 0.75)
            << "\" fill=\"#bbf7d0\" font-size=\"11\" text-anchor=\"end\" font-family=\"Helvetica, Arial, sans-serif\">"
            << std::fixed << std::setprecision(1) << (band + 0.5) * (max_y - min_y) / height_bands << "</text>\n";
        const long long band_total = std::accumulate(band_hits[band].begin(), band_hits[band].end(), 0LL);
        double x = band_left;
        if (band_total <= 0) {
            out << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << band_width << "\" height=\"" << h
                << "\" fill=\"#07130f\"/>\n";
            continue;
        }
        for (int t = 0; t < 4; ++t) {
            const double w = band_width * (static_cast<double>(band_hits[band][t]) / band_total);
            out << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h
                << "\" fill=\"" << hex_color(transform_colors[t]) << "\"/>\n";
            x += w;
        }
    }
    out << "<text x=\"" << (band_left + band_width * 0.5) << "\" y=\"" << (band_top + band_height + 44.0)
        << "\" fill=\"#c7d2fe\" font-size=\"13\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">share of in-bounds hits within that height band</text>\n";

    out << "<rect x=\"" << chart_left << "\" y=\"" << bottom_chart_top << "\" width=\"" << chart_width
        << "\" height=\"" << bottom_chart_height << "\" rx=\"22\" fill=\"#061711\" fill-opacity=\"0.88\" stroke=\""
        << hex_color(frame_color) << "\" stroke-width=\"1.5\"/>\n";
    out << "<text x=\"972\" y=\"900\" fill=\"#d1fae5\" font-size=\"22\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Global share: hits vs dominant cells</text>\n";
    out << "<text x=\"972\" y=\"924\" fill=\"#a7f3d0\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">Left bar = share of in-bounds orbit hits. Right bar = share of visited cells where that rule wins locally.</text>\n";

    const double bar_left = chart_left + 70.0;
    const double bar_top = bottom_chart_top + 110.0;
    const double bar_width = chart_width - 120.0;
    const double bar_height = 196.0;
    const double baseline = bar_top + bar_height;
    for (int tick = 0; tick <= 4; ++tick) {
        const double frac = tick / 4.0;
        const double y = baseline - frac * bar_height;
        out << "<line x1=\"" << bar_left << "\" y1=\"" << y << "\" x2=\"" << (bar_left + bar_width)
            << "\" y2=\"" << y << "\" stroke=\"#173328\" stroke-width=\"1\"/>\n";
        out << "<text x=\"" << (bar_left - 10.0) << "\" y=\"" << (y + 4.0)
            << "\" fill=\"#bbf7d0\" font-size=\"11\" text-anchor=\"end\" font-family=\"Helvetica, Arial, sans-serif\">"
            << static_cast<int>(frac * 100.0) << "%</text>\n";
    }

    for (int t = 0; t < 4; ++t) {
        const double group_center = bar_left + (t + 0.5) / 4.0 * bar_width;
        const double bar_w = 42.0;
        const double hit_share = static_cast<double>(global_hits[t]) / total_hit_count;
        const double cell_share = static_cast<double>(dominant_cells[t]) / visited_cells;
        const double hit_h = hit_share * bar_height;
        const double cell_h = cell_share * bar_height;
        out << "<rect x=\"" << (group_center - 52.0) << "\" y=\"" << (baseline - hit_h) << "\" width=\"" << bar_w
            << "\" height=\"" << hit_h << "\" rx=\"8\" fill=\"" << hex_color(transform_colors[t]) << "\" fill-opacity=\"0.95\"/>\n";
        out << "<rect x=\"" << (group_center + 10.0) << "\" y=\"" << (baseline - cell_h) << "\" width=\"" << bar_w
            << "\" height=\"" << cell_h << "\" rx=\"8\" fill=\"" << hex_color(transform_colors[t]) << "\" fill-opacity=\"0.45\" stroke=\""
            << hex_color(transform_colors[t]) << "\" stroke-width=\"1.4\"/>\n";
        out << "<text x=\"" << group_center << "\" y=\"" << (baseline + 24.0)
            << "\" fill=\"#d1fae5\" font-size=\"12\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">"
            << transform_names[t] << "</text>\n";
        out << "<text x=\"" << group_center << "\" y=\"" << (baseline + 40.0)
            << "\" fill=\"#94a3b8\" font-size=\"11\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">hits / cells</text>\n";
    }

    auto bullet_y = bottom_chart_top + 374.0;
    const int bulk_index = 1;
    const int stem_index = 0;
    const int left_index = 2;
    const int right_index = 3;
    const double bulk_hit_share = static_cast<double>(global_hits[bulk_index]) / total_hit_count;
    const double bulk_cell_share = static_cast<double>(dominant_cells[bulk_index]) / visited_cells;
    const double stem_hit_share = static_cast<double>(global_hits[stem_index]) / total_hit_count;
    const double stem_cell_share = static_cast<double>(dominant_cells[stem_index]) / visited_cells;
    const double flank_delta = std::abs(static_cast<double>(global_hits[left_index] - global_hits[right_index])) / total_hit_count;

    out << "<text x=\"972\" y=\"" << bullet_y << "\" fill=\"#d1fae5\" font-size=\"15\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">What this card says</text>\n";
    out << "<text x=\"972\" y=\"" << (bullet_y + 28.0) << "\" fill=\"#cbd5e1\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">• bulk frond rule: "
        << format_percent(bulk_hit_share) << " of hits, " << format_percent(bulk_cell_share) << " of dominant cells</text>\n";
    out << "<text x=\"972\" y=\"" << (bullet_y + 52.0) << "\" fill=\"#cbd5e1\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">• stem rule: only "
        << format_percent(stem_hit_share) << " of hits, but still owns " << format_percent(stem_cell_share) << " of the visited grid</text>\n";
    out << "<text x=\"972\" y=\"" << (bullet_y + 76.0) << "\" fill=\"#cbd5e1\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">• left/right leaflet hit imbalance stays small at "
        << format_percent(flank_delta) << " of all in-bounds hits</text>\n";
    out << "<text x=\"972\" y=\"" << (bullet_y + 110.0) << "\" fill=\"#86efac\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">Mean purity "
        << format_percent(mean_purity) << " · strong-rule cells (≥75% one rule) " << format_percent(strong_cell_share)
        << " · total in-bounds hits " << total_hit_count << "</text>\n";

    out << "</svg>\n";
    out.close();

    std::ofstream csv(output_csv);
    csv << "gx,gy,x_center,y_center,total_hits,dominant_transform,dominant_share,entropy_bits,share_stem,share_bulk,share_left,share_right\n";
    for (int gy = 0; gy < grid_h; ++gy) {
        for (int gx = 0; gx < grid_w; ++gx) {
            const Cell& cell = grid[gy * grid_w + gx];
            if (cell.total_hits <= 0) continue;
            const auto max_it = std::max_element(cell.transform_hits.begin(), cell.transform_hits.end());
            const int dominant = static_cast<int>(std::distance(cell.transform_hits.begin(), max_it));
            const double dominant_share = static_cast<double>(*max_it) / cell.total_hits;
            const double entropy = entropy_bits(cell.transform_hits, cell.total_hits);
            const double x_center = min_x + (gx + 0.5) / grid_w * (max_x - min_x);
            const double y_center = min_y + (gy + 0.5) / grid_h * (max_y - min_y);
            csv << gx << ',' << gy << ',' << std::fixed << std::setprecision(6)
                << x_center << ',' << y_center << ',' << cell.total_hits << ',' << dominant << ','
                << dominant_share << ',' << entropy;
            for (int t = 0; t < 4; ++t) {
                csv << ',' << (static_cast<double>(cell.transform_hits[t]) / cell.total_hits);
            }
            csv << '\n';
        }
    }

    std::cout << "wrote " << output_svg << " and " << output_csv << "\n";
    return 0;
}
