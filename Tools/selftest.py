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
    p.replace('\tuint batchFlushes;',
              '#ifdef BLOCKS5_TEST_HOOKS\n\tuint batchFlushes;\n#endif')


# Two files, because the two builds fail differently: a list in the game's own
# sources compiles everywhere and only misbehaves in the browser, and a stub in
# gl_compat.cpp is what used to make that link succeed quietly.
@case('display_lists', 'Blocks5/src/lightning.cpp')
def c_display_lists(p):
    p.replace('void Lightning::render()',
              'void Lightning::renderOld() { glCallList(1); }\n\nvoid Lightning::render()')


@case('display_lists', 'WebBuild/gl_compat.cpp')
def c_display_list_stub(p):
    p.append('\nGLAPI void GLAPIENTRY glEndList(void) {}\n')


# The shape the conversion to RenderLayer actually got wrong: a comparison
# outside onRender, which compiles and is silently never true.
# The shape the 1.2.0 translation sweep actually left behind: one German noun
# inside an otherwise English line, which the majority rule cannot see.
@case('comments', 'Blocks5/src/level.cpp')
def c_german_word(p):
    p.replace('// render the sparkle layer', '// render the "Funkel-Layer"')


@case('render_layers', 'Blocks5/src/object.cpp')
def c_render_layers(p):
    p.replace('if(layer == RL_WIRE)', 'if(layer == 939)')


# The one raw draw left inside an onRender: the lava's two quads, which change
# no state on the way in and so are reached by nothing but this flush.
@case('sprite_batch', 'Blocks5/src/lava.cpp')
def c_sprite_batch(p):
    p.replace('\t\tEngine::inst().flushSprites();\n', '')


# Each of the four things gl_state bans, because a case that injects only one
# of them leaves the other three free to be dropped from the pattern without
# anything noticing. Disable and enable are separate halves of one alternation,
# and the pop is a separate call from the push.
@case('gl_state', 'Blocks5/src/player.cpp')
def c_gl_state_disable(p):
    p.replace('GL::setTexturing(false);', 'glDisable(GL_TEXTURE_2D);')


@case('gl_state', 'Blocks5/src/player.cpp')
def c_gl_state_enable(p):
    p.replace('GL::setTexturing(true);', 'glEnable(GL_TEXTURE_2D);')


@case('gl_state', 'Blocks5/src/teleporter.cpp')
def c_gl_state_push(p):
    p.replace('GL::pushTexturing();', 'glPushAttrib(GL_ENABLE_BIT);')


@case('gl_state', 'Blocks5/src/teleporter.cpp')
def c_gl_state_pop(p):
    p.replace('GL::popTexturing();', 'glPopAttrib();')


@case('gl_state', 'Blocks5/src/hint.cpp')
def c_gl_state_bind(p):
    p.replace('GL::bindTexture(noteTexture);',
              'glBindTexture(GL_TEXTURE_2D, noteTexture);')


# The texture matrix is the third of the three the docstring names, and the one
# the first draft of the check did not look at.
@case('gl_state', 'Blocks5/src/hint.cpp')
def c_gl_state_matrix(p):
    p.replace('GL::pushTextureMatrix();',
              'glMatrixMode(GL_TEXTURE);\n\tglPushMatrix();\n\tglLoadIdentity();\n\tglMatrixMode(GL_MODELVIEW);')


# A call split over two lines, which a line-at-a-time search cannot see.
@case('gl_state', 'Blocks5/src/electronics.cpp')
def c_gl_state_wrapped(p):
    p.replace('GL::setTexturing(false);', 'glDisable(\n\t\t\tGL_TEXTURE_2D);')


# A flush covers only as far as the block it stands in. A loop body is the
# shape that isolates the rule: the branches of an if/else chain are read as
# alternatives, so a flush in one of those is a different question.
@case('sprite_batch', 'Blocks5/src/lava.cpp')
def c_sprite_batch_scope(p):
    p.replace('\t\tEngine::inst().flushSprites();',
              '\t\tfor(int pass = 0; pass < 1; pass++)\n\t\t{\n\t\t\tEngine::inst().flushSprites();\n\t\t}')


# A helper is a new function and must not inherit the flush of the one above
# it. Texture::bind() is the one that ends flushed, so it is what the case has
# to follow for the inheritance to be the thing under test.
@case('sprite_batch', 'Blocks5/src/texture.cpp')
def c_sprite_batch_helper(p):
    p.replace('void Texture::unbind() const',
              'static void drawBlob()\n{\n\tglBegin(GL_QUADS);\n\tglEnd();\n}\n\nvoid Texture::unbind() const')


