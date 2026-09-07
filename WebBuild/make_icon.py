#!/usr/bin/env python3
"""make_icon.py - blows data/window.png up for the web app, pixel by pixel.

    python3 make_icon.py <in.png> <out.png> [--scale N] [--canvas N]
                         [--background RRGGBB]

The page's icon is the one the game window carries: Bob's face, 32x32. A phone
scales an image that small up itself for the home screen and takes a smoothing
filter to do it - 32 pixels then become a blurred smear. Enlarged beforehand by
pure pixel replication, the image keeps every edge hard, because each output
pixel is an unchanged copy of an input pixel.

--scale is that integer factor, --canvas the edge length of the finished image;
where it is larger, the image sits centred in it. That is the difference
between the two kinds a web app needs:

  purpose "any"        full-bleed, with transparency. Shown as it is.
  purpose "maskable"   The launcher crops a shape of its own choosing out of
                       it - circle, rounded square, teardrop. Only a centred
                       circle of 80% of the edge length is guaranteed to
                       survive; everything outside it can be missing. Hence
                       scaled smaller, centred, and filled opaque with
                       --background: a transparent pixel becomes a hole when
                       it is masked.

Deliberately without Pillow: the web build needs a python3 anyway (Emscripten
requires one), and a further dependency would be a bad trade. So it reads
exactly the one PNG case window.png comes in - 8-bit RGBA, not interlaced - and
rejects everything else rather than getting it quietly wrong.
"""
import struct
import sys
import zlib


# Bytes per pixel for the 8-bit colour types from RFC 2083. The reader accepts
# exactly these and rejects everything else rather than getting it quietly
# wrong: window.png is RGBA, data/font.png a palette.
_BPP = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}


def read_png(path):
    """Returns (width, height, RGBA bytes) for an 8-bit PNG without interlacing."""
    data = open(path, 'rb').read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise SystemExit('%s is not a PNG' % path)

    pos = 8
    width = height = color = 0
    palette = b''
    alpha = b''
    idat = []
    while pos + 8 <= len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length          # length, kind, body, checksum
        if kind == b'IHDR':
            width, height, depth, color, comp, filt, interlace = struct.unpack('>IIBBBBB', body)
            if depth != 8 or comp != 0 or filt != 0 or interlace != 0 or color not in _BPP:
                raise SystemExit('%s: expected 8 bit without interlacing, found '
                                 'depth=%d color=%d interlace=%d'
                                 % (path, depth, color, interlace))
        elif kind == b'PLTE':
            palette = body
        elif kind == b'tRNS':
            alpha = body
        elif kind == b'IDAT':
            idat.append(body)
        elif kind == b'IEND':
            break

    bpp = _BPP[color]
    raw = zlib.decompress(b''.join(idat))
    stride = width * bpp
    out = bytearray(height * stride)
    prev = bytearray(stride)
    at = 0
    for y in range(height):
        method = raw[at]
        line = bytearray(raw[at + 1:at + 1 + stride])
        at += 1 + stride
        # The five row filters from RFC 2083, section 6.
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if method == 0:
                pass
            elif method == 1:
                line[x] = (line[x] + a) & 0xFF
            elif method == 2:
                line[x] = (line[x] + b) & 0xFF
            elif method == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 0xFF
            elif method == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pred) & 0xFF
            else:
                raise SystemExit('%s: unknown row filter %d' % (path, method))
        out[y * stride:(y + 1) * stride] = line
        prev = line

    if color == 6:
        return width, height, bytes(out)

    # Bring everything else to RGBA, leaving callers only one case to know.
    rgba = bytearray(width * height * 4)
    for i in range(width * height):
        if color == 3:
            k = out[i] * 3
            if k + 3 > len(palette):
                raise SystemExit('%s: palette index outside the PLTE' % path)
            r, g, b = palette[k], palette[k + 1], palette[k + 2]
            a = alpha[out[i]] if out[i] < len(alpha) else 255
        elif color == 2:
            r, g, b, a = out[i * 3], out[i * 3 + 1], out[i * 3 + 2], 255
        elif color == 0:
            r = g = b = out[i]
            a = 255
        else:                        # 4: grey with alpha
            r = g = b = out[i * 2]
            a = out[i * 2 + 1]
        rgba[i * 4:i * 4 + 4] = bytes((r, g, b, a))
    return width, height, bytes(rgba)


def write_png(target, width, height, pixels):
    """Writes an 8-bit RGBA PNG - target is a path or an open buffer."""
    def chunk(kind, body):
        return (struct.pack('>I', len(body)) + kind + body +
                struct.pack('>I', zlib.crc32(kind + body) & 0xFFFFFFFF))

    stride = width * 4
    # Filter 0 per row: the image is made of large blocks of one colour, which
    # zlib shortens by itself.
    raw = b''.join(b'\x00' + pixels[y * stride:(y + 1) * stride] for y in range(height))
    body = (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9))
            + chunk(b'IEND', b''))
    if hasattr(target, 'write'):
        target.write(body)
    else:
        open(target, 'wb').write(body)


def main():
    args = sys.argv[1:]
    scale = canvas = None
    background = None
    positional = []
    while args:
        a = args.pop(0)
        if a == '--scale':
            scale = int(args.pop(0))
        elif a == '--canvas':
            canvas = int(args.pop(0))
        elif a == '--background':
            background = args.pop(0).lstrip('#')
        else:
            positional.append(a)
    if len(positional) < 2:
        raise SystemExit(__doc__)
    src, dst = positional[0], positional[1]
    if len(positional) > 2 and canvas is None:
        canvas = int(positional[2])

    width, height, pixels = read_png(src)
    if width != height:
        raise SystemExit('%s is %dx%d - an icon needs a square'
                         % (src, width, height))

    if scale is None:
        target = canvas if canvas else 512
        if target % width:
            raise SystemExit('%d is not an integer multiple of %d - the enlargement '
                             'would then not be pure pixel replication'
                             % (target, width))
        scale = target // width
    art = width * scale
    if canvas is None:
        canvas = art
    if art > canvas:
        raise SystemExit('%dx%d does not fit on a canvas of %d'
                         % (art, art, canvas))

    if background is None:
        fill = bytes((0, 0, 0, 0))
    else:
        if len(background) != 6:
            raise SystemExit('--background wants RRGGBB, not "%s"' % background)
        fill = bytes((int(background[0:2], 16), int(background[2:4], 16),
                      int(background[4:6], 16), 255))

    out = bytearray(fill * (canvas * canvas))
    offset = (canvas - art) // 2
    stride = width * 4
    for y in range(height):
        # Stretch one input row out ...
        row = bytearray(art * 4)
        for x in range(width):
            px = pixels[y * stride + 4 * x:y * stride + 4 * x + 4]
            row[4 * scale * x:4 * scale * (x + 1)] = px * scale
        # ... and, where the background shows through, lay the row over it.
        if background is not None:
            for i in range(art):
                if row[4 * i + 3] != 255:
                    a = row[4 * i + 3] / 255.0
                    for c in range(3):
                        row[4 * i + c] = int(round(row[4 * i + c] * a + fill[c] * (1.0 - a)))
                    row[4 * i + 3] = 255
        # ... and scale times, one below the other.
        for k in range(scale):
            at = ((y * scale + k + offset) * canvas + offset) * 4
            out[at:at + art * 4] = row

    write_png(dst, canvas, canvas, bytes(out))
    print('%s: %dx%d -> %dx%d on %dx%d (%dx, nearest%s)'
          % (dst, width, height, art, art, canvas, canvas, scale,
             '' if background is None else ', background #' + background))


if __name__ == '__main__':
    main()
