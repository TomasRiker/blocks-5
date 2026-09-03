#!/usr/bin/env python3
"""make_ico.py - baut das Programmsymbol fuer Windows aus data/window.png.

    python3 Tools/make_ico.py Blocks5/data/window.png Blocks5/src/icon1.ico
    python3 Tools/make_ico.py --sizes 16,32,48,256 <ein.png> <aus.ico>

Ein .ico enthaelt mehrere Bilder, und Windows sucht sich daraus eines aus.
Fehlt die gefragte Groesse, skaliert es selbst - glaettend, und damit ist der
Pixellook weg. Das gilt in beide Richtungen: Hochskalieren verwischt sichtbar,
Herunterskalieren mittelt benachbarte Pixel und macht aus harten Kanten weiche.
Ein einzelnes grosses Bild reicht also nicht; gebraucht wird jede Groesse, die
die Shell wirklich anfragt, jede davon hier mit Nearest Neighbour vorgerendert.

Die Kunst ist 16x16 - window.png ist deren sauberes 2x -, deshalb wird zuerst
darauf zurueckgerechnet und dann vergroessert, und zwar **immer ganzzahlig**.
Fuer 16, 32, 48, 64 und 256 geht das glatt auf. 20, 24 und 40 nicht: dort wird
nicht krumm skaliert, sondern die naechstkleinere ganzzahlige Stufe mittig in
die geforderte Flaeche gesetzt und der Rest durchsichtig gelassen -

    20 -> 16 (1x) mit 2 Pixeln Rand      40 -> 32 (2x) mit 4 Pixeln Rand

Eine krumme Vergroesserung wuerde einen Teil der Spalten verdoppeln und den
Rest nicht; genau das Gleichmass des Rasters ist aber, was Pixelgrafik als
solche lesbar macht, und ohne es sieht das Bild nach Versehen aus statt nach
Absicht. Ein etwas kleineres Symbol im Kasten faellt dagegen nicht auf - die
Symbolvorlagen von Windows lassen selbst Rand.

Eine Beschraenkung auf Zweierpotenzen gibt es nicht: im Verzeichniseintrag
eines .ico steht die Kantenlaenge in je einem Byte, erlaubt ist also alles von
1 bis 255 (und 0 bedeutet 256).

Das Ergebnis wird eingecheckt, wie icon1.ico es immer war: der Windows-Build
ruft kein Python auf, und das soll so bleiben.
"""
import os
import struct
import sys

# Der PNG-Leser steht in WebBuild/make_icon.py und wird hier nur benutzt, statt
# ihn ein zweites Mal hinzuschreiben.
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                'WebBuild'))
from make_icon import read_png, write_png            # noqa: E402

# Was die Windows-Shell anfragt: 16/32/48 sind die klassischen Groessen, 20 und
# 40 kommen bei 125% DPI dazu, 64 bei 200%, und 256 ist die Kachel der Ansicht
# "Extra grosse Symbole".
#
# 24 fehlt mit Absicht. Es ist die einzige Groesse, bei der die naechstkleinere
# ganzzahlige Stufe die 1x waere: 16 von 24 Pixeln, also zwei Drittel der Kante
# und knapp die Haelfte der Flaeche. So klein faellt der Rand auf. Windows
# rechnet sich 24 stattdessen aus dem 32er herunter - weich, aber in voller
# Groesse, und das ist an dieser einen Stelle das kleinere Uebel.
DEFAULT_SIZES = (16, 20, 32, 40, 48, 64, 256)


