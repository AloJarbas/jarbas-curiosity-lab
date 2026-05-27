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

static std::string format_fixed(double value, int places = 2) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(places) << value;
    return out.str();
}

static std::string format_percent(double value) {
    return format_fixed(value * 100.0, 1) + "%";
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        std::cout << "usage: " << argv[0] << " [output.svg] [output.csv]\n";
        std::cout << "render a Barnsley fern rule-entropy card\n";
        return 0;
    }

    const std::string output_svg = argc > 1 ? argv[1] : "art/barnsley-fern-rule-entropy.svg";
    const std::string output_csv = argc > 2 ? argv[2] : "art/barnsley-fern-rule-entropy.csv";

    constexpr double min_x = -3.0;
    constexpr double max_x = 3.0;
    constexpr double min_y = 0.0;
    constexpr double max_y = 10.0;
    constexpr int burn_in = 120;
    constexpr int total_iterations = 140000;
    constexpr int grid_w = 120;
    constexpr int grid_h = 200;
    constexpr int height_bands = 16;
    constexpr int histogram_bins = 10;
    constexpr double max_entropy = 2.0;

    const std::array<int, 3> empty_color{4, 15, 12};
    const std::array<int, 3> low_entropy_color{21, 94, 117};
    const std::array<int, 3> mid_entropy_color{168, 85, 247};
    const std::array<int, 3> high_entropy_color{250, 204, 21};
    const std::array<int, 3> frame_color{41, 59, 92};

    std::vector<Cell> grid(grid_w * grid_h);
    std::array<int, histogram_bins> entropy_histogram{};
    std::array<int, height_bands> band_visited{};
    std::array<double, height_bands> band_entropy_sum{};
    std::array<double, height_bands> band_mixed_share_sum{};
    std::array<int, height_bands> band_high_mix_cells{};

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
    }

    int visited_cells = 0;
    int dense_mix_cells = 0;
    int low_mix_cells = 0;
    double entropy_sum = 0.0;
    double normalized_entropy_sum = 0.0;
    int max_hits = 0;

    for (const Cell& cell : grid) {
        max_hits = std::max(max_hits, cell.total_hits);
    }

    for (int gy = 0; gy < grid_h; ++gy) {
        const int band = std::clamp(gy * height_bands / grid_h, 0, height_bands - 1);
        for (int gx = 0; gx < grid_w; ++gx) {
            const Cell& cell = grid[gy * grid_w + gx];
            if (cell.total_hits <= 0) continue;
            ++visited_cells;
            const double entropy = entropy_bits(cell.transform_hits, cell.total_hits);
            const double normalized_entropy = entropy / max_entropy;
            entropy_sum += entropy;
            normalized_entropy_sum += normalized_entropy;
            if (entropy >= 1.4) ++dense_mix_cells;
            if (entropy <= 0.35) ++low_mix_cells;
            const int bin = std::clamp(static_cast<int>(std::floor(normalized_entropy * histogram_bins)), 0, histogram_bins - 1);
            entropy_histogram[bin] += 1;
            band_visited[band] += 1;
            band_entropy_sum[band] += entropy;
            band_mixed_share_sum[band] += normalized_entropy;
            if (entropy >= 1.4) band_high_mix_cells[band] += 1;
        }
    }

    if (visited_cells == 0) {
        std::cerr << "no visited cells recorded\n";
        return 1;
    }

    const double mean_entropy = entropy_sum / visited_cells;
    const double mean_normalized_entropy = normalized_entropy_sum / visited_cells;
    const double dense_mix_share = static_cast<double>(dense_mix_cells) / visited_cells;
    const double low_mix_share = static_cast<double>(low_mix_cells) / visited_cells;

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
    out << "    <stop offset=\"0%\" stop-color=\"#071118\"/>\n";
    out << "    <stop offset=\"55%\" stop-color=\"#0f172a\"/>\n";
    out << "    <stop offset=\"100%\" stop-color=\"#120b1f\"/>\n";
    out << "  </linearGradient>\n";
    out << "</defs>\n";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"url(#bg)\"/>\n";
    out << "<text x=\"60\" y=\"58\" fill=\"#e0f2fe\" font-size=\"34\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Barnsley fern rule-mixing entropy</text>\n";
    out << "<text x=\"60\" y=\"86\" fill=\"#bae6fd\" font-size=\"18\" font-family=\"Helvetica, Arial, sans-serif\">Some fern cells belong almost entirely to one affine rule. Others stay genuinely mixed.</text>\n";
    out << "<text x=\"60\" y=\"112\" fill=\"#c4b5fd\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">Cell color = local rule entropy in bits. Dark teal means one rule dominates; gold means several rules keep sharing that patch.</text>\n";

    out << "<rect x=\"" << map_left << "\" y=\"" << map_top << "\" width=\"" << map_width
        << "\" height=\"" << map_height << "\" rx=\"24\" fill=\"#08111f\" fill-opacity=\"0.88\" stroke=\""
        << hex_color(frame_color) << "\" stroke-width=\"1.5\"/>\n";
    out << "<text x=\"84\" y=\"208\" fill=\"#e0f2fe\" font-size=\"22\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Local entropy field</text>\n";
    out << "<text x=\"84\" y=\"232\" fill=\"#c4b5fd\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">Low entropy = near-single-rule ownership. High entropy = mixed-rule overlap.</text>\n";

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
            const double entropy = entropy_bits(cell.transform_hits, cell.total_hits);
            const double normalized_entropy = entropy / max_entropy;
            const double density = std::sqrt(static_cast<double>(cell.total_hits) / std::max(1, max_hits));
            std::array<int, 3> gradient;
            if (normalized_entropy < 0.5) {
                gradient = mix_rgb(low_entropy_color, mid_entropy_color, normalized_entropy / 0.5);
            } else {
                gradient = mix_rgb(mid_entropy_color, high_entropy_color, (normalized_entropy - 0.5) / 0.5);
            }
            const std::array<int, 3> shaded = mix_rgb(empty_color, gradient, 0.30 + 0.70 * density);
            out << "<rect x=\"" << std::fixed << std::setprecision(2) << x << "\" y=\"" << y
                << "\" width=\"" << w << "\" height=\"" << h << "\" fill=\"" << hex_color(shaded) << "\"/>\n";
        }
    }

    const double legend_x = map_left + 34.0;
    const double legend_y = map_top + map_height - 72.0;
    const double legend_w = map_width - 68.0;
    for (int i = 0; i < 90; ++i) {
        const double frac = i / 89.0;
        std::array<int, 3> gradient;
        if (frac < 0.5) {
            gradient = mix_rgb(low_entropy_color, mid_entropy_color, frac / 0.5);
        } else {
            gradient = mix_rgb(mid_entropy_color, high_entropy_color, (frac - 0.5) / 0.5);
        }
        const double x = legend_x + frac * legend_w;
        out << "<rect x=\"" << x << "\" y=\"" << legend_y << "\" width=\"" << (legend_w / 89.0 + 1.0)
            << "\" height=\"18\" fill=\"" << hex_color(gradient) << "\"/>\n";
    }
    out << "<text x=\"" << legend_x << "\" y=\"" << (legend_y - 10.0)
        << "\" fill=\"#cbd5e1\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">0 bits</text>\n";
    out << "<text x=\"" << (legend_x + legend_w * 0.5) << "\" y=\"" << (legend_y - 10.0)
        << "\" fill=\"#cbd5e1\" font-size=\"13\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">1 bit</text>\n";
    out << "<text x=\"" << (legend_x + legend_w) << "\" y=\"" << (legend_y - 10.0)
        << "\" fill=\"#cbd5e1\" font-size=\"13\" text-anchor=\"end\" font-family=\"Helvetica, Arial, sans-serif\">2 bits</text>\n";
    out << "<text x=\"84\" y=\"" << (legend_y + 44.0)
        << "\" fill=\"#cbd5e1\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">visited cells "
        << visited_cells << " / " << (grid_w * grid_h) << " · mean entropy " << format_fixed(mean_entropy)
        << " bits · dense-mix cells " << format_percent(dense_mix_share) << "</text>\n";

    out << "<rect x=\"" << chart_left << "\" y=\"" << top_chart_top << "\" width=\"" << chart_width
        << "\" height=\"" << top_chart_height << "\" rx=\"22\" fill=\"#08111f\" fill-opacity=\"0.88\" stroke=\""
        << hex_color(frame_color) << "\" stroke-width=\"1.5\"/>\n";
    out << "<text x=\"972\" y=\"246\" fill=\"#e0f2fe\" font-size=\"22\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Entropy by height band</text>\n";
    out << "<text x=\"972\" y=\"270\" fill=\"#c4b5fd\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">Left bar = mean entropy. Gold marker = share of high-mix cells in that band.</text>\n";

    const double band_left = chart_left + 112.0;
    const double band_top = top_chart_top + 100.0;
    const double band_width = chart_width - 172.0;
    const double band_height = top_chart_height - 170.0;
    for (int tick = 0; tick <= 4; ++tick) {
        const double frac = tick / 4.0;
        const double x = band_left + frac * band_width;
        out << "<line x1=\"" << x << "\" y1=\"" << band_top << "\" x2=\"" << x << "\" y2=\""
            << (band_top + band_height) << "\" stroke=\"#22304f\" stroke-width=\"1\"/>\n";
        out << "<text x=\"" << x << "\" y=\"" << (band_top + band_height + 22.0)
            << "\" fill=\"#cbd5e1\" font-size=\"11\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">"
            << format_fixed(frac * max_entropy, 1) << "</text>\n";
    }
    for (int band = 0; band < height_bands; ++band) {
        const double y = band_top + band_height - (band + 1.0) / height_bands * band_height;
        const double h = band_height / height_bands - 4.0;
        out << "<text x=\"" << (band_left - 12.0) << "\" y=\"" << (y + h * 0.75)
            << "\" fill=\"#cbd5e1\" font-size=\"11\" text-anchor=\"end\" font-family=\"Helvetica, Arial, sans-serif\">"
            << format_fixed((band + 0.5) * (max_y - min_y) / height_bands, 1) << "</text>\n";
        if (band_visited[band] <= 0) continue;
        const double mean_band_entropy = band_entropy_sum[band] / band_visited[band];
        const double mean_band_mix = band_mixed_share_sum[band] / band_visited[band];
        const double high_mix_share = static_cast<double>(band_high_mix_cells[band]) / band_visited[band];
        const double bar_w = band_width * (mean_band_entropy / max_entropy);
        const auto bar_color = mean_band_mix < 0.5
            ? mix_color(low_entropy_color, mid_entropy_color, mean_band_mix / 0.5)
            : mix_color(mid_entropy_color, high_entropy_color, (mean_band_mix - 0.5) / 0.5);
        out << "<rect x=\"" << band_left << "\" y=\"" << y << "\" width=\"" << bar_w << "\" height=\"" << h
            << "\" rx=\"5\" fill=\"" << bar_color << "\"/>\n";
        const double marker_x = band_left + high_mix_share * band_width;
        out << "<circle cx=\"" << marker_x << "\" cy=\"" << (y + h * 0.5)
            << "\" r=\"5\" fill=\"#fde68a\" stroke=\"#f59e0b\" stroke-width=\"1.2\"/>\n";
    }
    out << "<text x=\"" << (band_left + band_width * 0.5) << "\" y=\"" << (band_top + band_height + 46.0)
        << "\" fill=\"#cbd5e1\" font-size=\"13\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">bar length = mean entropy in bits · dot = share of cells with entropy ≥ 1.4 bits</text>\n";

    out << "<rect x=\"" << chart_left << "\" y=\"" << bottom_chart_top << "\" width=\"" << chart_width
        << "\" height=\"" << bottom_chart_height << "\" rx=\"22\" fill=\"#08111f\" fill-opacity=\"0.88\" stroke=\""
        << hex_color(frame_color) << "\" stroke-width=\"1.5\"/>\n";
    out << "<text x=\"972\" y=\"900\" fill=\"#e0f2fe\" font-size=\"22\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Global entropy spread</text>\n";
    out << "<text x=\"972\" y=\"924\" fill=\"#c4b5fd\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">This is the full distribution across visited cells, not just one average.</text>\n";

    const double hist_left = chart_left + 62.0;
    const double hist_top = bottom_chart_top + 112.0;
    const double hist_width = chart_width - 112.0;
    const double hist_height = 176.0;
    const int hist_max = *std::max_element(entropy_histogram.begin(), entropy_histogram.end());
    for (int tick = 0; tick <= 4; ++tick) {
        const double frac = tick / 4.0;
        const double y = hist_top + hist_height - frac * hist_height;
        out << "<line x1=\"" << hist_left << "\" y1=\"" << y << "\" x2=\"" << (hist_left + hist_width)
            << "\" y2=\"" << y << "\" stroke=\"#22304f\" stroke-width=\"1\"/>\n";
        out << "<text x=\"" << (hist_left - 10.0) << "\" y=\"" << (y + 4.0)
            << "\" fill=\"#cbd5e1\" font-size=\"11\" text-anchor=\"end\" font-family=\"Helvetica, Arial, sans-serif\">"
            << static_cast<int>(std::round(frac * hist_max)) << "</text>\n";
    }
    for (int bin = 0; bin < histogram_bins; ++bin) {
        const double frac0 = static_cast<double>(bin) / histogram_bins;
        const double frac1 = static_cast<double>(bin + 1) / histogram_bins;
        const double x = hist_left + frac0 * hist_width + 4.0;
        const double w = (frac1 - frac0) * hist_width - 8.0;
        const double bar_h = hist_max > 0 ? hist_height * static_cast<double>(entropy_histogram[bin]) / hist_max : 0.0;
        const double normalized = (frac0 + frac1) * 0.5;
        const auto bar_color = normalized < 0.5
            ? mix_color(low_entropy_color, mid_entropy_color, normalized / 0.5)
            : mix_color(mid_entropy_color, high_entropy_color, (normalized - 0.5) / 0.5);
        out << "<rect x=\"" << x << "\" y=\"" << (hist_top + hist_height - bar_h) << "\" width=\"" << w
            << "\" height=\"" << bar_h << "\" rx=\"5\" fill=\"" << bar_color << "\"/>\n";
        out << "<text x=\"" << (x + w * 0.5) << "\" y=\"" << (hist_top + hist_height + 22.0)
            << "\" fill=\"#cbd5e1\" font-size=\"10\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">"
            << format_fixed(frac1 * max_entropy, 1) << "</text>\n";
    }
    out << "<text x=\"" << (hist_left + hist_width * 0.5) << "\" y=\"" << (hist_top + hist_height + 46.0)
        << "\" fill=\"#cbd5e1\" font-size=\"13\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">upper edge of entropy bin in bits</text>\n";

    const double bullet_y = bottom_chart_top + 360.0;
    out << "<text x=\"972\" y=\"" << bullet_y << "\" fill=\"#e0f2fe\" font-size=\"15\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">What this card says</text>\n";
    out << "<text x=\"972\" y=\"" << (bullet_y + 28.0) << "\" fill=\"#cbd5e1\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">• mean entropy is "
        << format_fixed(mean_entropy) << " bits across visited cells</text>\n";
    out << "<text x=\"972\" y=\"" << (bullet_y + 52.0) << "\" fill=\"#cbd5e1\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">• only "
        << format_percent(low_mix_share) << " of cells are near-single-rule patches (≤ 0.35 bits)</text>\n";
    out << "<text x=\"972\" y=\"" << (bullet_y + 76.0) << "\" fill=\"#cbd5e1\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">• "
        << format_percent(dense_mix_share) << " of cells stay strongly mixed (≥ 1.4 bits)</text>\n";
    out << "<text x=\"972\" y=\"" << (bullet_y + 110.0) << "\" fill=\"#93c5fd\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">Bright mixed zones cluster around the fern body and leaflet boundaries rather than the emptiest background or the stem line.</text>\n";

    out << "</svg>\n";
    out.close();

    std::ofstream csv(output_csv);
    csv << "gx,gy,x_center,y_center,total_hits,entropy_bits,normalized_entropy,dominant_share,share_stem,share_bulk,share_left,share_right\n";
    for (int gy = 0; gy < grid_h; ++gy) {
        for (int gx = 0; gx < grid_w; ++gx) {
            const Cell& cell = grid[gy * grid_w + gx];
            if (cell.total_hits <= 0) continue;
            const auto max_it = std::max_element(cell.transform_hits.begin(), cell.transform_hits.end());
            const double dominant_share = static_cast<double>(*max_it) / cell.total_hits;
            const double entropy = entropy_bits(cell.transform_hits, cell.total_hits);
            const double x_center = min_x + (gx + 0.5) / grid_w * (max_x - min_x);
            const double y_center = min_y + (gy + 0.5) / grid_h * (max_y - min_y);
            csv << gx << ',' << gy << ',' << std::fixed << std::setprecision(6)
                << x_center << ',' << y_center << ',' << cell.total_hits << ',' << entropy << ','
                << (entropy / max_entropy) << ',' << dominant_share;
            for (int t = 0; t < 4; ++t) {
                csv << ',' << (static_cast<double>(cell.transform_hits[t]) / cell.total_hits);
            }
            csv << '\n';
        }
    }

    std::cout << "wrote " << output_svg << " and " << output_csv << "\n";
    return 0;
}
