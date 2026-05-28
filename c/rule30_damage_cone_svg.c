#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_WIDTH 241
#define DEFAULT_STEPS 120
#define DEFAULT_OFFSET 3

static int next_cell(int left, int center, int right) {
    int pattern = (left << 2) | (center << 1) | right;
    return (30 >> pattern) & 1;
}

static void step_rule30(const unsigned char *current, unsigned char *next, int width) {
    memset(next, 0, (size_t)width);
    for (int i = 1; i < width - 1; ++i) {
        next[i] = (unsigned char)next_cell(current[i - 1], current[i], current[i + 1]);
    }
}

static void write_text(FILE *out, double x, double y, const char *text, int size, const char *fill, const char *anchor, int weight) {
    fprintf(
        out,
        "<text x=\"%.1f\" y=\"%.1f\" fill=\"%s\" font-size=\"%d\" font-family=\"Inter, Arial, sans-serif\" text-anchor=\"%s\" font-weight=\"%d\">%s</text>\n",
        x,
        y,
        fill,
        size,
        anchor,
        weight,
        text
    );
}

static void write_rect(FILE *out, double x, double y, double width, double height, const char *fill, const char *stroke, double radius) {
    fprintf(
        out,
        "<rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" fill=\"%s\" stroke=\"%s\" rx=\"%.1f\"/>\n",
        x,
        y,
        width,
        height,
        fill,
        stroke,
        radius
    );
}

static void write_panel_background(FILE *out, double x, double y, double width, double height) {
    write_rect(out, x, y, width, height, "#ffffff", "#e5e7eb", 18.0);
}

static void write_polyline(FILE *out, const double *xs, const double *ys, int count, const char *stroke, double width) {
    fprintf(
        out,
        "<polyline fill=\"none\" stroke=\"%s\" stroke-width=\"%.2f\" stroke-linejoin=\"round\" stroke-linecap=\"round\" points=\"",
        stroke,
        width
    );
    for (int i = 0; i < count; ++i) {
        fprintf(out, "%.1f,%.1f", xs[i], ys[i]);
        if (i + 1 < count) {
            fputc(' ', out);
        }
    }
    fprintf(out, "\"/>\n");
}

