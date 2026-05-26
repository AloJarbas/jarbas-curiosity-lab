#!/usr/bin/env python3
from __future__ import annotations

import csv
import math
import random
import shutil
import subprocess
import tempfile
from dataclasses import dataclass
from html import escape
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SVG_OUT = REPO / 'art' / 'random-quadratic-attractor-screening.svg'
PNG_OUT = REPO / 'art' / 'random-quadratic-attractor-screening.png'
CSV_OUT = REPO / 'art' / 'random-quadratic-attractor-screening.csv'

WIDTH = 1700
HEIGHT = 1520
SEED = 20260526
MAX_CANDIDATES = 400
PARAM_BOUND = 1.2
WARMUP_STEPS = 1200
SAMPLE_STEPS = 4500
DIVERGENCE_LIMIT = 50.0
GRID_SIZE = 40
MIN_SPAN = 0.05
MIN_OCCUPANCY = 0.03
MIN_LYAPUNOV = 0.02
SELECT_COUNT = 6
COLOR_PALETTE = ['#7dd3fc', '#67e8f9', '#c4b5fd', '#f9a8d4', '#86efac', '#fdba74']


@dataclass
class CandidateResult:
    candidate_id: int
    status: str
    score: float
    lambda_max: float
    occupancy: float
    span_x: float
    span_y: float
    aspect_ratio: float
    min_x: float
    max_x: float
    min_y: float
    max_y: float
    params: tuple[float, ...]
    points: list[tuple[float, float]]
    selected_rank: int | None = None


def step(params: tuple[float, ...], x: float, y: float) -> tuple[float, float]:
    a0, a1, a2, a3, a4, a5, b0, b1, b2, b3, b4, b5 = params
    xx = x * x
    yy = y * y
    xy = x * y
    return (
        a0 + a1 * x + a2 * xx + a3 * xy + a4 * y + a5 * yy,
        b0 + b1 * x + b2 * xx + b3 * xy + b4 * y + b5 * yy,
    )


def jacobian(params: tuple[float, ...], x: float, y: float) -> tuple[float, float, float, float]:
    a0, a1, a2, a3, a4, a5, b0, b1, b2, b3, b4, b5 = params
    return (
        a1 + 2.0 * a2 * x + a3 * y,
        a3 * x + a4 + 2.0 * a5 * y,
        b1 + 2.0 * b2 * x + b3 * y,
        b3 * x + b4 + 2.0 * b5 * y,
    )


