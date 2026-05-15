#!/usr/bin/env python3
from __future__ import annotations

import math
import statistics
from pathlib import Path

WIDTH = 1380
HEIGHT = 620
BG = '#050302'
OUT = Path(__file__).resolve().parents[1] / 'art' / 'phyllotaxis-angle-comparison.svg'
PANEL_W = 380
PANEL_H = 430
PANEL_Y = 132
LEFT_MARGIN = 90
GAP = 40
SEEDS = 1400
MAX_RADIUS = 142.0
CENTER_Y = PANEL_Y + PANEL_H / 2 + 24
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
        dot = clamp(3.5 - 2.2 * t, 0.9, 3.5)
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
    parts: list[str] = []
    parts.append(f'<g transform="translate({panel_x} 0)">')
    parts.append(f'<rect x="0" y="{PANEL_Y}" width="{PANEL_W}" height="{PANEL_H}" rx="28" fill="#0f0a07" fill-opacity="0.92" stroke="#f59e0b" stroke-opacity="0.18"/>')
    parts.append(f'<text x="{PANEL_W/2:.2f}" y="70" fill="#fde68a" font-size="24" text-anchor="middle" font-family="Helvetica, Arial, sans-serif" font-weight="700">{title}</text>')
    parts.append(f'<text x="{PANEL_W/2:.2f}" y="98" fill="#fcd34d" font-size="15" text-anchor="middle" font-family="Helvetica, Arial, sans-serif">{subtitle}</text>')
    parts.append(f'<text x="{PANEL_W/2:.2f}" y="542" fill="#a3a3a3" font-size="14" text-anchor="middle" font-family="Helvetica, Arial, sans-serif">θ = {angle_deg:.6f}° · r ∝ √n · n = {SEEDS}</text>')
    parts.append(f'<text x="{PANEL_W/2:.2f}" y="565" fill="#94a3b8" font-size="13" text-anchor="middle" font-family="Helvetica, Arial, sans-serif">{SECTOR_COUNT}-sector occupancy CV = {cv:.3f}</text>')
    parts.append(f'<circle cx="{PANEL_W/2:.2f}" cy="{CENTER_Y:.2f}" r="{MAX_RADIUS + 18:.2f}" fill="#facc15" fill-opacity="0.05"/>')
    for x, y, dot, color in seed_points(angle_deg):
        parts.append(
            f'<circle cx="{cx + x - panel_x:.2f}" cy="{CENTER_Y + y:.2f}" r="{dot:.2f}" fill="{color}" fill-opacity="0.97"/>'
        )
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
    parts.append('<text x="90" y="54" fill="#fff7ed" font-size="36" font-family="Helvetica, Arial, sans-serif" font-weight="700">Phyllotaxis angle comparison</text>')
    parts.append('<text x="90" y="88" fill="#fed7aa" font-size="18" font-family="Helvetica, Arial, sans-serif">In the simple Vogel model, nearby rational turns create spokes; the golden angle spreads the seeds more evenly across the disk.</text>')
    parts.append('<text x="90" y="112" fill="#94a3b8" font-size="15" font-family="Helvetica, Arial, sans-serif">A small sector-occupancy score is included below each panel so the visual story is not only qualitative.</text>')
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
