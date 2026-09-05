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
        # Auch der Zeitstempel gehoert zurueckgelegt. Sonst gilt jede Quelle,
        # die hier angefasst wurde, danach als juenger als alles, was aus ihr
        # gebaut wurde: der naechste Build uebersetzt den halben Baum neu, und
        # die Altersprobe der Testumgebung schlaegt grundlos an.
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


# Dasselbe eine Datei weiter: <CrtUpscaler> legt der Filter selbst an, und die
# Pruefung sieht diese Haelfte nur, weil sie u_*.cpp mitliest. Ohne diesen Fall
# waere nicht zu bemerken, dass sie es nicht mehr tut.
@case('config', 'Blocks5/src/u_crt.cpp')
def c_config_upscaler(p):
    p.replace('new TiXmlElement("CrtUpscaler")', 'new TiXmlElement("CrtUpscalerX")')


@case('ctor_init', 'Blocks5/src/engine.cpp')
def c_ctor(p):
    # Die erste der drei Stellen ist die im Konstruktor.
    p.replace('\tframeDepthStencilID = 0;\n\trenderTargetID = 0;\n',
              '\tframeDepthStencilID = 0;\n')


@case('assets', 'Blocks5/src/gs_menu.cpp')
def c_assets(p):
    p.replace('"menu.xml"', '"menu_typo.xml"')


# Die zweite Haelfte derselben Pruefung: den Namen gibt es, nur anders
# geschrieben. Unter Windows faellt das nie auf, unter Linux ist es ein
# Ladefehler zur Laufzeit.
@case('assets', 'Blocks5/src/gs_menu.cpp')
def c_assets_case(p):
    p.replace('"menu.xml"', '"Menu.xml"')


@case('sounds', 'Blocks5/src/gs_loading.cpp')
def c_sounds(p):
    # Einen vorgeladenen Klang aus der Liste nehmen. Gespielt wird er
    # weiterhin, und genau diese Luecke ist der Fehler: Engine::playSound()
    # gibt die Ressource sofort wieder frei, und ohne den Halter hier loescht
    # ~Sound die Instanz, bevor ein Ton herauskommt.
    p.replace('\tsndMgr.request("rewind.ogg");\n', '')


@case('style', 'Blocks5/src/level.cpp')
def c_style(p):
    p.append('\n// eine Zeile mit Leerzeichen am Ende   \nvoid b5SelfTest() { if (1) {} }\n')


@case('windows_icon', 'Blocks5/src/icon1.ico')
def c_windows_icon(p):
    # Die Zahl der Bilder im Verzeichniskopf auf zwei setzen: die uebrigen
    # Groessen sind damit nicht mehr angemeldet, und genau das soll auffallen.
    p.raw(p.original[:4] + b'\x02\x00' + p.original[6:])


@case('comments', 'Blocks5/src/level.cpp')
def c_comments(p):
    p.append('\n// This comment is written in English and should be reported.\n')


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
