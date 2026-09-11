#!/usr/bin/env python3
"""How many cells of the editor's field carry something other than the field.

    count_painted.py <png> <canvasW> <presentX> <presentY> <presentW> <presentH>

The rectangle is where the game's 640x480 frame sits inside the canvas, in the
canvas's own drawing-buffer pixels; the screenshot may be at another scale, so
the width of the canvas is given as well. Every 16x16 cell above the status bar
is sampled in its middle.

What counts as empty is the median of all of them rather than a colour written
down here: the empty field is textured grass, so its cells differ from each
other by a few levels, and almost every cell is empty in any picture this is
pointed at. A tile is nowhere near - grass reads (121,136,30) and the default
brush (204,160,97) - so the threshold has an order of magnitude of room.

Reading it off a picture is not a preference. The canvas is WebGL without
preserveDrawingBuffer, so drawImage after a frame hands back an empty image,
and no test hook reports the level's tiles.
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from make_icon import read_png

FIELD_HEIGHT = 400   # the play field; palette and status bar are below it
CELL = 16
DIFFERENT = 60       # sum over the three channels


def main():
    path = sys.argv[1]
    canvasW = float(sys.argv[2])
    px, py, pw, ph = (float(v) for v in sys.argv[3:7])

    w, h, rgba = read_png(path)
    scale = w / canvasW

    def at(gx, gy):
        x = max(0, min(w - 1, int(round((px + gx * pw / 640.0) * scale))))
        y = max(0, min(h - 1, int(round((py + gy * ph / 480.0) * scale))))
        i = (y * w + x) * 4
        return rgba[i], rgba[i + 1], rgba[i + 2]

    cells = [at(gx, gy)
             for gy in range(CELL // 2, FIELD_HEIGHT, CELL)
             for gx in range(CELL // 2, 640, CELL)]

    median = tuple(sorted(c[i] for c in cells)[len(cells) // 2] for i in range(3))
    print(sum(1 for c in cells if sum(abs(c[i] - median[i]) for i in range(3)) > DIFFERENT))


main()