def analyze_candidate(candidate_id: int, params: tuple[float, ...]) -> CandidateResult:
    x = 0.1
    y = 0.0
    for _ in range(WARMUP_STEPS + SAMPLE_STEPS):
        x, y = step(params, x, y)
        if not math.isfinite(x + y) or max(abs(x), abs(y)) > DIVERGENCE_LIMIT:
            return CandidateResult(
                candidate_id=candidate_id,
                status='diverged',
                score=0.0,
                lambda_max=float('nan'),
                occupancy=0.0,
                span_x=0.0,
                span_y=0.0,
                aspect_ratio=0.0,
                min_x=0.0,
                max_x=0.0,
                min_y=0.0,
                max_y=0.0,
                params=params,
                points=[],
            )

    x = 0.1
    y = 0.0
    vx = 1.0
    vy = 0.0
    points: list[tuple[float, float]] = []
    log_sum = 0.0

    for step_index in range(WARMUP_STEPS + SAMPLE_STEPS):
        x, y = step(params, x, y)
        if step_index < WARMUP_STEPS:
            continue
        points.append((x, y))
        j00, j01, j10, j11 = jacobian(params, x, y)
        wx = j00 * vx + j01 * vy
        wy = j10 * vx + j11 * vy
        norm = math.hypot(wx, wy)
        if norm < 1e-12:
            wx = 1.0
            wy = 0.0
            norm = 1e-12
        else:
            wx /= norm
            wy /= norm
        vx = wx
        vy = wy
        log_sum += math.log(norm)

    xs = [point[0] for point in points]
    ys = [point[1] for point in points]
    min_x = min(xs)
    max_x = max(xs)
    min_y = min(ys)
    max_y = max(ys)
    span_x = max_x - min_x
    span_y = max_y - min_y
    lambda_max = log_sum / SAMPLE_STEPS
    span_max = max(span_x, span_y)

    if span_max < MIN_SPAN:
        return CandidateResult(
            candidate_id=candidate_id,
            status='collapsed',
            score=0.0,
            lambda_max=lambda_max,
            occupancy=0.0,
            span_x=span_x,
            span_y=span_y,
            aspect_ratio=0.0,
            min_x=min_x,
            max_x=max_x,
            min_y=min_y,
            max_y=max_y,
            params=params,
            points=[],
        )

    occupancy_cells: set[tuple[int, int]] = set()
    for px, py in points:
        gx = min(
            GRID_SIZE - 1,
            max(0, int((px - min_x) / (span_x if span_x > 1e-12 else 1.0) * (GRID_SIZE - 1))),
        )
        gy = min(
            GRID_SIZE - 1,
            max(0, int((py - min_y) / (span_y if span_y > 1e-12 else 1.0) * (GRID_SIZE - 1))),
        )
        occupancy_cells.add((gx, gy))
    occupancy = len(occupancy_cells) / (GRID_SIZE * GRID_SIZE)

    aspect_ratio = min(span_x, span_y) / max(span_x, span_y) if max(span_x, span_y) > 1e-12 else 0.0

    if occupancy < MIN_OCCUPANCY:
        return CandidateResult(
            candidate_id=candidate_id,
            status='collapsed',
            score=0.0,
            lambda_max=lambda_max,
            occupancy=occupancy,
            span_x=span_x,
            span_y=span_y,
            aspect_ratio=aspect_ratio,
            min_x=min_x,
            max_x=max_x,
            min_y=min_y,
            max_y=max_y,
            params=params,
            points=[],
        )

    if lambda_max <= MIN_LYAPUNOV:
        return CandidateResult(
            candidate_id=candidate_id,
            status='nonchaotic',
            score=0.0,
            lambda_max=lambda_max,
            occupancy=occupancy,
            span_x=span_x,
            span_y=span_y,
            aspect_ratio=aspect_ratio,
            min_x=min_x,
            max_x=max_x,
            min_y=min_y,
            max_y=max_y,
            params=params,
            points=[],
        )

    return CandidateResult(
        candidate_id=candidate_id,
        status='accepted',
        score=lambda_max * occupancy,
        lambda_max=lambda_max,
        occupancy=occupancy,
        span_x=span_x,
        span_y=span_y,
        aspect_ratio=aspect_ratio,
        min_x=min_x,
        max_x=max_x,
        min_y=min_y,
        max_y=max_y,
        params=params,
        points=points,
    )


def scan_candidates() -> list[CandidateResult]:
    rng = random.Random(SEED)
    results: list[CandidateResult] = []
    for candidate_id in range(1, MAX_CANDIDATES + 1):
        params = tuple(rng.uniform(-PARAM_BOUND, PARAM_BOUND) for _ in range(12))
        results.append(analyze_candidate(candidate_id, params))
    accepted = [result for result in results if result.status == 'accepted']
    accepted.sort(key=lambda item: item.score, reverse=True)
    for rank, result in enumerate(accepted[:SELECT_COUNT], start=1):
        result.selected_rank = rank
    return results


def write_csv(results: list[CandidateResult], path: Path = CSV_OUT) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', newline='') as handle:
        writer = csv.writer(handle, lineterminator='\n')
        writer.writerow([
            'candidate_id',
            'status',
            'selected_rank',
            'score',
            'lambda_max',
            'occupancy',
            'span_x',
            'span_y',
            'aspect_ratio',
            'min_x',
            'max_x',
            'min_y',
            'max_y',
            'a0',
            'a1',
            'a2',
            'a3',
            'a4',
            'a5',
            'b0',
            'b1',
            'b2',
            'b3',
            'b4',
            'b5',
        ])
        for result in results:
            writer.writerow([
                result.candidate_id,
                result.status,
                '' if result.selected_rank is None else result.selected_rank,
                f'{result.score:.6f}',
                '' if math.isnan(result.lambda_max) else f'{result.lambda_max:.6f}',
                f'{result.occupancy:.6f}',
                f'{result.span_x:.6f}',
                f'{result.span_y:.6f}',
                f'{result.aspect_ratio:.6f}',
                f'{result.min_x:.6f}',
                f'{result.max_x:.6f}',
                f'{result.min_y:.6f}',
                f'{result.max_y:.6f}',
                *[f'{value:.6f}' for value in result.params],
            ])


