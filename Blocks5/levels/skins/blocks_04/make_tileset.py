"""The laboratory skin's tileset.png, drawn tile by tile in blocks_01's
layout, so tileset.xml is blocks_01's unchanged.
    make_tileset.py [<tileset.png out>]
Without an argument it writes the tileset.png beside this script.

The noise comes from numpy's Generator with fixed seeds, and numpy does not
promise that a Generator draws the same numbers in every version. So before
changing a tile, check that the unchanged script still reproduces the
committed picture, or every noisy tile changes along with the one meant."""
import os
import sys
import numpy as np
from PIL import Image

T = 16
SS = 4  # supersampling for shapes

out = np.zeros((128, 128, 4))


def col(*rgb):
    return np.array(rgb, float) / 255.0


def noise(seed, sigma=1.0, n=T):
    """Tileable smooth noise, zero mean, unit deviation."""
    r = np.random.default_rng(seed).standard_normal((n, n))
    f = np.fft.fftfreq(n)
    g = np.exp(-2 * (np.pi * sigma) ** 2 * (f[:, None] ** 2 + f[None, :] ** 2))
    o = np.real(np.fft.ifft2(np.fft.fft2(r) * g))
    return (o - o.mean()) / (o.std() + 1e-9)


def speckle(seed, density, n=T):
    r = np.random.default_rng(seed)
    m = r.random((n, n)) < density
    s = r.choice([-1.0, 1.0], (n, n))
    return m * s


def coverage(fn, w=T, h=T, ox=0.0, oy=0.0):
    """Area coverage of the region fn(x, y) <= 0, by supersampling."""
    ys, xs = np.mgrid[0:h * SS, 0:w * SS]
    x = (xs + 0.5) / SS + ox
    y = (ys + 0.5) / SS + oy
    inside = (fn(x, y) <= 0).astype(float)
    return inside.reshape(h, SS, w, SS).mean(axis=(1, 3))


def sd_rrect(x, y, x0, y0, x1, y1, r):
    cx = np.clip(x, x0 + r, x1 - r)
    cy = np.clip(y, y0 + r, y1 - r)
    d = np.hypot(x - cx, y - cy) - r
    return d


def px_centres(w=T, h=T, ox=0.0, oy=0.0):
    ys, xs = np.mgrid[0:h, 0:w]
    return xs + 0.5 + ox, ys + 0.5 + oy


def rgba(rgb, a=None):
    t = np.zeros(rgb.shape[:2] + (4,))
    t[..., :3] = rgb
    t[..., 3] = 1.0 if a is None else a
    return t


def fill(c, n=T):
    return np.ones((n, n, 3)) * c


def over(dst, src):
    """src over dst, both RGBA, straight alpha."""
    sa = src[..., 3:4]
    da = dst[..., 3:4]
    oa = sa + da * (1 - sa)
    rgb = (src[..., :3] * sa + dst[..., :3] * da * (1 - sa)) / np.maximum(oa, 1e-9)
    return np.concatenate([rgb, oa], axis=-1)


def put(x, y, tile):
    out[y:y + T, x:x + T] = tile


def bevel(rgb, width=2, light=0.22, dark=0.28, outline=True):
    """A square block's bevel, lit from the top left."""
    x, y = np.mgrid[0:T, 0:T][::-1]
    dt, dl, db, dr = y, x, T - 1 - y, T - 1 - x
    d = np.stack([dt, dl, db, dr])
    nearest = d.argmin(axis=0)
    dist = d.min(axis=0)
    f = np.clip(1.0 - dist / width, 0, 1)
    shade = np.where(nearest < 2, light, -dark) * f
    o = rgb + shade[..., None]
    if outline:
        o[T - 1, :] *= 0.55
        o[:, T - 1] *= 0.55
        o[0, :] = np.minimum(o[0, :] + 0.08, 1)
        o[:, 0] = np.minimum(o[:, 0] + 0.08, 1)
    return o


# ---------------------------------------------------------------- floors

