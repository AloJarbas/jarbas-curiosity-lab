from __future__ import annotations

import csv
from dataclasses import dataclass
from html import escape
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from textwrap import wrap
from typing import Iterable, Sequence


REPO = Path(__file__).resolve().parents[1]
ART = REPO / 'art'
NOTEBOOKS = REPO / 'notebooks'

DEFAULT_STEPS = 320
DEFAULT_MAX_DEPTH = 24
DEFAULT_TAIL_WINDOW = 192
DEFAULT_MAX_PERIOD = 64
DEFAULT_BLOCK_SIZE = 5
DEFAULT_SVG_PATH = ART / 'rule30-left-right-asymmetry.svg'
DEFAULT_CSV_PATH = ART / 'rule30-left-right-asymmetry.csv'
DEFAULT_NOTEBOOK_PATH = NOTEBOOKS / 'rule30-left-right-asymmetry.ipynb'
PNG_PREVIEW_SIZE = 2400

LEFT_COLOR = '#d97706'
RIGHT_COLOR = '#2563eb'
HEAT_LIVE = '#111827'
HEAT_BG = '#ffffff'
SUMMARY_BG = '#f8fafc'


@dataclass(frozen=True)
class DiagonalMetrics:
    side: str
    depth: int
    tail_length: int
    black_fraction: float
    transition_rate: float
    block_entropy_bits: float
    normalized_block_entropy: float
    detected_period: int

    def as_dict(self) -> dict[str, int | float | str]:
        return {
            'side': self.side,
            'depth': self.depth,
            'tail_length': self.tail_length,
            'black_fraction': self.black_fraction,
            'transition_rate': self.transition_rate,
            'block_entropy_bits': self.block_entropy_bits,
            'normalized_block_entropy': self.normalized_block_entropy,
            'detected_period': self.detected_period,
        }


@dataclass(frozen=True)
class AsymmetryStudy:
    steps: int
    max_depth: int
    tail_window: int
    max_period: int
    block_size: int
    width: int
    center: int
    rows: tuple[tuple[int, ...], ...]
    metrics: tuple[DiagonalMetrics, ...]

    @property
    def left_metrics(self) -> tuple[DiagonalMetrics, ...]:
        return tuple(metric for metric in self.metrics if metric.side == 'left')

    @property
    def right_metrics(self) -> tuple[DiagonalMetrics, ...]:
        return tuple(metric for metric in self.metrics if metric.side == 'right')


def next_cell(left: int, center: int, right: int) -> int:
    pattern = (left << 2) | (center << 1) | right
    return (30 >> pattern) & 1


def step_rule30(row: Sequence[int]) -> tuple[int, ...]:
    width = len(row)
    out = [0] * width
    for idx in range(1, width - 1):
        out[idx] = next_cell(row[idx - 1], row[idx], row[idx + 1])
    return tuple(out)


def simulate_rule30(*, steps: int = DEFAULT_STEPS, margin: int = 8) -> tuple[tuple[tuple[int, ...], ...], int]:
    width = 2 * steps + 1 + 2 * margin
    center = width // 2
    row = [0] * width
    row[center] = 1
    rows = [tuple(row)]
    for _ in range(steps - 1):
        row = list(step_rule30(row))
        rows.append(tuple(row))
    return tuple(rows), center


def diagonal_sequence(rows: Sequence[Sequence[int]], center: int, *, side: str, depth: int) -> tuple[int, ...]:
    values: list[int] = []
    for step, row in enumerate(rows):
        if step < depth:
            continue
        if side == 'left':
            idx = center - step + depth
        elif side == 'right':
            idx = center + step - depth
        else:
            raise ValueError(f'unknown side: {side}')
        if 0 <= idx < len(row):
            values.append(int(row[idx]))
    return tuple(values)


def tail_period(sequence: Sequence[int], *, max_period: int, tail_window: int) -> int:
    if not sequence:
        return 0
    tail = list(sequence[-min(len(sequence), tail_window):])
    for period in range(1, max_period + 1):
        if len(tail) < 4 * period:
            continue
        if all(tail[idx] == tail[idx - period] for idx in range(period, len(tail))):
            return period
    return 0


