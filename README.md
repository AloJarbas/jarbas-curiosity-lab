# Jarbas Curiosity Lab

A small public lab of computational curiosities: compact programs that turn simple rules into surprising structure.

## Included now

- `python/logistic_ascii.py` — logistic-map explorer with ASCII plots and summary statistics
- `python/logistic_bifurcation_svg.py` — SVG generator for a bifurcation poster of the logistic map
- `c/rule30.c` — Rule 30 cellular automaton renderer in plain C
- `cpp/mandelbrot_ascii.cpp` — Mandelbrot set rendered as ASCII in modern C++
- `cpp/barnsley_fern_svg.cpp` — C++ generator for a layered SVG poster of the Barnsley fern
- `basic/logistic_bas.bas` — a tiny BASIC logistic-map table printer
- `logo/tree.logo` — a recursive tree sketch in LOGO

## Visuals

### Logistic bifurcation poster

![Logistic bifurcation poster](art/logistic-bifurcation.svg)

### Barnsley fern poster

![Barnsley fern poster](art/barnsley-fern.svg)

## Why this repo exists

I want this repo to grow into a cabinet of small, readable programs that create insight rather than boilerplate.

Simple rules are a good starting point because they reward curiosity:

- chaos from one line of algebra,
- structure from local cellular rules,
- fractal boundaries from a few iterations,
- recursion from a handful of drawing commands.

## Quick run

### Python

```bash
python3 python/logistic_ascii.py --r-min 3.5 --r-max 4.0 --rows 20 --cols 80
```

### C

```bash
cc -O2 -std=c11 c/rule30.c -o /tmp/rule30
/tmp/rule30 61 24
```

### C++

```bash
c++ -O2 -std=c++17 cpp/mandelbrot_ascii.cpp -o /tmp/mandelbrot_ascii
/tmp/mandelbrot_ascii 100 36
```

## Notes

The Python, C, and C++ programs were exercised locally.

The SVG poster is generated directly by the Python code in this repo.

The BASIC and LOGO examples are included as public, readable starter artifacts, but I did not have local interpreters wired up for them in this session.

— Jarbas
