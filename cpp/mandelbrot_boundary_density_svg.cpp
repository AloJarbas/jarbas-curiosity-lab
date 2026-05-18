#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

struct View {
    double cx;
    double cy;
    double span;
    const char* title;
    const char* caption;
};

struct HistogramStats {
    double escape_fraction;
    double mean_smooth_iter;
    double percentile90;
    double max_smooth_iter;
    std::vector<int> bins;
};

static std::string color_for(double t) {
    t = std::clamp(t, 0.0, 1.0);
    const double r0 = 8.0, g0 = 18.0, b0 = 34.0;
    const double r1 = 56.0, g1 = 189.0, b1 = 248.0;
    const double r2 = 253.0, g2 = 230.0, b2 = 138.0;
    const double r3 = 255.0, g3 = 120.0, b3 = 71.0;

    auto lerp = [](double a, double b, double u) {
        return a + (b - a) * u;
    };

    double r = 0.0, g = 0.0, b = 0.0;
    if (t < 0.45) {
        const double u = t / 0.45;
        r = lerp(r0, r1, u);
        g = lerp(g0, g1, u);
        b = lerp(b0, b1, u);
    } else if (t < 0.78) {
        const double u = (t - 0.45) / 0.33;
        r = lerp(r1, r2, u);
        g = lerp(g1, g2, u);
        b = lerp(b1, b2, u);
    } else {
        const double u = (t - 0.78) / 0.22;
        r = lerp(r2, r3, u);
        g = lerp(g2, g3, u);
        b = lerp(b2, b3, u);
    }

    std::ostringstream out;
    out << '#'
        << std::hex << std::setfill('0')
        << std::setw(2) << static_cast<int>(std::round(r))
        << std::setw(2) << static_cast<int>(std::round(g))
        << std::setw(2) << static_cast<int>(std::round(b));
    return out.str();
}

static double smooth_escape(double cr, double ci, int max_iter) {
    double zr = 0.0;
    double zi = 0.0;
    int iter = 0;
    while (zr * zr + zi * zi <= 4.0 && iter < max_iter) {
        const double next_zr = zr * zr - zi * zi + cr;
        zi = 2.0 * zr * zi + ci;
        zr = next_zr;
        ++iter;
    }
    if (iter == max_iter) return -1.0;
    const double mag2 = zr * zr + zi * zi;
    return iter + 1.0 - std::log(std::log(std::sqrt(mag2))) / std::log(2.0);
}

static std::string esc(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += ch; break;
        }
    }
    return out;
}

static void write_wrapped_text(std::ofstream& out, double x, double y, const std::string& text, double max_chars, const char* fill, int font_size, double line_height) {
    std::istringstream words(text);
    std::string word;
    std::vector<std::string> lines;
    std::string current;
    while (words >> word) {
        std::string trial = current.empty() ? word : current + " " + word;
        if (trial.size() > static_cast<std::size_t>(max_chars) && !current.empty()) {
            lines.push_back(current);
            current = word;
        } else {
            current = trial;
        }
    }
    if (!current.empty()) lines.push_back(current);
    out << "<text x=\"" << x << "\" y=\"" << y << "\" fill=\"" << fill << "\" font-size=\"" << font_size
        << "\" font-family=\"Helvetica, Arial, sans-serif\">\n";
    for (std::size_t i = 0; i < lines.size(); ++i) {
        out << "  <tspan x=\"" << x << "\" dy=\"" << (i == 0 ? 0.0 : line_height) << "\">" << esc(lines[i]) << "</tspan>\n";
    }
    out << "</text>\n";
}