def transition_rate(sequence: Sequence[int]) -> float:
    if len(sequence) < 2:
        return 0.0
    changes = sum(1 for prev, cur in zip(sequence, sequence[1:]) if prev != cur)
    return changes / (len(sequence) - 1)


def block_entropy(sequence: Sequence[int], *, block_size: int) -> float:
    if len(sequence) < block_size:
        return 0.0
    counts: dict[tuple[int, ...], int] = {}
    total = 0
    for idx in range(len(sequence) - block_size + 1):
        block = tuple(sequence[idx:idx + block_size])
        counts[block] = counts.get(block, 0) + 1
        total += 1
    entropy = 0.0
    for count in counts.values():
        probability = count / total
        entropy -= probability * math.log2(probability)
    return entropy


def summarize_sequence(sequence: Sequence[int], *, max_period: int, tail_window: int, block_size: int) -> tuple[int, float, float, float, float, int]:
    tail = tuple(sequence[-min(len(sequence), tail_window):])
    black = sum(tail) / len(tail) if tail else 0.0
    transitions = transition_rate(tail)
    entropy_bits = block_entropy(tail, block_size=block_size)
    normalized = entropy_bits / block_size if block_size else 0.0
    period = tail_period(tail, max_period=max_period, tail_window=tail_window)
    return period, black, transitions, entropy_bits, normalized, len(tail)


def study_asymmetry(
    *,
    steps: int = DEFAULT_STEPS,
    max_depth: int = DEFAULT_MAX_DEPTH,
    tail_window: int = DEFAULT_TAIL_WINDOW,
    max_period: int = DEFAULT_MAX_PERIOD,
    block_size: int = DEFAULT_BLOCK_SIZE,
) -> AsymmetryStudy:
    rows, center = simulate_rule30(steps=steps, margin=max_depth + 8)
    metrics: list[DiagonalMetrics] = []
    for side in ('left', 'right'):
        for depth in range(1, max_depth + 1):
            sequence = diagonal_sequence(rows, center, side=side, depth=depth)
            period, black, transitions, entropy_bits, normalized, tail_length = summarize_sequence(
                sequence,
                max_period=max_period,
                tail_window=tail_window,
                block_size=block_size,
            )
            metrics.append(
                DiagonalMetrics(
                    side=side,
                    depth=depth,
                    tail_length=tail_length,
                    black_fraction=black,
                    transition_rate=transitions,
                    block_entropy_bits=entropy_bits,
                    normalized_block_entropy=normalized,
                    detected_period=period,
                )
            )
    return AsymmetryStudy(
        steps=steps,
        max_depth=max_depth,
        tail_window=tail_window,
        max_period=max_period,
        block_size=block_size,
        width=len(rows[0]),
        center=center,
        rows=tuple(rows),
        metrics=tuple(metrics),
    )


def _text(x: float, y: float, text: str, *, size: int = 16, fill: str = '#111827', anchor: str = 'start', weight: str = '400') -> str:
    return (
        f'<text x="{x:.1f}" y="{y:.1f}" fill="{fill}" font-size="{size}" '
        f'font-family="Inter, Arial, sans-serif" text-anchor="{anchor}" font-weight="{weight}">{escape(text)}</text>'
    )


def _paragraph(x: float, y: float, text: str, *, width: int, size: int = 14, fill: str = '#475569', anchor: str = 'start', line_height: float = 18.0) -> str:
    lines = wrap(text, width=width) or [text]
    spans = [f'<tspan x="{x:.1f}" dy="0" text-anchor="{anchor}">{escape(lines[0])}</tspan>']
    spans.extend(f'<tspan x="{x:.1f}" dy="{line_height:.1f}" text-anchor="{anchor}">{escape(line)}</tspan>' for line in lines[1:])
    return (
        f'<text x="{x:.1f}" y="{y:.1f}" fill="{fill}" font-size="{size}" '
        f'font-family="Inter, Arial, sans-serif">{"".join(spans)}</text>'
    )