# The same helper one level in, inside an anonymous namespace - the idiom
# hint.cpp, font.cpp and diamondmachine.cpp already use - and placed right
# after an exempt function, which is what it would silently inherit if a line
# walk looked only at column 0 for a new one.
@case('gl_state', 'Blocks5/src/texture.cpp')
def c_gl_state_namespace(p):
    p.replace('void Texture::cleanUp()',
              'namespace\n{\n\tvoid debugBind(GLuint id)\n\t{\n'
              '\t\tglBindTexture(GL_TEXTURE_2D, id);\n\t}\n}\n\nvoid Texture::cleanUp()')


# Queue and draw on one line, after a flush that really does stand above them
# and with nothing drawing after: the draw comes after the queue and is not
# covered by it, which only reading the two in column order can see.
@case('sprite_batch', 'Blocks5/src/stdobject.cpp')
def c_sprite_batch_sameline(p):
    p.replace('\t\tlevel.renderShine(0.35, 0.35 + random(-0.05, 0.05));\n\t}\n',
              '\t\tlevel.renderShine(0.35, 0.35 + random(-0.05, 0.05));\n\t}\n'
              '\tEngine::inst().flushSprites();\n'
              '\tEngine::inst().renderSprites(sprites, color); drawQuadArray(0, 0);\n')


# A loop whose body queues: the second turn round begins with the batch
# non-empty, so a flush standing outside the loop does not cover a draw inside
# it - and a walk down the lines reads the draw before the queue.
@case('sprite_batch', 'Blocks5/src/bomb.cpp')
def c_sprite_batch_loop(p):
    p.replace('\tif(layer == RL_MAIN)',
              '\tEngine::inst().flushSprites();\n\tfor(int i = 0; i < 4; i++)\n\t{\n'
              '\t\tglBegin(GL_QUADS);\n\t\tglEnd();\n'
              '\t\tEngine::inst().renderSprites(sprites, color);\n\t}\n\tif(layer == RL_MAIN)')


# An onRender whose parameter is spelled differently still overrides, and would
# take its whole file out of both checks with nothing to say so.
@case('sprite_batch', 'Blocks5/src/bomb.cpp')
def c_sprite_batch_unscanned(p):
    p.replace('void Bomb::onRender(RenderLayer layer,', 'void Bomb::onRender(const RenderLayer layer,')


# A character literal holding a quote opens a string to the rest of the file
# for anything that lexes only ". testhooks.cpp writes exactly that byte
# sequence, and it blanked 165 lines of code that no check then read.
@case('gl_state', 'Blocks5/src/bomb.cpp')
def c_gl_state_char_literal(p):
    p.replace('\tif(layer == RL_MAIN)',
              '\tchar quote = \'"\'; glBindTexture(GL_TEXTURE_2D, 0);\n\tif(layer == RL_MAIN)')


# glPushAttrib saves whatever its mask asks for, so the ban names none: one
# written GL_ENABLE_BIT | GL_TEXTURE_BIT is the same mistake.
@case('gl_state', 'Blocks5/src/lava.cpp')
def c_gl_state_push_mask(p):
    p.replace('GL::pushTexturing();', 'glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT);')


# texture.cpp has no exception of its own: its two glPushAttrib brackets are
# inside the two exempt functions, so a pop added to the funnel is reported.
@case('gl_state', 'Blocks5/src/texture.cpp')
def c_gl_state_texture_pop(p):
    p.replace('void Texture::unbind() const\n{', 'void Texture::unbind() const\n{\n\tglPopAttrib();')


# The gl_state half of the dead-name guard. It has to name one of the helpers
# read by name rather than an exemption: renaming an exempt function reports
# through the ordinary path as well, so it would not tell the two apart.
@case('gl_state', 'Blocks5/src/font.cpp')
def c_gl_state_dead_name(p):
    p.replace('void Font::drawText(', 'void Font::drawGlyphs(')


# A flush written as the body of a braceless loop covers the rest of its own
# line and nothing after it, which no indentation rule can see: what follows
# stands at the same column as the flush itself. A loop rather than an if,
# because an if/else chain is read branch by branch and would answer this on
# its own.
@case('sprite_batch', 'Blocks5/src/projectile.cpp')
def c_sprite_batch_braceless(p):
    p.replace('\t\tGL::setTexturing(false);',
              '\t\tfor(int i = 0; i < 1; i++) GL::setTexturing(false);')


