#!/usr/bin/env python3
"""Criterion.rs sampling-distribution SVG -> the report's PreSign figure (PDF).

Why this exists.  Criterion's gnuplot backend draws on a 1280x720 canvas with
12-unit type.  Included at \\linewidth (~450 pt) that type renders at about
4.2 pt -- the axis labels, tick labels and legend are unreadable next to 12 pt
body text.  This step enlarges ONLY the type and re-flows the margins that the
larger type needs; no plotted value, curve, marker or colour is touched, so the
figure is still Criterion's own plot of Criterion's own run.

The re-flow is mechanical.  Enlarging the type alone would make the right-hand
axis label collide with the right tick labels and push the legend off the
canvas, so the three right-hand columns (tick labels | density axis label |
legend) are re-spaced from their measured widths, the left axis title is moved
clear of the left tick labels, and the viewBox is widened to hold the result.
Everything inside the plot frame keeps its exact coordinates.

Layout is DERIVED from the file, never hardcoded: the plot frame and the
label columns are found by clustering the coordinates actually present.  If
Criterion's layout ever changes shape, the assertions below fail loudly
instead of silently emitting a broken figure.

    python3 scripts/gen_criterion_figure.py <in.svg> <out.pdf> [font-scale]

Called by scripts/run_criterion_fig.sh; can also be run on its own against a
captured evidence SVG (evidence/criterion/<run>/presign_pdf.svg) to rebuild the
figure without re-running the benchmark.
"""
import re
import sys

# Type scale.  2.6 puts the labels at roughly 8.8 pt when the 1600-unit-wide
# result is included at \linewidth -- readable beside 12 pt body text without
# shrinking the plot area to a stamp.
DEFAULT_SCALE = 2.6

# Helvetica advance width per unit of font size, averaged over digits and
# lower-case text.  Only used to space columns, so an estimate is enough.
ADVANCE = 0.55
BASE_FONT = 12.0