def floor_vinyl():
    c = col(146, 160, 156)
    t = fill(c)
    t += 0.018 * noise(1, 2.0)[..., None]
    t += 0.045 * speckle(2, 0.10)[..., None]
    t += 0.025 * speckle(3, 0.08)[..., None] * col(0.6, 1.0, 1.0)
    # the floor tile's own seam
    t[0, :] += 0.035
    t[:, 0] += 0.035
    t[T - 1, :] -= 0.06
    t[:, T - 1] -= 0.06
    return t


def floor_zone():
    c = col(84, 102, 116)
    t = fill(c)
    t += 0.02 * noise(11, 2.5)[..., None]
    t += 0.03 * speckle(12, 0.12)[..., None]
    t[0, :] += 0.03
    t[:, 0] += 0.03
    t[T - 1, :] -= 0.05
    t[:, T - 1] -= 0.05
    return t


def floor_tread():
    """Steel checker plate: raised lentils, alternating in direction."""
    c = col(120, 126, 132)
    t = fill(c)
    t += 0.02 * noise(21, 0.7)[..., None]
    t += 0.010 * np.random.default_rng(22).standard_normal(T)[:, None, None]
    for cy in range(0, T, 4):
        for cx in range(0, T, 4):
            if ((cx // 4) + (cy // 4)) % 2:
                hi = [(cx + 3, cy), (cx + 2, cy + 1), (cx + 1, cy + 2)]
                sh = [(cx + 3, cy + 1), (cx + 2, cy + 2)]
            else:
                hi = [(cx, cy), (cx + 1, cy + 1), (cx + 2, cy + 2)]
                sh = [(cx + 1, cy + 2), (cx + 2, cy + 3)]
            for k, (px, py) in enumerate(hi):
                t[py % T, px % T] += 0.15 if k == 1 else 0.09
            for px, py in sh:
                t[py % T, px % T] -= 0.10
    t[0, :] += 0.025
    t[:, 0] += 0.025
    t[T - 1, :] -= 0.06
    t[:, T - 1] -= 0.06
    return t


def floor_grating():
    """Perforated steel floor: 2 px holes on a 4 px pitch over a dark void."""
    hole = col(44, 48, 54)
    plate = col(104, 112, 118)
    t = fill(plate)
    t += 0.02 * noise(31, 0.8)[..., None]
    x, y = np.mgrid[0:T, 0:T][::-1]
    lx, ly = x % 4, y % 4
    h = (lx >= 1) & (lx <= 2) & (ly >= 1) & (ly <= 2)
    t[h] = hole
    t[h & (ly == 1)] *= 0.8
    # each hole's rim catches the light below and to the right
    t[(ly == 3) & (lx >= 1) & (lx <= 2)] += 0.05
    t[(lx == 3) & (ly >= 1) & (ly <= 2)] += 0.04
    t += (0.03 * noise(32, 2.5) * h)[..., None]
    t[0, :] += 0.03
    t[:, 0] += 0.03
    t[T - 1, :] -= 0.05
    t[:, T - 1] -= 0.05
    return t


VINYL = floor_vinyl()
ZONE = floor_zone()
TREAD = floor_tread()
GRATING = floor_grating()

put(32, 0, rgba(VINYL))
put(96, 0, rgba(TREAD))
put(112, 80, rgba(GRATING))
put(80, 0, rgba(fill(col(0, 0, 0))))


# ------------------------------------------------- the zone and its stripes

YELLOW = col(232, 190, 40)
BLACK = col(36, 36, 38)
BAND = 1.9  # half the hazard band's width


def zone_tile(fn_dist, x0, y0):
    """A transition tile. fn_dist(x, y) is the signed distance to the
    boundary in canvas coordinates, negative inside the zone. The stripes
    run in tile coordinates, period 8, so they meet across tiles."""
    inside = coverage(lambda x, y: fn_dist(x, y), ox=x0, oy=y0)
    rgb = VINYL * (1 - inside[..., None]) + ZONE * inside[..., None]
    band = coverage(lambda x, y: np.abs(fn_dist(x, y)) - BAND, ox=x0, oy=y0)
    xs, ys = px_centres()
    stripe = ((xs.astype(int) + ys.astype(int)) // 4) % 2
    sc = np.where(stripe[..., None] == 0, YELLOW, BLACK)
    sc = sc + 0.03 * speckle(41, 0.15)[..., None]
    rgb = rgb * (1 - band[..., None]) + sc * band[..., None]
    return rgb


# The 3x3 patch: the zone is the rounded square (8, 8)-(40, 40), radius 8.
def d_patch(x, y):
    return sd_rrect(x, y, 8, 8, 40, 40, 8)


for k in range(9):
    tx, ty = k % 3, k // 3
    tile = zone_tile(d_patch, tx * T, ty * T)
    put(tx * T, 16 + ty * T, rgba(tile))


# Inner corners: the 2x2 is vinyl in a disc of radius 8 about its centre,
# zone everywhere else.
def d_inner(x, y):
    return -(np.hypot(x - 16, y - 16) - 8)


for k, (tx, ty) in enumerate([(0, 0), (1, 0), (0, 1), (1, 1)]):
    tile = zone_tile(d_inner, tx * T, ty * T)
    put(48 + tx * T, 16 + ty * T, rgba(tile))


# ---------------------------------------------------------- abyss lips

def lip(floor):
    """The floor's slab seen edge-on above the void."""
    t = np.zeros((T, T, 3))
    t[0:2] = floor[0:2]
    t[2] = np.minimum(floor[2] * 1.12 + 0.07, 1)
    face = col(110, 116, 120) * 0.9 + floor.mean(axis=(0, 1)) * 0.1
    fall = np.array([0.80, 0.62, 0.46, 0.32, 0.20, 0.11, 0.05, 0.02])
    tex = 1 + 0.07 * noise(51, 0.6)
    for i, f in enumerate(fall):
        t[3 + i] = face * f * tex[3 + i][:, None]
    return t


put(48, 48, rgba(lip(VINYL)))
put(64, 48, rgba(lip(ZONE)))
put(80, 48, rgba(lip(GRATING)))
put(96, 48, rgba(lip(TREAD)))


# ---------------------------------------------------------------- walls

def white_tiles():
    base = col(212, 220, 224)
    grout = col(146, 156, 160)
    t = fill(base)
    x, y = np.mgrid[0:T, 0:T][::-1]
    lx, ly = x % 8, y % 8
    # each 8x8 glazed tile: lighter towards its top left, a gloss streak
    t += (0.05 - 0.012 * (lx + ly))[..., None]
    gloss = ((lx + ly == 3) | (lx + ly == 4)) & (lx >= 1) & (ly >= 1)
    t[gloss] += 0.06
    t[(lx == 6) & (ly >= 1) & (ly <= 6)] -= 0.05
    t[(ly == 6) & (lx >= 1) & (lx <= 6)] -= 0.05
    t[(lx == 7) | (ly == 7)] = grout
    t[(lx == 0) & (ly != 7)] += 0.04
    t[(ly == 0) & (lx != 7)] += 0.04
    t += 0.01 * noise(61, 0.5)[..., None]
    return t


def subway_tiles():
    base = col(150, 196, 178)
    grout = col(196, 210, 204)
    t = fill(base)
    x, y = np.mgrid[0:T, 0:T][::-1]
    row = y // 4
    lx = (x + (row % 2) * 4) % 8
    ly = y % 4
    t += (0.04 - 0.01 * lx - 0.015 * ly)[..., None]
    t[(ly == 0) & (lx < 7)] += 0.08
    t[(ly == 2) & (lx >= 1) & (lx <= 5)] -= 0.03
    t[(lx == 7) | (ly == 3)] = grout
    t[(ly == 3)] -= 0.08
    t += 0.012 * noise(71, 0.6)[..., None]
    return t


def crumble(rgb, seed, holes=0.28):
    """The destroyable version: cracks and missing chunks, crumbly at the
    edges like the shipped skins' broken walls."""
    r = np.random.default_rng(seed)
    n = noise(seed, 1.3) + 0.30 * r.standard_normal((T, T))
    thr = np.quantile(n, holes)
    a = np.clip((n - thr) * 3.0, 0, 1)
    a[a < 0.35] = 0
    t = rgb.copy()
    # darker towards the holes, as broken edges
    t *= (0.62 + 0.38 * np.clip((n - thr) * 1.5, 0, 1))[..., None]
    for _ in range(3):
        px, py = r.integers(2, 14, 2)
        for _ in range(8):
            t[py % T, px % T] *= 0.5
            px += r.integers(-1, 2)
            py += r.integers(0, 2)
    return rgba(t, a)


WHITE = white_tiles()
SUBWAY = subway_tiles()
put(16, 0, rgba(WHITE))
put(48, 0, crumble(WHITE, 81))
put(0, 64, rgba(SUBWAY))
put(16, 64, crumble(SUBWAY, 91))


def steel_block():
    c = col(176, 188, 200)
    t = fill(c)
    x, y = np.mgrid[0:T, 0:T][::-1]
    t += (0.07 - 0.009 * (x + y))[..., None]
    t += 0.012 * noise(101, 0.6)[..., None]
    t = bevel(t, width=2, light=0.16, dark=0.22)
    return t


def polished_block():
    x, y = np.mgrid[0:T, 0:T][::-1]
    d = (x + y) / 30.0
    g = 0.42 + 0.45 * (1 - d)
    spec = np.exp(-((x + y - 12) / 2.4) ** 2) * 0.35
    t = (g + spec)[..., None] * col(222, 228, 236)
    t += 0.015 * np.random.default_rng(111).standard_normal(T)[None, :, None]
    t = bevel(t, width=3, light=0.10, dark=0.25)
    return t


put(0, 0, rgba(steel_block()))
put(64, 0, rgba(polished_block()))


def riveted_plate(seed=121, damaged=False):
    c = col(104, 124, 144)
    t = fill(c)
    x, y = np.mgrid[0:T, 0:T][::-1]
    t += (0.05 - 0.006 * (x + y))[..., None]
    t += 0.018 * noise(seed, 0.8)[..., None]
    t = bevel(t, width=1, light=0.14, dark=0.18)
    for rx, ry in [(3, 3), (12, 3), (3, 12), (12, 12)]:
        t[ry, rx] += 0.22
        t[ry + 1, rx + 1] -= 0.18
        t[ry, rx + 1] += 0.05
        t[ry + 1, rx] += 0.02
    return t


PLATE = riveted_plate()
put(0, 80, rgba(PLATE))


def damaged_plate():
    t = riveted_plate(131)
    n = noise(132, 1.4)
    rust = col(150, 84, 44)
    m = np.clip(n - 0.2, 0, 1)[..., None]
    t = t * (1 - 0.7 * m) + rust * 0.7 * m
    a = crumble(t, 133, 0.26)
    return a


put(16, 80, damaged_plate())


def instrument_panel():
    t = fill(col(150, 156, 160))
    t = bevel(t, width=1, light=0.15, dark=0.25)
    face = col(66, 72, 78)
    t[2:14, 2:14] = face
    t[2, 2:14] -= 0.12
    t[2:14, 2] -= 0.08
    # screen
    t[3:8, 3:13] = col(16, 44, 26)
    wave = [5, 4, 4, 5, 6, 6, 5, 4, 4, 5]
    for i, wy in enumerate(wave):
        t[wy, 3 + i] = col(90, 240, 120)
    t[3, 3:13] += 0.05
    # lamps and a knob
    t[10, 4] = col(240, 70, 50)
    t[10, 6] = col(250, 210, 60)
    t[10, 8] = col(80, 230, 100)
    t[9:12, 10:13] = col(190, 196, 200)
    t[9, 10:13] += 0.1
    t[11, 10:13] -= 0.2
    t[10, 11] = col(40, 40, 44)
    t[12, 3:9] = face - 0.08
    return t


put(32, 64, rgba(instrument_panel()))


def vent():
    t = fill(col(186, 192, 196))
    t = bevel(t, width=1, light=0.14, dark=0.28)
    for i in range(5):
        y0 = 3 + i * 2
        t[y0, 2:14] = col(30, 34, 38)
        t[y0 + 1, 2:14] = col(150, 156, 160)
        t[y0 + 1, 2:14] += 0.08
    t[2, 2:14] -= 0.08
    t[13, 2:14] = col(30, 34, 38)
    return t


put(48, 64, rgba(vent()))


def monitor():
    t = fill(col(70, 76, 82))
    t = bevel(t, width=1, light=0.18, dark=0.20)
    scr = col(14, 40, 30)
    t[2:12, 2:14] = scr
    x, y = np.mgrid[0:10, 0:12][::-1]
    t[2:12, 2:14] += (0.04 * np.exp(-((x - 6) ** 2 + (y - 5) ** 2) / 30))[..., None]
    # grid
    t[2:12:3, 2:14] += col(0, 20, 10)
    t[2:12, 2:14:4] += col(0, 20, 10)
    # an ECG trace one pixel wide: the lit run of each column, (x, top,
    # bottom). The spike's two strokes stand a column apart with the peak
    # between them, so no two neighbouring columns are lit side by side.
    trace = [(2, 8, 8), (3, 7, 7), (4, 8, 8), (5, 4, 8), (6, 3, 3), (7, 4, 10),
             (8, 11, 11), (9, 9, 10), (10, 8, 8), (11, 7, 7), (12, 7, 7), (13, 8, 8)]
    for x, top, bottom in trace:
        t[top:bottom + 1, x] = col(110, 255, 150)
    t[2, 2:14] += 0.06
    # stand / buttons
    t[13, 4] = col(80, 230, 100)
    t[13, 6:12] = col(110, 116, 122)
    return t


put(32, 80, rgba(monitor()))


def radiation_sign():
    t = fill(col(236, 196, 40))
    t = bevel(t, width=1, light=0.12, dark=0.30)

    def blades(x, y):
        dx, dy = x - 8, y - 7.5
        r = np.hypot(dx, dy)
        ang = (np.degrees(np.arctan2(dy, dx)) - 60) % 120
        ring = (r >= 2.2) & (r <= 5.6) & (ang < 60)
        hub = r <= 1.3
        return ~(ring | hub)

    m = coverage(lambda x, y: blades(x, y).astype(float) - 0.5)
    ink = col(28, 26, 24)
    t = t * (1 - m[..., None]) + ink * m[..., None]
    return t


put(48, 80, rgba(radiation_sign()))


# ---------------------------------------------------------------- pipes

PIPE = col(170, 176, 182)


def pipe_column(y0, y1, x0=4, x1=12):
    """Cylinder shading across x, transparent outside."""
    t = np.zeros((T, T, 4))
    xs = np.arange(T) + 0.5
    u = (xs - x0) / (x1 - x0) * 2 - 1
    inside = np.abs(u) <= 1
    shade = np.clip(1 - u ** 2, 0, 1) ** 0.5
    lum = 0.45 + 0.55 * shade - 0.18 * u
    lum += 0.25 * np.exp(-((u + 0.45) / 0.18) ** 2)
    for y in range(y0, y1):
        t[y, inside, :3] = PIPE * lum[inside, None]
        t[y, inside, 3] = 1
    return t


def flange(t, y, h, x0=2, x1=14, c=col(150, 156, 162)):
    for yy in range(y, y + h):
        xs = np.arange(x0, x1)
        u = (xs + 0.5 - x0) / (x1 - x0) * 2 - 1
        lum = 0.55 + 0.45 * np.clip(1 - u ** 2, 0, 1) ** 0.5 - 0.15 * u
        t[yy, x0:x1, :3] = c * lum[:, None]
        t[yy, x0:x1, 3] = 1
    t[y, x0:x1, :3] += 0.12
    t[y + h - 1, x0:x1, :3] -= 0.15
    return t


shaft = pipe_column(0, T)
shaft = flange(shaft, 7, 2, 3, 13)
put(96, 80, np.clip(shaft, 0, 1))

top = pipe_column(3, T)
top = flange(top, 1, 3, 3, 13)
# the valve's hand wheel
xs, ys = px_centres()
ring = coverage(lambda x, y: np.abs(np.hypot(x - 8, y - 9) - 4.0) - 0.55)
spoke = coverage(lambda x, y: np.minimum(np.abs(x - 8), np.abs(y - 9)) - 0.35
                 + (np.hypot(x - 8, y - 9) > 4.0) * 9)
red = col(200, 40, 34)
w = np.clip(ring + spoke, 0, 1)
wheel = rgba(fill(red) + 0.15 * ((ys < 9)[..., None]) - 0.1 * ((ys > 10)[..., None]), w)
top = over(top, wheel)
top[9, 8, :3] = col(220, 220, 224)
put(80, 80, np.clip(top, 0, 1))

base = pipe_column(0, 11)
base = flange(base, 11, 2, 2, 14)
base = flange(base, 13, 2, 1, 15, c=col(120, 126, 132))
base[12, 3, :3] = col(60, 60, 64)
base[12, 12, :3] = col(60, 60, 64)
put(64, 80, np.clip(base, 0, 1))


# ------------------------------------------------- the bench (rock pieces)

BENCH_TOP = col(62, 70, 76)
BENCH_RIM = col(120, 128, 134)


def sd_bench(x, y):
    return sd_rrect(x, y, 1, 1, 47, 47, 5)


def sd_quarter(x, y, cx, cy, sx, sy):
    """Signed distance to the quarter plane sx*(x-cx) >= 0, sy*(y-cy) >= 0."""
    dx = -sx * (x - cx)
    dy = -sy * (y - cy)
    return np.hypot(np.maximum(dx, 0), np.maximum(dy, 0)) + np.minimum(np.maximum(dx, dy), 0)


def bench_canvas(sd=sd_bench):
    """The 3x3 bench, 48x48: a black epoxy top on the region sd <= 0,
    with a narrow lit rim."""
    n = 48
    x, y = np.mgrid[0:n * SS, 0:n * SS][::-1]
    xf = (x + 0.5) / SS
    yf = (y + 0.5) / SS
    d = sd(xf, yf)
    a = (d <= 0).astype(float).reshape(n, SS, n, SS).mean(axis=(1, 3))
    xc, yc = px_centres(n, n)
    dc = sd(xc, yc)
    gy, gx = np.gradient(dc)
    face = (-gx - gy) / np.sqrt(2)  # >0 facing the light at the top left
    rgb = np.ones((n, n, 3)) * BENCH_TOP
    rim = np.clip(dc + 2.5, 0, 1)
    rimc = BENCH_RIM + (face * 0.18)[..., None]
    rgb = rgb * (1 - rim[..., None]) + rimc * rim[..., None]
    groove = np.clip(1 - np.abs(dc + 3.0), 0, 1) * 0.22
    rgb *= (1 - groove)[..., None]
    return rgb, a


BRGB, BA = bench_canvas()
top_tex = 0.022 * noise(151, 1.4)[..., None] + 0.03 * speckle(152, 0.10)[..., None]
bench_ids = {'P': (0, 0), 'Q': (1, 0), 'R': (2, 0),
             'U': (0, 1), 'V': (1, 1), 'W': (2, 1),
             'Z': (0, 2), 'i': (1, 2), 'j': (2, 2)}
pos = {'P': (16, 96), 'Q': (32, 96), 'R': (48, 96), 'S': (64, 96),
       'U': (96, 96), 'V': (112, 96), 'W': (0, 112), 'X': (16, 112),
       'Z': (48, 112), 'i': (64, 112), 'j': (80, 112), 'k': (96, 112)}
for tid, (tx, ty) in bench_ids.items():
    rgb = BRGB[ty * T:(ty + 1) * T, tx * T:(tx + 1) * T] + top_tex
    a = BA[ty * T:(ty + 1) * T, tx * T:(tx + 1) * T]
    x, y = pos[tid]
    put(x, y, rgba(np.clip(rgb, 0, 1), a))

# Inner corners, for a bench that turns: the centre cell of a canvas whose
# bench is everything but a quarter plane. Its corner sits where the edges of
# the two neighbouring pieces meet, 1px inside the cell, so the rim comes in
# from one neighbour, turns and leaves into the other. The outline's corner
# stays sharp because a fillet would reach into the empty cell diagonally
# across, which has no tile to draw it on.
inner_ids = {'O': (31, 31, 1, 1, (0, 96)),      # notch at the bottom right
             'T': (17, 31, -1, 1, (80, 96)),    # bottom left
             'Y': (31, 17, 1, -1, (32, 112)),   # top right
             'o': (17, 17, -1, -1, (112, 112))}  # top left
for tid, (cx, cy, sx, sy, (x, y)) in inner_ids.items():
    rgb, a = bench_canvas(lambda xs, ys: -sd_quarter(xs, ys, cx, cy, sx, sy))
    rgb = rgb[T:2 * T, T:2 * T] + top_tex
    put(x, y, rgba(np.clip(rgb, 0, 1), a[T:2 * T, T:2 * T]))

V = out[96:112, 112:128].copy()


def fan(R=6.0, blades=5):
    t = V[..., :3].copy()
    xs, ys = px_centres()
    d = np.hypot(xs - 8, ys - 8)
    # the housing, a steel square with rounded corners, lit from the top left
    h = coverage(lambda x, y: sd_rrect(x, y, 1, 1, 15, 15, 2.0))
    steel = col(150, 156, 162) + (0.10 * np.clip((16 - xs - ys) / 10, -1, 1))[..., None]
    t = t * (1 - h[..., None]) + steel * h[..., None]
    t[1, 2:14] += 0.08
    t[2:14, 1] += 0.05
    t[14, 2:14] -= 0.12
    t[2:14, 14] -= 0.10
    # the opening, on whole pixels like the guard below
    hole = d <= R
    t[hole] = col(16, 18, 22)
    # five blades rather than four, whose swept shape at this size comes out
    # close to a swastika. Each is shaded across its width, so it looks pitched.
    sector = 360.0 / blades

    def phase(x, y):
        return (np.degrees(np.arctan2(y - 8, x - 8)) - 30.0 * np.hypot(x - 8, y - 8) / R) % sector

    def blade(x, y):
        r = np.hypot(x - 8, y - 8)
        return np.where((phase(x, y) < 0.55 * sector) & (r >= 1.2) & (r <= R - 0.4), -1.0, 1.0)

    bm = coverage(blade) * hole
    shade = 0.14 * (1 - np.clip(phase(xs, ys) / (0.55 * sector), 0, 1)) - 0.03
    t = t * (1 - bm[..., None]) + (col(118, 126, 134) + shade[..., None]) * bm[..., None]
    # the hub
    t[7:9, 7:9] = col(170, 176, 182)
    t[7, 7] += 0.08
    t[8, 8] -= 0.08
    # the guard: one wire ring, on whole pixels because an antialiased wire
    # turns to mush at this size, and partly see-through because a square grid
    # or a second ring hides the blades and reads as a drain
    ring = np.abs(d - 4.0) < 0.5
    t[ring] = t[ring] * 0.35 + col(208, 214, 220) * 0.65
    # the opening's lip and four screws
    t[(d > R) & (d <= R + 0.9)] -= 0.06
    for sx, sy in ((2, 2), (13, 2), (2, 13), (13, 13)):
        t[sy, sx] = col(104, 110, 116)
    return rgba(np.clip(t, 0, 1))


def flask():
    t = V[..., :3].copy()
    xs, ys = px_centres()

    def body(x, y):
        # neck x 6.5..9.5 for y 2..7, then a cone to y 14
        neck = np.maximum(np.abs(x - 8) - 1.5, np.maximum(2 - y, y - 7.5))
        half = 1.5 + (y - 7) * 0.85
        cone = np.maximum(np.abs(x - 8) - half, np.maximum(7 - y, y - 14))
        return np.minimum(neck, cone)

    m = coverage(body)
    glass = col(200, 220, 230)
    liquid = col(70, 220, 90)
    lm = coverage(lambda x, y: np.maximum(body(x, y), 9.5 - y))
    t = t * (1 - 0.55 * m[..., None]) + glass * 0.55 * m[..., None]
    t = t * (1 - lm[..., None]) + (liquid * (1.1 - 0.04 * (ys - 9))[..., None]) * lm[..., None]
    t[9:10, :] = np.where(lm[9:10, :, None] > 0, t[9:10, :] + 0.15, t[9:10, :])
    # rim of the neck and a glint
    t[2, 6:10] = col(230, 240, 244)
    t[4:7, 7] += 0.25
    t[10:13, 5] += 0.2
    # bubbles
    t[11, 9] += 0.25
    t[12, 7] += 0.2
    return rgba(np.clip(t, 0, 1))


put(64, 96, fan())
put(16, 112, flask())
put(96, 112, crumble(V[..., :3], 161, 0.30))

out = np.clip(out, 0, 1)
img = Image.fromarray((out * 255 + 0.5).astype(np.uint8), 'RGBA')
here = os.path.dirname(os.path.abspath(__file__))
img.save(sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, 'tileset.png'))
