"""The laboratory skin's sprites.png: blocks_01's with splash goggles drawn
over the eyes of each character, active and asleep.
    make_sprites.py [<sprites.png in> <sprites.png out>]
Without arguments it reads blocks_01's and writes the sprites.png beside
this script.

Each character gets a template over its 16x16 cell. F is the frame, H its
lit top edge, L lens (the face seen through tinted plastic), G a glint,
S the strap, kept to where the head is opaque. The frame steps up over the
nose, so the nose stays in sight below the goggles. The top row of the lens
catches more of the room's light than the rest, which is what tells clear
plastic from sunglasses on the drop, whose eyes fill the lens."""
import os
import sys
import numpy as np
from PIL import Image

# Bob's eyes are on rows 5 to 7 with the cap's brim right above them, the
# square one's on rows 6 to 8, and both noses start on row 8. The drop's
# eyes are only rows 7 and 8, but a lens that small is all eye and reads as
# sunglasses, so its lens takes in the brow above as well.
FACE = ['...HHHHHHHHHH...',
        'SSFGLLLLLLLLLFSS',
        'SSFLLLLLLLLLLFSS',
        '..FLLLLFFLLLLF..',
        '...FFFF..FFFF...']
DROP = ['...HHHHHHHHHH...',
        '..FGLLLLLLLLLF..',
        'SSFLLLLFFLLLLFSS',
        'SSFLLLL..LLLLFSS',
        '...FFFF..FFFF...']
TEMPLATES = {0: (4, FACE), 1: (5, DROP), 2: (5, FACE)}

rgb = lambda *v: np.array(v) / 255
FRAME = rgb(30, 56, 70)
LIT = rgb(88, 140, 160)
STRAP = rgb(52, 56, 62)
STRAP_LIT = rgb(84, 90, 98)
TINT = rgb(190, 235, 250)
GLINT = rgb(255, 255, 255)
# how far the lens moves a pixel towards TINT, on its top row and below
SHEEN, CLEAR = 0.55, 0.3


def wear(cell, top, rows):
    out = cell.copy()
    strap_top = min(i for i, r in enumerate(rows) if 'S' in r)
    lens_top = min(i for i, r in enumerate(rows) if 'L' in r)
    for i, r in enumerate(rows):
        y = top + i
        for x, ch in enumerate(r):
            if ch == '.' or cell[y, x, 3] == 0:
                continue
            if ch == 'F':
                out[y, x, :3] = FRAME
            elif ch == 'H':
                out[y, x, :3] = LIT
            elif ch == 'L':
                k = SHEEN if i == lens_top else CLEAR
                out[y, x, :3] = cell[y, x, :3] * (1 - k) + TINT * k
            elif ch == 'G':
                out[y, x, :3] = GLINT
            elif ch == 'S':
                out[y, x, :3] = STRAP_LIT if i == strap_top else STRAP
            if ch in 'FHLG':
                out[y, x, 3] = 1.0
    return out


if __name__ == '__main__':
    here = os.path.dirname(os.path.abspath(__file__))
    src, dst = (sys.argv[1:3] if len(sys.argv) > 2 else
                (os.path.join(here, '..', 'blocks_01', 'sprites.png'), os.path.join(here, 'sprites.png')))
    a = np.asarray(Image.open(src).convert('RGBA')).astype(float) / 255
    for character, (top, rows) in TEMPLATES.items():
        for asleep in (0, 1):
            x0, y0 = character * 64 + asleep * 32, 224
            a[y0:y0 + 16, x0:x0 + 16] = wear(a[y0:y0 + 16, x0:x0 + 16], top, rows)
    Image.fromarray(np.round(a * 255).astype(np.uint8), 'RGBA').save(dst)