# The other half: a flush hoisted to the top of a function is not made wrong by
# a queue in one branch of an if/else chain below it, because the branch that
# draws is the branch that did not queue.
@case('sprite_batch', 'Blocks5/src/lava.cpp', quiet=True)
def c_sprite_batch_chain(p):
    p.replace('\t\t// Raw quads from here on, so anything queued by an object before this\n'
              '\t\t// one has to be on the screen first - there is no depth buffer to put\n'
              '\t\t// the two in order afterwards.\n\t\tEngine::inst().flushSprites();\n\n', '')
    p.replace('void Lava::onRender(RenderLayer layer,\n\t\t\t\t\tconst Vec4d& color)\n{\n',
              'void Lava::onRender(RenderLayer layer,\n\t\t\t\t\tconst Vec4d& color)\n{\n'
              '\tEngine::inst().flushSprites();\n')


# LineDrawer::draw() is the raw glDrawArrays behind every laser, wire and shot,
# and its file defines no onRender - so it is in the scanned set by name.
@case('sprite_batch', 'Blocks5/src/linedrawer.cpp')
def c_sprite_batch_linedrawer(p):
    p.replace('\tEngine::inst().flushSprites();\n', '')


# An exemption says the batch was put up before the function was entered. It
# does not say the function may queue and then draw.
@case('sprite_batch', 'Blocks5/src/hint.cpp')
def c_sprite_batch_exempt_queues(p):
    p.replace('\tconst double uWidth',
              '\tEngine::inst().renderSprites(sprites, color);\n\tconst double uWidth')


# Level::renderShine draws with a batch open although the rest of level.cpp
# does not, so that one function is read and the rest of the file is not.
@case('sprite_batch', 'Blocks5/src/level.cpp')
def c_sprite_batch_reached(p):
    p.replace('\tEngine::inst().renderSprite(p_shine,',
              '\tglBegin(GL_QUADS);\n\tglEnd();\n\tEngine::inst().renderSprite(p_shine,')


# An exemption for a function that no longer exists is silent, and says the
# case was thought about.
@case('sprite_batch', 'Blocks5/src/level.cpp')
def c_sprite_batch_dead_exemption(p):
    p.replace('void Level::renderShine(', 'void Level::renderGlow(')


# The third way an exemption goes stale: the function is still there and still
# read, and no longer holds what it was excused for. dead_names() cannot see
# that one, because the name resolves.
@case('sprite_batch', 'Blocks5/src/hint.cpp')
def c_sprite_batch_idle_exemption(p):
    p.replace('\t\tglBegin(GL_TRIANGLE_STRIP);', '\t\tEngine::inst().flushSprites();')


# Whatever an if/else chain queued in any of its branches stands after the
# chain, even though the branches are read as alternatives while inside it. The
# last branch must not flush, or the indentation rule would answer this on its
# own and the merge would go untested.
@case('sprite_batch', 'Blocks5/src/eye.cpp')
def c_sprite_batch_chain_queued(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);',
              '\tEngine::inst().flushSprites();\n'
              '\tif(layer == RL_MAIN)\n\t{\n'
              '\t\tEngine::inst().renderSprites(sprites, color);\n\t}\n'
              '\telse\n\t{\n\t\tglPushMatrix();\n\t\tglPopMatrix();\n\t}\n'
              '\tglBegin(GL_QUADS);\n\tglEnd();')


# A flush written as a braceless else body is as conditional as one written as
# a braceless if body, and stands at the same column as what follows it.
@case('sprite_batch', 'Blocks5/src/exit.cpp')
def c_sprite_batch_braceless_else(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);\n'
              '\telse if(layer == RL_LIGHT)',
              '\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);\n'
              '\telse Engine::inst().flushSprites();\n'
              '\tglBegin(GL_QUADS);\n\tglEnd();\n'
              '\tif(layer == RL_LIGHT)')


# glRect is a draw as much as glBegin and glDrawArrays are.
@case('sprite_batch', 'Blocks5/src/exit.cpp')
def c_sprite_batch_glrect(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);',
              '\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);\n'
              '\tglRecti(0, 0, 16, 16);')


# A flush written as a same-line case body reaches to the end of that line and
# no further, exactly as a braceless if does - and the switch's own braces sit
# at the column of the statement after it, so no indentation rule can see it.
@case('sprite_batch', 'Blocks5/src/eye.cpp')
def c_sprite_batch_case(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);',
              '\tswitch(layer)\n\t{\n'
              '\tcase RL_MAIN: Engine::inst().flushSprites(); break;\n\t}\n'
              '\tglBegin(GL_QUADS);\n\tglEnd();')