def reduce_to_art(width, height, pixels):
    """window.png ist ein 2x-Nearest der 16x16-Kunst; das macht es rueckgaengig."""
    if width % 2 or height % 2:
        return width, height, pixels
    stride = width * 4
    half = width // 2
    out = bytearray(half * (height // 2) * 4)
    uniform = True
    for y in range(0, height, 2):
        for x in range(0, width, 2):
            at = y * stride + 4 * x
            px = pixels[at:at + 4]
            for dy in (0, 1):
                for dx in (0, 1):
                    q = (y + dy) * stride + 4 * (x + dx)
                    if pixels[q:q + 4] != px:
                        uniform = False
            o = (y // 2) * half * 4 + 4 * (x // 2)
            out[o:o + 4] = px
    if not uniform:
        return width, height, pixels     # doch kein reines 2x - unveraendert lassen
    return half, height // 2, bytes(out)


def render(width, height, pixels, size):
    """Die groesste ganzzahlige Vergroesserung, die in size x size passt, mittig
    darin. Liefert (bytes, faktor, rand)."""
    scale = max(1, size // width)
    art = width * scale
    offset = (size - art) // 2
    stride = width * 4
    out = bytearray(size * size * 4)
    for y in range(height):
        row = bytearray(art * 4)
        for x in range(width):
            px = pixels[y * stride + 4 * x:y * stride + 4 * x + 4]
            row[4 * scale * x:4 * scale * (x + 1)] = px * scale
        for k in range(scale):
            at = ((y * scale + k + offset) * size + offset) * 4
            out[at:at + art * 4] = row
    return bytes(out), scale, offset


def dib_entry(size, rgba):
    """Ein Bild als DIB, wie es vor Vista das einzig Moegliche war: 32 Bit
    BGRA von unten nach oben, dahinter die 1-Bit-Maske. Die Maske traegt zwar
    dieselbe Auskunft wie der Alphakanal, aber die alten Pfade lesen nur sie."""
    header = struct.pack('<IiiHHIIiiII', 40, size, size * 2, 1, 32, 0, 0, 0, 0, 0, 0)

    rows = []
    for y in range(size - 1, -1, -1):
        row = bytearray(size * 4)
        for x in range(size):
            r, g, b, a = rgba[(y * size + x) * 4:(y * size + x) * 4 + 4]
            row[4 * x:4 * x + 4] = bytes((b, g, r, a))
        rows.append(bytes(row))
    xor = b''.join(rows)

    maskStride = ((size + 31) // 32) * 4      # auf 4 Byte aufgerundet
    mask = bytearray()
    for y in range(size - 1, -1, -1):
        line = bytearray(maskStride)
        for x in range(size):
            if rgba[(y * size + x) * 4 + 3] == 0:
                line[x // 8] |= 0x80 >> (x % 8)
        mask += line
    return header + xor + bytes(mask)


def main():
    args = sys.argv[1:]
    sizes = DEFAULT_SIZES
    positional = []
    while args:
        a = args.pop(0)
        if a == '--sizes':
            sizes = tuple(int(v) for v in args.pop(0).split(','))
        else:
            positional.append(a)
    if len(positional) != 2:
        raise SystemExit(__doc__)
    src, dst = positional

    width, height, pixels = read_png(src)
    if width != height:
        raise SystemExit('%s ist %dx%d - fuer ein Symbol wird ein Quadrat gebraucht'
                         % (src, width, height))
    width, height, pixels = reduce_to_art(width, height, pixels)

    entries, blobs, notes = [], [], []
    for size in sorted(sizes):
        rgba, scale, margin = render(width, height, pixels, size)
        notes.append((scale, margin))
        if size >= 256:
            # Ab Vista darf ein Eintrag ein PNG sein, und bei 256x256 spart das
            # eine Viertelmegabyte gegenueber einem DIB.
            tmp = dst + '.tmp.png'
            write_png(tmp, size, size, rgba)
            blob = open(tmp, 'rb').read()
            os.remove(tmp)
        else:
            blob = dib_entry(size, rgba)
        entries.append((size, len(blob)))
        blobs.append(blob)

    offset = 6 + 16 * len(entries)
    out = bytearray(struct.pack('<HHH', 0, 1, len(entries)))
    for (size, length) in entries:
        # 0 in einem Byte heisst 256 - groesser passt dort nicht hinein.
        out += struct.pack('<BBBBHHII', size & 0xFF, size & 0xFF, 0, 0, 1, 32, length, offset)
        offset += length
    for blob in blobs:
        out += blob
    open(dst, 'wb').write(bytes(out))

    print('%s: %d Bilder aus %dx%d-Kunst, %d Bytes' % (dst, len(entries), width, height, len(out)))
    for (size, length), (scale, margin) in zip(entries, notes):
        how = '%dx' % scale if not margin else '%dx + %d Rand' % (scale, margin)
        print('    %3dx%-3d  %-14s %6d Bytes' % (size, size, how, length))


if __name__ == '__main__':
    main()
