#!/usr/bin/env python3
# -*- coding: ascii -*-
"""selftest.py - proves that the checks in verify.py find something.

A set of checks that always says "ok" is worthless: it may long since have
stopped matching what it looks for, without anybody noticing. This script
therefore injects, for each check, exactly the fault it is meant to catch,
runs the check, and restores the file.

Every change goes back when its with-block ends (Patch.__exit__), and the
file is then compared byte for byte. If the run breaks off in the wrong place anyway, "git status"
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
    """Whether the check reported something, and what it said.

    A check that throws is reported by verify.py as one finding and exits 1,
    which would otherwise read here exactly like the fault being caught - so an
    injection that merely breaks the check would pass this script."""
    out = subprocess.run([sys.executable, VERIFY, '--only', name],
                         capture_output=True, text=True, cwd=ROOT)
    if 'the check itself failed' in out.stdout:
        return False, out.stdout
    return out.returncode == 1, out.stdout


class Patch(object):
    """Change a file temporarily and put it back safely."""

    def __init__(self, rel):
        self.path = os.path.join(ROOT, rel)
        self.rel = rel

    def __enter__(self):
        self.original = open(self.path, 'rb').read()
        st = os.stat(self.path)
        # Nanoseconds, not the float seconds: st_mtime near 1.8e9 has a ulp of
        # about 240 ns, and the harness's age check compares with find -newer,
        # which is exact. Putting a file back a hair *newer* than it was is the
        # one direction that costs a rebuild.
        self.times = (st.st_atime_ns, st.st_mtime_ns)
        return self

    def replace(self, old, new):
        # From what is on disk, not from self.original: a case may inject twice,
        # and reading the pristine bytes each time would undo the first edit.
        text = io.open(self.path, encoding='latin-1', newline='').read()
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
        os.utime(self.path, ns=self.times)
        return False


CASES = []


def case(name, rel, quiet=False):
    """Register an injection for `name`. Normally it is a fault and the check
    has to report it; with quiet=True it is a legitimate edit and the check has
    to stay silent, which is the other half of a check being worth keeping. A
    check nobody can refactor around gets deleted rather than fixed."""
    def wrap(fn):
        CASES.append((name, rel, fn, quiet))
        return fn
    return wrap


@case('encoding', 'Blocks5/src/util.h')
def c_encoding(p):
    p.raw(p.original + b'\n// ein Umlaut: \xe4\n')


# A batch file saved with bare LF endings, which is what an editor under Linux
# writes unless told otherwise.
@case('encoding', 'Blocks5/zip_campaign.bat')
def c_encoding_bat(p):
    p.raw(p.original.replace(b'\r\n', b'\n'))


# A last line without its CRLF is what Notepad saves, and harms nothing.
@case('encoding', 'Blocks5/zip_campaign.bat', quiet=True)
def c_encoding_bat_unterminated(p):
    assert p.original.endswith(b'\r\n')
    p.raw(p.original[:-2])


@case('project_files', 'Blocks5/Blocks5.vcxproj')
def c_project(p):
    p.replace('src\\engine.cpp', 'src\\engine_typo.cpp')


@case('version', 'Blocks5/src/resources.rc')
def c_version(p):
    p.replace('FILEVERSION 1,2,0,0', 'FILEVERSION 1,1,9,0')


@case('gui_paths', 'Blocks5/src/gs_menu.cpp')
def c_gui(p):
    p.replace('gui["Menu.Quit"]', 'gui["Menu.Qiut"]')


@case('strings', 'Blocks5/src/gs_menu.cpp')
def c_strings(p):
    p.replace('"$TR_DELETED"', '"$TR_DELETED_TYPO"')


@case('xml_attrs', 'Blocks5/src/level.cpp')
def c_attrs(p):
    p.replace('SetAttribute("numLayers"', 'SetAttribute("NUM_LAYERS"')


# The bug this catches is the one that costs the most to find by hand: the two
# sizes are consistent within each translation unit, so the compiler is happy
# and the wreckage turns up as a corrupt pointer somewhere else entirely.
@case('hooks_layout', 'Blocks5/src/engine.h')
def c_hooks_layout(p):
    p.replace('\tuint renderDraws;',
              '#ifdef BLOCKS5_TEST_HOOKS\n\tuint renderDraws;\n#endif')


# The shape the 1.2.0 translation sweep actually left behind: one German noun
# inside an otherwise English line, which the majority rule cannot see.
@case('comments', 'Blocks5/src/level.cpp')
def c_german_word(p):
    p.replace('// render the sparkle layer', '// render the "Funkel-Layer"')


# The shape the conversion to RenderLayer actually got wrong: a comparison
# outside onRender, which compiles and is silently never true.
@case('render_layers', 'Blocks5/src/object.cpp')
def c_render_layers(p):
    p.replace('if(layer == RL_WIRE)', 'if(layer == 939)')


# The shape that cost every wire in the game: a part replacing the render
# layers its base set instead of adding to them.
@case('layer_bits', 'Blocks5/src/e_clock.cpp')
def c_layer_bits(p):
    p.replace('renderLayers |= RL_MAIN;', 'renderLayers = RL_MAIN;')


# A raw draw in an onRender with no bracket around it: the lava's edge pass,
# which otherwise hands everything to the renderer.
# --- raw_gl: every gl* call outside the files that own raw GL, whatever its
# shape. One case per family of name - gl*, glu*, glExt* - because the pattern
# is one alternation and a family dropped from it would go unnoticed.
@case('raw_gl', 'Blocks5/src/lava.cpp')
def c_raw_gl_draw(p):
    p.replace('\tif(layer == RL_LAVA_EDGE)\n\t{\n\t\tEngine& engine = Engine::inst();\n',
              '\tif(layer == RL_LAVA_EDGE)\n\t{\n\t\tEngine& engine = Engine::inst();\n'
              '\t\tglBegin(GL_QUADS);\n\t\tglEnd();\n')


@case('raw_gl', 'Blocks5/src/cf_blend.cpp')
def c_raw_gl_bind(p):
    p.replace('\tdrawImage(oldImageID,',
              '\tglBindTexture(GL_TEXTURE_2D, oldImageID);\n\tdrawImage(oldImageID,')


@case('raw_gl', 'Blocks5/src/cf_cube.cpp')
def c_raw_gl_glu(p):
    p.replace('\tconst Mat4 projection = Mat4::perspective(90.0f, 1.0f, 0.1f, 100.0f);',
              '\tgluPerspective(90.0, 1.0, 0.1, 100.0);\n'
              '\tconst Mat4 projection = Mat4::perspective(90.0f, 1.0f, 0.1f, 100.0f);')


@case('raw_gl', 'Blocks5/src/gs_credits.cpp')
def c_raw_gl_ext(p):
    p.replace('engine.captureFrame(bufferID);',
              'glExtUseProgram(0);\n\tengine.captureFrame(bufferID);')


# A call split over two lines, which a line-at-a-time search cannot see.
@case('raw_gl', 'Blocks5/src/hint.cpp')
def c_raw_gl_wrapped(p):
    p.replace('\tRenderer::inst().clear(Vec4f(0.0f, 0.0f, 0.0f, 0.0f));',
              '\tglDisable(\n\t\tGL_TEXTURE_2D);\n\tRenderer::inst().clear(Vec4f(0.0f, 0.0f, 0.0f, 0.0f));')


# What is written in a comment is not code. blank_noncode() blanks them, and
# without that a commented-out draw would be a finding.
@case('raw_gl', 'Blocks5/src/eye.cpp', quiet=True)
def c_raw_gl_comment(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);',
              '\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);\n'
              '\t// glBegin(GL_QUADS); glEnd();')


# A character literal holding a quote opens a string to the rest of the file
# for anything that lexes only ". testhooks.cpp writes exactly that byte
# sequence, and it blanked 165 lines of code that no check then read.
@case('raw_gl', 'Blocks5/src/bomb.cpp')
def c_raw_gl_char_literal(p):
    p.replace('\tif(layer == RL_MAIN)',
              '\tchar quote = \'"\'; glBindTexture(GL_TEXTURE_2D, 0);\n\tif(layer == RL_MAIN)')


# A file that owns raw GL may call it.
@case('raw_gl', 'Blocks5/src/renderer.cpp', quiet=True)
def c_raw_gl_owner(p):
    p.replace('void Renderer::frameEnd()\n{\n', 'void Renderer::frameEnd()\n{\n\tglFlush();\n')


# An owner with nothing left to own is reported: the entry would otherwise
# read as a considered exception while standing for nothing. u_crt.cpp owns
# raw GL for its nine uniform lookups and nothing else.
@case('raw_gl', 'Blocks5/src/u_crt.cpp')
def c_raw_gl_idle_owner(p):
    for i in range(9):
        p.replace('glExtGetUniformLocation(', 'lookupUniform(')


# --- direct_gl_scope: raw GL in the two owners that run while the renderer
# may hold quads, read with the same block, chain and preprocessor rules a
# C++ object obeys. Texture::cleanUp() is the site: it starts with a call
# through the renderer and no bracket of its own.
@case('direct_gl_scope', 'Blocks5/src/texture.cpp')
def c_scope_unbracketed(p):
    p.replace('void Texture::cleanUp()\n{\n', 'void Texture::cleanUp()\n{\n\tglFinish();\n')


@case('direct_gl_scope', 'Blocks5/src/texture.cpp', quiet=True)
def c_scope_bracketed(p):
    p.replace('void Texture::cleanUp()\n{\n',
              'void Texture::cleanUp()\n{\n\tRenderer::DirectGL direct;\n\tglFinish();\n')


# A query touches nothing and needs no bracket.
@case('direct_gl_scope', 'Blocks5/src/texture.cpp', quiet=True)
def c_scope_query(p):
    p.replace('void Texture::cleanUp()\n{\n', 'void Texture::cleanUp()\n{\n\tglGetError();\n')


# The other file read.
@case('direct_gl_scope', 'Blocks5/src/engine.cpp')
def c_scope_engine(p):
    p.replace('void Engine::drawOverlays()\n{\n', 'void Engine::drawOverlays()\n{\n\tglFinish();\n')


# A bracket lives to the end of the block it stands in and no further.
@case('direct_gl_scope', 'Blocks5/src/texture.cpp')
def c_scope_block(p):
    p.replace('void Texture::cleanUp()\n{\n',
              'void Texture::cleanUp()\n{\n\tfor(int pass = 0; pass < 1; pass++)\n\t{\n'
              '\t\tRenderer::DirectGL direct;\n\t}\n\tglFinish();\n')


# A bracket before a loop covers the calls inside it.
@case('direct_gl_scope', 'Blocks5/src/texture.cpp', quiet=True)
def c_scope_loop(p):
    p.replace('void Texture::cleanUp()\n{\n',
              'void Texture::cleanUp()\n{\n\tRenderer::DirectGL direct;\n'
              '\tfor(int i = 0; i < 4; i++)\n\t{\n\t\tglFinish();\n\t}\n')


# A helper is a new function and must not inherit the bracket of the one
# above it: the first is fine, the second is the finding.
@case('direct_gl_scope', 'Blocks5/src/texture.cpp')
def c_scope_helper(p):
    p.replace('void Texture::cleanUp()',
              'static void finishA()\n{\n\tRenderer::DirectGL direct;\n\tglFinish();\n}\n\n'
              'static void finishB()\n{\n\tglFinish();\n}\n\nvoid Texture::cleanUp()')


# The same helper one level in, inside an anonymous namespace, with a
# bracket of its own.
@case('direct_gl_scope', 'Blocks5/src/texture.cpp', quiet=True)
def c_scope_namespace_helper(p):
    p.replace('void Texture::cleanUp()',
              'namespace\n{\n\tvoid finish()\n\t{\n\t\tRenderer::DirectGL direct;\n\t\tglFinish();\n\t}\n}\n\n'
              'void Texture::cleanUp()')


# A bracket written as the body of a braceless loop is destroyed at the end of
# its own line, which no indentation rule can see.
@case('direct_gl_scope', 'Blocks5/src/texture.cpp')
def c_scope_braceless(p):
    p.replace('void Texture::cleanUp()\n{\n',
              'void Texture::cleanUp()\n{\n\tfor(int i = 0; i < 1; i++) Renderer::DirectGL direct;\n\tglFinish();\n')


# A bracket inside one branch of an if/else chain is gone after the chain,
# whichever branch ran.
@case('direct_gl_scope', 'Blocks5/src/texture.cpp')
def c_scope_chain_branch(p):
    p.replace('void Texture::cleanUp()\n{\n',
              'void Texture::cleanUp()\n{\n\tif(texID)\n\t{\n\t\tRenderer::DirectGL direct;\n\t}\n'
              '\telse\n\t{\n\t\tp_rgba = 0;\n\t}\n\tglFinish();\n')


# A bracket at the top of a function covers a call in any branch below it.
@case('direct_gl_scope', 'Blocks5/src/texture.cpp', quiet=True)
def c_scope_chain_top(p):
    p.replace('void Texture::cleanUp()\n{\n',
              'void Texture::cleanUp()\n{\n\tRenderer::DirectGL direct;\n\tif(texID)\n\t{\n\t\tglFinish();\n\t}\n')


# The arms of a preprocessor conditional never both compile, so a bracket in
# one does not cover a call in the other.
@case('direct_gl_scope', 'Blocks5/src/texture.cpp')
def c_scope_pp_arms(p):
    p.replace('void Texture::cleanUp()\n{\n',
              'void Texture::cleanUp()\n{\n#ifdef __EMSCRIPTEN__\n\tRenderer::DirectGL direct;\n#else\n'
              '\tglFinish();\n#endif\n')


# A preprocessor line carries no scope. Read as a dedent it would end the
# bracket, and this tree writes them at column 0 wherever they sit.
@case('direct_gl_scope', 'Blocks5/src/texture.cpp', quiet=True)
def c_scope_preprocessor(p):
    p.replace('void Texture::cleanUp()\n{\n',
              'void Texture::cleanUp()\n{\n\tRenderer::DirectGL direct;\n#ifdef __EMSCRIPTEN__\n'
              '\tglFinish();\n#endif\n')


# A macro body is not code at the point the directive stands.
@case('direct_gl_scope', 'Blocks5/src/texture.cpp', quiet=True)
def c_scope_macro_body(p):
    p.replace('void Texture::cleanUp()',
              '#define FINISH() \\\n\tglFinish();\n\nvoid Texture::cleanUp()')


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


# The pointer half of the same check, which has neither of the exemptions the
# scalar half grants: GS_Game sets one of its six pointers outside the
# constructor and p_level is years old, so the majority rule and the baseline
# would each have hidden the one that actually crashed the game.
@case('ctor_init', 'Blocks5/src/gs_game.cpp')
def c_ctor_pointer(p):
    p.replace('\tp_level = 0;\n\tp_selectLevel = 0;\n', '\tp_selectLevel = 0;\n')


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
    p.replace('Press %BINDING{$A_SAVE_IN_HOTEL}',
              'Press %BINDING{$A_SAVE_IN_HOTELL}')


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
    print('%-14s %s' % ('CHECK', 'answers an injected edit correctly?'))
    print('-' * 52)
    failures = 0
    for name, rel, mutate, quiet in CASES:
        clean_fired, _ = run_check(name)
        if clean_fired:
            print('%-14s SKIPPED - reports something even without a fault' % name)
            failures += 1
            continue
        with Patch(rel) as p:
            mutate(p)
            fired, output = run_check(name)
        if fired != quiet:
            print('%-14s %s' % (name, 'stays quiet' if quiet else 'yes'))
        elif quiet:
            print('%-14s FALSE POSITIVE - reports a change that is not a fault' % name)
            print(output)
            failures += 1
        else:
            print('%-14s NO - the check does not fire' % name)
            print(output)
            failures += 1

    print('')
    if failures:
        print('%d case(s) answered wrongly' % failures)
    else:
        print('all %d cases answered correctly' % len(CASES))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
