#!/usr/bin/env python3
from __future__ import annotations

import math
import statistics
from html import escape
from pathlib import Path

WIDTH = 1320
HEIGHT = 800
BG = '#050302'
OUT = Path(__file__).resolve().parents[1] / 'art' / 'phyllotaxis-angle-comparison.svg'
PANEL_W = 380
PANEL_H = 560
PANEL_Y = 158
LEFT_MARGIN = 65
GAP = 25
SEEDS = 1400
MAX_RADIUS = 128.0
CENTER_Y = PANEL_Y + 272
SECTOR_COUNT = 72

PANELS = [
    ('3/8 turn · 135°', 135.0, 'Rational turn locks into visible spokes'),
    ('Golden angle · 137.51°', 180.0 * (3.0 - math.sqrt(5.0)), 'Irrational turn keeps reusing the disk more evenly'),
    ('2/5 turn · 144°', 144.0, 'Another nearby rational turn, with a different spoke family'),
]


def clamp(x: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, x))


def hsl(h: float, s: float, l: float) -> str:
    return f'hsl({h:.2f} {s:.2f}% {l:.2f}%)'


def wrap_text(text: str, max_chars: int) -> list[str]:
    words = text.split()
    lines: list[str] = []
    current = words[0]
    for word in words[1:]:
        proposal = f'{current} {word}'
        if len(proposal) <= max_chars:
            current = proposal
        else:
            lines.append(current)
            current = word
    lines.append(current)
    return lines


def text_block(x: float, y: float, lines: list[str], *, size: int, fill: str, anchor: str = 'middle', weight: str = 'normal', line_step: int | None = None) -> str:
    if line_step is None:
        line_step = int(size * 1.25)
    tspans = []
    for idx, line in enumerate(lines):
        dy = 0 if idx == 0 else line_step
        tspans.append(f'<tspan x="{x:.2f}" dy="{dy}">{escape(line)}</tspan>')
    return (
        f'<text x="{x:.2f}" y="{y:.2f}" fill="{fill}" font-size="{size}" text-anchor="{anchor}" '
        f'font-family="Helvetica, Arial, sans-serif" font-weight="{weight}">' + ''.join(tspans) + '</text>'
    )


def seed_points(angle_deg: float) -> list[tuple[float, float, float, str]]:
    angle = math.radians(angle_deg)
    scale = MAX_RADIUS / math.sqrt(SEEDS)
    pts = []
    for i in range(1, SEEDS + 1):
        radius = scale * math.sqrt(i)
        theta = i * angle
        t = i / SEEDS
        x = radius * math.cos(theta)
        y = radius * math.sin(theta)
        dot = clamp(3.4 - 2.1 * t, 0.85, 3.4)
        hue = 52.0 - 22.0 * t
        sat = 92.0 - 24.0 * t
        light = 74.0 - 40.0 * (t ** 0.88)
        pts.append((x, y, dot, hsl(hue, sat, light)))
    return pts


def sector_cv(angle_deg: float, sectors: int = SECTOR_COUNT) -> float:
    angle = math.radians(angle_deg)
    counts = [0] * sectors
    for i in range(1, SEEDS + 1):
        theta = (i * angle) % (2.0 * math.pi)
        idx = min(sectors - 1, int(theta / (2.0 * math.pi) * sectors))
        counts[idx] += 1
    mean = sum(counts) / sectors
    return statistics.pstdev(counts) / mean if mean else 0.0


def panel_svg(index: int, title: str, subtitle: str, angle_deg: float) -> str:
    panel_x = LEFT_MARGIN + index * (PANEL_W + GAP)
    cx = panel_x + PANEL_W / 2
    cv = sector_cv(angle_deg)
    title_lines = wrap_text(title, 22)
    subtitle_lines = wrap_text(subtitle, 38)
    footer_y = PANEL_Y + PANEL_H - 56
    parts: list[str] = []
    parts.append(f'<g transform="translate({panel_x} 0)">')
    parts.append(f'<rect x="0" y="{PANEL_Y}" width="{PANEL_W}" height="{PANEL_H}" rx="28" fill="#0f0a07" fill-opacity="0.92" stroke="#f59e0b" stroke-opacity="0.18"/>')
    parts.append(text_block(PANEL_W / 2, PANEL_Y + 36, title_lines, size=24, fill='#fde68a', weight='700'))
    parts.append(text_block(PANEL_W / 2, PANEL_Y + 72 + (len(title_lines) - 1) * 28, subtitle_lines, size=15, fill='#fcd34d'))
    parts.append(f'<circle cx="{PANEL_W/2:.2f}" cy="{CENTER_Y:.2f}" r="{MAX_RADIUS + 18:.2f}" fill="#facc15" fill-opacity="0.05"/>')
    for x, y, dot, color in seed_points(angle_deg):
        parts.append(
            f'<circle cx="{cx + x - panel_x:.2f}" cy="{CENTER_Y + y:.2f}" r="{dot:.2f}" fill="{color}" fill-opacity="0.97"/>'
        )
    parts.append(text_block(PANEL_W / 2, footer_y, [f'θ = {angle_deg:.6f}° · r ∝ √n · n = {SEEDS}'], size=14, fill='#a3a3a3'))
    parts.append(text_block(PANEL_W / 2, footer_y + 24, [f'{SECTOR_COUNT}-sector occupancy CV = {cv:.3f}'], size=13, fill='#94a3b8'))
    parts.append('</g>')
    return ''.join(parts)


def build_svg() -> str:
    parts: list[str] = []
    parts.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {WIDTH} {HEIGHT}" width="{WIDTH}" height="{HEIGHT}">')
    parts.append('<defs>')
    parts.append('<linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">')
    parts.append('<stop offset="0%" stop-color="#160b07"/>')
    parts.append('<stop offset="55%" stop-color="#090404"/>')
    parts.append('<stop offset="100%" stop-color="#030202"/>')
    parts.append('</linearGradient>')
    parts.append('<radialGradient id="glow" cx="50%" cy="45%" r="64%">')
    parts.append('<stop offset="0%" stop-color="#f59e0b" stop-opacity="0.16"/>')
    parts.append('<stop offset="100%" stop-color="#000000" stop-opacity="0"/>')
    parts.append('</radialGradient>')
    parts.append('</defs>')
    parts.append(f'<rect width="{WIDTH}" height="{HEIGHT}" fill="url(#bg)"/>')
    parts.append(f'<rect width="{WIDTH}" height="{HEIGHT}" fill="url(#glow)"/>')
    parts.append(text_block(70, 54, ['Phyllotaxis angle comparison'], size=36, fill='#fff7ed', anchor='start', weight='700'))
    parts.append(text_block(70, 88, [
        'In the simple Vogel model, nearby rational turns create spokes;',
        'the golden angle spreads the seeds more evenly across the disk.'
    ], size=18, fill='#fed7aa', anchor='start', line_step=22))
    parts.append(text_block(70, 132, [
        'A small sector-occupancy score is included below each panel so the visual story stays quantitative too.'
    ], size=15, fill='#94a3b8', anchor='start'))
    for i, (title, angle_deg, subtitle) in enumerate(PANELS):
        parts.append(panel_svg(i, title, subtitle, angle_deg))
    parts.append('</svg>')
    return ''.join(parts)


def main() -> None:
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(build_svg(), encoding='utf-8')
    print(OUT)


if __name__ == '__main__':
    main()
