#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

struct FrameSpec {
    double cx;
    double cy;
    double span;
};

struct FrameStats {
    double span;
    double escape_fraction;
    double mean_smooth_iter;
    double percentile90;
    double slow_tail_share;
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

static std::string scientific_span(double span) {
    std::ostringstream out;
    out << std::scientific << std::setprecision(2) << span;
    return out.str();
}

static FrameStats render_frame(
    std::ofstream& out,
    const FrameSpec& frame,
    double panel_x,
    double panel_y,
    double panel_w,
    double panel_h,
    int width,
    int height,
    int max_iter,
    int palette_steps
) {
    const double x_min = frame.cx - frame.span / 2.0;
    const double x_max = frame.cx + frame.span / 2.0;
    const double y_span = frame.span * panel_h / panel_w;
    const double y_min = frame.cy - y_span / 2.0;
    const double y_max = frame.cy + y_span / 2.0;
    const double cell_w = panel_w / width;
    const double cell_h = panel_h / height;

    std::vector<std::string> palette;
    palette.reserve(palette_steps + 1);
    for (int i = 0; i <= palette_steps; ++i) {
        palette.push_back(color_for(i / static_cast<double>(palette_steps)));
    }

    std::vector<double> escaped_values;
    escaped_values.reserve(width * height);
    int slow_tail_count = 0;

    const double slow_threshold = max_iter * 0.65;

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
            if (nu >= slow_threshold) ++slow_tail_count;
            const double wrapped = std::fmod(nu * 0.085, 1.0);
            row[px] = std::clamp(static_cast<int>(wrapped * palette_steps), 0, palette_steps);
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
    const double mean_escape = escaped_values.empty()
        ? 0.0
        : std::accumulate(escaped_values.begin(), escaped_values.end(), 0.0) / escaped_values.size();
    const std::size_t p90_index = escaped_values.empty()
        ? 0
        : std::min(escaped_values.size() - 1, static_cast<std::size_t>(0.90 * escaped_values.size()));
    const double p90 = escaped_values.empty() ? 0.0 : escaped_values[p90_index];
    const double slow_tail_share = escaped_values.empty() ? 0.0 : slow_tail_count / static_cast<double>(escaped_values.size());

    return {frame.span, escape_fraction, mean_escape, p90, slow_tail_share};
}

static void write_chart(
    std::ofstream& out,
    const std::vector<FrameStats>& stats,
    double left,
    double top,
    double width,
    double height,
    const std::string& title,
    const std::string& subtitle,
    double (*value_fn)(const FrameStats&),
    double y_max,
    const std::string& y_label,
    const std::string& line_color
) {
    out << "<rect x=\"" << left << "\" y=\"" << top << "\" width=\"" << width << "\" height=\"" << height
        << "\" rx=\"18\" fill=\"#0b1220\" stroke=\"#334155\" stroke-width=\"1.2\"/>\n";

    const double plot_left = left + 62.0;
    const double plot_top = top + 48.0;
    const double plot_right = left + width - 24.0;
    const double plot_bottom = top + height - 52.0;

    auto map_x = [&](double span) {
        const double log_lo = std::log10(stats.front().span);
        const double log_hi = std::log10(stats.back().span);
        const double t = (std::log10(span) - log_lo) / (log_hi - log_lo);
        return plot_left + t * (plot_right - plot_left);
    };
    auto map_y = [&](double value) {
        return plot_bottom - (value / y_max) * (plot_bottom - plot_top);
    };

    out << "<text x=\"" << (left + 18.0) << "\" y=\"" << (top + 25.0)
        << "\" fill=\"#e2e8f0\" font-size=\"16\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">"
        << esc(title) << "</text>\n";
    out << "<text x=\"" << (left + 18.0) << "\" y=\"" << (top + 42.0)
        << "\" fill=\"#94a3b8\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">"
        << esc(subtitle) << "</text>\n";

    for (int tick = 0; tick <= 5; ++tick) {
        const double y_value = y_max * tick / 5.0;
        const double y = map_y(y_value);
        out << "<line x1=\"" << plot_left << "\" y1=\"" << y << "\" x2=\"" << plot_right << "\" y2=\"" << y
            << "\" stroke=\"#1e293b\" stroke-width=\"1\"/>\n";
        out << "<text x=\"" << (plot_left - 8.0) << "\" y=\"" << (y + 4.0)
            << "\" fill=\"#94a3b8\" font-size=\"11\" text-anchor=\"end\" font-family=\"Helvetica, Arial, sans-serif\">"
            << std::fixed << std::setprecision(y_max <= 1.0 ? 2 : 1) << y_value << "</text>\n";
    }

    for (const auto& frame : stats) {
        const double x = map_x(frame.span);
        out << "<line x1=\"" << x << "\" y1=\"" << plot_top << "\" x2=\"" << x << "\" y2=\"" << plot_bottom
            << "\" stroke=\"#172131\" stroke-width=\"1\" stroke-dasharray=\"4 6\"/>\n";
        out << "<text x=\"" << x << "\" y=\"" << (plot_bottom + 18.0)
            << "\" fill=\"#94a3b8\" font-size=\"10\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">10"
            << "<tspan dy=\"-5\" font-size=\"8\">" << std::llround(std::log10(frame.span)) << "</tspan></text>\n";
    }

    std::ostringstream polyline;
    for (std::size_t i = 0; i < stats.size(); ++i) {
        if (i) polyline << ' ';
        polyline << std::fixed << std::setprecision(2) << map_x(stats[i].span) << ',' << map_y(value_fn(stats[i]));
    }
    out << "<polyline fill=\"none\" stroke=\"" << line_color << "\" stroke-width=\"3.0\" points=\""
        << polyline.str() << "\"/>\n";

    for (const auto& frame : stats) {
        out << "<circle cx=\"" << map_x(frame.span) << "\" cy=\"" << map_y(value_fn(frame))
            << "\" r=\"4.2\" fill=\"" << line_color << "\" stroke=\"#020617\" stroke-width=\"1.2\"/>\n";
    }

    out << "<line x1=\"" << plot_left << "\" y1=\"" << plot_bottom << "\" x2=\"" << plot_right << "\" y2=\"" << plot_bottom
        << "\" stroke=\"#475569\" stroke-width=\"1.4\"/>\n";
    out << "<text x=\"" << ((plot_left + plot_right) / 2.0) << "\" y=\"" << (top + height - 18.0)
        << "\" fill=\"#cbd5e1\" font-size=\"12\" text-anchor=\"middle\" font-family=\"Helvetica, Arial, sans-serif\">"
        << "zoom level on a fixed Seahorse Valley center" << "</text>\n";
    out << "<text x=\"" << (left + 18.0) << "\" y=\"" << (plot_top - 10.0)
        << "\" fill=\"#93c5fd\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">"
        << esc(y_label) << "</text>\n";
}

static double mean_value(const FrameStats& stats) { return stats.mean_smooth_iter; }
static double slow_tail_value(const FrameStats& stats) { return stats.slow_tail_share; }

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        std::cout << "usage: " << argv[0] << " [output.svg] [output.csv]\n";
        std::cout << "render a Mandelbrot zoom-sequence contact sheet with frame metrics\n";
        return 0;
    }

    const std::string svg_output = argc > 1 ? argv[1] : "art/mandelbrot-zoom-sequence.svg";
    const std::string csv_output = argc > 2 ? argv[2] : "art/mandelbrot-zoom-sequence.csv";

    const std::vector<FrameSpec> frames{
        {-0.743643887037151, 0.13182590420533, 1.60},
        {-0.743643887037151, 0.13182590420533, 0.64},
        {-0.743643887037151, 0.13182590420533, 0.256},
        {-0.743643887037151, 0.13182590420533, 0.1024},
        {-0.743643887037151, 0.13182590420533, 0.04096},
        {-0.743643887037151, 0.13182590420533, 0.016384},
    };

    const int width = 1680;
    const int height = 1320;
    const int frame_px_w = 280;
    const int frame_px_h = 180;
    const int max_iter = 420;
    const int palette_steps = 240;

    std::ofstream out(svg_output);
    if (!out) {
        std::cerr << "failed to open " << svg_output << " for writing\n";
        return 1;
    }

    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << width << ' ' << height
        << "\" width=\"" << width << "\" height=\"" << height << "\">\n";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"#020617\"/>\n";
    out << "<text x=\"58\" y=\"58\" fill=\"#e2e8f0\" font-size=\"34\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Mandelbrot zoom sequence</text>\n";
    out << "<text x=\"58\" y=\"89\" fill=\"#93c5fd\" font-size=\"18\" font-family=\"Helvetica, Arial, sans-serif\">One fixed Seahorse Valley center, six spans, and a tighter question than a single glamour zoom: how does the slow-escape mix change as the boundary folds inward?</text>\n";
    out << "<text x=\"58\" y=\"116\" fill=\"#94a3b8\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">Each frame uses the same smooth-escape palette. The charts below track mean smooth escape and the share of escaped pixels still dragging above 65% of the iteration budget.</text>\n";

    const double left = 58.0;
    const double top = 156.0;
    const double gap_x = 24.0;
    const double gap_y = 26.0;
    const double panel_w = 505.0;
    const double panel_h = 270.0;

    std::vector<FrameStats> stats;
    stats.reserve(frames.size());

    for (std::size_t idx = 0; idx < frames.size(); ++idx) {
        const int row = static_cast<int>(idx / 3);
        const int col = static_cast<int>(idx % 3);
        const double panel_x = left + col * (panel_w + gap_x);
        const double panel_y = top + row * (panel_h + gap_y);
        const double inner_x = panel_x + 18.0;
        const double inner_y = panel_y + 44.0;
        const double inner_w = panel_w - 36.0;
        const double inner_h = panel_h - 86.0;

        out << "<rect x=\"" << panel_x << "\" y=\"" << panel_y << "\" width=\"" << panel_w << "\" height=\"" << panel_h
            << "\" rx=\"18\" fill=\"#0b1220\" stroke=\"#334155\" stroke-width=\"1.2\"/>\n";

        FrameStats frame_stats = render_frame(out, frames[idx], inner_x, inner_y, inner_w, inner_h, frame_px_w, frame_px_h, max_iter, palette_steps);
        stats.push_back(frame_stats);

        out << "<text x=\"" << (panel_x + 18.0) << "\" y=\"" << (panel_y + 26.0)
            << "\" fill=\"#e2e8f0\" font-size=\"16\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">frame "
            << (idx + 1) << "</text>\n";
        out << "<text x=\"" << (panel_x + 84.0) << "\" y=\"" << (panel_y + 26.0)
            << "\" fill=\"#93c5fd\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">span = "
            << scientific_span(frames[idx].span) << "</text>\n";

        std::ostringstream note;
        note << std::fixed << std::setprecision(2)
             << "escape " << (frame_stats.escape_fraction * 100.0)
             << "%  ·  mean " << frame_stats.mean_smooth_iter
             << "  ·  slow tail " << (frame_stats.slow_tail_share * 100.0) << "%";
        out << "<text x=\"" << (panel_x + 18.0) << "\" y=\"" << (panel_y + panel_h - 20.0)
            << "\" fill=\"#94a3b8\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">"
            << note.str() << "</text>\n";
    }

    const double chart_y = top + 2.0 * (panel_h + gap_y) + 16.0;
    write_chart(
        out,
        stats,
        58.0,
        chart_y,
        760.0,
        360.0,
        "Mean smooth escape",
        "Average dwell among escaped pixels. This is the broad texture meter.",
        mean_value,
        std::max(180.0, std::max_element(stats.begin(), stats.end(), [](const FrameStats& a, const FrameStats& b) {
            return a.mean_smooth_iter < b.mean_smooth_iter;
        })->mean_smooth_iter * 1.15),
        "higher means more pixels stay near the boundary longer",
        "#38bdf8"
    );
    write_chart(
        out,
        stats,
        858.0,
        chart_y,
        760.0,
        360.0,
        "High-dwell share",
        "Escaped pixels still above 65% of the iteration budget. This catches the slow tail directly.",
        slow_tail_value,
        std::max(0.20, std::max_element(stats.begin(), stats.end(), [](const FrameStats& a, const FrameStats& b) {
            return a.slow_tail_share < b.slow_tail_share;
        })->slow_tail_share * 1.20),
        "fraction of escaped pixels in the slow tail",
        "#f59e0b"
    );

    out << "<text x=\"58\" y=\"1270\" fill=\"#cbd5e1\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">The point is not that zooming always makes the same metric rise. The point is that the slow-escape mix changes shape with scale, so a good zoom sequence is more than six prettier posters.</text>\n";
    out << "</svg>\n";
    out.close();

    std::ofstream csv(csv_output);
    if (!csv) {
        std::cerr << "failed to open " << csv_output << " for writing\n";
        return 1;
    }
    csv << "frame,span,escape_fraction,mean_smooth_iter,percentile90,slow_tail_share\n";
    for (std::size_t i = 0; i < stats.size(); ++i) {
        csv << (i + 1) << ','
            << std::setprecision(12) << stats[i].span << ','
            << stats[i].escape_fraction << ','
            << stats[i].mean_smooth_iter << ','
            << stats[i].percentile90 << ','
            << stats[i].slow_tail_share << '\n';
    }

    std::cout << "wrote " << svg_output << " and " << csv_output << "\n";
    return 0;
}