static void write_grid(FILE *out, double left, double top, double right, double bottom, int x_ticks, int y_ticks) {
    for (int i = 0; i <= x_ticks; ++i) {
        double x = left + (right - left) * (double)i / (double)x_ticks;
        fprintf(out, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#f1f5f9\" stroke-width=\"1.0\" stroke-dasharray=\"4 6\"/>\n", x, top, x, bottom);
    }
    for (int i = 0; i <= y_ticks; ++i) {
        double y = bottom - (bottom - top) * (double)i / (double)y_ticks;
        fprintf(out, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#e5e7eb\" stroke-width=\"1.0\" stroke-dasharray=\"4 6\"/>\n", left, y, right, y);
    }
}

static int first_live(const unsigned char *row, int width) {
    for (int i = 0; i < width; ++i) {
        if (row[i]) {
            return i;
        }
    }
    return -1;
}

static int last_live(const unsigned char *row, int width) {
    for (int i = width - 1; i >= 0; --i) {
        if (row[i]) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char **argv) {
    const char *svg_path = argc > 1 ? argv[1] : "art/rule30-damage-cone.svg";
    const char *csv_path = argc > 2 ? argv[2] : "art/rule30-damage-cone.csv";
    int width = argc > 3 ? atoi(argv[3]) : DEFAULT_WIDTH;
    int steps = argc > 4 ? atoi(argv[4]) : DEFAULT_STEPS;
    int offset = argc > 5 ? atoi(argv[5]) : DEFAULT_OFFSET;

    if (width < 31 || steps < 16 || offset <= 0 || offset >= width / 4) {
        fprintf(stderr, "usage: %s [svg-path] [csv-path] [width>=31] [steps>=16] [offset>0]\n", argv[0]);
        return 1;
    }

    unsigned char *base_current = calloc((size_t)width, 1);
    unsigned char *base_next = calloc((size_t)width, 1);
    unsigned char *perturbed_current = calloc((size_t)width, 1);
    unsigned char *perturbed_next = calloc((size_t)width, 1);
    unsigned char *base_rows = calloc((size_t)width * (size_t)steps, 1);
    unsigned char *diff_rows = calloc((size_t)width * (size_t)steps, 1);
    double *base_active = calloc((size_t)steps, sizeof(double));
    double *perturbed_active = calloc((size_t)steps, sizeof(double));
    double *diff_active = calloc((size_t)steps, sizeof(double));
    double *diff_width = calloc((size_t)steps, sizeof(double));
    if (!base_current || !base_next || !perturbed_current || !perturbed_next || !base_rows || !diff_rows || !base_active || !perturbed_active || !diff_active || !diff_width) {
        fprintf(stderr, "allocation failed\n");
        free(base_current);
        free(base_next);
        free(perturbed_current);
        free(perturbed_next);
        free(base_rows);
        free(diff_rows);
        free(base_active);
        free(perturbed_active);
        free(diff_active);
        free(diff_width);
        return 1;
    }

    int center = width / 2;
    base_current[center] = 1;
    perturbed_current[center] = 1;
    perturbed_current[center + offset] = 1;

    double max_diff_active = 0.0;
    double max_diff_width = 0.0;
    double first_full_width_step = -1.0;

    FILE *csv = fopen(csv_path, "w");
    if (!csv) {
        fprintf(stderr, "failed to open %s\n", csv_path);
        free(base_current);
        free(base_next);
        free(perturbed_current);
        free(perturbed_next);
        free(base_rows);
        free(diff_rows);
        free(base_active);
        free(perturbed_active);
        free(diff_active);
        free(diff_width);
        return 1;
    }
    fprintf(csv, "step,base_active_fraction,perturbed_active_fraction,diff_active_fraction,diff_width_fraction,leftmost_diff,rightmost_diff\n");

    for (int step = 0; step < steps; ++step) {
        int base_count = 0;
        int perturbed_count = 0;
        int diff_count = 0;
        for (int i = 0; i < width; ++i) {
            unsigned char base_value = base_current[i];
            unsigned char perturbed_value = perturbed_current[i];
            unsigned char diff_value = (unsigned char)(base_value ^ perturbed_value);
            base_rows[step * width + i] = base_value;
            diff_rows[step * width + i] = diff_value;
            base_count += base_value ? 1 : 0;
            perturbed_count += perturbed_value ? 1 : 0;
            diff_count += diff_value ? 1 : 0;
        }
        int left = first_live(&diff_rows[step * width], width);
        int right = last_live(&diff_rows[step * width], width);
        double width_fraction = 0.0;
        if (left >= 0 && right >= left) {
            width_fraction = (double)(right - left + 1) / (double)width;
        }
        base_active[step] = (double)base_count / (double)width;
        perturbed_active[step] = (double)perturbed_count / (double)width;
        diff_active[step] = (double)diff_count / (double)width;
        diff_width[step] = width_fraction;
        if (diff_active[step] > max_diff_active) {
            max_diff_active = diff_active[step];
        }
        if (diff_width[step] > max_diff_width) {
            max_diff_width = diff_width[step];
        }
        if (first_full_width_step < 0.0 && width_fraction > 0.95) {
            first_full_width_step = (double)step;
        }
        fprintf(csv, "%d,%.6f,%.6f,%.6f,%.6f,%d,%d\n", step, base_active[step], perturbed_active[step], diff_active[step], diff_width[step], left, right);
        step_rule30(base_current, base_next, width);
        step_rule30(perturbed_current, perturbed_next, width);
        unsigned char *tmp = base_current;
        base_current = base_next;
        base_next = tmp;
        tmp = perturbed_current;
        perturbed_current = perturbed_next;
        perturbed_next = tmp;
    }
    fclose(csv);

    if (max_diff_active <= 0.0) {
        max_diff_active = 1.0;
    }
    if (max_diff_width <= 0.0) {
        max_diff_width = 1.0;
    }

    FILE *svg = fopen(svg_path, "w");
    if (!svg) {
        fprintf(stderr, "failed to open %s\n", svg_path);
        free(base_current);
        free(base_next);
        free(perturbed_current);
        free(perturbed_next);
        free(base_rows);
        free(diff_rows);
        free(base_active);
        free(perturbed_active);
        free(diff_active);
        free(diff_width);
        return 1;
    }

    const double total_width = 1760.0;
    const double total_height = 1220.0;
    const double left = 60.0;
    const double top = 198.0;
    const double right = total_width - 160.0;
    const double bottom = total_height - 72.0;
    const double panel_gap_x = 34.0;
    const double panel_gap_y = 46.0;
    const double panel_width = (right - left - panel_gap_x) / 2.0;
    const double panel_height = (bottom - top - panel_gap_y) / 2.0;

    fprintf(svg, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%.0f\" height=\"%.0f\" viewBox=\"0 0 %.0f %.0f\">\n", total_width, total_height, total_width, total_height);
    fprintf(svg, "<rect width=\"100%%\" height=\"100%%\" fill=\"#fcfcfd\"/>\n");
    write_text(svg, total_width / 2.0, 46.0, "Rule 30 damage cone from one extra live cell", 31, "#111827", "middle", 700);
    write_text(svg, total_width / 2.0, 78.0, "The left panel is the ordinary one-cell seed. The right panel is the XOR difference after adding one extra live cell nearby.", 16, "#475569", "middle", 400);
    write_text(svg, total_width / 2.0, 100.0, "Rule 30 does not just spread activity. It amplifies a tiny local perturbation into a wide irregular cone that keeps shedding structure row after row.", 16, "#475569", "middle", 400);

    double panel1_x = left;
    double panel2_x = left + panel_width + panel_gap_x;
    double panel3_x = left;
    double panel4_x = left + panel_width + panel_gap_x;
    double row1_y = top;
    double row2_y = top + panel_height + panel_gap_y;

    write_panel_background(svg, panel1_x, row1_y, panel_width, panel_height);
    write_panel_background(svg, panel2_x, row1_y, panel_width, panel_height);
    write_panel_background(svg, panel3_x, row2_y, panel_width, panel_height);
    write_panel_background(svg, panel4_x, row2_y, panel_width, panel_height);

    write_text(svg, panel1_x + 24.0, row1_y + 34.0, "Base Rule 30 spacetime", 20, "#111827", "start", 700);
    write_text(svg, panel1_x + 24.0, row1_y + 56.0, "Single live cell at the center. The familiar wedge appears immediately, but it stays internally jagged.", 13, "#475569", "start", 400);
    write_text(svg, panel2_x + 24.0, row1_y + 34.0, "Damage cone after one adjacent flip", 20, "#111827", "start", 700);
    write_text(svg, panel2_x + 24.0, row1_y + 56.0, "XOR difference between the base run and a run with one extra live cell offset by +3 at step 0.", 13, "#475569", "start", 400);
    write_text(svg, panel3_x + 24.0, row2_y + 34.0, "How much of each row is changed?", 20, "#111827", "start", 700);
    write_text(svg, panel3_x + 24.0, row2_y + 56.0, "The difference never settles into a smooth ramp. The changed share keeps breathing as the cone roughens.", 13, "#475569", "start", 400);
    write_text(svg, panel4_x + 24.0, row2_y + 34.0, "How wide is the damaged region?", 20, "#111827", "start", 700);
    if (first_full_width_step >= 0.0) {
        char summary[256];
        snprintf(summary, sizeof(summary), "By step %.0f the difference span already covers more than 95%% of the row width in this bounded run.", first_full_width_step);
        write_text(svg, panel4_x + 24.0, row2_y + 56.0, summary, 13, "#475569", "start", 400);
    } else {
        write_text(svg, panel4_x + 24.0, row2_y + 56.0, "The damaged span keeps widening across the whole run, but it still does so with visible irregular breathing.", 13, "#475569", "start", 400);
    }

    double heat_left = panel1_x + 40.0;
    double heat_top = row1_y + 92.0;
    double heat_width = panel_width - 80.0;
    double heat_height = panel_height - 128.0;
    double cell_w = heat_width / (double)width;
    double cell_h = heat_height / (double)steps;

    for (int step = 0; step < steps; ++step) {
        for (int i = 0; i < width; ++i) {
            if (base_rows[step * width + i]) {
                fprintf(svg, "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"#111827\"/>\n", heat_left + cell_w * (double)i, heat_top + cell_h * (double)step, cell_w + 0.05, cell_h + 0.05);
            }
        }
    }
    double diff_left = panel2_x + 40.0;
    for (int step = 0; step < steps; ++step) {
        for (int i = 0; i < width; ++i) {
            if (diff_rows[step * width + i]) {
                fprintf(svg, "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"#db2777\"/>\n", diff_left + cell_w * (double)i, heat_top + cell_h * (double)step, cell_w + 0.05, cell_h + 0.05);
            }
        }
    }

    fprintf(svg, "<rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" fill=\"none\" stroke=\"#cbd5e1\" rx=\"10\"/>\n", heat_left, heat_top, heat_width, heat_height);
    fprintf(svg, "<rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" fill=\"none\" stroke=\"#cbd5e1\" rx=\"10\"/>\n", diff_left, heat_top, heat_width, heat_height);
    write_text(svg, heat_left, heat_top - 12.0, "columns", 12, "#64748b", "start", 600);
    write_text(svg, diff_left, heat_top - 12.0, "columns", 12, "#64748b", "start", 600);
    write_text(svg, heat_left - 14.0, heat_top + 14.0, "0", 12, "#64748b", "end", 400);
    write_text(svg, heat_left - 14.0, heat_top + heat_height, "later", 12, "#64748b", "end", 400);
    write_text(svg, diff_left - 14.0, heat_top + 14.0, "0", 12, "#64748b", "end", 400);
    write_text(svg, diff_left - 14.0, heat_top + heat_height, "later", 12, "#64748b", "end", 400);

    double chart_left = panel3_x + 70.0;
    double chart_top = row2_y + 118.0;
    double chart_right = panel3_x + panel_width - 40.0;
    double chart_bottom = row2_y + panel_height - 58.0;
    double width_chart_left = panel4_x + 70.0;
    double width_chart_top = row2_y + 118.0;
    double width_chart_right = panel4_x + panel_width - 40.0;
    double width_chart_bottom = row2_y + panel_height - 58.0;

    write_grid(svg, chart_left, chart_top, chart_right, chart_bottom, 6, 5);
    fprintf(svg, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#334155\" stroke-width=\"1.5\"/>\n", chart_left, chart_bottom, chart_right, chart_bottom);
    fprintf(svg, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#334155\" stroke-width=\"1.5\"/>\n", chart_left, chart_top, chart_left, chart_bottom);
    write_text(svg, (chart_left + chart_right) / 2.0, chart_bottom + 46.0, "step", 14, "#334155", "middle", 600);
    write_text(svg, chart_left, chart_top - 16.0, "changed fraction of row", 13, "#334155", "start", 600);

    double *xs = calloc((size_t)steps, sizeof(double));
    double *ys = calloc((size_t)steps, sizeof(double));
    double *base_ys = calloc((size_t)steps, sizeof(double));
    if (!xs || !ys || !base_ys) {
        fprintf(stderr, "allocation failed\n");
        fclose(svg);
        free(xs);
        free(ys);
        free(base_ys);
        free(base_current);
        free(base_next);
        free(perturbed_current);
        free(perturbed_next);
        free(base_rows);
        free(diff_rows);
        free(base_active);
        free(perturbed_active);
        free(diff_active);
        free(diff_width);
        return 1;
    }

    for (int step = 0; step < steps; ++step) {
        double x = chart_left + (chart_right - chart_left) * (double)step / (double)(steps - 1);
        xs[step] = x;
        ys[step] = chart_bottom - (chart_bottom - chart_top) * diff_active[step] / max_diff_active;
        base_ys[step] = chart_bottom - (chart_bottom - chart_top) * base_active[step] / 0.6;
    }
    write_polyline(svg, xs, ys, steps, "#db2777", 3.0);
    write_polyline(svg, xs, base_ys, steps, "#111827", 2.2);
    for (int i = 0; i <= 5; ++i) {
        double y_value = max_diff_active * (double)i / 5.0;
        double y = chart_bottom - (chart_bottom - chart_top) * (double)i / 5.0;
        char label[32];
        snprintf(label, sizeof(label), "%.2f", y_value);
        write_text(svg, chart_left - 12.0, y + 5.0, label, 12, "#64748b", "end", 400);
    }
    for (int i = 0; i <= 6; ++i) {
        int tick_step = (steps - 1) * i / 6;
        double x = chart_left + (chart_right - chart_left) * (double)i / 6.0;
        char label[32];
        snprintf(label, sizeof(label), "%d", tick_step);
        write_text(svg, x, chart_bottom + 26.0, label, 12, "#64748b", "middle", 400);
    }
    write_text(svg, chart_right - 180.0, chart_top + 18.0, "magenta: diff share", 12, "#db2777", "start", 600);
    write_text(svg, chart_right - 180.0, chart_top + 36.0, "black: base live share", 12, "#111827", "start", 600);

    write_grid(svg, width_chart_left, width_chart_top, width_chart_right, width_chart_bottom, 6, 5);
    fprintf(svg, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#334155\" stroke-width=\"1.5\"/>\n", width_chart_left, width_chart_bottom, width_chart_right, width_chart_bottom);
    fprintf(svg, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#334155\" stroke-width=\"1.5\"/>\n", width_chart_left, width_chart_top, width_chart_left, width_chart_bottom);
    write_text(svg, (width_chart_left + width_chart_right) / 2.0, width_chart_bottom + 46.0, "step", 14, "#334155", "middle", 600);
    write_text(svg, width_chart_left, width_chart_top - 16.0, "difference span / row width", 13, "#334155", "start", 600);
    for (int step = 0; step < steps; ++step) {
        xs[step] = width_chart_left + (width_chart_right - width_chart_left) * (double)step / (double)(steps - 1);
        ys[step] = width_chart_bottom - (width_chart_bottom - width_chart_top) * diff_width[step] / max_diff_width;
    }
    write_polyline(svg, xs, ys, steps, "#7c3aed", 3.0);
    for (int i = 0; i <= 5; ++i) {
        double y_value = max_diff_width * (double)i / 5.0;
        double y = width_chart_bottom - (width_chart_bottom - width_chart_top) * (double)i / 5.0;
        char label[32];
        snprintf(label, sizeof(label), "%.2f", y_value);
        write_text(svg, width_chart_left - 12.0, y + 5.0, label, 12, "#64748b", "end", 400);
    }
    for (int i = 0; i <= 6; ++i) {
        int tick_step = (steps - 1) * i / 6;
        double x = width_chart_left + (width_chart_right - width_chart_left) * (double)i / 6.0;
        char label[32];
        snprintf(label, sizeof(label), "%d", tick_step);
        write_text(svg, x, width_chart_bottom + 26.0, label, 12, "#64748b", "middle", 400);
    }
    write_text(svg, width_chart_right - 165.0, width_chart_top + 18.0, "purple: damaged span", 12, "#7c3aed", "start", 600);

    write_text(svg, total_width / 2.0, total_height - 20.0, "Generated by c/rule30_damage_cone_svg.c with width 241, 120 steps, and a +3-cell perturbation.", 12, "#64748b", "middle", 400);
    fprintf(svg, "</svg>\n");
    fclose(svg);

    free(xs);
    free(ys);
    free(base_ys);
    free(base_current);
    free(base_next);
    free(perturbed_current);
    free(perturbed_next);
    free(base_rows);
    free(diff_rows);
    free(base_active);
    free(perturbed_active);
    free(diff_active);
    free(diff_width);
    return 0;
}