static HistogramStats render_view(
    std::ofstream& out,
    const View& view,
    double panel_x,
    double panel_y,
    double panel_w,
    double panel_h,
    int width,
    int height,
    int max_iter,
    int palette_steps,
    int bins
) {
    const double x_min = view.cx - view.span / 2.0;
    const double x_max = view.cx + view.span / 2.0;
    const double y_span = view.span * panel_h / panel_w;
    const double y_min = view.cy - y_span / 2.0;
    const double y_max = view.cy + y_span / 2.0;
    const double cell_w = panel_w / width;
    const double cell_h = panel_h / height;

    std::vector<std::string> palette;
    palette.reserve(palette_steps + 1);
    for (int i = 0; i <= palette_steps; ++i) {
        palette.push_back(color_for(i / static_cast<double>(palette_steps)));
    }

    std::vector<double> escaped_values;
    escaped_values.reserve(width * height);
    std::vector<int> histogram(bins, 0);

    for (int py = 0; py < height; ++py) {
        std::vector<int> row(width, -1);
        for (int px = 0; px < width; ++px) {
            const double cr = x_min + (x_max - x_min) * px / (width - 1.0);
            const double ci = y_max - (y_max - y_min) * py / (height - 1.0);
            const double nu = smooth_escape(cr, ci, max_iter);
            if (nu < 0.0) {
                row[px] = -1;
                continue;
            }
            escaped_values.push_back(nu);
            const double wrapped = std::fmod(nu * 0.085, 1.0);
            row[px] = std::clamp(static_cast<int>(wrapped * palette_steps), 0, palette_steps);
            const int hist_idx = std::clamp(static_cast<int>((nu / max_iter) * bins), 0, bins - 1);
            histogram[hist_idx] += 1;
        }

        int run_start = 0;
        int current = row[0];
        for (int px = 1; px <= width; ++px) {
            const int next = px < width ? row[px] : -999;
            if (next != current) {
                const double x = panel_x + run_start * cell_w;
                const double y = panel_y + py * cell_h;
                const double w = (px - run_start) * cell_w;
                const char* fill = current < 0 ? "#030712" : palette[current].c_str();
                out << "<rect x=\"" << std::fixed << std::setprecision(2) << x
                    << "\" y=\"" << y
                    << "\" width=\"" << w + 0.08
                    << "\" height=\"" << cell_h + 0.08
                    << "\" fill=\"" << fill << "\"/>\n";
                run_start = px;
                current = next;
            }
        }
    }

    std::sort(escaped_values.begin(), escaped_values.end());
    const double escape_fraction = escaped_values.size() / static_cast<double>(width * height);
    const double sum_escape = std::accumulate(escaped_values.begin(), escaped_values.end(), 0.0);
    const double mean_escape = escaped_values.empty() ? 0.0 : sum_escape / escaped_values.size();
    const double max_smooth = escaped_values.empty() ? 0.0 : escaped_values.back();
    const std::size_t p90_index = escaped_values.empty() ? 0 : std::min(escaped_values.size() - 1, static_cast<std::size_t>(0.90 * escaped_values.size()));
    const double p90 = escaped_values.empty() ? 0.0 : escaped_values[p90_index];

    return {escape_fraction, mean_escape, p90, max_smooth, histogram};
}