def cluster(values, gap):
    """Group sorted coordinates into columns separated by more than `gap`."""
    out = []
    for v in sorted(values):
        if out and v - out[-1][-1] <= gap:
            out[-1].append(v)
        else:
            out.append([v])
    return out


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    scale = float(sys.argv[3]) if len(sys.argv) == 4 else DEFAULT_SCALE
    svg = open(src, encoding="utf-8").read()

    # gnuplot writes "10^3" as three tspans offset by dy; renderers place the
    # raised one loosely, and the gap only grows with the type.  Fold a
    # single-digit exponent into the running text as a Unicode superscript.
    svg = re.sub(
        r'(<tspan[^>]*>)([^<]*)</tspan>'
        r'<tspan[^>]*dy="-[\d.]+px">(\d)</tspan>'
        r'<tspan[^>]*dy="[\d.]+px">([^<]*)</tspan>',
        lambda m: "%s%s%s%s</tspan>" % (m.group(1), m.group(2),
                                        "⁰¹²³⁴"
                                        "⁵⁶⁷⁸⁹"
                                        [int(m.group(3))], m.group(4)),
        svg)

    size = BASE_FONT * scale          # enlarged type size, in canvas units
    gap = 0.60 * size                 # inter-column breathing space

    # ---- plot frame: the closed 5-point rectangle gnuplot draws as the border
    frame = re.search(
        r"d='M([\d.]+),([\d.]+) L([\d.]+),([\d.]+) L([\d.]+),([\d.]+) "
        r"L([\d.]+),([\d.]+) L([\d.]+),([\d.]+) Z",
        svg)
    assert frame, "plot frame not found -- Criterion's SVG layout has changed"
    xs = [float(frame.group(i)) for i in (1, 3, 5, 7, 9)]
    ys = [float(frame.group(i)) for i in (2, 4, 6, 8, 10)]
    plot_x0, plot_x1 = min(xs), max(xs)
    plot_y0, plot_y1 = min(ys), max(ys)

    # ---- text groups: <g transform="translate(x,y)[ rotate(a)]" ... font-size
    groups = []
    for m in re.finditer(
            r'<g transform="translate\(([-\d.]+),([-\d.]+)\)([^"]*)"'
            r'[^>]*?(?:text-anchor="(\w+)")?[^>]*>\s*<text>(.*?)</text>',
            svg, re.S):
        x, y = float(m.group(1)), float(m.group(2))
        label = re.sub(r"<[^>]+>", "", m.group(5))
        groups.append((x, y, m.group(4) or "start", label,
                       "rotate" in m.group(3)))
    assert groups, "no text groups found"

    def width(label):
        return ADVANCE * size * len(label)

    # ---- LEFT: the rotated y-axis title must clear the y tick labels
    left = [g for g in groups if g[0] < plot_x0]
    left_cols = cluster({g[0] for g in left}, 25)
    assert len(left_cols) == 2, f"expected title+ticks on the left, got {left_cols}"
    title_x = left_cols[0][0]
    tick_x = left_cols[1][0]
    tick_left = tick_x - max(width(g[3]) for g in left if g[0] == tick_x)
    new_title_x = tick_left - gap - 0.25 * size
    d_title = title_x - new_title_x          # shift left by this much

    # ---- RIGHT: tick labels | density axis label | legend
    right = [g for g in groups if g[0] > plot_x1]
    right_cols = cluster({g[0] for g in right}, 25)
    assert len(right_cols) >= 3, f"expected >=3 right-hand columns, got {right_cols}"
    rtick_x = right_cols[0][0]
    dens_x = right_cols[1][0]
    legend_x0 = right_cols[2][0]

    rtick_right = rtick_x + max(width(g[3]) for g in right if g[0] == rtick_x)
    d_dens = max(0.0, (rtick_right + gap + 0.75 * size) - dens_x)
    dens_right = dens_x + d_dens + 0.25 * size

    # the legend block starts at its key strokes, which sit left of its text
    key_x0 = min([float(v) for v in re.findall(r"[ML]([\d.]+),[\d.]+", svg)
                  if float(v) > dens_x + 20] or [legend_x0])
    d_legend = max(0.0, (dens_right + gap) - key_x0)
    legend_right = max(g[0] + d_legend + width(g[3])
                       for g in right if g[0] >= legend_x0)

    # the legend's rows are pitched for 12-unit type, so they must be spread by
    # the same factor or the enlarged labels overprint each other
    legend_y0 = min(y for x, y, *_ in
                    [(g[0], g[1]) for g in right if g[0] >= legend_x0])
    legend_y0 = min([legend_y0] +
                    [float(v) for x, v in
                     re.findall(r"[ML]([\d.]+),([\d.]+)", svg)
                     if float(x) >= key_x0 - 1e-6])

    def map_point(x, y):
        """Joint map: the legend block moves and spreads as one, the plot
        interior is the identity, and the outer labels get clearance."""
        if x >= key_x0 - 1e-6:                       # legend block
            return x + d_legend, legend_y0 + (y - legend_y0) * scale
        if x <= title_x + 1e-6 and x < plot_x0:      # rotated y-axis title
            x -= d_title
        elif x > plot_x1 and x >= dens_x - 1e-6:     # right-hand axis label
            x += d_dens
        # margins above and below the frame scale with the type, so the title,
        # the x tick labels and the x axis label keep their relative clearance
        if y < plot_y0:                              # plot title
            y = plot_y0 - (plot_y0 - y) * scale
        elif y > plot_y1:                            # x tick labels + axis label
            y = plot_y1 + (y - plot_y1) * scale
        return x, y

    # ---- rewrite every coordinate through the map.  Inside the frame the map
    # is the identity, so the plotted data stays exactly where Criterion put it.
    svg = re.sub(r"translate\(([-\d.]+),([-\d.]+)\)",
                 lambda m: "translate(%.2f,%.2f)"
                 % map_point(float(m.group(1)), float(m.group(2))), svg)
    svg = re.sub(r"([ML])([\d.]+),([\d.]+)",
                 lambda m: "%s%.2f,%.2f"
                 % ((m.group(1),) + map_point(float(m.group(2)),
                                              float(m.group(3)))), svg)

    # ---- enlarge the type (and the superscript offsets that travel with it)
    svg = re.sub(r'font-size="([\d.]+)"',
                 lambda m: 'font-size="%.2f"' % (float(m.group(1)) * scale), svg)
    svg = re.sub(r'dy="([-\d.]+)px"',
                 lambda m: 'dy="%.2fpx"' % (float(m.group(1)) * scale), svg)

    # ---- widen the canvas to hold the re-flowed margins
    pad = 0.5 * size
    min_x = min(0.0, new_title_x - 0.75 * size) - pad
    max_x = legend_right + pad
    min_y = min([0.0] + [map_point(*g[:2])[1] - 0.80 * size
                         for g in groups if g[1] < plot_y0]) - pad
    max_y = max([map_point(*g[:2])[1] + 0.30 * size for g in groups]
                + [720.0]) + pad
    box = "%.2f %.2f %.2f %.2f" % (min_x, min_y, max_x - min_x, max_y - min_y)
    svg = re.sub(r'viewBox="[^"]*"', 'viewBox="%s"' % box, svg, count=1)
    svg = re.sub(r'<rect x="0" y="0" width="[\d.]+" height="[\d.]+"',
                 '<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f"'
                 % (min_x, min_y, max_x - min_x, max_y - min_y), svg, count=1)

    import cairosvg
    cairosvg.svg2pdf(bytestring=svg.encode("utf-8"), write_to=dst)
    print("wrote %s (type x%.2f, canvas %.0f x %.0f units)"
          % (dst, scale, max_x - min_x, max_y - min_y))


if __name__ == "__main__":
    main()
