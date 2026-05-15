#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
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

struct Stats {
    double escape_fraction;
    double mean_escape_iter;
    double max_smooth_iter;
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
    const double nu = iter + 1.0 - std::log(std::log(std::sqrt(mag2))) / std::log(2.0);
    return nu;
}

static Stats render_panel(
    std::ofstream& out,
    const View& view,
    double panel_x,
    double panel_y,
    double panel_w,
    double panel_h,
    int width,
    int height,
    int max_iter,
    int palette_steps
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

    int escaped = 0;
    double sum_escape = 0.0;
    double max_smooth = 0.0;

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
            ++escaped;
            sum_escape += nu;
            max_smooth = std::max(max_smooth, nu);
            const double wrapped = std::fmod(nu * 0.085, 1.0);
            const int idx = std::clamp(static_cast<int>(wrapped * palette_steps), 0, palette_steps);
            row[px] = idx;
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

    const double escape_fraction = escaped / static_cast<double>(width * height);
    const double mean_escape_iter = escaped > 0 ? sum_escape / escaped : 0.0;
    return {escape_fraction, mean_escape_iter, max_smooth};
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        std::cout << "usage: " << argv[0] << " [output.svg]\n";
        std::cout << "render a Mandelbrot zoom triptych as SVG\n";
        return 0;
    }

    const std::string output = argc > 1 ? argv[1] : "art/mandelbrot-zoom-triptych.svg";
    const int width = 1500;
    const int height = 830;
    const int panel_px_w = 320;
    const int panel_px_h = 228;
    const int max_iter = 220;
    const int palette_steps = 40;

    const std::vector<View> views{
        {-0.5, 0.0, 3.2, "whole set", "The blunt first read: cardioid, bulbs, and the dark interior where the orbit does not escape on this budget."},
        {-0.745, 0.112, 0.28, "seahorse valley", "A boundary region where tiny coordinate changes flip the orbit between slow escape, fast escape, and apparent attachment."},
        {-0.743643887037151, 0.13182590420533, 0.0028, "mini-brot neighborhood", "A deeper zoom where self-similar copies and tendrils start reading as structure instead of noise."},
    };

    std::ofstream out(output);
    if (!out) {
        std::cerr << "failed to open " << output << '\n';
        return 1;
    }

    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << width << ' ' << height
        << "\" width=\"" << width << "\" height=\"" << height << "\">\n";
    out << "<defs>\n";
    out << "  <linearGradient id=\"bg\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"1\">\n";
    out << "    <stop offset=\"0%\" stop-color=\"#020617\"/>\n";
    out << "    <stop offset=\"55%\" stop-color=\"#081225\"/>\n";
    out << "    <stop offset=\"100%\" stop-color=\"#160b22\"/>\n";
    out << "  </linearGradient>\n";
    out << "</defs>\n";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"url(#bg)\"/>\n";
    out << "<text x=\"60\" y=\"60\" fill=\"#eef2ff\" font-size=\"36\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">Mandelbrot zoom triptych</text>\n";
    out << "<text x=\"60\" y=\"90\" fill=\"#93c5fd\" font-size=\"18\" font-family=\"Helvetica, Arial, sans-serif\">Same quadratic map, three scales. The point is not just that the set exists. It is that the boundary keeps paying rent when you move closer.</text>\n";

    const double panel_w = 390.0;
    const double panel_h = 278.0;
    const double top = 150.0;
    const double left = 60.0;
    const double gap = 55.0;

    for (std::size_t i = 0; i < views.size(); ++i) {
        const double panel_x = left + i * (panel_w + gap);
        const double panel_y = top;
        out << "<rect x=\"" << panel_x << "\" y=\"" << panel_y << "\" width=\"" << panel_w
            << "\" height=\"" << panel_h << "\" rx=\"18\" fill=\"#020617\" stroke=\"#334155\" stroke-width=\"1.4\"/>\n";
        const Stats stats = render_panel(out, views[i], panel_x, panel_y, panel_w, panel_h, panel_px_w, panel_px_h, max_iter, palette_steps);
        out << "<text x=\"" << (panel_x + 18.0) << "\" y=\"" << (panel_y + panel_h + 36.0)
            << "\" fill=\"#e2e8f0\" font-size=\"24\" font-family=\"Helvetica, Arial, sans-serif\" font-weight=\"700\">"
            << views[i].title << "</text>\n";
        out << "<text x=\"" << (panel_x + 18.0) << "\" y=\"" << (panel_y + panel_h + 64.0)
            << "\" fill=\"#cbd5e1\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">"
            << views[i].caption << "</text>\n";

        std::ostringstream stat_line;
        stat_line << std::fixed << std::setprecision(1)
                  << "escaped: " << (100.0 * stats.escape_fraction) << "%"
                  << "  ·  mean smooth iter: " << stats.mean_escape_iter
                  << "  ·  max smooth iter: " << stats.max_smooth_iter;
        out << "<text x=\"" << (panel_x + 18.0) << "\" y=\"" << (panel_y + panel_h + 88.0)
            << "\" fill=\"#93c5fd\" font-size=\"13\" font-family=\"Helvetica, Arial, sans-serif\">"
            << stat_line.str() << "</text>\n";

        std::ostringstream bounds;
        bounds << std::setprecision(6) << std::fixed
               << "center = (" << views[i].cx << ", " << views[i].cy << ")"
               << "  ·  span = " << views[i].span;
        out << "<text x=\"" << (panel_x + 18.0) << "\" y=\"" << (panel_y + panel_h + 110.0)
            << "\" fill=\"#94a3b8\" font-size=\"12\" font-family=\"Helvetica, Arial, sans-serif\">"
            << bounds.str() << "</text>\n";
    }

    out << "<text x=\"60\" y=\"770\" fill=\"#cbd5e1\" font-size=\"14\" font-family=\"Helvetica, Arial, sans-serif\">Rendered directly in C++ with smoothed escape-time coloring and row-run SVG compression.</text>\n";
    out << "</svg>\n";

    if (!out) {
        std::cerr << "failed to write " << output << '\n';
        return 1;
    }

    std::cout << output << '\n';
    return 0;
}
