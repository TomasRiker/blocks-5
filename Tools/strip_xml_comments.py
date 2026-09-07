#!/usr/bin/env python3
# -*- coding: ascii -*-
"""strip_xml_comments.py - takes the XML comments out of the files that get packed.

The dialogs in Blocks5/data are commented the way the source is, and those
notes are nobody's business who opens data.zip - but they belong in the
files. So the source is left alone and a copy without the comments is made
while packing, and that is what the archive is built from.

    python3 Tools/strip_xml_comments.py --out DIRECTORY FILE ...
    python3 Tools/strip_xml_comments.py --out DIRECTORY SOURCEDIRECTORY

A directory stands for the *.xml in it - which is what the Windows command
line needs, since it does not expand wildcards itself.

Every file lands under its own name in the target directory, even one that
has no comments - the caller simply packs the whole directory. Exit code 1
as soon as something is wrong.

The work is done on bytes and not on text: the encoding of these files is
none of this step's business, and everything but the comments stays byte for
byte as it was.

A comment is removed only if it has its lines to itself - nothing but
whitespace before it and after it on its first and its last line. Then it goes
with those lines, or a line of blanks would be left standing.

That rule is the guard, and it is one for a reason that has to be seen once:
a level stores one tile id per character inside <Row>, so arbitrary bytes,
and "<!--" would be the tiles 60, 33, 45, 45. If a "-->" turns up anywhere
later, that is a comment like any other to an XML parser - it swallows it,
and the level is quietly destroyed. Asking whether the file is well-formed
does not help against that: it is well-formed, precisely because the parser
takes the whole of it for a comment. A row of tiles always has data before it
on its line; a comment in a dialog never does.

A comment written after something else on the same line is reported and kept
in the archive.

After that the result is read once more and compared against the original:
same tags, same attributes, same text but for whitespace. A comment is pure
markup, so its removal must move nothing else. A file that is not well-formed
XML - the levels with their tile bytes are not - is passed through unchanged.

CDATA is skipped before the search for comments: "<!--" is ordinary text in
there. The game's files hold none today; the rule is here so that it does not
become a problem if one of them holds some later.
"""

import os
import re
import sys
import xml.etree.ElementTree as ET

USAGE = ('python3 Tools/strip_xml_comments.py --out DIRECTORY'
         ' {FILE ... | SOURCEDIRECTORY}')


def collect(names):
    """The files to work on: a directory stands for the *.xml in it."""
    files = []
    for name in names:
        if os.path.isdir(name):
            files += sorted(os.path.join(name, e) for e in os.listdir(name)
                            if e.lower().endswith('.xml'))
        else:
            files.append(name)
    return files


def stripComments(data):
    """Take the comments out of an XML document. Returns the new bytes and the
    line numbers of the comments that had to stay. Raises ValueError if a
    comment or a CDATA section does not end."""
    out = bytearray()
    kept = []
    i = 0
    n = len(data)

    while i < n:
        if data.startswith(b'<![CDATA[', i):
            end = data.find(b']]>', i)
            if end < 0: raise ValueError('CDATA with no end')
            out += data[i:end + 3]
            i = end + 3
            continue

        if data.startswith(b'<!--', i):
            end = data.find(b'-->', i)
            if end < 0: raise ValueError('comment with no end')
            end += 3

            lineStart = data.rfind(b'\n', 0, i) + 1
            lineEnd = data.find(b'\n', end)
            if lineEnd < 0: lineEnd = n
            alone = (data[lineStart:i].strip() == b''
                     and data[end:lineEnd].strip() == b'')

            if alone:
                # This line's indentation is already in out and goes with it.
                del out[out.rfind(b'\n') + 1:]
                i = min(lineEnd + 1, n)
            else:
                kept.append(data.count(b'\n', 0, i) + 1)
                out += data[i:end]
                i = end
            continue

        out += data[i:i + 1]
        i += 1

    return bytes(out), kept


def readTree(data):
    """The tree without comments, with whitespace squeezed to a single space.
    None if the file is not well-formed XML."""
    def node(e):
        squeeze = lambda s: re.sub(r'\s+', ' ', s or '').strip()
        return (e.tag, sorted(e.attrib.items()),
                squeeze(e.text), squeeze(e.tail), [node(c) for c in e])
    try:
        return node(ET.fromstring(data))
    except ET.ParseError:
        return None


def main(argv):
    if len(argv) < 4 or argv[1] != '--out':
        sys.stderr.write(USAGE + '\n')
        return 2

    target = argv[2]
    if not os.path.isdir(target):
        try:
            os.makedirs(target)
        except OSError as e:
            sys.stderr.write('%s cannot be created: %s\n' % (target, e))
            return 1

    files = collect(argv[3:])
    changed = 0
    for path in files:
        try:
            with open(path, 'rb') as f: data = f.read()
        except IOError as e:
            sys.stderr.write('%s: %s\n' % (path, e))
            return 1

        tree = readTree(data)
        if tree is None:
            out = data
        else:
            try:
                out, kept = stripComments(data)
            except ValueError as e:
                sys.stderr.write('%s: %s\n' % (path, e))
                return 1
            if readTree(out) != tree:
                sys.stderr.write('%s: the tree would have changed\n' % path)
                return 1
            for line in kept:
                print('  %s line %d: comment not alone on its line, stays in'
                      ' the archive' % (os.path.basename(path), line))
            if out != data: changed += 1

        with open(os.path.join(target, os.path.basename(path)), 'wb') as f:
            f.write(out)

    print('  %d of %d XML files without their comments'
          % (changed, len(files)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