def _rect(x: float, y: float, width: float, height: float, *, fill: str, stroke: str = '#e5e7eb', radius: float = 16.0, stroke_width: float = 1.0) -> str:
    return (
        f'<rect x="{x:.1f}" y="{y:.1f}" width="{width:.1f}" height="{height:.1f}" '
        f'fill="{fill}" stroke="{stroke}" stroke-width="{stroke_width:.1f}" rx="{radius:.1f}"/>'
    )


def _line(x1: float, y1: float, x2: float, y2: float, *, stroke: str = '#334155', width: float = 1.0, dash: str | None = None) -> str:
    dash_attr = f' stroke-dasharray="{dash}"' if dash else ''
    return f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{stroke}" stroke-width="{width}"{dash_attr}/>'


def _polyline(points: Iterable[tuple[float, float]], *, stroke: str, width: float = 3.0) -> str:
    payload = ' '.join(f'{x:.1f},{y:.1f}' for x, y in points)
    return f'<polyline fill="none" stroke="{stroke}" stroke-width="{width}" stroke-linejoin="round" stroke-linecap="round" points="{payload}"/>'


def _circle(x: float, y: float, radius: float, *, fill: str) -> str:
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{radius:.1f}" fill="{fill}" stroke="#ffffff" stroke-width="2.0"/>'


def _metric_for(study: AsymmetryStudy, side: str, depth: int) -> DiagonalMetrics:
    return next(metric for metric in study.metrics if metric.side == side and metric.depth == depth)


