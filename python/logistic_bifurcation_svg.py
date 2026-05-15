#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

WIDTH = 1520
HEIGHT = 980
LEFT_MARGIN = 116
RIGHT_MARGIN = 120
TOP_MARGIN = 128
BOTTOM_MARGIN = 112
R_MIN = 2.8
R_MAX = 4.0
X_MIN = 0.0
X_MAX = 1.0
WARMUP = 700
SAMPLES = 1400
KEEP = 90
OUT = Path(__file__).resolve().parents[1] / 'art' / 'logistic-bifurcation.svg'


def sx(r: float) -> float:
    return LEFT_MARGIN + (r - R_MIN) / (R_MAX - R_MIN) * (WIDTH - LEFT_MARGIN - RIGHT_MARGIN)


def sy(x: float) -> float:
    return HEIGHT - BOTTOM_MARGIN - (x - X_MIN) / (X_MAX - X_MIN) * (HEIGHT - TOP_MARGIN - BOTTOM_MARGIN)


def logistic(r: float, x: float) -> float:
    return r * x * (1.0 - x)


def build_svg() -> str:
    parts: list[str] = []
    parts.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {WIDTH} {HEIGHT}" width="{WIDTH}" height="{HEIGHT}">')
    parts.append('<defs>')
    parts.append('<linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">')
    parts.append('<stop offset="0%" stop-color="#040814"/>')
    parts.append('<stop offset="55%" stop-color="#0a1630"/>')
    parts.append('<stop offset="100%" stop-color="#13091f"/>')
    parts.append('</linearGradient>')
    parts.append('<radialGradient id="glow" cx="50%" cy="48%" r="60%">')
    parts.append('<stop offset="0%" stop-color="#5eead4" stop-opacity="0.22"/>')
    parts.append('<stop offset="45%" stop-color="#60a5fa" stop-opacity="0.10"/>')
    parts.append('<stop offset="100%" stop-color="#000000" stop-opacity="0"/>')
    parts.append('</radialGradient>')
    parts.append(f'<clipPath id="plot-clip"><rect x="{LEFT_MARGIN}" y="{TOP_MARGIN}" width="{WIDTH - LEFT_MARGIN - RIGHT_MARGIN}" height="{HEIGHT - TOP_MARGIN - BOTTOM_MARGIN}" rx="18"/></clipPath>')
    parts.append('</defs>')
    parts.append(f'<rect width="{WIDTH}" height="{HEIGHT}" fill="url(#bg)"/>')
    parts.append(f'<rect width="{WIDTH}" height="{HEIGHT}" fill="url(#glow)"/>')

    for frac, label in [(3.0, '3.0'), (3.2, '3.2'), (3.4, '3.4'), (3.6, '3.6'), (3.8, '3.8'), (4.0, '4.0')]:
        x = sx(frac)
        parts.append(f'<line x1="{x:.2f}" y1="{TOP_MARGIN}" x2="{x:.2f}" y2="{HEIGHT - BOTTOM_MARGIN}" stroke="#9fb3ff" stroke-opacity="0.16" stroke-width="1"/>')
        parts.append(f'<text x="{x:.2f}" y="{HEIGHT - 52}" fill="#b9c6ff" font-size="20" text-anchor="middle" font-family="Helvetica, Arial, sans-serif">{label}</text>')
    for frac, label in [(0.0, '0.0'), (0.25, '0.25'), (0.5, '0.5'), (0.75, '0.75'), (1.0, '1.0')]:
        y = sy(frac)
        parts.append(f'<line x1="{LEFT_MARGIN}" y1="{y:.2f}" x2="{WIDTH - RIGHT_MARGIN}" y2="{y:.2f}" stroke="#9fb3ff" stroke-opacity="0.12" stroke-width="1"/>')
        parts.append(f'<text x="54" y="{y + 6:.2f}" fill="#b9c6ff" font-size="20" text-anchor="start" font-family="Helvetica, Arial, sans-serif">{label}</text>')

    parts.append(f'<rect x="{LEFT_MARGIN}" y="{TOP_MARGIN}" width="{WIDTH - LEFT_MARGIN - RIGHT_MARGIN}" height="{HEIGHT - TOP_MARGIN - BOTTOM_MARGIN}" fill="none" stroke="#c7d2fe" stroke-opacity="0.35" stroke-width="1.4" rx="18"/>')
    parts.append('<text x="90" y="48" fill="#e2e8f0" font-size="36" font-family="Helvetica, Arial, sans-serif" font-weight="700">Logistic map bifurcation diagram</text>')
    parts.append('<text x="90" y="78" fill="#94a3b8" font-size="18" font-family="Helvetica, Arial, sans-serif">Simple nonlinear feedback, elaborate structure.</text>')
    parts.append('<text x="90" y="104" fill="#94a3b8" font-size="18" font-family="Helvetica, Arial, sans-serif">Generated directly from xₙ₊₁ = r xₙ (1 - xₙ).</text>')
    parts.append(f'<text x="{WIDTH / 2:.2f}" y="{HEIGHT - 22}" fill="#94a3b8" font-size="20" text-anchor="middle" font-family="Helvetica, Arial, sans-serif">growth parameter r</text>')
    parts.append(f'<text x="30" y="{HEIGHT / 2:.2f}" fill="#94a3b8" font-size="20" text-anchor="middle" transform="rotate(-90 30 {HEIGHT / 2:.2f})" font-family="Helvetica, Arial, sans-serif">long-run population x</text>')

    dot_paths = {
        '#7dd3fc': [],
        '#67e8f9': [],
        '#c4b5fd': [],
    }
    for i in range(SAMPLES):
        r = R_MIN + (R_MAX - R_MIN) * i / (SAMPLES - 1)
        x = 0.31
        for _ in range(WARMUP):
            x = logistic(r, x)
        color = '#7dd3fc' if r < 3.55 else ('#67e8f9' if r < 3.86 else '#c4b5fd')
        for _ in range(KEEP):
            x = logistic(r, x)
            dot_paths[color].append(f'M {sx(r):.2f} {sy(x):.2f} h 0.01')
    parts.append('<g clip-path="url(#plot-clip)">')
    for color, commands in dot_paths.items():
        parts.append(f'<path d="{" ".join(commands)}" stroke="{color}" stroke-opacity="0.42" stroke-width="0.85" stroke-linecap="round" fill="none"/>')
    parts.append('</g>')
    parts.append('</svg>')
    return ''.join(parts)


def main() -> None:
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(build_svg())
    print(OUT)


if __name__ == '__main__':
    main()
