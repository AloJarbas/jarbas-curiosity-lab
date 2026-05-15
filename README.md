# Jarbas Curiosity Lab

Small code experiments that earn their keep.

No filler, no tutorial sludge, just compact programs that do something worth seeing.

## Included now

- `python/logistic_ascii.py`: logistic-map explorer with ASCII plots and summary statistics
- `python/logistic_bifurcation_svg.py`: SVG generator for a bifurcation poster of the logistic map
- `c/rule30.c`: Rule 30 cellular automaton renderer in plain C
- `c/phyllotaxis_svg.c`: C generator for a sunflower-style phyllotaxis poster from the golden angle
- `python/phyllotaxis_angle_comparison_svg.py`: SVG comparison showing how nearby rational turn angles create spokes while the golden angle stays more evenly packed
- `notebooks/phyllotaxis-angle-comparison.ipynb`: companion notebook explaining the Vogel model, the golden angle, spoke formation under rational turns, and a simple evenness metric
- `cpp/mandelbrot_ascii.cpp`: Mandelbrot set rendered as ASCII in modern C++
- `cpp/mandelbrot_zoom_svg.cpp`: Mandelbrot zoom triptych renderer in modern C++, with three scales and panel statistics
- `cpp/barnsley_fern_svg.cpp`: C++ generator for a layered SVG poster of the Barnsley fern
- `cpp/barnsley_fern_growth_svg.cpp`: C++ growth-study renderer showing how the same fern attractor fills in across six iteration budgets
- `basic/logistic_bas.bas`: a tiny BASIC logistic-map table printer
- `logo/tree.logo`: a recursive tree sketch in LOGO

## Visuals

### Logistic bifurcation poster

![Logistic bifurcation poster](art/logistic-bifurcation.svg)

### Barnsley fern poster

![Barnsley fern poster](art/barnsley-fern.svg)

### Barnsley fern growth study

![Barnsley fern growth study](art/barnsley-fern-growth-stages.svg)

### Mandelbrot zoom triptych

![Mandelbrot zoom triptych](art/mandelbrot-zoom-triptych.svg)

### Phyllotaxis sunflower poster

![Phyllotaxis sunflower poster](art/phyllotaxis-sunflower.svg)

### Phyllotaxis angle comparison

![Phyllotaxis angle comparison](art/phyllotaxis-angle-comparison.svg)

## Why this repo exists

Because a lot of software writing is dead on arrival.

This repo is for the opposite kind of thing: small programs with a pulse. A few lines, a strong idea, and an output that makes you stop for a second.

Simple rules are a good place to start:

- one algebraic rule can go chaotic,
- a tiny local rule can explode into structure,
- a recursion can turn into a convincing organism,
- a handful of affine maps can fake a fern.
- one quadratic map can keep rewarding deeper zooms.

## Quick run

### Python

```bash
python3 python/logistic_ascii.py --r-min 3.5 --r-max 4.0 --rows 20 --cols 80
```

### C

```bash
cc -O2 -std=c11 c/rule30.c -o /tmp/rule30
/tmp/rule30 61 24

cc -O2 -std=c11 c/phyllotaxis_svg.c -lm -o /tmp/phyllotaxis_svg
/tmp/phyllotaxis_svg art/phyllotaxis-sunflower.svg 1800
```

### Python comparison card

```bash
python3 python/phyllotaxis_angle_comparison_svg.py
```

### C++

```bash
c++ -O2 -std=c++17 cpp/mandelbrot_ascii.cpp -o /tmp/mandelbrot_ascii
/tmp/mandelbrot_ascii 100 36

c++ -O2 -std=c++17 cpp/mandelbrot_zoom_svg.cpp -o /tmp/mandelbrot_zoom_svg
/tmp/mandelbrot_zoom_svg art/mandelbrot-zoom-triptych.svg

c++ -O2 -std=c++17 cpp/barnsley_fern_svg.cpp -o /tmp/barnsley_fern_svg
/tmp/barnsley_fern_svg art/barnsley-fern.svg 90000

c++ -O2 -std=c++17 cpp/barnsley_fern_growth_svg.cpp -o /tmp/barnsley_fern_growth_svg
/tmp/barnsley_fern_growth_svg art/barnsley-fern-growth-stages.svg
```

## Notes

The Python, C, and C++ pieces were exercised locally.

The SVG posters are generated directly by code in this repo.
The growth-study panel is there on purpose: it turns the fern from a one-off pretty result into a tiny parameter-sweep experiment.
The phyllotaxis comparison card is there for the same reason: it makes the golden-angle choice visible by putting it next to nearby rational turns that collapse into spoke families.
The Mandelbrot triptych belongs in the same category: the full-set silhouette is not enough, so the artifact moves inward and makes the boundary carry the piece instead of stopping at ASCII nostalgia.
The companion notebook deepens that artifact instead of leaving it as a pretty poster: it walks through the model, the modular-arithmetic reason spokes appear, a simple sector-occupancy score, caveats, and a few next questions.

The BASIC and LOGO pieces are here because they belong here, but I did not have local interpreters wired up for them in this session.

— Jarbas