# The other direction: what is written in a comment is not code. batch_sources()
# blanks them, and without that a commented-out draw would be a finding.
@case('sprite_batch', 'Blocks5/src/eye.cpp', quiet=True)
def c_sprite_batch_commented_draw(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);',
              '\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);\n'
              '\t// glBegin(GL_QUADS); glEnd();')


# A preprocessor line carries no scope. Read as a dedent it would ask for the
# flush again, and this tree writes them at column 0 wherever they sit.
@case('sprite_batch', 'Blocks5/src/eye.cpp', quiet=True)
def c_sprite_batch_preprocessor(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);',
              '\tEngine::inst().flushSprites();\n#ifdef __EMSCRIPTEN__\n'
              '\tglBegin(GL_QUADS);\n\tglEnd();\n#endif')


# An attribute mask that carries nothing a batched quad is drawn under is left
# alone, because GLState has no entry point that could stand in for it.
@case('gl_state', 'Blocks5/src/object.cpp', quiet=True)
def c_gl_state_safe_mask(p):
    p.replace('\t\tglDisable(GL_LINE_SMOOTH);',
              '\t\tglPushAttrib(GL_LINE_BIT);\n\t\tglDisable(GL_LINE_SMOOTH);')
    p.replace('\t\tglEnable(GL_LINE_SMOOTH);', '\t\tglPopAttrib();')


# And one that does. GL_COLOR_BUFFER_BIT carries the blend function.
@case('gl_state', 'Blocks5/src/lava.cpp')
def c_gl_state_colour_mask(p):
    p.replace('GL::pushTexturing();', 'glPushAttrib(GL_COLOR_BUFFER_BIT);')


# A preprocessor line carries no scope in the body scan either, or everything a
# loop queues below its first #ifdef is invisible and the loop is never marked.
@case('sprite_batch', 'Blocks5/src/eye.cpp')
def c_sprite_batch_loop_ifdef(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);',
              '\tEngine::inst().flushSprites();\n\tfor(int i = 0; i < 4; i++)\n\t{\n'
              '\t\tglBegin(GL_QUADS);\n\t\tglEnd();\n'
              '#ifdef PREFETCH_RENDER\n\t\tprefetch(i);\n#endif\n'
              '\t\tEngine::inst().renderSprites(sprites, color);\n\t}')


# The arms of a preprocessor conditional never both compile, so a queue in one
# does not ask the other to flush - the same rule the branches of an else chain
# get, and the shape an #ifdef around a raw pass would take.
@case('sprite_batch', 'Blocks5/src/eye.cpp', quiet=True)
def c_sprite_batch_pp_arms(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);',
              '\tEngine::inst().flushSprites();\n#ifdef __EMSCRIPTEN__\n'
              '\tEngine::inst().renderSprites(sprites, color);\n#else\n'
              '\tglBegin(GL_QUADS);\n\tglEnd();\n#endif')


# What an arm queued still stands after the #endif, though.
@case('sprite_batch', 'Blocks5/src/eye.cpp')
def c_sprite_batch_pp_merge(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);',
              '\tEngine::inst().flushSprites();\n#ifdef __EMSCRIPTEN__\n'
              '\tEngine::inst().renderSprites(sprites, color);\n#endif\n'
              '\tglBegin(GL_QUADS);\n\tglEnd();')


# A loop head need not begin its line.
@case('sprite_batch', 'Blocks5/src/eye.cpp')
def c_sprite_batch_inline_loop(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);',
              '\tEngine::inst().flushSprites();\n\tif(alive) for(int i = 0; i < 4; i++)\n\t{\n'
              '\t\tglBegin(GL_QUADS);\n\t\tglEnd();\n'
              '\t\tEngine::inst().renderSprites(sprites, color);\n\t}')


# A do-block is a loop as much as a for is.
@case('sprite_batch', 'Blocks5/src/eye.cpp')
def c_sprite_batch_do_loop(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);',
              '\tEngine::inst().flushSprites();\n\tdo\n\t{\n'
              '\t\tglBegin(GL_QUADS);\n\t\tglEnd();\n'
              '\t\tEngine::inst().renderSprites(sprites, color);\n\t} while(alive);')