def render_svg(study: AsymmetryStudy) -> str:
    width = 1640
    height = 1340
    left = 70.0
    top = 190.0
    right = width - 70.0
    bottom = height - 120.0
    gap_x = 34.0
    gap_y = 38.0
    panel_width = (right - left - gap_x) / 2.0
    panel_height = (bottom - top - gap_y) / 2.0

    selected_depths = (1, 4, 8, 16)
    overlay_colors = ('#dc2626', '#ea580c', '#7c3aed', '#0f766e')

    left_periodic = sum(1 for metric in study.left_metrics if metric.detected_period > 0)
    right_periodic = sum(1 for metric in study.right_metrics if metric.detected_period > 0)
    left_entropy = sum(metric.normalized_block_entropy for metric in study.left_metrics) / len(study.left_metrics)
    right_entropy = sum(metric.normalized_block_entropy for metric in study.right_metrics) / len(study.right_metrics)
    left_transitions = sum(metric.transition_rate for metric in study.left_metrics) / len(study.left_metrics)
    right_transitions = sum(metric.transition_rate for metric in study.right_metrics) / len(study.right_metrics)

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#fcfcfd"/>',
        _text(width / 2.0, 44.0, 'Rule 30 is not equally chaotic on both flanks', size=30, anchor='middle', weight='700'),
        _paragraph(
            width / 2.0,
            78.0,
            'This pass stops treating the wedge as one undifferentiated chaos picture. It measures diagonals the same depth in from the left and right edges, then asks which side still admits short tail periods and which side keeps the higher finite-block entropy.',
            width=116,
            size=15,
            anchor='middle',
        ),
    ]
    legend_y = 130.0
    legend_left = width / 2.0 - 110.0
    parts.append(_line(legend_left, legend_y, legend_left + 28.0, legend_y, stroke=LEFT_COLOR, width=3.2))
    parts.append(_text(legend_left + 40.0, legend_y + 5.0, 'left edge diagonals', size=14, fill=LEFT_COLOR))
    parts.append(_line(legend_left + 210.0, legend_y, legend_left + 238.0, legend_y, stroke=RIGHT_COLOR, width=3.2))
    parts.append(_text(legend_left + 250.0, legend_y + 5.0, 'right edge diagonals', size=14, fill=RIGHT_COLOR))

    panel1_x = left
    panel2_x = left + panel_width + gap_x
    panel3_x = left
    panel4_x = left + panel_width + gap_x
    row1_y = top
    row2_y = top + panel_height + gap_y

    for x, y in ((panel1_x, row1_y), (panel2_x, row1_y), (panel3_x, row2_y), (panel4_x, row2_y)):
        parts.append(_rect(x, y, panel_width, panel_height, fill='#ffffff'))

    parts.extend([
        _text(panel1_x + 24.0, row1_y + 32.0, 'Spacetime wedge with matched diagonal cuts', size=20, weight='700'),
        _paragraph(panel1_x + 24.0, row1_y + 58.0, 'Depth means distance in from the edge, not a vertical offset from the center. The same depth is sampled on both flanks.', width=64),
        _text(panel2_x + 24.0, row1_y + 32.0, 'Detected tail period by depth', size=20, weight='700'),
        _paragraph(panel2_x + 24.0, row1_y + 58.0, f'For each depth, the last {study.tail_window} diagonal cells are checked for an exact repeating period up to {study.max_period}.', width=62),
        _text(panel3_x + 24.0, row2_y + 32.0, 'Tail entropy by depth', size=20, weight='700'),
        _paragraph(panel3_x + 24.0, row2_y + 58.0, f'Normalized {study.block_size}-bit block entropy keeps the left lane and right lane on the same scale from 0 to 1.', width=62),
        _text(panel4_x + 24.0, row2_y + 32.0, 'Tail transition rate by depth', size=20, weight='700'),
        _paragraph(panel4_x + 24.0, row2_y + 58.0, 'Transition rate is a simpler statistic than entropy, but it still shows the same directional split: the right flank keeps flipping harder.', width=62),
    ])

    heat_left = panel1_x + 36.0
    heat_top = row1_y + 96.0
    heat_width = panel_width - 72.0
    heat_height = panel_height - 132.0
    visible_half_width = min(study.steps - 1, 160)
    x_min = study.center - visible_half_width
    x_max = study.center + visible_half_width
    visible_width = x_max - x_min + 1
    cell_w = heat_width / visible_width
    cell_h = heat_height / study.steps

    for step, row in enumerate(study.rows):
        for idx in range(x_min, x_max + 1):
            if row[idx]:
                x = heat_left + (idx - x_min) * cell_w
                y = heat_top + step * cell_h
                parts.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{cell_w + 0.08:.2f}" height="{cell_h + 0.08:.2f}" fill="{HEAT_LIVE}"/>')

    parts.append(_rect(heat_left, heat_top, heat_width, heat_height, fill='none', stroke='#cbd5e1', radius=10.0, stroke_width=1.0))
    for depth, color in zip(selected_depths, overlay_colors):
        left_points = []
        right_points = []
        for step in range(depth, study.steps):
            left_idx = study.center - step + depth
            right_idx = study.center + step - depth
            if x_min <= left_idx <= x_max:
                left_points.append((heat_left + (left_idx - x_min + 0.5) * cell_w, heat_top + (step + 0.5) * cell_h))
            if x_min <= right_idx <= x_max:
                right_points.append((heat_left + (right_idx - x_min + 0.5) * cell_w, heat_top + (step + 0.5) * cell_h))
        parts.append(_polyline(left_points, stroke=color, width=2.1))
        parts.append(_polyline(right_points, stroke=color, width=2.1))
    legend_y = heat_top + 16.0
    legend_x = heat_left + heat_width - 180.0
    parts.append(_rect(legend_x - 14.0, heat_top + 10.0, 168.0, 108.0, fill='#ffffff', stroke='#e2e8f0', radius=12.0, stroke_width=0.8))
    parts.append(_text(legend_x, legend_y, 'matched depths', size=12, weight='700'))
    for row_idx, (depth, color) in enumerate(zip(selected_depths, overlay_colors), start=1):
        y = legend_y + row_idx * 20.0
        parts.append(_line(legend_x, y, legend_x + 24.0, y, stroke=color, width=3.0))
        parts.append(_text(legend_x + 34.0, y + 4.0, f'depth {depth}', size=12))

    def draw_metric_panel(
        left_x: float,
        top_y: float,
        title_y_label: str,
        y_min: float,
        y_max: float,
        metric_name: str,
        fmt: str,
        right_color: str = RIGHT_COLOR,
        left_color: str = LEFT_COLOR,
        show_zero_note: bool = False,
    ) -> None:
        plot_left = left_x + 72.0
        plot_right = left_x + panel_width - 44.0
        plot_top = top_y + 116.0
        plot_bottom = top_y + panel_height - 86.0

        def map_x(depth: int) -> float:
            fraction = (depth - 1) / max(1, study.max_depth - 1)
            return plot_left + fraction * (plot_right - plot_left)

        def map_y(value: float) -> float:
            return plot_bottom - (value - y_min) / (y_max - y_min) * (plot_bottom - plot_top)

        for tick in range(6):
            y_value = y_min + (y_max - y_min) * tick / 5.0
            y = map_y(y_value)
            parts.append(_line(plot_left, y, plot_right, y, stroke='#e5e7eb', dash='4 6'))
            parts.append(_text(plot_left - 10.0, y + 5.0, fmt.format(y_value), size=12, fill='#64748b', anchor='end'))
        for tick in range(6):
            depth = 1 + (study.max_depth - 1) * tick / 5.0
            rounded = int(round(depth))
            x = map_x(rounded)
            parts.append(_line(x, plot_top, x, plot_bottom, stroke='#eef2f7', dash='4 6'))
            parts.append(_text(x, plot_bottom + 24.0, str(rounded), size=12, fill='#64748b', anchor='middle'))
        parts.append(_line(plot_left, plot_top, plot_left, plot_bottom, stroke='#334155', width=1.5))
        parts.append(_line(plot_left, plot_bottom, plot_right, plot_bottom, stroke='#334155', width=1.5))
        parts.append(_text(plot_left, top_y + 98.0, title_y_label, size=13, fill='#334155', weight='600'))

        left_metrics = study.left_metrics
        right_metrics = study.right_metrics
        left_points = [(map_x(metric.depth), map_y(float(getattr(metric, metric_name)))) for metric in left_metrics]
        right_points = [(map_x(metric.depth), map_y(float(getattr(metric, metric_name)))) for metric in right_metrics]
        parts.append(_polyline(left_points, stroke=left_color))
        parts.append(_polyline(right_points, stroke=right_color))
        parts.append(_circle(left_points[-1][0], left_points[-1][1], 4.8, fill=left_color))
        parts.append(_circle(right_points[-1][0], right_points[-1][1], 4.8, fill=right_color))
        parts.append(_text((plot_left + plot_right) / 2.0, plot_bottom + 44.0, 'depth in from edge (cells)', size=14, fill='#334155', anchor='middle', weight='600'))
        if show_zero_note:
            parts.append(_text(plot_left, plot_bottom + 66.0, '0 means no exact tail period found within the search limit', size=13, fill='#64748b'))

    draw_metric_panel(panel2_x, row1_y, 'detected period (steps)', 0.0, float(study.max_period), 'detected_period', '{:.0f}', show_zero_note=True)
    draw_metric_panel(panel3_x, row2_y, 'normalized block entropy', 0.0, 1.0, 'normalized_block_entropy', '{:.2f}')
    draw_metric_panel(panel4_x, row2_y, 'tail bit-flip rate', 0.0, 1.0, 'transition_rate', '{:.2f}')

    summary_x = left + 26.0
    summary_y = bottom + 22.0
    summary_width = right - left - 52.0
    parts.append(_rect(summary_x, summary_y, summary_width, 64.0, fill=SUMMARY_BG, stroke='#dbe3ee', radius=12.0, stroke_width=0.8))
    parts.append(_text(summary_x + 18.0, summary_y + 24.0, f'short tail periods found at {left_periodic}/{study.max_depth} left depths but only {right_periodic}/{study.max_depth} right depths', size=13, weight='700'))
    parts.append(_text(summary_x + 18.0, summary_y + 46.0, f'mean normalized entropy: left {left_entropy:.3f}, right {right_entropy:.3f} | mean transition rate: left {left_transitions:.3f}, right {right_transitions:.3f}', size=13, fill='#475569'))

    footer = f'Generated by python/rule30_left_right_asymmetry.py with {study.steps} steps, {study.max_depth} diagonal depths, a {study.tail_window}-cell tail window, and exact period search up to {study.max_period}.'
    parts.append(_text(width / 2.0, height - 20.0, footer, size=12, fill='#64748b', anchor='middle'))
    parts.append('</svg>')
    return '\n'.join(parts)


