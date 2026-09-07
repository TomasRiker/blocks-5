#!/usr/bin/env python3
"""make_text.py - writes one line in the game's own font into a PNG.

    python3 make_text.py <font.xml> <text> <out.png>
    python3 make_text.py <font.xml> --string $LOADING --lang de <out.png>
    python3 make_text.py --js <font.xml> $LOADING

The page's boot screen stands before the game, so it has neither data.zip nor a
GL context and cannot draw the game's font itself. So the line is drawn here, at
build time, and ships beside the page as an image - in its real pixels, which
the page then enlarges by a whole factor.

It reproduces exactly what Font::renderTextPure and Font::renderText do: every
character is a rectangle in font.png, the pen advances by its width, and under
it lie two black copies at alpha 0.35, offset by (2,1) and (1,2) - the shadow
from options.shadows == 2.

--js instead writes both languages to standard output as a JS literal, with the
image as a data URI. build.sh stamps that into the page, so the boot screen has
the line without a second request.

Deliberately without Pillow, like make_icon.py, whose PNG reader and writer are
used here.
"""
import base64
import io
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_icon import read_png, write_png            # noqa: E402

SHADOW_OFFSETS = ((2, 1), (1, 2))
SHADOW_ALPHA = 0.7 / len(SHADOW_OFFSETS)


def read_font(path):
    """(image, line height, offset, {code: (x, y, w, h)}) from a font.xml."""
    text = open(path, encoding='latin-1').read()
    head = re.search(r'<Font\b([^>]*)>', text)
    if not head:
        raise SystemExit('%s: no <Font> element' % path)
    attrs = dict(re.findall(r'(\w+)\s*=\s*"([^"]*)"', head.group(1)))
    image = os.path.join(os.path.dirname(path), attrs['image'])

    chars = {}
    for body in re.findall(r'<Character\b([^>]*)/?>', text):
        a = dict(re.findall(r'(\w+)\s*=\s*"([^"]*)"', body))
        chars[int(a['code'])] = (int(a['x']), int(a['y']), int(a['w']), int(a['h']))
    if not chars:
        raise SystemExit('%s: no <Character> elements' % path)
    return image, int(attrs['lineHeight']), int(attrs['offset']), chars


def load_string(languages, ident, lang):
    """Fetch a text out of data/languages.txt, the way loadString does at runtime.

    The file is Latin-1, the language tag comes after the section sign (0xA7),
    and the pilcrow (0xB6) would be a line break - for a single line that is an
    error and not a silent truncation.
    """
    body = None
    found = False
    for line in open(languages, encoding='latin-1').read().split('\n'):
        line = line.rstrip('\r')
        if line.startswith('$'):
            if found:
                break
            found = line.strip() == ident
        elif found and line.startswith('\xa7'):
            tag, _, rest = line[1:].partition(':')
            if tag == lang:
                body = rest
                break
    if body is None:
        raise SystemExit('%s: %s has no text for "%s"' % (languages, ident, lang))
    if '\xb6' in body or '\n' in body:
        raise SystemExit('%s: %s is multi-line' % (languages, ident))
    return body


def render(font_xml, text):
    image, line_height, offset, chars = read_font(font_xml)
    fw, fh, font = read_png(image)

    missing = sorted({c for c in text.encode('latin-1') if c not in chars})
    if missing:
        raise SystemExit('the font does not know %s' % missing)

    glyphs = [chars[c] for c in text.encode('latin-1')]
    width = sum(g[2] for g in glyphs)
    height = max(g[3] for g in glyphs)
    if width == 0:
        raise SystemExit('empty text')

    # Margin for the shadow, and the offset from the font.xml as the first row.
    pad_x = max(dx for dx, _ in SHADOW_OFFSETS)
    pad_y = max(dy for _, dy in SHADOW_OFFSETS)
    out_w, out_h = width + pad_x, height + pad_y
    out = bytearray(out_w * out_h * 4)

    def blit(dx, dy, colour, alpha):
        cursor = 0
        for gx, gy, gw, gh in glyphs:
            for y in range(gh):
                src = ((gy + y) * fw + gx) * 4
                dst = ((dy + y) * out_w + dx + cursor) * 4
                for x in range(gw):
                    a = font[src + x * 4 + 3] / 255.0 * alpha
                    if a <= 0.0:
                        continue
                    if colour is None:
                        r, g, b = font[src + x * 4], font[src + x * 4 + 1], font[src + x * 4 + 2]
                    else:
                        r, g, b = colour
                    at = dst + x * 4
                    # Lay it over what is there, the way glBlendFunc(SRC_ALPHA,
                    # ONE_MINUS_SRC_ALPHA) does.
                    old = out[at + 3] / 255.0
                    new = a + old * (1.0 - a)
                    for k, v in enumerate((r, g, b)):
                        blended = (v * a + out[at + k] * old * (1.0 - a)) / new if new > 0 else 0
                        out[at + k] = int(round(min(255.0, blended)))
                    out[at + 3] = int(round(new * 255.0))
            cursor += gw

    for dx, dy in SHADOW_OFFSETS:
        blit(dx, dy, (0, 0, 0), SHADOW_ALPHA)
    blit(0, 0, None, 1.0)
    return out_w, out_h, bytes(out), line_height, offset


def top_inset(width, height, pixels):
    """How many rows at the top are fully transparent.

    The page needs it to make up for the gap above the line: the lettering
    starts a few rows below the top edge of the image, and without this number
    the space above the text would look larger than the space below.
    """
    for y in range(height):
        if any(pixels[(y * width + x) * 4 + 3] for x in range(width)):
            return y
    return 0


def as_js(font_xml, ident):
    """{en:{d:"...",w:183,h:27,t:4},de:{...}} - both languages as a data URI."""
    languages = os.path.join(os.path.dirname(font_xml), 'languages.txt')
    parts = []
    for lang in ('en', 'de'):
        w, h, pixels, _, _ = render(font_xml, load_string(languages, ident, lang))
        buffer = io.BytesIO()
        write_png(buffer, w, h, pixels)
        data = base64.b64encode(buffer.getvalue()).decode('ascii')
        parts.append("%s:{d:'%s',w:%d,h:%d,t:%d}"
                     % (lang, data, w, h, top_inset(w, h, pixels)))
    return '{' + ','.join(parts) + '}'


def main():
    args = sys.argv[1:]
    if len(args) == 3 and args[0] == '--js':
        print(as_js(args[1], args[2]))
        return
    if len(args) < 3:
        raise SystemExit(__doc__.strip())

    font_xml = args[0]
    if args[1] == '--string':
        if len(args) != 6 or args[3] != '--lang':
            raise SystemExit(__doc__.strip())
        languages = os.path.join(os.path.dirname(font_xml), 'languages.txt')
        text = load_string(languages, args[2], args[4])
        out_path = args[5]
    else:
        text, out_path = args[1], args[2]

    w, h, pixels, _, _ = render(font_xml, text)
    write_png(out_path, w, h, pixels)
    print('%s: %dx%d from "%s"' % (out_path, w, h, text))


if __name__ == '__main__':
    main()
