#!/usr/bin/env python3
# -*- coding: ascii -*-
"""strip_comments.py - takes the comments out of the files that get packed.

The dialogs in Blocks5/data are commented the way the source is, and those
notes are nobody's business who opens data.zip - but they belong in the
files. So the source is left alone and a copy without the comments is made
while packing, and that is what the archive is built from.

    python3 Tools/strip_comments.py --out DIRECTORY FILE ...
    python3 Tools/strip_comments.py --out DIRECTORY SOURCEDIRECTORY

A directory stands for the *.xml and *.txt in it - which is what the Windows
command line needs, since it does not expand wildcards itself. A text file that
is not languages.txt is copied through untouched, so that the target directory
holds all of both kinds and the caller can pack it without a second source.

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

languages.txt is the second format and the simpler one. Engine::loadStringDB
reads it a line at a time and ignores every line that begins with a slash, so
dropping those lines whole cannot change a single string: the branch that
matches them does nothing at all, and in particular does not touch the count of
empty lines that the next text line flushes into the string. The blank lines
around a comment therefore stay - they are data.

It is that one file by name, not *.txt, and the reason is the same one the
packing scripts have: a skin's password.txt is packed unencrypted and deliberately,
and a pattern that swept up every .txt would one day be handed one of those and
take a line out of a password.

The check afterwards mirrors the one for XML: the string table the game would
build is read out of the bytes before and after, by a parser written to match
Engine::loadStringDB line for line - down to skipping the byte after a \r
whatever it is, and to storing a string only when the next $ID arrives, so that
the last one in the file never lands in the table.
"""

import os
import re
import sys
import xml.etree.ElementTree as ET

USAGE = ('python3 Tools/strip_comments.py --out DIRECTORY'
         ' {FILE ... | SOURCEDIRECTORY}')

# The one file that is line-commented rather than XML.
TEXT_FILE = 'languages.txt'


def collect(names):
    """The files to work on: a directory stands for the *.xml in it."""
    files = []
    for name in names:
        if os.path.isdir(name):
            entries = [e for e in os.listdir(name)
                       if e.lower().endswith(('.xml', '.txt'))]
            files += sorted(os.path.join(name, e) for e in entries)
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


def stripTextComments(data):
    """Take the comment lines out of languages.txt. Returns the new bytes and
    how many lines went."""
    out = bytearray()
    dropped = 0
    i = 0
    n = len(data)

    while i < n:
        end = i
        while end < n and data[end] != 0x0D and data[end] != 0x0A:
            end += 1

        # The terminator belongs to the line, and \r\n is one of them.
        stop = end
        if stop < n:
            stop += 1
            if data[end] == 0x0D and stop < n and data[stop] == 0x0A: stop += 1

        if data[i:end].startswith(b'//'): dropped += 1
        else: out += data[i:stop]
        i = stop

    return bytes(out), dropped


def readStrings(data):
    """The string table Engine::loadStringDB would build out of these bytes.

    Written to match that function rather than to be sensible, since the point
    of it is to answer what the game would read."""
    db = {}
    line = b''
    ident = b''
    texts = b''
    numEmptyLines = 0
    dontCollapse = False

    i = 0
    n = len(data)
    while i < n:
        c = data[i:i + 1]
        if c == b'\r' or c == b'\n':
            if not line:
                # An empty line is stored unless it stands at the beginning.
                if texts: numEmptyLines += 1
            elif line[0:1] == b'/':
                # find_first_of("//") takes a character set, so one slash is
                # already a comment to the game.
                pass
            elif line[0:1] == b'$':
                db[ident] = texts
                ident = line
                dontCollapse = ident.endswith(b'#')
                if dontCollapse: ident = ident[:-1]
                texts = b''
                numEmptyLines = 0
            elif not texts:
                texts = line
            elif line[0:1] == b'\xa7':
                texts += (b'\n' if dontCollapse else b'') + line
                numEmptyLines = 0
            else:
                if numEmptyLines:
                    texts += b'\n' * numEmptyLines
                    numEmptyLines = 0
                texts += b'\n' + line

            line = b''

            # The byte after a \r is skipped whatever it is.
            if c == b'\r': i += 1
        else:
            line += c

        i += 1

    # Deliberately no store here: the game has none either, so the last string
    # in the file is not in the table.
    return db


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

        if path.lower().endswith('.txt') and os.path.basename(path).lower() != TEXT_FILE:
            # Another text file is none of this tool's business - a skin's
            # password.txt is one, and it is packed as it stands.
            out = data
        elif os.path.basename(path).lower() == TEXT_FILE:
            strings = readStrings(data)
            out, dropped = stripTextComments(data)
            if readStrings(out) != strings:
                sys.stderr.write('%s: the strings would have changed\n' % path)
                return 1
            if dropped:
                print('  %s: %d comment lines out'
                      % (os.path.basename(path), dropped))
                changed += 1
        else:
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
                    print('  %s line %d: comment not alone on its line, stays'
                          ' in the archive' % (os.path.basename(path), line))
                if out != data: changed += 1

        with open(os.path.join(target, os.path.basename(path)), 'wb') as f:
            f.write(out)

    print('  %d of %d files without their comments' % (changed, len(files)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