def write_csv(study: AsymmetryStudy, path: str | Path = DEFAULT_CSV_PATH) -> Path:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', newline='') as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                'side',
                'depth',
                'tail_length',
                'black_fraction',
                'transition_rate',
                'block_entropy_bits',
                'normalized_block_entropy',
                'detected_period',
            ],
        )
        writer.writeheader()
        for metric in study.metrics:
            writer.writerow(metric.as_dict())
    return path


def export_png(svg_path: Path) -> Path | None:
    if sys.platform != 'darwin':
        return None
    qlmanage = shutil.which('qlmanage')
    if qlmanage is None:
        return None
    png_path = svg_path.with_suffix('.png')
    with tempfile.TemporaryDirectory() as tmpdir:
        subprocess.run([qlmanage, '-t', '-s', str(PNG_PREVIEW_SIZE), '-o', tmpdir, str(svg_path.resolve())], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        candidate = Path(tmpdir) / f'{svg_path.name}.png'
        if not candidate.exists():
            return None
        shutil.copyfile(candidate, png_path)
    sips = shutil.which('sips')
    if sips is not None:
        subprocess.run([sips, '--setProperty', 'dpiWidth', '300', '--setProperty', 'dpiHeight', '300', str(png_path)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return png_path


def write_notebook(study: AsymmetryStudy, path: str | Path = DEFAULT_NOTEBOOK_PATH) -> Path:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    left_periodic = sum(1 for metric in study.left_metrics if metric.detected_period > 0)
    right_periodic = sum(1 for metric in study.right_metrics if metric.detected_period > 0)
    left_entropy = sum(metric.normalized_block_entropy for metric in study.left_metrics) / len(study.left_metrics)
    right_entropy = sum(metric.normalized_block_entropy for metric in study.right_metrics) / len(study.right_metrics)
    left_transition = sum(metric.transition_rate for metric in study.left_metrics) / len(study.left_metrics)
    right_transition = sum(metric.transition_rate for metric in study.right_metrics) / len(study.right_metrics)
    left_depth8 = _metric_for(study, 'left', 8)
    right_depth8 = _metric_for(study, 'right', 8)
    left_depth16 = _metric_for(study, 'left', 16)
    right_depth16 = _metric_for(study, 'right', 16)

    notebook = {
        'cells': [
            {
                'cell_type': 'markdown',
                'metadata': {},
                'source': [
                    '# Rule 30 left-right asymmetry\n',
                    '\n',
                    'This notebook is the slower companion to the new Rule 30 asymmetry card. The question is not whether Rule 30 looks chaotic in general. The question is whether the two flanks of the wedge behave the same once you measure matched diagonals the same depth in from each edge.\n',
                ],
            },
            {
                'cell_type': 'markdown',
                'metadata': {},
                'source': [
                    '## The diagonal coordinates\n',
                    '\n',
                    'If `s(t, x)` is the cell at step `t` and horizontal offset `x` from the seed, then the matched diagonals are\n',
                    '\n',
                    '$$L_n(t) = s(t, -t + n), \\qquad R_n(t) = s(t, t - n).$$\n',
                    '\n',
                    'The depth `n` is the same on both sides. That keeps the left-versus-right comparison honest.\n',
                ],
            },
            {
                'cell_type': 'code',
                'execution_count': None,
                'metadata': {},
                'outputs': [],
                'source': [
                    'from python.rule30_left_right_asymmetry import study_asymmetry\n',
                    '\n',
                    f'study = study_asymmetry(steps={study.steps}, max_depth={study.max_depth}, tail_window={study.tail_window}, max_period={study.max_period}, block_size={study.block_size})\n',
                    '[(metric.side, metric.depth, metric.detected_period, round(metric.normalized_block_entropy, 4)) for metric in study.metrics[:8]]\n',
                ],
            },
            {
                'cell_type': 'markdown',
                'metadata': {},
                'source': [
                    '## Main bounded result\n',
                    '\n',
                    f'- short tail periods appear at `{left_periodic}` of the first `{study.max_depth}` left-edge depths but only `{right_periodic}` of the matched right-edge depths\n',
                    f'- mean normalized `{study.block_size}`-bit entropy is `{left_entropy:.3f}` on the left versus `{right_entropy:.3f}` on the right\n',
                    f'- mean transition rate is `{left_transition:.3f}` on the left versus `{right_transition:.3f}` on the right\n',
                    '\n',
                    'That is a real directional split, not just a prettier rendering of the same wedge.\n',
                ],
            },
            {
                'cell_type': 'markdown',
                'metadata': {},
                'source': [
                    '## Figure\n',
                    '\n',
                    '![Rule 30 left-right asymmetry](../art/rule30-left-right-asymmetry.png)\n',
                    '\n',
                    'The top-right panel is the sharpest one. Small exact tail periods survive repeatedly on the left. The matched right diagonals mostly refuse them inside the same search window.\n',
                ],
            },
            {
                'cell_type': 'code',
                'execution_count': None,
                'metadata': {},
                'outputs': [],
                'source': [
                    'selected = [metric for metric in study.metrics if metric.depth in (8, 16)]\n',
                    '[(metric.side, metric.depth, metric.detected_period, round(metric.transition_rate, 4), round(metric.normalized_block_entropy, 4)) for metric in selected]\n',
                ],
            },
            {
                'cell_type': 'markdown',
                'metadata': {},
                'source': [
                    '## Two concrete depths\n',
                    '\n',
                    f'- at depth `8`, the left diagonal shows period `{left_depth8.detected_period}` with entropy `{left_depth8.normalized_block_entropy:.3f}`, while the right diagonal shows period `{right_depth8.detected_period}` with entropy `{right_depth8.normalized_block_entropy:.3f}`\n',
                    f'- at depth `16`, the left diagonal shows period `{left_depth16.detected_period}` with entropy `{left_depth16.normalized_block_entropy:.3f}`, while the right diagonal shows period `{right_depth16.detected_period}` with entropy `{right_depth16.normalized_block_entropy:.3f}`\n',
                    '\n',
                    'Those are not isolated cherry-picked oddities. They sit inside the broader pattern shown by the depth sweep.\n',
                ],
            },
            {
                'cell_type': 'markdown',
                'metadata': {},
                'source': [
                    '## Caveats\n',
                    '\n',
                    '1. This is a bounded finite-window audit, not a proof packet about infinite asymptotics.\n',
                    '2. The exact-period search only looks up to the configured limit, so `0` means "none found here," not a proof of aperiodicity.\n',
                    '3. Entropy is finite-block entropy on diagonal sequences, so it is a local complexity read rather than a universal chaos score.\n',
                ],
            },
        ],
        'metadata': {
            'kernelspec': {'display_name': 'Python 3', 'language': 'python', 'name': 'python3'},
            'language_info': {'name': 'python', 'version': '3.11'},
        },
        'nbformat': 4,
        'nbformat_minor': 5,
    }
    path.write_text(json.dumps(notebook, indent=2) + '\n')
    return path


def main(argv: Sequence[str] | None = None) -> int:
    argv = list(argv or sys.argv[1:])
    svg_path = Path(argv[0]) if len(argv) >= 1 else DEFAULT_SVG_PATH
    csv_path = Path(argv[1]) if len(argv) >= 2 else DEFAULT_CSV_PATH
    notebook_path = Path(argv[2]) if len(argv) >= 3 else DEFAULT_NOTEBOOK_PATH

    study = study_asymmetry()
    svg_path.parent.mkdir(parents=True, exist_ok=True)
    svg_path.write_text(render_svg(study))
    write_csv(study, csv_path)
    write_notebook(study, notebook_path)
    export_png(svg_path)
    print(f'wrote {svg_path}')
    print(f'wrote {csv_path}')
    print(f'wrote {notebook_path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