def wrap(text: str, width: int) -> list[str]:
    words = text.split()
    if not words:
        return ['']
    lines = [words[0]]
    for word in words[1:]:
        candidate = f'{lines[-1]} {word}'
        if len(candidate) <= width:
            lines[-1] = candidate
        else:
            lines.append(word)
    return lines


def rect(x: float, y: float, width: float, height: float, fill: str, *, stroke: str = '#334155', rx: float = 18.0, stroke_width: float = 2.0, opacity: float = 1.0) -> str:
    return (
        f'<rect x="{x:.1f}" y="{y:.1f}" width="{width:.1f}" height="{height:.1f}" '
        f'rx="{rx:.1f}" fill="{fill}" opacity="{opacity}" stroke="{stroke}" stroke-width="{stroke_width:.1f}"/>'
    )


def line(x1: float, y1: float, x2: float, y2: float, stroke: str, *, width: float = 2.0, opacity: float = 1.0, dash: str | None = None) -> str:
    dash_attr = f' stroke-dasharray="{dash}"' if dash else ''
    return (
        f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" '
        f'stroke="{stroke}" stroke-width="{width:.1f}" opacity="{opacity}" stroke-linecap="round"{dash_attr}/>'
    )


def text(x: float, y: float, body: str, cls: str, *, anchor: str = 'start') -> str:
    return f'<text x="{x:.1f}" y="{y:.1f}" class="{cls}" text-anchor="{anchor}">{escape(body)}</text>'


def block(x: float, y: float, lines: list[str], cls: str, *, anchor: str = 'start', line_step: int = 20) -> str:
    tspans = []
    for index, line_value in enumerate(lines):
        dy = 0 if index == 0 else line_step
        tspans.append(f'<tspan x="{x:.1f}" dy="{dy}">{escape(line_value)}</tspan>')
    return f'<text x="{x:.1f}" y="{y:.1f}" class="{cls}" text-anchor="{anchor}">{"".join(tspans)}</text>'


