#!/usr/bin/env python3
# -*- coding: ascii -*-
"""selftest.py - beweist, dass die Pruefungen aus verify.py etwas finden.

Eine Pruefsammlung, die immer "in Ordnung" sagt, ist wertlos: sie koennte
laengst an ihrem Muster vorbeigreifen, ohne dass es jemandem auffiele. Dieses
Skript baut deshalb je Pruefung genau den Fehler ein, den sie fangen soll,
laesst sie laufen und stellt die Datei wieder her.

Jede Aenderung geht in einem finally zurueck, und am Ende wird byteweise
verglichen. Bricht der Lauf trotzdem an der falschen Stelle ab, hilft
"git status" - alle betroffenen Dateien stehen unter Versionsverwaltung.

    python3 Tools/selftest.py
"""

import io
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERIFY = os.path.join(ROOT, 'Tools', 'verify.py')


def run_check(name):
    out = subprocess.run([sys.executable, VERIFY, '--only', name],
                         capture_output=True, text=True, cwd=ROOT)
    return out.returncode != 0, out.stdout


class Patch(object):
    """Eine Datei voruebergehend veraendern und sicher zuruecklegen."""

    def __init__(self, rel):
        self.path = os.path.join(ROOT, rel)
        self.rel = rel

    def __enter__(self):
        self.original = open(self.path, 'rb').read()
        st = os.stat(self.path)
        self.times = (st.st_atime, st.st_mtime)
        return self

    def replace(self, old, new):
        text = self.original.decode('latin-1')
        assert text.count(old) >= 1, 'Muster nicht gefunden in %s: %r' % (self.rel, old[:60])
        io.open(self.path, 'w', encoding='latin-1', newline='').write(text.replace(old, new, 1))

    def append(self, text):
        io.open(self.path, 'w', encoding='latin-1', newline='').write(
            self.original.decode('latin-1') + text)

    def raw(self, data):
        open(self.path, 'wb').write(data)

    def __exit__(self, *exc):
        open(self.path, 'wb').write(self.original)
        assert open(self.path, 'rb').read() == self.original, 'konnte %s nicht zuruecklegen!' % self.rel
        # The timestamp has to be put back too. Otherwise every source touched
        # here counts afterwards as younger than everything built from it: the
        # next build recompiles half the tree, and the test harness's age check
        # fires for no reason.
        os.utime(self.path, self.times)
        return False


CASES = []


def case(name, rel):
    def wrap(fn):
        CASES.append((name, rel, fn))
        return fn
    return wrap


@case('encoding', 'Blocks5/src/util.h')
def c_encoding(p):
    p.raw(p.original + b'\n// ein Umlaut: \xe4\n')


@case('project_files', 'Blocks5/Blocks5.vcxproj')
def c_project(p):
    p.replace('src\\engine.cpp', 'src\\engine_typo.cpp')


@case('version', 'Blocks5/src/resources.rc')
def c_version(p):
    p.replace('FILEVERSION 1,2,0,0', 'FILEVERSION 1,1,9,0')


@case('gui_paths', 'Blocks5/src/gs_menu.cpp')
def c_gui(p):
    p.replace('"Menu.Quit"', '"Menu.Qiut"')


@case('strings', 'Blocks5/src/gs_menu.cpp')
def c_strings(p):
    p.replace('"$TR_DELETED"', '"$TR_DELETED_TYPO"')


@case('xml_attrs', 'Blocks5/src/level.cpp')
def c_attrs(p):
    p.replace('SetAttribute("numLayers"', 'SetAttribute("NUM_LAYERS"')


@case('naming', 'Blocks5/src/u_crt.h')
def c_naming(p):
    p.replace('class U_Crt : public Upscaler', 'class U_Tube : public Upscaler')


@case('config', 'Blocks5/src/engine.cpp')
def c_config(p):
    p.replace('new TiXmlElement("Upscaler")', 'new TiXmlElement("UpscalerX")')


# The same thing one file on: <CrtUpscaler> is created by the filter itself,
# and the check sees that half only because it reads u_*.cpp too. Without this
# case there would be nothing to show that it had stopped.
@case('config', 'Blocks5/src/u_crt.cpp')
def c_config_upscaler(p):
    p.replace('new TiXmlElement("CrtUpscaler")', 'new TiXmlElement("CrtUpscalerX")')


@case('ctor_init', 'Blocks5/src/engine.cpp')
def c_ctor(p):
    # The first of the three places is the one in the constructor.
    p.replace('\tframeDepthStencilID = 0;\n\trenderTargetID = 0;\n',
              '\tframeDepthStencilID = 0;\n')


@case('assets', 'Blocks5/src/gs_menu.cpp')
def c_assets(p):
    p.replace('"menu.xml"', '"menu_typo.xml"')


# The second half of the same check: the name exists, only spelled differently.
# Under Windows that never shows up, under Linux it is a load failure at
# runtime.
@case('assets', 'Blocks5/src/gs_menu.cpp')
def c_assets_case(p):
    p.replace('"menu.xml"', '"Menu.xml"')


@case('sounds', 'Blocks5/src/gs_loading.cpp')
def c_sounds(p):
    # Take a preloaded sound out of the list. It is still played, and exactly
    # that gap is the bug: Engine::playSound() releases the resource again at
    # once, and without the holder here ~Sound deletes the instance before any
    # sound comes out.
    p.replace('\tsndMgr.request("rewind.ogg");\n', '')


@case('sound_volumes', 'Blocks5/data/sounds.xml')
def c_sound_volumes(p):
    # Scramble a filename. Nowhere in the game does that show up: Engine
    # returns 1.0 for an unknown name, and the sound would simply be as loud as
    # its own file again.
    p.replace('file="ricochet.ogg"', 'file="richochet.ogg"')


@case('style', 'Blocks5/src/level.cpp')
def c_style(p):
    p.append('\n// eine Zeile mit Leerzeichen am Ende   \nvoid b5SelfTest() { if (1) {} }\n')


@case('windows_icon', 'Blocks5/src/icon1.ico')
def c_windows_icon(p):
    # Set the image count in the directory header to two: the remaining sizes
    # are then no longer declared, and that is exactly what has to show up.
    p.raw(p.original[:4] + b'\x02\x00' + p.original[6:])


@case('comments', 'Blocks5/src/level.cpp')
def c_comments(p):
    p.append('\n// Das ist ein deutscher Kommentar und der muss gemeldet werden.\n')


def main():
    print('%-14s %s' % ('PRUEFUNG', 'faellt bei eingebautem Fehler auf?'))
    print('-' * 52)
    failures = 0
    for name, rel, mutate in CASES:
        clean_fired, _ = run_check(name)
        if clean_fired:
            print('%-14s UEBERSPRUNGEN - meldet schon ohne Fehler etwas' % name)
            failures += 1
            continue
        with Patch(rel) as p:
            mutate(p)
            fired, output = run_check(name)
        if fired:
            print('%-14s ja' % name)
        else:
            print('%-14s NEIN - die Pruefung greift daneben' % name)
            print(output)
            failures += 1

    print('')
    if failures:
        print('%d Pruefung(en) ohne Wirkung' % failures)
    else:
        print('alle %d Pruefungen schlagen an' % len(CASES))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
