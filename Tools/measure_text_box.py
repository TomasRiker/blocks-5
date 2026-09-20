#!/usr/bin/env python3
"""Where a piece of text sits in a picture, measured rather than estimated.

    python3 Tools/measure_text_box.py <with-text.png> <without-text.png> [x y w h]

Two versions of the same picture, one carrying the text and one without it: the
pixels that differ are the text and nothing else - glyphs, antialiasing, glow
and drop shadow alike, which is exactly the extent a button over it has to
cover. Give the current rectangle as four numbers and it says what the
rectangle clips.

Thresholding one picture cannot do this. In menu.png the address at the top
right sits on a background that is dark in places and light in others, so no
brightness cut separates the text from the art under it; the difference does,
because the art under it is identical in both files.

The rectangle it prints is the box itself, in the attribute form menu.xml wants.
No margin is added: the visible extent is what a click should reach, and a
button wider than its word steals presses from the art beside it.
"""
import sys

import numpy as np
from PIL import Image


def box(with_text, without_text):
    a = np.asarray(Image.open(with_text).convert("RGBA")).astype(int)
    b = np.asarray(Image.open(without_text).convert("RGBA")).astype(int)
    if a.shape != b.shape:
        raise SystemExit("the two pictures are %dx%d and %dx%d - they have to match"
                         % (a.shape[1], a.shape[0], b.shape[1], b.shape[0]))
    diff = np.abs(a - b).sum(axis=2)
    ys, xs = np.nonzero(diff)
    if len(ys) == 0:
        raise SystemExit("the two pictures are identical - nothing to measure")
    return xs, ys, a.shape[1], a.shape[0]


def main():
    if len(sys.argv) not in (3, 7):
        raise SystemExit(__doc__)
    xs, ys, width, height = box(sys.argv[1], sys.argv[2])

    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    print("picture       %dx%d" % (width, height))
    print("changed       %d px" % len(xs))
    print("text          x %d..%d  y %d..%d  (%dx%d)"
          % (x0, x1, y0, y1, x1 - x0 + 1, y1 - y0 + 1))

    print("rectangle     x=\"%d\" y=\"%d\" w=\"%d\" h=\"%d\""
          % (x0, y0, x1 - x0 + 1, y1 - y0 + 1))
    for side, room in (("left", x0), ("top", y0),
                       ("right", width - 1 - x1), ("bottom", height - 1 - y1)):
        if room == 0:
            print("              the text touches the picture's %s edge" % side)

    if len(sys.argv) == 7:
        bx, by, bw, bh = (int(v) for v in sys.argv[3:7])
        print()
        print("current       x %d..%d  y %d..%d  (%dx%d)"
              % (bx, bx + bw - 1, by, by + bh - 1, bw, bh))
        out = ~((xs >= bx) & (xs < bx + bw) & (ys >= by) & (ys < by + bh))
        if not out.any():
            slack = (x0 - bx, bx + bw - 1 - x1, y0 - by, by + bh - 1 - y1)
            print("              covers the text, margins L%d R%d T%d B%d" % slack)
        else:
            print("              CLIPS %d of %d px (%.1f%%)"
                  % (out.sum(), len(xs), 100.0 * out.sum() / len(xs)))
            for name, n in (("left of it", (xs < bx).sum()),
                            ("right of it", (xs >= bx + bw).sum()),
                            ("above it", (ys < by).sum()),
                            ("below it", (ys >= by + bh).sum())):
                if n:
                    print("                %-12s %d px" % (name, n))


if __name__ == "__main__":
    main()