def export_png(svg_path: Path, png_path: Path) -> bool:
    brave_candidates = [
        Path('/Applications/Brave Browser.app/Contents/MacOS/Brave Browser'),
        Path(shutil.which('brave-browser') or ''),
    ]
    browser = next((candidate for candidate in brave_candidates if candidate and candidate.exists()), None)
    if browser is not None:
        command = [
            str(browser),
            '--headless',
            '--disable-gpu',
            '--hide-scrollbars',
            '--run-all-compositor-stages-before-draw',
            '--virtual-time-budget=1000',
            f'--screenshot={png_path.resolve()}',
            f'--window-size={WIDTH},{HEIGHT}',
            svg_path.resolve().as_uri(),
        ]
        try:
            subprocess.run(command, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
        except subprocess.TimeoutExpired:
            if not png_path.exists():
                raise
        sips = shutil.which('sips')
        if sips is not None:
            subprocess.run([sips, '--setProperty', 'dpiWidth', '300', '--setProperty', 'dpiHeight', '300', str(png_path)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return True
    qlmanage = shutil.which('qlmanage')
    if qlmanage is None:
        return False
    with tempfile.TemporaryDirectory() as tmpdir:
        subprocess.run([qlmanage, '-t', '-s', '2200', '-o', tmpdir, str(svg_path.resolve())], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        generated = Path(tmpdir) / f'{svg_path.name}.png'
        if not generated.exists():
            raise FileNotFoundError(f'Quick Look did not generate {generated}')
        png_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(generated, png_path)
    sips = shutil.which('sips')
    if sips is not None:
        subprocess.run([sips, '--setProperty', 'dpiWidth', '300', '--setProperty', 'dpiHeight', '300', str(png_path)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return True


def draw_orbit(result: CandidateResult, x: float, y: float, width: float, height: float, color: str) -> str:
    pad = 22.0
    plot_x = x + pad
    plot_y = y + 56.0
    plot_width = width - 2.0 * pad
    plot_height = height - 82.0
    span_x = max(result.span_x, 1e-9)
    span_y = max(result.span_y, 1e-9)
    scale = min(plot_width / span_x, plot_height / span_y)
    offset_x = plot_x + (plot_width - span_x * scale) / 2.0
    offset_y = plot_y + (plot_height - span_y * scale) / 2.0
    commands = []
    for px, py in result.points:
        sx = offset_x + (px - result.min_x) * scale
        sy = offset_y + plot_height - (py - result.min_y) * scale
        commands.append(f'M {sx:.2f} {sy:.2f} h 0.01')
    return '\n'.join([
        rect(x, y, width, height, '#0f1724', stroke='#32445a', rx=22.0),
        text(x + 18, y + 30, f'candidate {result.candidate_id}', 'tile_title'),
        text(x + width - 18, y + 30, f'pick {result.selected_rank}', 'tile_meta', anchor='end'),
        text(x + 18, y + height - 20, f'λ≈{result.lambda_max:.3f}   occ={result.occupancy:.3f}   aspect={result.aspect_ratio:.3f}', 'tile_meta'),
        f'<clipPath id="clip-{result.candidate_id}">{rect(plot_x, plot_y, plot_width, plot_height, '#000000', stroke='#000000', rx=18.0)}</clipPath>',
        f'<g clip-path="url(#clip-{result.candidate_id})"><path d="{" ".join(commands)}" stroke="{color}" stroke-opacity="0.50" stroke-width="0.95" stroke-linecap="round" fill="none"/></g>',
        rect(plot_x, plot_y, plot_width, plot_height, 'none', stroke='#243447', rx=18.0, stroke_width=1.5),
    ])


def build_svg(results: list[CandidateResult]) -> str:
    selected = sorted([result for result in results if result.selected_rank is not None], key=lambda item: item.selected_rank or 0)
    counts: dict[str, int] = {'diverged': 0, 'collapsed': 0, 'nonchaotic': 0, 'accepted': 0}
    for result in results:
        counts[result.status] = counts.get(result.status, 0) + 1
    accepted = [result for result in results if result.status == 'accepted']
    median_lambda = sorted(result.lambda_max for result in accepted)[len(accepted) // 2]
    median_occupancy = sorted(result.occupancy for result in accepted)[len(accepted) // 2]

    bars = [
        ('diverged', counts['diverged'], '#475569'),
        ('collapsed', counts['collapsed'], '#f59e0b'),
        ('non-chaotic', counts['nonchaotic'], '#fb7185'),
        ('accepted', counts['accepted'], '#22c55e'),
    ]
    max_count = max(value for _, value, _ in bars)

    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {WIDTH} {HEIGHT}" width="{WIDTH}" height="{HEIGHT}">',
        '<defs>',
        '  <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">',
        '    <stop offset="0%" stop-color="#071019"/>',
        '    <stop offset="100%" stop-color="#0f1c2b"/>',
        '  </linearGradient>',
        '  <style>',
        '    .title { font: 700 40px Helvetica, Arial, sans-serif; fill: #e2e8f0; }',
        '    .subtitle { font: 500 18px Helvetica, Arial, sans-serif; fill: #b7c8d8; }',
        '    .section { font: 700 22px Helvetica, Arial, sans-serif; fill: #e2e8f0; }',
        '    .body { font: 500 16px Helvetica, Arial, sans-serif; fill: #cbd5e1; }',
        '    .small { font: 500 14px Helvetica, Arial, sans-serif; fill: #cbd5e1; }',
        '    .tiny { font: 500 13px Helvetica, Arial, sans-serif; fill: #94a3b8; }',
        '    .tile_title { font: 700 16px Helvetica, Arial, sans-serif; fill: #e2e8f0; }',
        '    .tile_meta { font: 500 13px Helvetica, Arial, sans-serif; fill: #cbd5e1; }',
        '  </style>',
        '</defs>',
        f'<rect width="{WIDTH}" height="{HEIGHT}" fill="url(#bg)"/>',
        rect(24, 24, WIDTH - 48, HEIGHT - 48, '#0b1522', stroke='#1f3042', rx=26.0),
        text(64, 72, 'Random quadratic attractor screening', 'title'),
        block(64, 108, wrap('Instead of shipping one more chaos poster, this lane samples 400 random quadratic maps, rejects the ones that blow up or collapse, and keeps the survivors that still show a positive Lyapunov-style proxy plus enough occupied area to read as real structure.', 122), 'subtitle', line_step=24),
        text(64, 170, 'xₙ₊₁ = a₀ + a₁x + a₂x² + a₃xy + a₄y + a₅y²      yₙ₊₁ = b₀ + b₁x + b₂x² + b₃xy + b₄y + b₅y²', 'small'),
        rect(52, 204, 1060, 1268, '#101a28', stroke='#2b3d51', rx=24.0),
        rect(1138, 204, 510, 1268, '#101a28', stroke='#2b3d51', rx=24.0),
        text(84, 238, 'selected survivors', 'section'),
        text(1170, 238, 'screening readout', 'section'),
    ]

    tile_width = 500.0
    tile_height = 360.0
    tile_positions = [
        (84.0, 272.0),
        (600.0, 272.0),
        (84.0, 652.0),
        (600.0, 652.0),
        (84.0, 1032.0),
        (600.0, 1032.0),
    ]
    for result, (tile_x, tile_y), color in zip(selected, tile_positions, COLOR_PALETTE):
        svg.append(draw_orbit(result, tile_x, tile_y, tile_width, tile_height, color))

    svg.extend([
        block(1170, 282, wrap('Most random coefficient sets are junk. That is the point. The artifact is the filter, not just the six survivors.', 42), 'body', line_step=22),
        text(1170, 366, 'pipeline counts', 'section'),
    ])

    bar_left = 1170.0
    bar_width = 390.0
    bar_top = 394.0
    bar_height = 28.0
    row_gap = 64.0
    for index, (label, value, color) in enumerate(bars):
        y = bar_top + index * row_gap
        filled = bar_width * value / max_count
        svg.extend([
            text(bar_left, y - 10, label, 'small'),
            text(bar_left + bar_width + 30, y + 10, str(value), 'small', anchor='end'),
            rect(bar_left, y, bar_width, bar_height, '#0f1724', stroke='#223246', rx=14.0, stroke_width=1.4),
            rect(bar_left, y, filled, bar_height, color, stroke=color, rx=14.0, stroke_width=0.0),
        ])

    summary_y = 690.0
    svg.extend([
        text(1170, summary_y, 'selection rule', 'section'),
        block(1170, summary_y + 34, [
            '1. reject divergence: |x| or |y| > 50',
            '2. reject tiny survivors: span < 0.05 or occupancy < 3%',
            '3. reject calm runs: λ <= 0.02',
            '4. rank survivors by λ × occupancy; keep top six',
        ], 'body', line_step=24),
        text(1170, 884, 'accepted-pool medians', 'section'),
        block(1170, 918, [
            f'accepted pool: {counts["accepted"]} / {MAX_CANDIDATES}',
            f'median λ among survivors: {median_lambda:.3f}',
            f'median occupancy among survivors: {median_occupancy:.3f}',
            f'seed: {SEED}',
        ], 'body', line_step=24),
        text(1170, 1038, 'why this earns a slot', 'section'),
        block(1170, 1072, wrap('The repo already had fixed artifacts: one fern, one zoom, one comparison card. This lane adds a different behavior. It searches, rejects most candidates, and makes the surviving structures legible without pretending the filter is a proof of chaos.', 42), 'body', line_step=22),
        text(1170, 1264, 'caveat', 'section'),
        block(1170, 1298, wrap('A positive finite-step Lyapunov estimate is a useful screen, not a theorem. Some accepted maps are still best read as bounded interesting candidates, not certified strange attractors in the strongest mathematical sense.', 42), 'tiny', line_step=20),
        text(64, 1454, 'Generated by python/random_quadratic_attractor_screening.py • CSV sidecar includes all 400 candidates, scores, and coefficients.', 'tiny'),
    ])

    svg.append('</svg>')
    return '\n'.join(svg)


def write_svg(results: list[CandidateResult], path: Path = SVG_OUT) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(build_svg(results))


def main() -> None:
    results = scan_candidates()
    write_csv(results, CSV_OUT)
    write_svg(results, SVG_OUT)
    export_png(SVG_OUT, PNG_OUT)
    print(CSV_OUT)
    print(SVG_OUT)
    print(PNG_OUT)


if __name__ == '__main__':
    main()
