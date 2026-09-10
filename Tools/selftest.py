#!/usr/bin/env python3
# -*- coding: ascii -*-
"""selftest.py - proves that the checks in verify.py find something.

A set of checks that always says "ok" is worthless: it may long since have
stopped matching what it looks for, without anybody noticing. This script
therefore injects, for each check, exactly the fault it is meant to catch,
runs the check, and restores the file.

Every change goes back in a finally, and at the end the file is compared
byte for byte. If the run breaks off in the wrong place anyway, "git status"
helps - every file involved is under version control.

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
    """Change a file temporarily and put it back safely."""

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
        assert text.count(old) >= 1, 'pattern not found in %s: %r' % (self.rel, old[:60])
        io.open(self.path, 'w', encoding='latin-1', newline='').write(text.replace(old, new, 1))

    def append(self, text):
        io.open(self.path, 'w', encoding='latin-1', newline='').write(
            self.original.decode('latin-1') + text)

    def raw(self, data):
        open(self.path, 'wb').write(data)

    def __exit__(self, *exc):
        open(self.path, 'wb').write(self.original)
        assert open(self.path, 'rb').read() == self.original, 'could not restore %s!' % self.rel
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


@case('bindings', 'Blocks5/data/languages.txt')
def c_bindings(p):
    # Misspell the action a %BINDING names. Nothing anywhere says so: the
    # expansion answers an unknown action exactly as it answers an unbound one,
    # so the sentence quietly stops naming a key.
    p.replace('%BINDING{$A_SAVE_IN_HOTEL}', '%BINDING{$A_SAVE_IN_HOTELL}')


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


@case('font_metrics', 'Blocks5/levels/skins/blocks_01/hintfont.xml')
def c_font_metrics(p):
    # Take the correction off the note's font. Nothing else in the tree says
    # where its letters sit, so the keycap frame goes back to the line box -
    # and that one hangs five rows below the writing.
    p.replace(' capTop="4" capBottom="16"', '')


@case('comments', 'Blocks5/src/level.cpp')
def c_comments(p):
    p.append('\n// Das ist ein deutscher Kommentar und der muss gemeldet werden.\n')


@case('comments', 'WebBuild/htaccess')
def c_comments_prose(p):
    # The same check on a file source_files() does not reach. It has no
    # extension at all, which is how it kept a wholly German header through the
    # translation sweep with every check passing.
    p.append('\n# Das ist ein deutscher Kommentar und der muss gemeldet werden.\n')


def main():
    print('%-14s %s' % ('CHECK', 'fires on an injected fault?'))
    print('-' * 52)
    failures = 0
    for name, rel, mutate in CASES:
        clean_fired, _ = run_check(name)
        if clean_fired:
            print('%-14s SKIPPED - reports something even without a fault' % name)
            failures += 1
            continue
        with Patch(rel) as p:
            mutate(p)
            fired, output = run_check(name)
        if fired:
            print('%-14s yes' % name)
        else:
            print('%-14s NO - the check does not fire' % name)
            print(output)
            failures += 1

    print('')
    if failures:
        print('%d check(s) with no effect' % failures)
    else:
        print('all %d checks fire' % len(CASES))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
