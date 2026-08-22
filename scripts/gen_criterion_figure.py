#!/usr/bin/env python3
"""Criterion.rs sampling-distribution SVG -> the report's PreSign figure (PDF).

Why this exists.  Criterion's gnuplot backend draws on a 1280x720 canvas with
12-unit type and parks the key in a tall column to the RIGHT of the plot.
Included at \\linewidth (~412 pt) that type renders at about 4 pt -- the axis
labels, tick labels and legend are unreadable next to 12 pt body text -- and the
right-hand key column spends roughly a fifth of the figure's width on five short
strings, squeezing the plot itself.

This step therefore makes four presentational changes and no others:

  1. enlarges ONLY the type, re-flowing the margins the larger type needs;
  2. moves the legend from the right-hand column into a single row centred
     BELOW the plot, under the x axis label;
  3. crops the canvas to the content that survives the move;
  4. folds gnuplot's three-tspan "10^3" into the single glyph "10³" -- renderers
     place that raised tspan loosely, and the gap only grows with the type.

Nothing inside the plot frame is touched -- no plotted value, curve, marker,
mean line, axis scale or colour -- because there the coordinate map is the
identity, so the figure is still Criterion's own plot of Criterion's own run
(a coordinate-for-coordinate check of a regenerated figure against the source
SVG is the way to confirm that after editing this file).  The key samples
(shaded box, mean line, the three sample markers) keep their size and colour --
they are the same marks the plot uses -- and only move, so the figure carries no
empty band where the old key column stood.

Layout is DERIVED from the file, never hardcoded: the plot frame, the label
columns and the legend's key/label columns are found by clustering the
coordinates actually present.  If Criterion's layout ever changes shape, the
assertions below fail loudly instead of silently emitting a broken figure.

    python3 scripts/gen_criterion_figure.py <in.svg> <out.pdf> [font-scale]

Called by scripts/run_criterion_fig.sh; can also be run on its own against a
captured evidence SVG (evidence/criterion/<run>/presign_pdf.svg) to rebuild the
figure without re-running the benchmark.
"""
import re
import sys

# Type scale.  2.6 puts the labels at roughly 10 pt once the cropped result is
# included at \linewidth -- readable beside 12 pt body text without shrinking
# the plot area to a stamp.
DEFAULT_SCALE = 2.6

# Helvetica advance width per unit of font size, averaged over digits and
# lower-case text.  Only used to space columns, so an estimate is enough.
ADVANCE = 0.55
BASE_FONT = 12.0

# gnuplot centres each key sample this far above its label's baseline, in units
# of the base font size (measured: -3.90 at 12-unit type, every entry).
KEY_RISE = 0.325

EPS = 1e-6


def cluster(values, gap):
    """Group sorted coordinates into columns separated by more than `gap`."""
    out = []
    for v in sorted(values):
        if out and v - out[-1][-1] <= gap:
            out[-1].append(v)
        else:
            out.append([v])
    return out