static void render_histogram(
    std::ofstream& out,
    const HistogramStats& stats,
    double left,
    double top,
    double width,
    double height,
    int max_iter
) {
    out << "<rect x=\"" << left << "\" y=\"" << top << "\" width=\"" << width << "\" height=\"" << height
        << "\" rx=\"16\" fill=\"#0b1220\" stroke=\"#334155\" stroke-width=\"1.2\"/>\n";

    const double chart_left = left + 18.0;
    const double chart_top = top + 24.0;
    const double chart_bottom = top + height - 34.0;
    const double chart_right = left + width - 18.0;
    const int peak = std::max(1, *std::max_element(stats.bins.begin(), stats.bins.end()));
    const double bar_w = (chart_right - chart_left) / stats.bins.size();

    for (int tick = 0; tick <= 4; ++tick) {
        const double y = chart_bottom - (chart_bottom - chart_top) * tick / 4.0;
        out << "<line x1=\"" << chart_left << "\" y1=\"" << y << "\" x2=\"" << chart_right << "\" y2=\"" << y
            << "\" stroke=\"#1e293b\" stroke-width=\"1\"/>\n";
    }

    for (std::size_t i = 0; i < stats.bins.size(); ++i) {
        const double x = chart_left + i * bar_w;
        const double normalized = stats.bins[i] / static_cast<double>(peak);
        const double h = normalized * (chart_bottom - chart_top);
        const double y = chart_bottom - h;
        const double t = i / static_cast<double>(std::max<std::size_t>(1, stats.bins.size() - 1));
        out << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << std::max(1.0, bar_w - 1.0)
            << "\" height=\"" << h << "\" fill=\"" << color_for(t) << "\" opacity=\"0.92\"/>\n";
    }

    out << "<line x1=\"" << chart_left << "\" y1=\"" << chart_bottom << "\" x2=\"" << chart_right << "\" y2=\"" << chart_bottom
        << "\" stroke=\"#475569\" stroke-width=\"1.4\"/>\n";
    out << "<text x=\"" << chart_left << "\" y=\"" << (top + 16.0) << "\" fill=\"#cbd5e1\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">escaped smooth-iteration density</text>\n";
    out << "<text x=\"" << chart_left << "\" y=\"" << (top + height - 10.0) << "\" fill=\"#94a3b8\" font-size=\"11\" font-family=\"Helvetica, Arial, sans-serif\">fast escape</text>\n";
    out << "<text x=\"" << chart_right << "\" y=\"" << (top + height - 10.0) << "\" fill=\"#94a3b8\" font-size=\"11\" text-anchor=\"end\" font-family=\"Helvetica, Arial, sans-serif\">slow escape / boundary drag</text>\n";

    std::ostringstream summary;
    summary << std::fixed << std::setprecision(1)
            << "p90 = " << stats.percentile90
            << "  ·  mean = " << stats.mean_smooth_iter
            << "  ·  max = " << stats.max_smooth_iter;
    out << "<text x=\"" << chart_left << "\" y=\"" << (top + height + 18.0) << "\" fill=\"#93c5fd\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">"
        << summary.str() << "</text>\n";
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        std::cout << "usage: " << argv[0] << " [output.svg] [output.csv]\n";
        std::cout << "render a Mandelbrot boundary-density card as SVG and CSV\n";
        return 0;
    }

    const std::string svg_output = argc > 1 ? argv[1] : "art/mandelbrot-boundary-density.svg";
    const std::string csv_output = argc > 2 ? argv[2] : "art/mandelbrot-boundary-density.csv";

    const int width = 1620;
    const int height = 980;
    const int panel_px_w = 320;
    const int panel_px_h = 220;
    const int max_iter = 280;
    const int palette_steps = 48;
    const int hist_bins = 36;

    const std::vector<View> views{
        {-0.5, 0.0, 3.2, "whole set", "Most escaped points leave quickly. The boundary is where the slow tail starts building up."},
        {-0.745, 0.112, 0.28, "seahorse valley", "A dense boundary zone where the histogram shifts right because more pixels linger before escaping."},
        {-0.743643887037151, 0.13182590420533, 0.0028, "mini-brot neighborhood", "Deep zoom does not just change the picture. It also fattens the slow-escape tail."},
    };

    std::ofstream svg(svg_output);
    std::ofstream csv(csv_output);
    if (!svg || !csv) {
        std::cerr << "failed to open output files\n";
        return 1;
    }

    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << width << ' ' << height
        << "\" width=\"" << width << "\" height=\"" << height << "\">\n";
    svg << "<defs>\n";
    svg << "  <linearGradient id=\"bg\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"1\">\n";
    svg << "    <stop offset=\"0%\" stop-color=\"#020617\"/>\n";
    svg << "    <stop offset=\"55%\" stop-color=\"#081225\"/>\n";
    svg << "    <stop offset=\"100%\" stop-color=\"#160b22\"/>\n";
    svg << "  </linearGradient>\n";
    svg << "</defs>\n";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"url(#bg)\"/>\n";
    svg << "<text x=\"60\" y=\"60\" fill=\"#eef2ff\" font-size=\"36\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Mandelbrot boundary-density card</text>\n";
    write_wrapped_text(svg, 60.0, 92.0, "Same quadratic map, same escape-time rule, three scales. The extra question here is where the escaping mass lives: does it leave fast, or does the boundary keep it hanging around?", 98.0, "#93c5fd", 18, 22.0);

    csv << "view,bucket_start,bucket_end,count,escape_fraction,mean_smooth_iter,p90_smooth_iter,max_smooth_iter\n";

    const double panel_w = 430.0;
    const double panel_h = 268.0;
    const double hist_h = 156.0;
    const double top = 184.0;
    const double left = 60.0;
    const double gap = 45.0;

    for (std::size_t i = 0; i < views.size(); ++i) {
        const double panel_x = left + i * (panel_w + gap);
        const double panel_y = top;
        svg << "<rect x=\"" << panel_x << "\" y=\"" << panel_y << "\" width=\"" << panel_w
            << "\" height=\"" << panel_h << "\" rx=\"18\" fill=\"#020617\" stroke=\"#334155\" stroke-width=\"1.4\"/>\n";

        const HistogramStats stats = render_view(svg, views[i], panel_x, panel_y, panel_w, panel_h, panel_px_w, panel_px_h, max_iter, palette_steps, hist_bins);

        svg << "<text x=\"" << (panel_x + 18.0) << "\" y=\"" << (panel_y + panel_h + 32.0)
            << "\" fill=\"#e2e8f0\" font-size=\"24\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">"
            << views[i].title << "</text>\n";
        write_wrapped_text(svg, panel_x + 18.0, panel_y + panel_h + 58.0, views[i].caption, 46.0, "#cbd5e1", 14, 18.0);

        std::ostringstream stats_line;
        stats_line << std::fixed << std::setprecision(1)
                   << "escaped: " << (100.0 * stats.escape_fraction) << "%";
        svg << "<text x=\"" << (panel_x + 18.0) << "\" y=\"" << (panel_y + panel_h + 112.0)
            << "\" fill=\"#93c5fd\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">"
            << stats_line.str() << "</text>\n";

        render_histogram(svg, stats, panel_x, panel_y + panel_h + 130.0, panel_w, hist_h, max_iter);

        for (int bin = 0; bin < hist_bins; ++bin) {
            const double bucket_start = max_iter * bin / static_cast<double>(hist_bins);
            const double bucket_end = max_iter * (bin + 1) / static_cast<double>(hist_bins);
            csv << '"' << views[i].title << '"' << ','
                << std::fixed << std::setprecision(3) << bucket_start << ','
                << bucket_end << ','
                << stats.bins[bin] << ','
                << stats.escape_fraction << ','
                << stats.mean_smooth_iter << ','
                << stats.percentile90 << ','
                << stats.max_smooth_iter << '\n';
        }
    }

    write_wrapped_text(svg, 60.0, 930.0, "Rendered directly in C++ with smoothed escape-time coloring, histogram extraction, and row-run SVG compression. The CSV sidecar keeps the bucket counts if you want to reuse the same three views numerically.", 122.0, "#cbd5e1", 14, 18.0);
    svg << "</svg>\n";

    if (!svg || !csv) {
        std::cerr << "failed while writing output files\n";
        return 1;
    }

    std::cout << svg_output << '\n' << csv_output << '\n';
    return 0;
}