# The loop's own head line is part of its body: a one-liner queues there.
@case('sprite_batch', 'Blocks5/src/eye.cpp')
def c_sprite_batch_loop_oneline(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);',
              '\tEngine::inst().flushSprites();\n'
              '\tfor(int i = 0; i < 4; i++) { glBegin(GL_QUADS); glEnd(); '
              'Engine::inst().renderSprite(pos, t, s, color); }')


# renderSprite in the singular queues as much as renderSprites does - it is
# what Level::renderShine calls.
@case('sprite_batch', 'Blocks5/src/eye.cpp')
def c_sprite_batch_singular_queue(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);',
              '\tEngine::inst().flushSprites();\n'
              '\tEngine::inst().renderSprite(pos, t, s, color);\n'
              '\tglBegin(GL_QUADS);\n\tglEnd();')


# An else belongs to its if wherever it is written. A reindent moves one
# without making it any less an alternative to the branch above.
@case('sprite_batch', 'Blocks5/src/eye.cpp', quiet=True)
def c_sprite_batch_else_column(p):
    p.replace('\tif(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);',
              '\tEngine::inst().flushSprites();\n\tif(layer == RL_MAIN)\n\t{\n'
              '\t\tEngine::inst().renderSprites(sprites, color);\n\t}\n'
              '\t\telse\n\t\t{\n\t\t\tglBegin(GL_QUADS);\n\t\t\tglEnd();\n\t\t}')


# A macro body is not code at the point the directive stands, and reading it as
# code attributes whatever it holds to the function above.
@case('sprite_batch', 'Blocks5/src/eye.cpp', quiet=True)
def c_sprite_batch_macro_body(p):
    p.replace('void Eye::onRender(RenderLayer layer,',
              '#define DRAW_QUAD() \\\n\tglBegin(GL_QUADS); \\\n\tglEnd();\n\n'
              'void Eye::onRender(RenderLayer layer,')


# A name is a function boundary only where it is a definition: a macro taking
# an argument at column 0 is not one, and reading it as one would hand the
# lines below it a name of their own.
#
# level.cpp is where that is observable, because it is the file gl_state reads
# by named function rather than whole - so a raw call wrongly attributed to
# "TEX_TRACE" falls outside the scope and is silently let through. The macro
# and the fault therefore go in together, and the check has to report anyway.
@case('gl_state', 'Blocks5/src/level.cpp')
def c_gl_state_macro_not_a_start(p):
    p.replace('\tEngine::inst().renderSprite(p_shine,',
              '#define TEX_TRACE(x) ((void)0)\n'
              '\tglBindTexture(GL_TEXTURE_2D, 0);\n'
              '\tEngine::inst().renderSprite(p_shine,')


# An attribute mask this check cannot read is not thereby safe.
@case('gl_state', 'Blocks5/src/lava.cpp')
def c_gl_state_opaque_mask(p):
    p.replace('GL::pushTexturing();', 'glPushAttrib(savedBits);')


# Whether a bracket is safe is a question about its own function, not the file.
@case('gl_state', 'Blocks5/src/teleporter.cpp', quiet=True)
def c_gl_state_mask_per_function(p):
    p.replace('GL::pushTexturing();',
              'glPushAttrib(GL_LINE_BIT);\n\tGL::pushTexturing();')
    p.replace('GL::popTexturing();',
              'GL::popTexturing();\n\tglPopAttrib();')


# A helper indented some other way is still a function, and must not inherit
# the name - and so the exemption - of the one above it.
@case('gl_state', 'Blocks5/src/texture.cpp')
def c_gl_state_space_indent(p):
    p.replace('void Texture::cleanUp()',
              'namespace\n{\n    void debugBind(GLuint id)\n    {\n'
              '        glBindTexture(GL_TEXTURE_2D, id);\n    }\n}\n\nvoid Texture::cleanUp()')


# An onRender named in a comment is prose, not a declaration, and must not pull
# a whole file into the scanned set or report one that is already in it.
@case('gl_state', 'Blocks5/src/gs_menu.cpp', quiet=True)
def c_gl_state_comment_signature(p):
    p.replace('void GS_Menu::onRender()',
              '// The object layer draws through Object::onRender(RenderLayer layer, ...).\n'
              'void GS_Menu::onRender()')


# A commented-out draw inside one of the two functions read by name is not a
# draw either.
@case('sprite_batch', 'Blocks5/src/level.cpp', quiet=True)
def c_sprite_batch_reached_comment(p):
    p.replace('\tEngine::inst().renderSprite(p_shine,',
              '\t// glBegin(GL_QUADS); glEnd();\n\tEngine::inst().renderSprite(p_shine,')


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