def coords(svg, predicate):
    """Every (x, y) the drawing actually contains, filtered by `predicate`.

    Covers the three ways this SVG carries a point: group transforms, path
    move/line commands, and polygon vertex lists (the shaded PDF area and the
    legend's own key box).
    """
    out = []
    for m in re.finditer(r"translate\(([-\d.]+),([-\d.]+)\)", svg):
        out.append((float(m.group(1)), float(m.group(2))))
    for m in re.finditer(r"[ML]([\d.]+),([\d.]+)", svg):
        out.append((float(m.group(1)), float(m.group(2))))
    for m in re.finditer(r"points\s*=\s*'([^']*)'", svg):
        for pair in m.group(1).split():
            x, _, y = pair.partition(",")
            if y:
                out.append((float(x), float(y)))
    return [p for p in out if predicate(*p)]


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

    # ---- RIGHT: tick labels | density axis label | legend labels
    right = [g for g in groups if g[0] > plot_x1]
    right_cols = cluster({g[0] for g in right}, 25)
    assert len(right_cols) >= 3, f"expected >=3 right-hand columns, got {right_cols}"
    rtick_x = right_cols[0][0]
    dens_x = right_cols[1][0]
    legend_text_x = right_cols[2][0]

    rtick_right = rtick_x + max(width(g[3]) for g in right if g[0] == rtick_x)
    d_dens = max(0.0, (rtick_right + gap + 0.75 * size) - dens_x)
    dens_right = dens_x + d_dens + 0.25 * size

    # ---- the legend, as it stands: one row per entry, key samples in a narrow
    # column left of the labels.  Both columns are read off the drawing.
    entries = sorted([g for g in right if g[0] >= legend_text_x - EPS],
                     key=lambda g: g[1])
    assert len(entries) >= 2, f"expected a multi-entry legend, got {entries}"
    key_pts = coords(svg, lambda x, y: dens_x + 20 < x < legend_text_x - EPS)
    assert key_pts, "legend key samples not found"
    key_x0 = min(x for x, _ in key_pts)
    key_x1 = max(x for x, _ in key_pts)
    key_w = key_x1 - key_x0

    # every key sample must sit unambiguously on one entry's row
    label_ys = [g[1] for g in entries]
    row_pitch = min(b - a for a, b in zip(label_ys, label_ys[1:]))
    for _, y in key_pts:
        assert min(abs(y - ly) for ly in label_ys) < 0.5 * row_pitch, \
            f"legend key sample at y={y} matches no entry row"

    # ---- the legend, relocated: one horizontal row centred under the x axis
    # label.  Widths come from the same estimator that spaces the margins.
    key_gap = 0.35 * size                    # key sample -> its own label
    entry_gap = 0.90 * size                  # entry -> next entry
    entry_w = [key_w + key_gap + width(g[3]) for g in entries]
    row_w = sum(entry_w) + entry_gap * (len(entries) - 1)
    row_x0 = 0.5 * (plot_x0 + plot_x1) - 0.5 * row_w

    new_key_x, new_label_x, x_cursor = [], [], row_x0
    for w in entry_w:
        new_key_x.append(x_cursor)
        new_label_x.append(x_cursor + key_w + key_gap)
        x_cursor += w + entry_gap
    row_right = x_cursor - entry_gap

    # the row sits below the x axis label, which the margin map has already
    # pushed down by the type scale
    xlabel_y = max(g[1] for g in groups if g[1] > plot_y1)
    row_y = plot_y1 + (xlabel_y - plot_y1) * scale + 1.15 * size

    def entry_of(y):
        return min(range(len(entries)), key=lambda i: abs(y - label_ys[i]))

    def map_point(x, y):
        """Joint map: the plot interior is the identity, the outer labels get
        clearance, and the legend moves out from the right into a row below."""
        if x >= key_x0 - EPS:                        # legend block
            i = entry_of(y)
            if x >= legend_text_x - EPS:             # the entry's label
                return new_label_x[i], row_y
            # its key sample: same shape, same size, new slot
            return (x + new_key_x[i] - key_x0,
                    y + (row_y - label_ys[i]) - KEY_RISE * (size - BASE_FONT))
        if x <= title_x + EPS and x < plot_x0:       # rotated y-axis title
            x -= d_title
        elif x > plot_x1 and x >= dens_x - EPS:      # right-hand axis label
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
    # <defs> is held out: the marker shapes there are drawn relative to their own
    # <use> point, so mapping their coordinates would deform the markers.
    head, sep, body = svg.partition("</defs>")
    assert sep, "no <defs> block -- Criterion's SVG layout has changed"
    body = re.sub(r"translate\(([-\d.]+),([-\d.]+)\)",
                  lambda m: "translate(%.2f,%.2f)"
                  % map_point(float(m.group(1)), float(m.group(2))), body)
    body = re.sub(r"([ML])([\d.]+),([\d.]+)",
                  lambda m: "%s%.2f,%.2f"
                  % ((m.group(1),) + map_point(float(m.group(2)),
                                               float(m.group(3)))), body)
    body = re.sub(r"points\s*=\s*'([^']*)'",
                  lambda m: "points = '%s'" % " ".join(
                      "%.2f,%.2f" % map_point(float(p.split(",")[0]),
                                              float(p.split(",")[1]))
                      for p in m.group(1).split() if "," in p),
                  body)
    svg = head + sep + body

    # ---- enlarge the type (and the superscript offsets that travel with it)
    svg = re.sub(r'font-size="([\d.]+)"',
                 lambda m: 'font-size="%.2f"' % (float(m.group(1)) * scale), svg)
    svg = re.sub(r'dy="([-\d.]+)px"',
                 lambda m: 'dy="%.2fpx"' % (float(m.group(1)) * scale), svg)

    # ---- crop to what is actually drawn now that the key column has gone
    pad = 0.35 * size
    min_x = min([new_title_x - 0.75 * size, row_x0, plot_x0]) - pad
    max_x = max([dens_right, row_right, plot_x1]) + pad
    min_y = min([map_point(*g[:2])[1] - 0.80 * size
                 for g in groups if g[1] < plot_y0] + [plot_y0]) - pad
    max_y = max([row_y + 0.30 * size, plot_y1]) + pad
    box = "%.2f %.2f %.2f %.2f" % (min_x, min_y, max_x - min_x, max_y - min_y)
    svg = re.sub(r'viewBox="[^"]*"', 'viewBox="%s"' % box, svg, count=1)
    svg = re.sub(r'<rect x="0" y="0" width="[\d.]+" height="[\d.]+"',
                 '<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f"'
                 % (min_x, min_y, max_x - min_x, max_y - min_y), svg, count=1)

    import cairosvg
    cairosvg.svg2pdf(bytestring=svg.encode("utf-8"), write_to=dst)
    print("wrote %s (type x%.2f, canvas %.0f x %.0f units, "
          "legend: %d entries in one row below the plot)"
          % (dst, scale, max_x - min_x, max_y - min_y, len(entries)))


if __name__ == "__main__":
    main()
