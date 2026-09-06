#!/usr/bin/env python3
# -*- coding: ascii -*-
"""strip_xml_comments.py - XML-Kommentare aus den Dateien nehmen, die gepackt werden.

Die Dialoge in Blocks5/data sind kommentiert wie der Quelltext auch, und diese
Notizen gehen niemanden etwas an, der data.zip oeffnet. In den Dateien sollen
sie aber bleiben - also wird nicht die Quelle geaendert, sondern beim Packen
eine Kopie ohne Kommentare angelegt, aus der dann das Archiv entsteht.

    python3 Tools/strip_xml_comments.py --out VERZEICHNIS DATEI ...
    python3 Tools/strip_xml_comments.py --out VERZEICHNIS QUELLVERZEICHNIS

Ein Verzeichnis steht fuer die *.xml darin - das braucht die Kommandozeile von
Windows, die Jokerzeichen nicht selbst aufloest.

Jede Datei landet unter ihrem eigenen Namen im Zielverzeichnis, auch eine ohne
Kommentare - der Aufrufer packt einfach das ganze Verzeichnis. Rueckgabewert 1,
sobald etwas nicht stimmt.

Gearbeitet wird auf Bytes und nicht auf Text: die Kodierung dieser Dateien geht
den Vorgang nichts an, und alles ausser den Kommentaren bleibt Byte fuer Byte,
wie es war.

Entfernt wird ein Kommentar nur, wenn er seine Zeilen fuer sich hat - vor ihm
und hinter ihm steht auf seiner ersten und seiner letzten Zeile nichts als
Weissraum. Dann faellt er mitsamt diesen Zeilen weg, sonst bliebe eine Zeile
aus Leerzeichen stehen.

Diese Regel ist die Schutzvorrichtung, und sie ist es aus einem Grund, den man
gesehen haben muss: in einem Level steht in <Row> je Zeichen eine Kachelnummer,
also beliebige Bytes, und "<!--" waeren die Kacheln 60, 33, 45, 45. Trifft
irgendwo spaeter ein "-->", so ist das fuer einen XML-Parser ein Kommentar wie
jeder andere - er verschluckt ihn, und der Level ist still zerstoert. Zu fragen,
ob die Datei wohlgeformt ist, hilft dagegen nicht: sie ist es, gerade weil der
Parser das Ganze fuer einen Kommentar haelt. Eine Kachelzeile hat aber immer
Daten vor sich auf ihrer Zeile, ein Kommentar in einem Dialog nie.

Wer trotzdem einen Kommentar hinter etwas anderes auf dieselbe Zeile schreibt,
bekommt ihn gemeldet und behaelt ihn im Archiv.

Danach wird das Ergebnis noch einmal gelesen und mit dem Original verglichen:
gleiche Marken, gleiche Attribute, gleicher Text bis auf Weissraum. Ein
Kommentar ist reine Auszeichnung, sein Wegfall darf also nichts anderes bewegen.
Eine Datei, die kein wohlgeformtes XML ist - die Levels mit ihren Kachelbytes
sind es nicht -, wird unveraendert durchgereicht.

CDATA wird uebersprungen, bevor nach Kommentaren gesucht wird: dort ist "<!--"
gewoehnlicher Text. Die Dateien des Spiels enthalten heute keines; die Regel
steht hier, damit das kein Problem wird, falls eine spaeter welches enthaelt.
"""

import os
import re
import sys
import xml.etree.ElementTree as ET

USAGE = ('python3 Tools/strip_xml_comments.py --out VERZEICHNIS'
         ' {DATEI ... | QUELLVERZEICHNIS}')


def collect(names):
    """Die zu bearbeitenden Dateien: ein Verzeichnis steht fuer die *.xml darin."""
    files = []
    for name in names:
        if os.path.isdir(name):
            files += sorted(os.path.join(name, e) for e in os.listdir(name)
                            if e.lower().endswith('.xml'))
        else:
            files.append(name)
    return files


def stripComments(data):
    """Die Kommentare aus einem XML-Dokument nehmen. Liefert die neuen Bytes und
    die Zeilennummern der Kommentare, die stehen bleiben mussten. Wirft
    ValueError, wenn ein Kommentar oder ein CDATA-Abschnitt nicht endet."""
    out = bytearray()
    kept = []
    i = 0
    n = len(data)

    while i < n:
        if data.startswith(b'<![CDATA[', i):
            end = data.find(b']]>', i)
            if end < 0: raise ValueError('CDATA ohne Ende')
            out += data[i:end + 3]
            i = end + 3
            continue

        if data.startswith(b'<!--', i):
            end = data.find(b'-->', i)
            if end < 0: raise ValueError('Kommentar ohne Ende')
            end += 3

            lineStart = data.rfind(b'\n', 0, i) + 1
            lineEnd = data.find(b'\n', end)
            if lineEnd < 0: lineEnd = n
            alone = (data[lineStart:i].strip() == b''
                     and data[end:lineEnd].strip() == b'')

            if alone:
                # Die Einrueckung dieser Zeile steht schon in out und geht mit.
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
    """Den Baum ohne Kommentare, mit auf ein Leerzeichen zusammengezogenem
    Weissraum. None, wenn die Datei kein wohlgeformtes XML ist."""
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
            sys.stderr.write('%s laesst sich nicht anlegen: %s\n' % (target, e))
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
                sys.stderr.write('%s: der Baum haette sich geaendert\n' % path)
                return 1
            for line in kept:
                print('  %s Zeile %d: Kommentar nicht allein auf seiner Zeile,'
                      ' bleibt im Archiv' % (os.path.basename(path), line))
            if out != data: changed += 1

        with open(os.path.join(target, os.path.basename(path)), 'wb') as f:
            f.write(out)

    print('  %d von %d XML-Dateien ohne ihre Kommentare'
          % (changed, len(files)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
