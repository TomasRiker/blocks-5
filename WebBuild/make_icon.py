#!/usr/bin/env python3
"""make_icon.py - blaeht data/window.png fuer die Web-App auf, Pixel fuer Pixel.

    python3 make_icon.py <ein.png> <aus.png> [--scale N] [--canvas N]
                         [--background RRGGBB]

Das Symbol der Seite ist dasselbe, das das Spielfenster traegt: Bobs Gesicht,
32x32. Ein Telefon skaliert ein so kleines Bild fuer den Startbildschirm selbst
hoch und nimmt dafuer eine glaettende Filterung - aus 32 Pixeln wird dann ein
verwaschener Fleck. Vorher mit reiner Pixelvervielfachung vergroessert bleibt
jede Kante hart, weil jeder Ausgabepunkt die unveraenderte Kopie eines
Eingabepunkts ist.

--scale ist dieser ganzzahlige Faktor, --canvas die Kantenlaenge des fertigen
Bildes; ist sie groesser, sitzt das Bild mittig darin. Das ist der Unterschied
zwischen den beiden Sorten, die eine Web-App braucht:

  purpose "any"        randlos, mit Transparenz. Wird unveraendert angezeigt.
  purpose "maskable"   Der Startbildschirm schneidet sich daraus eine Form
                       eigener Wahl - Kreis, abgerundetes Quadrat, Tropfen.
                       Sicher ist nur ein Kreis von 80% der Kantenlaenge in der
                       Mitte; alles ausserhalb davon kann fehlen. Deshalb kleiner
                       skaliert, mittig, und mit --background deckend gefuellt:
                       ein durchsichtiges Pixel wird beim Maskieren zum Loch.

Absichtlich ohne Pillow: der Web-Build braucht ohnehin ein python3 (Emscripten
verlangt es), und eine weitere Abhaengigkeit waere schlecht getauscht. Gelesen
wird deshalb genau der PNG-Fall, in dem window.png vorliegt - 8 Bit RGBA, nicht
verschraenkt -, und alles andere wird abgelehnt statt still falsch gemacht.
"""
import struct
import sys
import zlib


def read_png(path):
    """Liefert (breite, hoehe, bytes) fuer ein 8-Bit-RGBA-PNG ohne Interlacing."""
    data = open(path, 'rb').read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise SystemExit('%s ist kein PNG' % path)

    pos = 8
    width = height = 0
    idat = []
    while pos + 8 <= len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length          # Laenge, Kennung, Rumpf, Pruefsumme
        if kind == b'IHDR':
            width, height, depth, color, comp, filt, interlace = struct.unpack('>IIBBBBB', body)
            if (depth, color, comp, filt, interlace) != (8, 6, 0, 0, 0):
                raise SystemExit('%s: erwartet 8-Bit RGBA ohne Interlacing, '
                                 'gefunden depth=%d color=%d interlace=%d'
                                 % (path, depth, color, interlace))
        elif kind == b'IDAT':
            idat.append(body)
        elif kind == b'IEND':
            break

    raw = zlib.decompress(b''.join(idat))
    stride = width * 4
    out = bytearray(height * stride)
    prev = bytearray(stride)
    at = 0
    for y in range(height):
        method = raw[at]
        line = bytearray(raw[at + 1:at + 1 + stride])
        at += 1 + stride
        # Die fuenf Zeilenfilter aus RFC 2083, Abschnitt 6.
        for x in range(stride):
            a = line[x - 4] if x >= 4 else 0
            b = prev[x]
            c = prev[x - 4] if x >= 4 else 0
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
                raise SystemExit('%s: unbekannter Zeilenfilter %d' % (path, method))
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return width, height, bytes(out)


def write_png(path, width, height, pixels):
    def chunk(kind, body):
        return (struct.pack('>I', len(body)) + kind + body +
                struct.pack('>I', zlib.crc32(kind + body) & 0xFFFFFFFF))

    stride = width * 4
    # Filter 0 je Zeile: das Bild besteht aus grossen gleichfarbigen Bloecken,
    # die zlib von sich aus kurz macht.
    raw = b''.join(b'\x00' + pixels[y * stride:(y + 1) * stride] for y in range(height))
    body = (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9))
            + chunk(b'IEND', b''))
    open(path, 'wb').write(body)


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
        raise SystemExit('%s ist %dx%d - fuer ein Symbol wird ein Quadrat gebraucht'
                         % (src, width, height))

    if scale is None:
        target = canvas if canvas else 512
        if target % width:
            raise SystemExit('%d ist kein ganzzahliges Vielfaches von %d - dann waere '
                             'die Vergroesserung keine reine Pixelvervielfachung'
                             % (target, width))
        scale = target // width
    art = width * scale
    if canvas is None:
        canvas = art
    if art > canvas:
        raise SystemExit('%dx%d passt nicht auf eine Flaeche von %d'
                         % (art, art, canvas))

    if background is None:
        fill = bytes((0, 0, 0, 0))
    else:
        if len(background) != 6:
            raise SystemExit('--background will RRGGBB, nicht "%s"' % background)
        fill = bytes((int(background[0:2], 16), int(background[2:4], 16),
                      int(background[4:6], 16), 255))

    out = bytearray(fill * (canvas * canvas))
    offset = (canvas - art) // 2
    stride = width * 4
    for y in range(height):
        # Eine Eingabezeile einmal breitziehen ...
        row = bytearray(art * 4)
        for x in range(width):
            px = pixels[y * stride + 4 * x:y * stride + 4 * x + 4]
            row[4 * scale * x:4 * scale * (x + 1)] = px * scale
        # ... und, wo der Untergrund durchscheint, ueber ihn legen.
        if background is not None:
            for i in range(art):
                if row[4 * i + 3] != 255:
                    a = row[4 * i + 3] / 255.0
                    for c in range(3):
                        row[4 * i + c] = int(round(row[4 * i + c] * a + fill[c] * (1.0 - a)))
                    row[4 * i + 3] = 255
        # ... und scale-mal untereinander.
        for k in range(scale):
            at = ((y * scale + k + offset) * canvas + offset) * 4
            out[at:at + art * 4] = row

    write_png(dst, canvas, canvas, bytes(out))
    print('%s: %dx%d -> %dx%d auf %dx%d (%dx, nearest%s)'
          % (dst, width, height, art, art, canvas, canvas, scale,
             '' if background is None else ', Untergrund #' + background))


if __name__ == '__main__':
    main()
