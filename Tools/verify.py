#!/usr/bin/env python3
# -*- coding: ascii -*-
"""verify.py - static checks over the Blocks 5 tree.

One pass without a compiler, in a few seconds. The checks look for exactly
the sort of mistake that appears silently while editing and that neither the
compiler nor a look at the diff will find: a string that a rename took with
it, a GUI path that no longer exists, an attribute that is written and read
nowhere, a new member variable with no initial value, a source file missing
from the project.

    python3 Tools/verify.py             all checks
    python3 Tools/verify.py --list      the names
    python3 Tools/verify.py --only gui_paths
    python3 Tools/verify.py --quiet     the summary only

Exit code 1 as soon as anything is reported.

Two slow checks stand beside it and run only on request:
    Tools/syntax.sh    compiles every source file with mingw (-fsyntax-only)
    WebBuild/build.sh  builds the browser build
"""

import glob
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'Blocks5', 'src')
DATA = os.path.join(ROOT, 'Blocks5', 'data')
WEB = os.path.join(ROOT, 'WebBuild')

# Vendored, not ours. stackwalker is third-party and CRLF.
VENDORED = ('stackwalker.cpp', 'stackwalker.h')

CHECKS = []


def check(name):
    def wrap(fn):
        CHECKS.append((name, fn))
        return fn
    return wrap


def read(path, encoding='latin-1'):
    return io.open(path, encoding=encoding, newline='').read()


def source_files(exts=('.cpp', '.h', '.c')):
    """Every source file that is ours - not libs/, not the build outputs."""
    out = []
    for base in ('Blocks5/src', 'WebBuild', 'PWEncrypt', 'ShowUserDir'):
        for root, dirs, files in os.walk(os.path.join(ROOT, base)):
            parts = root.split(os.sep)
            if 'libs' in parts or 'build' in parts or 'build-test' in parts or 'build-asan' in parts:
                continue
            for f in sorted(files):
                if f.endswith(exts) and f not in VENDORED:
                    out.append(os.path.join(root, f))
    return out


def prose_files():
    """Files whose comments are prose but which source_files() does not reach.

    LinuxBuild is not in that list at all - it is not part of the Windows
    project, which is what most of the checks are about - and neither list has
    ever held a script, a page or a config. That is how a wholly German
    .htaccess sat in WebBuild through the translation sweep: it has no
    extension, so nothing was looking at it. Each entry says how a comment
    begins there."""
    out = []
    for base in ('LinuxBuild', 'WebBuild', 'Tools'):
        for root, dirs, files in os.walk(os.path.join(ROOT, base)):
            parts = root.split(os.sep)
            if ('libs' in parts or 'build' in parts or 'build-test' in parts
                    or 'build-asan' in parts or 'node_modules' in parts
                    or '__pycache__' in parts):
                continue
            for f in sorted(files):
                if f in VENDORED:
                    continue
                if f.endswith(('.cpp', '.h', '.c', '.js')):
                    out.append((os.path.join(root, f), '//'))
                elif f.endswith(('.sh', '.py')) or f == 'htaccess':
                    out.append((os.path.join(root, f), '#'))
    return out


BASELINE = '95660bb'      # the last commit before this collaboration
_baseline_cache = {}


def original_lines(rel):
    """The file as it originally stood - for checks that are to judge only
    what is new. Empty set if the file did not exist back then or git cannot
    be reached."""
    if rel in _baseline_cache:
        return _baseline_cache[rel]
    lines = set()
    try:
        import subprocess
        names = subprocess.check_output(
            ['git', '-C', ROOT, 'ls-tree', '-r', '--name-only', BASELINE],
            stderr=subprocess.DEVNULL).decode('latin-1').split('\n')
        match = None
        for n in names:
            if n.lower() == rel.lower():
                match = n
                break
        if match:
            text = subprocess.check_output(['git', '-C', ROOT, 'show', BASELINE + ':' + match],
                                           stderr=subprocess.DEVNULL).decode('latin-1')
            lines = set(text.replace('\r\n', '\n').split('\n'))
    except Exception:                                            # noqa: BLE001
        pass
    _baseline_cache[rel] = lines
    return lines


def end_of_literal(text, i):
    """One past the closing quote of the literal opening at text[i], which is
    a \' or a ". The character literal has to be lexed even though nothing
    here looks inside one: '"' otherwise opens a string that runs to the next
    quote in the file, and testhooks.cpp writes exactly that - one line of it
    blanked 165 lines of code that no check then read."""
    quote, j, n = text[i], i + 1, len(text)
    while j < n and text[j] != quote:
        if text[j] == '\n':
            break                      # neither kind spans a line unescaped
        j += 2 if text[j] == '\\' else 1
    return min(j + 1, n)


def strip_comments(text):
    """Empty out the comments and keep the strings - so that a pattern does
    not fire inside a commented-out leftover."""
    out = []
    i, n = 0, len(text)
    while i < n:
        if text.startswith('//', i):
            j = text.find('\n', i)
            while j > 0 and text[j - 1] == '\\':       # a continued comment
                j = text.find('\n', j + 1)
            i = n if j < 0 else j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            i = n if j < 0 else j + 2
        elif text[i] in '"\'':
            j = end_of_literal(text, i)
            out.append(text[i:j])
            i = j
        else:
            out.append(text[i])
            i += 1
    return ''.join(out)


def blank_noncode(text):
    """Comments and the insides of strings replaced by spaces, with every
    newline kept - so a pattern cannot fire inside one and the line numbers
    still line up. strip_comments() collapses instead, which loses them."""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        if text.startswith('//', i):
            j = text.find('\n', i)
            while j > 0 and text[j - 1] == '\\':       # a continued comment
                j = text.find('\n', j + 1)
            j = n if j < 0 else j
            for k in range(i, j):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, min(j, n)):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        elif text[i] in '"\'':
            j = end_of_literal(text, i)
            for k in range(i + 1, j - 1):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        else:
            i += 1
    return ''.join(out)


def function_starts(code):
    """Where each function definition begins, as (offset, line, name) triples
    in order, over text already blanked by blank_noncode().

    A definition sits at column 0 - or one level in, inside the anonymous
    namespace that hint.cpp, font.cpp and diamondmachine.cpp open, which is
    why this counts the namespace nesting instead of testing for column 0. A
    helper missed there does not merely go unattributed: it inherits the name,
    and therefore the exemption, of the function above it."""
    out = []
    lines = code.split('\n')
    base, stack, offset = 0, [], 0
    for i, line in enumerate(lines):
        indent = len(line) - len(line.lstrip())
        bare = line.strip()
        if bare.startswith('namespace') and indent == base and not bare.endswith(';'):
            stack.append(base)
            # Where the body really sits, not one column in: a helper indented
            # with spaces would otherwise not be a function start at all, and
            # would inherit the name - and the exemption - of the one above.
            base = indent + 1
            for j in range(i + 1, len(lines)):
                nxt = lines[j].strip()
                if not nxt or nxt == '{':
                    continue
                if not nxt.startswith('}'):
                    base = len(lines[j]) - len(lines[j].lstrip())
                break
        elif bare.startswith('}') and stack and indent < base:
            base = stack.pop()
        elif indent == base and bare and bare[0] not in '#}/*' and '(' in line:
            m = re.search(r'\b(\w+::\w+|\w+)\s*\(', line)
            if m:
                out.append((offset + m.start(1), code.count('\n', 0, offset) + 1,
                            m.group(1)))
        offset += len(line) + 1
    return out


def batch_sources():
    """The sources the sprite batch can reach, as (path, code, only) triples
    with the comments and strings blanked out. `only` names the functions to
    read; None means the whole file.

    Those that define an Object::onRender, which is the set
    Level::renderObjects walks - matched loosely, because a wrapped signature
    is still one. Plus two files with no onRender in them that every one of
    those reaches: texture.cpp, whose Texture::bind() is the funnel they all
    bind through, and linedrawer.cpp, whose draw() is the raw glDrawArrays
    behind every laser, wire and shot.

    And two named helpers, Level::renderShine and Font::drawText, which draw
    with a batch open although the rest of their files do not. Those two files
    are read for those two functions alone - level.cpp and font.cpp are
    otherwise full of drawing that runs with no batch at all, and reading them
    whole would report every line of it."""
    signature = re.compile(r'::onRender\s*\(\s*RenderLayer')
    WHOLE = ('Blocks5/src/texture.cpp', 'Blocks5/src/linedrawer.cpp')
    REACHED = {
        'Blocks5/src/level.cpp': {'Level::renderShine'},
        'Blocks5/src/font.cpp': {'Font::drawText'},
    }
    out = []
    for p in source_files():
        rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
        if not rel.startswith('Blocks5/src/'):
            continue
        code = blank_noncode(read(p))
        if signature.search(code) or rel in WHOLE:
            out.append((rel, code, None))
        elif rel in REACHED:
            out.append((rel, code, REACHED[rel]))
    return out


def dead_names(names, seen, what):
    """Whichever of `names` no longer names a function that was read. An
    exemption for a function that has been renamed or deleted is worse than no
    exemption at all: it is silent, and it says the case was thought about."""
    return ['Tools/verify.py: %s names %s, which no source defines any more'
            % (what, ', '.join(sorted(set(names) - seen)))] if set(names) - seen else []


def idle_names(names, used, what):
    """Whichever of `names` suppressed nothing. The third way an exemption goes
    stale: the function is still there and still read, and no longer contains
    what it was excused for - so the entry reads as a considered decision while
    standing for nothing, and dead_names() cannot see it."""
    return ['Tools/verify.py: %s excuses %s, which has nothing left to excuse'
            % (what, ', '.join(sorted(set(names) - used)))] if set(names) - used else []


# ---------------------------------------------------------------------------


@check('encoding')
def check_encoding():
    """Pure ASCII and LF in the sources, CRLF in the shipped files.

    data/languages.txt and the two readme.txt are Latin-1 with CRLF and are
    shipped that way. One single umlaut in a comment makes the encoding of the
    tree a question again."""
    bad = []
    for p in source_files():
        data = open(p, 'rb').read()
        rel = os.path.relpath(p, ROOT)
        try:
            data.decode('ascii')
        except UnicodeDecodeError as e:
            bad.append('%s: not ASCII (%s)' % (rel, e))
        if b'\r' in data:
            bad.append('%s: CRLF, expected LF' % rel)

    for rel in ('Blocks5/readme.txt', 'Blocks5/levels/readme.txt', 'Blocks5/data/languages.txt'):
        p = os.path.join(ROOT, rel)
        if not os.path.exists(p):
            bad.append('%s: missing' % rel)
            continue
        data = open(p, 'rb').read()
        if b'\r\n' not in data:
            bad.append('%s: no CRLF - this file is shipped that way' % rel)
        if data.count(b'\n') != data.count(b'\r\n'):
            bad.append('%s: mixed line endings' % rel)
    return bad


@check('project_files')
def check_project_files():
    """A new source file must be in the .vcxproj and in its .filters.

    There is no glob build: a file missing from either one is not compiled by
    Visual Studio, and nobody notices under Linux."""
    proj = os.path.join(ROOT, 'Blocks5', 'Blocks5.vcxproj')
    filt = proj + '.filters'
    if not os.path.exists(proj) or not os.path.exists(filt):
        return ['Blocks5.vcxproj or .filters missing']

    ptext, ftext = read(proj), read(filt)
    bad = []
    for f in sorted(os.listdir(SRC)):
        if not f.endswith(('.cpp', '.h')):
            continue
        if f == 'pch.cpp':          # the Create-PCH unit, listed with a rule of its own
            pass
        for name, text in (('Blocks5.vcxproj', ptext), ('Blocks5.vcxproj.filters', ftext)):
            if ('src\\' + f) not in text and ('src/' + f) not in text:
                bad.append('%s missing from %s' % (f, name))
    return bad


@check('display_lists')
def check_display_lists():
    """No display lists anywhere, in either build.

    They were a second way of keeping geometry beside the vertex arrays, and
    one that WebGL does not have at all - so every place that used one carried
    a second path under #ifdef __EMSCRIPTEN__, and a stub in gl_compat.cpp to
    make the browser link. All of that is gone; a glNewList added back would
    compile on Windows, link on Linux and fail only in the browser, which is
    the build nobody runs first."""
    calls = re.compile(r'\bgl(GenLists|NewList|EndList|CallLists?|DeleteLists|ListBase|IsList)\b')
    bad = []
    for p in source_files():
        rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
        for n, line in enumerate(read(p).split('\n'), 1):
            m = calls.search(line)
            if m:
                bad.append('%s:%d: gl%s - display lists are gone from this tree'
                           % (rel, n, m.group(1)))
    return bad


@check('hooks_layout')
def check_hooks_layout():
    """No BLOCKS5_TEST_HOOKS conditional in a header under Blocks5/src.

    The define reaches engine.cpp and testhooks.cpp and no other translation
    unit of the hundred and twenty, so a member declared behind it gives its
    class two sizes: the two files that have the define see one layout and
    everything else sees another, and every access from the rest of the tree
    then lands at the wrong offset. What that looks like is not a compile
    error and not a wrong number - it is a corrupt pointer in some unrelated
    read, which is as far from the cause as a bug gets.

    The rule is therefore that a header declares the same thing whatever the
    build, and the cost of obeying it is a few unused members in a shipped
    binary. engine.h says so beside batchTexture, two hundred lines from
    where the next person will add theirs, which is what this check is for.

    testhooks.h is exempt: what it guards is free function declarations in a
    namespace, and a declaration nobody calls has no layout to disagree
    about."""
    EXEMPT = {'Blocks5/src/testhooks.h'}
    guard = re.compile(r'^\s*#\s*(?:if|ifdef|ifndef|elif)\b.*\bBLOCKS5_TEST_HOOKS\b')
    bad = []
    seen = set()
    for path in source_files():
        rel = os.path.relpath(path, ROOT).replace(os.sep, '/')
        if not rel.startswith('Blocks5/src/') or not rel.endswith('.h'):
            continue
        if rel in EXEMPT:
            seen.add(rel)
            continue
        for n, line in enumerate(read(path).split('\n'), 1):
            if guard.search(line):
                bad.append('%s:%d: BLOCKS5_TEST_HOOKS in a header - declare it '
                           'whatever the build, or the class gets two sizes' % (rel, n))
    bad.extend(dead_names(EXEMPT, seen, 'the hooks_layout check'))
    return bad


@check('render_layers')
def check_render_layers():
    """A render layer is named, never a number.

    RenderLayer's values are single bits, so every one of the old magic
    numbers - 0, 1, 16, 42, 255, 736, 939 - now means either nothing or the
    wrong layer. C++ compares an enum to an int without a word, so such a
    line builds on all three platforms and is simply never true: the sprite
    texture stops being bound for the lava passes, the wires lose their
    offset, the speech balloons stop appearing. That is what this found the
    first time, in the five places the conversion missed."""
    calls = re.compile(r'\blayer\s*[=!]=\s*-?\d|\brenderObjects\s*\(\s*-?\d')
    bad = []
    for p in source_files():
        rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
        if not rel.startswith('Blocks5/src/'):
            continue
        for n, line in enumerate(read(p).split('\n'), 1):
            # The editor walks the two TILE layers by number, which is a
            # different thing entirely and stays a plain int.
            if 'setTileAt' in line or 'getTileAt' in line or 'tile[layer]' in line:
                continue
            if calls.search(line):
                bad.append('%s:%d: a render layer written as a number - use the RenderLayer name'
                           % (rel, n))
    return bad



def unscanned_onrender(scanned):
    """Sources declaring an onRender that batch_sources() does not read.

    The set is discovered by matching one spelling, and a declaration that
    spells its parameter differently - `const RenderLayer` is legal, and still
    overrides - would take a whole file out of both checks with nothing
    anywhere to say so. Asserting the set is what turns that from silence into
    a finding, the same reasoning as dead_names()."""
    decl = re.compile(r'\bonRender\s*\(\s*(?:const\s+)?RenderLayer')
    bad = []
    for p in source_files():
        rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
        if not rel.startswith('Blocks5/src/') or not decl.search(blank_noncode(read(p))):
            continue
        stem = rel.rsplit('.', 1)[0]
        if stem + '.cpp' in scanned or stem + '.h' in scanned:
            continue
        bad.append('%s: declares an onRender that the sprite_batch and gl_state '
                   'checks do not read - see batch_sources()' % rel)
    return bad


@check('sprite_batch')
def check_sprite_batch():
    """Anything an object draws outside the sprite batch has to flush it first.

    Level::renderObjects keeps a batch of sprite corners open across the whole
    object loop, and one glDrawArrays puts them up at the end. This game has no
    depth buffer, so painter's order is the only order there is: a queued quad
    is drawn with the GL state standing at the flush, not at the call. An object
    that draws raw geometry therefore has to flush first, or everything queued
    before it lands on top of what it drew instead of underneath.

    Nothing else would catch it. The sprites of the objects around it are what
    move, so the object that broke the rule looks right and its neighbours do
    not - and only on the screen that happens to have both.

    How far a flush reaches, in the three shapes that are not the obvious one.
    It covers only as far as the block it stands in: one inside an if() says
    nothing about the code after it, which is the shape player.cpp has. One
    written as the body of a braceless if or loop covers less still - only the
    rest of its own line, since what follows stands at the same column and no
    indentation rule can tell the two apart. And one standing *outside* a loop
    whose body queues does not cover a draw inside that loop at all: the second
    turn round begins with the batch non-empty, which a walk down the lines
    cannot see because the draw is written above the queue.

    The branches of one if/else chain are read as the alternatives they are, so
    a queue in the first does not ask the second to flush again - but whatever
    the chain as a whole may have queued stands after it. The arms of a
    preprocessor conditional get the same treatment for a stronger reason: they
    never both compile. A preprocessor line is not a dedent either, and the body
    of a macro is not code at the point its #define stands.

    What it cannot see, besides a call: a one-line if/else holding both a flush
    and a draw, since a line is read as one sequence in column order.

    What it cannot see is a call: a helper of an object's own that queues, or
    one that draws, is a name to this and nothing more. That is why
    LineDrawer::draw flushes at its own definition rather than at its callers."""
    breaking = re.compile(r'\bglBegin\s*\(|\bglDraw\w*\s*\(|\bglRect\w*\s*\(|'
                          r'\bdrawQuadArray\s*\(')
    # The two spellings that leave a quad in the batch, and the whole set of
    # them: every other way of drawing a sprite from here - Level::renderShine,
    # Font::renderText - binds a texture of its own and so flushes on the way
    # in and on the way out again.
    queues = re.compile(r'\brenderSprites?\s*\(')
    # GLState's wrappers flush before they change anything, so reaching one is
    # reaching a flush - and so is Texture::bind()/unbind(), which is written
    # that way and nothing else in these files is.
    flushes = re.compile(r'\bflushSprites\s*\(|\bGLState::|'
                         r'[\w)\]]\s*(?:->|\.)\s*(?:un)?bind\s*\(\s*\)')
    # A statement written as the body of a braceless conditional, on the line
    # of the conditional itself. The bare `else` alternative is a backstop: the
    # chain bookkeeping below already answers an else it has paired with an if,
    # which is every one written at the same column - so nothing can reach it
    # while that pairing holds, and it costs one alternative to be right when it
    # does not.
    conditional = re.compile(r'^\s*(?:\}\s*)?(?:else\s+)?(?:if|for|while)\s*\(.*\)\s*\S'
                             r'|^\s*else\s+\S'
                             r'|^\s*(?:case\b[^:]*|default\s*):\s*\S')
    # A loop head anywhere on its line - `if(x) for(...)` is one, and the tree
    # writes that - but never the `} while(...)` that ends a do-block, whose
    # body is above it rather than below.
    loopHead = re.compile(r'\b(?:for|while)\s*\(|^\s*do\b')
    ifHead = re.compile(r'^\s*if\s*\(')
    elseHead = re.compile(r'^\s*(?:\}\s*)?else\b')
    ppIf = re.compile(r'^\s*#\s*if')
    ppElse = re.compile(r'^\s*#\s*el(?:se|if)')
    ppEnd = re.compile(r'^\s*#\s*endif')
    # Helpers entered with the batch already put up, which is the whole of the
    # exemption: what such a function does from there is read like any other.
    # Qualified, so that a same-named method of another class does not inherit
    # it.
    EXEMPT = {
        # Hint::renderNote() flushes and then binds its own texture; the mesh
        # is the geometry it draws under that binding.
        'Hint::renderNoteMesh',
    }
    bad, seen, named, scanned, used = [], set(), set(EXEMPT), set(), set()
    for rel, text, only in batch_sources():
        scanned.add(rel)
        named |= (only or set())
        lines = text.split('\n')
        starts = dict((n, name) for _, n, name in function_starts(text))
        seen |= set(starts.values())

        # Loops whose body queues, by line index: a flush before such a loop
        # says nothing about the draw inside it.
        requeues = set()
        for i, line in enumerate(lines):
            if not loopHead.search(line) or line.strip().startswith('}'):
                continue
            indent = len(line) - len(line.lstrip())
            body, j, opened = [line], i + 1, False
            while j < len(lines):
                bare = lines[j].strip()
                if bare and not bare.startswith('#'):
                    # The brace of the body stands at the loop's own column in
                    # this tree, so it cannot be the end of the body - and a
                    # preprocessor line carries no scope, so reading one as the
                    # end would hide everything a loop queues below its first
                    # #ifdef.
                    if bare == '{' and not opened:
                        opened = True
                    elif len(lines[j]) - len(lines[j].lstrip()) <= indent:
                        break
                    else:
                        body.append(lines[j])
                j += 1
            if any(queues.search(b) for b in body):
                requeues.add(i)

        # A closing brace whose `else` stands on the next line, which is how
        # this tree writes a chain: the branch is not over there, so the chain
        # must not be closed and merged until the last branch really ends.
        heldOpen = set()
        for i, line in enumerate(lines):
            if not line.strip().startswith('}'):
                continue
            j = i + 1
            while j < len(lines) and not lines[j].strip():
                j += 1
            if j < len(lines) and re.match(r'^\s*else\b', lines[j]):
                heldOpen.add(i)

        func, flushed, flushIndent, chain, exemptCover = '', False, 0, {}, False
        # The arms of a preprocessor conditional, as a stack of
        # [state at the #if, its indent, anything any arm queued]. It cannot be
        # keyed on the column the way an else chain is: these stand at column 0
        # whatever they wrap.
        pp, continued = [], False
        for n, line in enumerate(lines, 1):
            # A #define continued with a backslash is still the directive: its
            # body is not code at this point in the file, and reading it as code
            # attributes whatever it holds to the function above.
            if continued:
                continued = line.rstrip().endswith('\\')
                continue
            if line.lstrip().startswith('#') and line.rstrip().endswith('\\'):
                continued = True
                continue
            if ppIf.match(line):
                pp.append([flushed, flushIndent, False])
            elif ppElse.match(line) and pp:
                flushed, flushIndent = pp[-1][0], pp[-1][1]
            elif ppEnd.match(line) and pp:
                head, headIndent, queued = pp.pop()
                flushed, flushIndent = head and not queued, headIndent
            # A preprocessor line carries no scope, and this tree writes them at
            # column 0 wherever they sit - so reading one as a dedent would ask
            # for a flush again after every #ifdef inside a function body.
            if line.strip() and not line.lstrip().startswith('#'):
                indent = len(line) - len(line.lstrip())
                if n in starts:
                    func, chain, pp = starts[n], {}, []
                    flushed, flushIndent = func in EXEMPT, 0
                    exemptCover = flushed
                else:
                    # An if/else chain the line has stepped out of: what stands
                    # after it is the state before it, minus anything any
                    # branch queued.
                    # A lone brace is the chain's own body opening, and a
                    # closing one whose else stands on the next line ends a
                    # branch rather than the chain: this tree writes both at the
                    # column of the if they belong to.
                    if line.strip() != '{' and (n - 1) not in heldOpen:
                        for k in sorted(chain, reverse=True):
                            if indent < k or (indent == k and not elseHead.match(line)):
                                head, headIndent, queued = chain.pop(k)
                                flushed, flushIndent = head and not queued, headIndent
                    # A flush holds only inside the block it stands in.
                    if flushed and indent < flushIndent:
                        flushed = False
                    # The nearest chain this else can belong to, rather than
                    # one at its exact column: a reindent or a hand-merge moves
                    # an else without making it any less an alternative to the
                    # branch above it.
                    outer = [k for k in chain if k <= indent]
                    if elseHead.match(line) and outer:
                        head = chain[max(outer)]
                        flushed, flushIndent = head[0], head[1]
                    elif ifHead.match(line):
                        chain[indent] = [flushed, flushIndent, False]
            if (n - 1) in requeues:
                flushed = False
            wasFlushed, wasIndent = flushed, flushIndent
            # In column order, because a line can both queue and draw.
            events = ([(m.start(), 'f') for m in flushes.finditer(line)] +
                      [(m.start(), 'q') for m in queues.finditer(line)] +
                      [(m.start(), 'b') for m in breaking.finditer(line)])
            for e in sorted(events, key=lambda e: e[0]):
                if e[1] == 'f':
                    flushed = True
                    flushIndent = len(line) - len(line.lstrip())
                    exemptCover = False
                elif e[1] == 'q':
                    flushed = False
                    for entry in list(chain.values()) + pp:
                        entry[2] = True
                elif not flushed and (only is None or func in only):
                    bad.append('%s:%d: %s() draws with sprites possibly queued - '
                               'flush the batch first' % (rel, n, func))
                elif exemptCover:
                    # Drawn under the exemption rather than under a flush of
                    # this function's own, which is the entry doing its work.
                    used.add(func)
            if conditional.match(line):
                flushed, flushIndent = flushed and wasFlushed, wasIndent
    return (bad + dead_names(named, seen, 'the sprite_batch check')
            + idle_names(EXEMPT, used, 'the sprite_batch check')
            + unscanned_onrender(scanned))


@check('gl_state')
def check_gl_state():
    """An object changes the texture state through GLState, never raw.

    Three pieces of GL state decide what a queued sprite comes out looking
    like: the GL_TEXTURE_2D binding, whether texturing is on, and the texture
    matrix. A batched quad is drawn with the state standing at the flush and
    not at the call, and there is no depth buffer here to sort it out
    afterwards - so whatever moves one of the three has to put the batch up
    first. GLState does that; the raw calls do not.

    The two exemptions below are not about cost - a GLState call with nothing
    queued is one comparison, so going through it is free even where the batch
    provably cannot be open. They are the two places that build a value rather
    than set drawing state.

    Scoped to what Level::renderObjects can reach with a batch open - see
    batch_sources(). The crossfades, the GUI and the credits run with none open
    and are left alone deliberately, which keeps the ban small enough to read.

    What it cannot see: GL_TEXTURE_2D reached through a variable rather than
    written out, and anything a called function does. A regex has no types and
    follows no calls, and that is the honest limit."""
    raw = re.compile(r'\bgl(?:Enable|Disable)\s*\(\s*GL_TEXTURE_2D\s*\)'
                     r'|\bglBindTexture\s*\('
                     r'|\bglMatrixMode\s*\(\s*GL_TEXTURE\s*\)')
    # The attribute stack is judged by its mask, and a pop by the pushes in the
    # same function - there being no way to pair the two by reading. These five
    # masks carry nothing a GL_QUADS batch is drawn under, so a bracket around a
    # glLineWidth or a glPointSize is left alone: banning it would be a dead end,
    # since GLState has no entry point that could stand in for one. Everything
    # else is reported, a mask this list does not know included - GL_ENABLE_BIT
    # carries the texturing enable, and GL_COLOR_BUFFER_BIT the blend function.
    SAFE_BITS = ('GL_LINE_BIT', 'GL_POINT_BIT', 'GL_CURRENT_BIT',
                 'GL_TRANSFORM_BIT', 'GL_HINT_BIT')
    push = re.compile(r'\bglPushAttrib\s*\(([^);]*)\)')
    pop = re.compile(r'\bglPopAttrib\s*\(')
    # Two that build a value rather than set drawing state: they put a scale on
    # the texture matrix stack and read it straight back with glGetDoublev, at
    # load time, with no batch open. Doing the arithmetic in C++ instead would
    # be tidier and is not the same thing - a driver that keeps the stack in
    # floats hands back a rounded matrix, and that rounding is what the game has
    # always sampled with. Their glPushAttrib(GL_TRANSFORM_BIT) bracket is also
    # why the ban above names no mask: a push saves whatever its mask asks for,
    # and one written GL_ENABLE_BIT | GL_TEXTURE_BIT is the same mistake as one
    # written GL_ENABLE_BIT.
    EXEMPT = {'Texture::reload', 'Texture::loadSubTexture'}
    bad, seen, named = [], set(), set(EXEMPT)
    for rel, text, only in batch_sources():
        named |= (only or set())
        # Where each function starts, so a match can be attributed to one. The
        # search itself runs over the whole text and not line by line, because
        # every pattern above allows whitespace inside the call - a glDisable
        # with its argument on the next line is the same mistake.
        starts = function_starts(text)
        seen |= set(name for _, _, name in starts)

        def owner(at):
            func = ''
            for start, _, name in starts:
                if start > at:
                    break
                func = name
            return func

        # A function whose every glPushAttrib names only safe bits may pop as
        # well. One that pops without pushing is restoring something it cannot
        # be read against, so it is reported.
        #
        # Per function and not per file, which changes how much is reported
        # rather than whether: a risky push is a finding on its own, and asking
        # the file would add the safe bracket beside it. There is no selftest
        # case behind that half for the same reason - both shapes report.
        pushes = {}
        for m in push.finditer(text):
            bits = re.findall(r'\bGL_\w+', m.group(1))
            safe = bool(bits) and all(b in SAFE_BITS for b in bits)
            pushes.setdefault(owner(m.start()), []).append((m, safe))
        risky = set(f for f, ms in pushes.items() if not all(safe for _, safe in ms))

        hits = [(m, m.group(0)) for m in raw.finditer(text)]
        for f, ms in pushes.items():
            if f in risky:
                hits += [(m, m.group(0)) for m, _ in ms]
        for m in pop.finditer(text):
            f = owner(m.start())
            if f in risky or f not in pushes:
                hits.append((m, m.group(0)))

        for m, hit in sorted(hits, key=lambda h: h[0].start()):
            func = owner(m.start())
            if func in EXEMPT or (only is not None and func not in only):
                continue
            n = text.count('\n', 0, m.start()) + 1
            bad.append('%s:%d: %s - go through GLState, which flushes the sprite batch'
                       % (rel, n, ' '.join(hit.split())))
    return bad + dead_names(named, seen, 'the gl_state check')


@check('naming')
def check_naming():
    """The filename is the class name in lower case.

    That is the one naming rule of this tree that holds without exception -
    for every class with a base class, in every header, CF_Star in cf_star.h
    just as File_Real in file_real.h - and in a flat directory with over
    two hundred entries it is the whole navigation: symbol seen, file known,
    without a search. That is exactly why it stands here: a rule that lives
    only in CLAUDE.md goes stale.

    Only what has a base class is looked at. A forward declaration has none,
    and a helper class without a base - Sprites in sprite.h - follows a rule
    of its own."""
    pattern = re.compile(r'^\s*class\s+(\w+)\s*:\s*public\b', re.M)
    bad = []
    for name in sorted(os.listdir(SRC)):
        if not name.endswith('.h'):
            continue
        stem = name[:-2]
        for m in pattern.finditer(read(os.path.join(SRC, name))):
            cls = m.group(1)
            if cls.lower() != stem:
                bad.append('%s: class %s belongs in %s.h' % (name, cls, cls.lower()))
    return bad


@check('version')
def check_version():
    """The version number lives in four places and must not drift.

    The .rc has been overlooked once already and stayed wrong for a whole
    version - that is the number Explorer shows and the one that stands in a
    crash report."""
    found = {}

    m = re.search(r'p_localVersion\s*=\s*"([\d.]+)"', read(os.path.join(SRC, 'main.cpp')))
    if m:
        found['src/main.cpp p_localVersion'] = m.group(1)

    iss = os.path.join(ROOT, 'Blocks5', 'setup', 'Blocks 5.iss')
    if os.path.exists(iss):
        t = read(iss)
        m = re.search(r'AppVersion=([\d.]+)', t)
        if m:
            found['Blocks 5.iss AppVersion'] = m.group(1)
        m = re.search(r'OutputBaseFilename=blocks-5-([\d.]+)-setup', t)
        if m:
            found['Blocks 5.iss OutputBaseFilename'] = m.group(1)

    rc = read(os.path.join(SRC, 'resources.rc'))
    for key in ('FILEVERSION', 'PRODUCTVERSION'):
        m = re.search(key + r'\s+([\d,\s]+)', rc)
        if m:
            parts = [p.strip() for p in m.group(1).split(',')]
            found['resources.rc ' + key] = '.'.join(parts[:3])
    for key in ('FileVersion', 'ProductVersion'):
        m = re.search(r'VALUE\s+"' + key + r'",\s*"([\d.]+)"', rc)
        if m:
            found['resources.rc ' + key] = m.group(1)

    readme = os.path.join(ROOT, 'Blocks5', 'readme.txt')
    if os.path.exists(readme):
        m = re.search(r'==\s+v([\d.]+)', read(readme))
        if m:
            found['readme.txt Banner'] = m.group(1)

    if not found:
        return ['no version number found - the patterns no longer match']
    values = set(found.values())
    if len(values) == 1:
        return []
    return ['version numbers drift apart:'] + \
           ['    %-34s %s' % (k, v) for k, v in sorted(found.items())]


@check('gui_paths')
def check_gui_paths():
    """Every element path in the code must exist in a dialog XML.

    A renamed element otherwise shows up only as a button that no longer does
    anything - gui[...] then hands back a null pointer."""
    known = set()
    for f in sorted(os.listdir(DATA)):
        if not f.endswith('.xml'):
            continue
        for m in re.finditer(r'name="([^"]+)"', read(os.path.join(DATA, f))):
            known.add(m.group(1))

    # The code holds whole paths ("Menu.ManagerPane.Manager.Close"); each part
    # is checked on its own, since the XML names only the individual pieces.
    allowed_missing = {
        'Game',                                   # created at runtime
        'LevelEditor.EditHintPane.EditHint.Text',  # ditto
    }
    bad = []
    pattern = re.compile(r'(?:gui\s*\[|getChild\s*\(|getElement\s*\()\s*"([A-Za-z][A-Za-z0-9_.]*)"')
    for p in source_files(('.cpp',)):
        rel = os.path.relpath(p, ROOT)
        text = read(p)
        for m in pattern.finditer(text):
            full = m.group(1)
            if full in allowed_missing:
                continue
            for part in full.split('.'):
                if part not in known:
                    line = text.count('\n', 0, m.start()) + 1
                    bad.append('%s:%d: "%s" - no element named "%s" in data/*.xml'
                               % (rel, line, full, part))
                    break
    return bad


@check('strings')
def check_strings():
    """Every $ID must be in languages.txt, in English and in German.

    The IDs are collected from the code and from the dialog XML."""
    langs = os.path.join(DATA, 'languages.txt')
    if not os.path.exists(langs):
        return ['data/languages.txt missing']

    text = read(langs)
    ids, bodies, current = set(), {}, None
    for line in text.split('\r\n' if '\r\n' in text else '\n'):
        if line.startswith('$'):
            # A '#' at the end means "do not join the lines" and is no part of
            # the id - loadStringDB() cuts it off just the same.
            current = line.strip().rstrip('#')
            ids.add(current)
            bodies[current] = set()
        elif line.startswith('\xa7') and current:
            bodies[current].add(line[1:3])

    bad = []
    for i in sorted(ids):
        if i == '$END_OF_FILE':      # end marker, no text
            continue
        for want in ('en', 'de'):
            if want not in bodies.get(i, set()):
                bad.append('languages.txt: %s has no %s text' % (i, want))

    used = set()
    for p in source_files(('.cpp', '.h')):
        for m in re.finditer(r'"(\$[A-Z][A-Z0-9_]*)"', read(p)):
            used.add(m.group(1))
    for f in sorted(os.listdir(DATA)):
        if f.endswith('.xml'):
            for m in re.finditer(r'>(\$[A-Z][A-Z0-9_]*)<', read(os.path.join(DATA, f))):
                used.add(m.group(1))

    for i in sorted(used - ids):
        bad.append('%s is used but is not in languages.txt' % i)
    return bad


@check('bindings')
def check_bindings():
    """Every %BINDING{...} must name an action that main.cpp registers.

    The expansion answers an unknown action the same way it answers an unbound
    one - with the word for "not assigned", or with nothing at all in the
    optional form. A typo in the name therefore produces no error anywhere:
    the sentence simply stops naming a key, and only a player ever sees it."""
    langs = os.path.join(DATA, 'languages.txt')
    main = os.path.join(SRC, 'main.cpp')
    if not os.path.exists(langs) or not os.path.exists(main):
        return ['languages.txt or main.cpp missing']

    registered = set(re.findall(r'registerAction\s*\(\s*"(\$A_[A-Z0-9_]*)"', read(main)))
    if not registered:
        return ['no registerAction() calls found in main.cpp']

    bad = []
    text = read(langs)
    for n, line in enumerate(text.split('\r\n' if '\r\n' in text else '\n'), 1):
        # Only the bodies. A comment may well name the marker to explain it,
        # and the braces there hold no action.
        if not line.startswith('\xa7'):
            continue
        for m in re.finditer(r'%BINDING(?:_OPTIONAL_RIGHT)?\{([^}]*)\}', line):
            if m.group(1) not in registered:
                bad.append('languages.txt:%d: %%BINDING names %s, which no action is'
                           % (n, m.group(1)))
    return bad


@check('xml_attrs')
def check_xml_attrs():
    """An attribute written and read nowhere is dead weight or a typo.

    That is how "numLayers" once became "NUM_LAYERS" while a constant was
    being renamed, and the level's size check silently stopped working."""
    written, readd = {}, set()
    # The call arrow belongs in the pattern: otherwise "Attribute(" matches the
    # tail of "SetAttribute(" as well, every written attribute counts as read,
    # and the check never finds anything.
    wpat = re.compile(r'(?:->|\.)\s*Set(?:Double|Int)?Attribute\s*\(\s*"([A-Za-z_][\w]*)"')
    rpat = re.compile(r'(?:->|\.)\s*(?:Query(?:Int|Double|Float|Bool)?Attribute|Attribute)'
                      r'\s*\(\s*"([A-Za-z_][\w]*)"')
    for p in source_files(('.cpp',)):
        text = strip_comments(read(p))
        rel = os.path.relpath(p, ROOT)
        for m in wpat.finditer(text):
            written.setdefault(m.group(1), rel)
        for m in rpat.finditer(text):
            readd.add(m.group(1))

    # The GUI reads its attributes through readAttributes(), not through these
    # names; the XML is written there, not read.
    return ['%s: attribute "%s" is written but read nowhere' % (where, name)
            for name, where in sorted(written.items()) if name not in readd]


@check('config')
def check_config():
    """What Engine::saveConfig writes must be read back again.

    Otherwise config.xml loses a setting at every start.

    The check looks in engine.cpp *and* in every u_*.cpp: since the upscale
    filters are classes, each writes its own element (Upscaler::saveConfig),
    and a pair that only the Engine knows no longer exists. A check that did
    not follow that move would find neither half any more and would stay
    silent - and a check that can stay silent is worse than none."""
    files = ['engine.cpp'] + sorted(f for f in os.listdir(SRC)
                                    if f.startswith('u_') and f.endswith('.cpp'))

    def bodies(text, method):
        """The bodies of every function whose name ends in ::<method>(."""
        out = []
        for m in re.finditer(r'\b\w+::' + method + r'\s*\([^)]*\)\s*\{', text):
            depth, i = 0, m.end() - 1
            while i < len(text):
                if text[i] == '{':
                    depth += 1
                elif text[i] == '}':
                    depth -= 1
                    if depth == 0:
                        out.append(text[m.end():i])
                        break
                i += 1
        return out

    written, read_names = set(), set()
    found_save, found_load = False, False
    for name in files:
        text = read(os.path.join(SRC, name))
        for body in bodies(text, 'saveConfig'):
            found_save = True
            written |= set(re.findall(r'new TiXmlElement\(\s*"([A-Za-z][\w]*)"', body))
        for body in bodies(text, 'loadConfig'):
            found_load = True
            read_names |= set(re.findall(r'FirstChildElement\(\s*"([A-Za-z][\w]*)"', body))
            read_names |= set(re.findall(r'NextSiblingElement\(\s*"([A-Za-z][\w]*)"', body))

    if not found_load or not found_save:
        return ['loadConfig() or saveConfig() not found']

    bad = []
    for name in sorted(written - read_names - {'Config'}):
        bad.append('config.xml: <%s> is written but not read' % name)
    for name in sorted(read_names - written - {'Config'}):
        bad.append('config.xml: <%s> is read but not written' % name)
    return bad


@check('ctor_init')
def check_ctor_init():
    """Scalar members that the constructor does not set.

    That is exactly what presentVertexBuffer depended on: without a
    framebuffer object, Engine::exit() read a random value and took it for a
    GL name.

    The search runs per class that has a constructor of its own in the .cpp,
    and only in that constructor's body. If it leaves out the majority of the
    members, the class follows a different rule - objects get their fields
    from readAttributes() - and the check stays silent.

    Only what has come in since the state before this collaboration is judged.
    A member that was there back then is set somewhere before the first read -
    init(), loadConfig(), setLogicRate() - and has been for ten years."""
    scalar = re.compile(
        r'^\s*(?:unsigned\s+|signed\s+)?'
        r'(bool|char|short|int|long|float|double|uint|uchar|ushort|ulong|size_t)\s+'
        r'([a-z_]\w*)\s*;\s*(?://.*)?$')

    def classes(htext):
        """(name, members) per class. Members of a nested struct belong to
        whatever declares them and stay outside - they sit one brace level
        deeper."""
        out, stack, depth = [], [], 0
        pending = None
        for line in htext.split('\n'):
            m = re.match(r'^\s*(?:class|struct)\s+(\w+)', line)
            if m and not line.rstrip().endswith(';'):
                pending = m.group(1)
            opens = line.count('{')
            closes = line.count('}')
            if opens and pending is not None:
                stack.append((pending, depth + 1, []))
                pending = None
            if stack and depth == stack[-1][1]:
                mm = scalar.match(line)
                if mm and 'static' not in line and 'const' not in line:
                    stack[-1][2].append(mm.group(2))
            depth += opens - closes
            while stack and depth < stack[-1][1]:
                name, _, members = stack.pop()
                out.append((name, members))
        while stack:
            name, _, members = stack.pop()
            out.append((name, members))
        return out

    def ctor_body(text, cls):
        m = re.search(r'\b%s::%s\s*\(' % (re.escape(cls), re.escape(cls)), text)
        if not m:
            return None
        brace = text.find('{', m.end())
        if brace < 0:
            return None
        depth, i = 0, brace
        while i < len(text):
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
                if depth == 0:
                    return text[m.start():i]
            i += 1
        return None

    def is_set(name, body):
        if re.search(r'(?:^|[^\w.>])%s\s*=(?!=)' % re.escape(name), body, re.M):
            return True
        if re.search(r'this->%s\s*=(?!=)' % re.escape(name), body):
            return True
        if re.search(r'[:,]\s*%s\s*\(' % re.escape(name), body):        # initialiser list
            return True
        if re.search(r'&%s\b' % re.escape(name), body):                  # filled by address
            return True
        return False

    bad = []
    for header in source_files(('.h',)):
        cpp = header[:-2] + '.cpp'
        if not os.path.exists(cpp):
            continue
        text = strip_comments(read(cpp))
        rel = os.path.relpath(header, ROOT).replace(os.sep, '/')
        for cls, members in classes(read(header)):
            if not members:
                continue
            body = ctor_body(text, cls)
            if body is None:
                continue
            missing = [m for m in members if not is_set(m, body)]
            # If the constructor sets fewer than half of them, the class
            # follows a different rule and the check says nothing about it.
            if len(missing) * 2 > len(members):
                continue
            was = original_lines(rel)
            missing = [m for m in missing
                       if not any(re.search(r'\b%s\s*(\[|;)' % re.escape(m), l) for l in was)]
            for m in missing:
                bad.append('%s: %s::%s is not set in the constructor' % (rel, cls, m))
    return bad


@check('assets')
def check_assets():
    """A filename in the code must exist on disk, spelled exactly so.

    Under Windows "Sprites.png" is the same file as "sprites.png", under Linux
    it is not, and since the native build exists such a name falls flat there
    at runtime."""
    exact = set()
    lower = {}
    for base in ('Blocks5/data', 'Blocks5/levels', 'Blocks5'):
        for root, dirs, files in os.walk(os.path.join(ROOT, base)):
            if 'libs' in root.split(os.sep):
                continue
            for f in files:
                exact.add(f)
                lower.setdefault(f.lower(), set()).add(f)

    # Fragments, format strings and files created at runtime.
    runtime = re.compile(r'%|^\.|^/|:|\*|,|\s')
    generated = {'config.xml', 'progress.zip', 'crash_log.txt', 'log.txt',
                 'keyboard.dat', 'campaign.xml', '~campaignsave.zip'}
    bad = []
    lit = re.compile(r'"([A-Za-z0-9_][A-Za-z0-9_.\- ]*\.(?:png|xml|ogg|wav|txt|zip|dat))"')
    for p in source_files(('.cpp', '.h')):
        rel = os.path.relpath(p, ROOT)
        # Comments stripped: a filename standing in a comment as an example is
        # not one the game opens.
        for m in lit.finditer(strip_comments(read(p))):
            name = m.group(1)
            if runtime.search(name) or name.lower() in generated:
                continue
            if name in exact:
                continue
            if name.lower() in lower:
                bad.append('%s: "%s" is called %s on disk'
                           % (rel, name, ' or '.join(sorted(lower[name.lower()]))))
            else:
                bad.append('%s: "%s" does not exist' % (rel, name))
    return bad


@check('sounds')
def check_sounds():
    """Every sound playSound() names must be preloaded in loadSounds().

    Engine::playSound() requests the resource, creates the instance and
    releases it again at once. Manager::request() hands back a freshly loaded
    resource with a count of 1, so the release() after it puts that count at
    0 - and ~Sound deletes all of its instances together with the OpenAL
    source before play() ever reaches them. Nothing is audible then, and
    playSound() carries on with a pointer into nothing.

    All of it runs only because loadSounds() requests every sound once and
    never releases it. Anyone who plays a new one and forgets it there gets no
    compiler error and no line in the log - the sound simply stays away."""
    played = {}
    call = re.compile(r'playSound\s*\(')
    lit = re.compile(r'"([A-Za-z0-9_][A-Za-z0-9_.\-]*\.ogg)"')
    for p in source_files(('.cpp',)):
        rel = os.path.relpath(p, ROOT)
        text = strip_comments(read(p))
        for m in call.finditer(text):
            # Up to the closing bracket, to take in both branches of an
            # "x ? a : b" as well.
            depth, i = 1, m.end()
            while i < len(text) and depth:
                if text[i] == '(':
                    depth += 1
                elif text[i] == ')':
                    depth -= 1
                i += 1
            for name in lit.findall(text[m.end():i]):
                played.setdefault(name, rel)

    # The whole file and not just loadSounds(): the logo is requested in
    # onEnter() and never released, which does the same job. A new sound
    # belongs in the list all the same.
    loading = strip_comments(read(os.path.join(SRC, 'gs_loading.cpp')))
    preloaded = set(re.findall(r'request\("([^"]+\.ogg)"\)', loading))

    return ['%s: playSound("%s"), but gs_loading.cpp does not preload the sound'
            % (rel, name)
            for name, rel in sorted(played.items()) if name not in preloaded]


@check('sound_volumes')
def check_sound_volumes():
    """Every sound in data/sounds.xml must exist, with a factor under 1.

    The table is the only place left that still says a sound should play
    quieter than its file - before, that sat in the .ogg, and because the .wav
    beside it stayed louder, the intent was lost at the next re-encode. A typo
    in the filename would lead straight back there, silently: for an unknown
    name the Engine hands back 1.0."""
    import xml.etree.ElementTree as ET
    path = os.path.join(DATA, 'sounds.xml')
    if not os.path.exists(path):
        return ['data/sounds.xml missing']
    try:
        root = ET.parse(path).getroot()
    except Exception as e:
        return ['data/sounds.xml cannot be read: %s' % e]

    bad = []
    for elem in root.findall('Sound'):
        name = elem.get('file')
        if not name:
            bad.append('data/sounds.xml: <Sound> without file'); continue
        if not os.path.exists(os.path.join(DATA, name)):
            bad.append('data/sounds.xml: "%s" does not exist' % name)
        try:
            v = float(elem.get('volume', ''))
        except ValueError:
            bad.append('data/sounds.xml: "%s" has no readable volume value' % name)
            continue
        if not (0.0 < v < 1.0):
            bad.append('data/sounds.xml: "%s" has volume=%s - expected '
                       'something between 0 and 1' % (name, elem.get('volume')))
    return bad


@check('style')
def check_style():
    """Tabs, no space after a keyword, no whitespace at line end.

    That is how the rest of the tree keeps it. Only what has come in since the
    state before this collaboration (BASELINE above) is judged. A line that
    already stood there word for word is left alone: reporting it every time
    would mean reading the same fourteen places at every run, until nobody
    looks any more."""
    bad = []
    kw = re.compile(r'\b(if|for|while|switch|catch)\s\(')
    for p in source_files():
        rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
        text = read(p)
        lines = text.split('\n')
        was = original_lines(rel)

        spaces = sum(1 for l in lines if re.match(r'^    [^ *]', l))
        tabs = sum(1 for l in lines if l.startswith('\t'))
        if spaces > 2 and spaces > tabs:
            bad.append('%s: indented with spaces (%d lines)' % (rel, spaces))

        # EM_ASM holds JavaScript, where "if (" is correct. The body is masked
        # out line by line to keep the line numbers right.
        masked = list(lines)
        for i, line in enumerate(lines):
            if 'EM_ASM' not in line:
                continue
            depth, j = 0, i
            while j < len(lines):
                depth += lines[j].count('(') - lines[j].count(')')
                masked[j] = ''
                if depth <= 0:
                    break
                j += 1

        # Prose is masked out too. English comments say "for (getOverscan())"
        # where the German ones never did, and a comment is not code style.
        # The // lines are cheap to spot; a /* */ body needs the state.
        inBlock = False
        for i, line in enumerate(lines):
            stripped = line.strip()
            if inBlock:
                masked[i] = ''
                if '*/' in line:
                    inBlock = False
                continue
            if stripped.startswith('//'):
                masked[i] = ''
            elif '/*' in line and '*/' not in line[line.index('/*'):]:
                masked[i] = line[:line.index('/*')]
                inBlock = True

        for i, line in enumerate(masked):
            if kw.search(line) and line not in was:
                bad.append('%s:%d: "%s" - the tree writes "if(" without a space'
                           % (rel, i + 1, line.strip()[:60]))
        for i, line in enumerate(lines):
            if re.search(r'[ \t]+$', line) and line not in was:
                bad.append('%s:%d: whitespace at the end of the line' % (rel, i + 1))
    return bad


@check('windows_icon')
def check_windows_icon():
    """The program icon must match data/window.png.

    It has to bring along every size the Windows shell asks for, and in each
    of them be an integer multiple of the art.

    None of that shows up otherwise: icon1.ico is checked in and no build
    creates it, so it stays standing when the art changes. Where a size is
    missing, Windows scales one itself and smooths while doing so. And a
    fractional factor doubles some of the rows and not the rest, which tears
    apart the grid that makes the pixel look. Rebuild: Tools/make_ico.py.
    """
    import struct
    sys.path.insert(0, os.path.join(ROOT, 'WebBuild'))
    from make_icon import read_png

    ico = os.path.join(ROOT, 'Blocks5', 'src', 'icon1.ico')
    png = os.path.join(ROOT, 'Blocks5', 'data', 'window.png')
    if not os.path.exists(ico):
        return ['Blocks5/src/icon1.ico missing']

    data = open(ico, 'rb').read()
    if len(data) < 6 or struct.unpack('<HHH', data[:6])[1] != 1:
        return ['Blocks5/src/icon1.ico is not an icon']
    count = struct.unpack('<HHH', data[:6])[2]

    entries = []
    for i in range(count):
        w, h, c, r, planes, bpp, size, off = struct.unpack('<BBBBHHII', data[6 + 16 * i:22 + 16 * i])
        entries.append((w or 256, data[off:off + size]))

    bad = []
    want = [16, 20, 32, 40, 48, 64, 256]
    missing = [v for v in want if v not in [w for w, _ in entries]]
    if missing:
        bad.append('icon1.ico: the sizes %s are missing - Windows then scales them '
                   'itself and smooths while doing so' % ', '.join(str(m) for m in missing))

    # The art is 16x16; window.png is its clean 2x.
    width, height, pixels = read_png(png)
    step = max(1, width // 16)

    def art(x, y):
        q = (y * step) * width * 4 + (x * step) * 4
        return tuple(pixels[q:q + 4])

    for size, blob in entries:
        if blob[:8] == b'\x89PNG\r\n\x1a\n':
            continue                    # the 256 is stored as PNG
        bw, bh = struct.unpack('<ii', blob[4:12])
        bh //= 2
        if (bw, bh, struct.unpack('<H', blob[14:16])[0]) != (size, size, 32):
            bad.append('icon1.ico: the %dx%d entry is not a 32-bit DIB of that size' % (size, size))
            continue
        scale = max(1, size // 16)
        margin = (size - 16 * scale) // 2

        def pixel(x, y):
            at = 40 + ((size - 1 - y) * size + x) * 4
            b, g, r8, a = blob[at:at + 4]
            return (r8, g, b, a)

        wrong = 0
        for y in range(size):
            for x in range(size):
                inside = margin <= x < margin + 16 * scale and margin <= y < margin + 16 * scale
                want_px = art((x - margin) // scale, (y - margin) // scale) if inside else (0, 0, 0, 0)
                got = pixel(x, y)
                if got[3] == 0 and want_px[3] == 0:
                    continue            # transparent is transparent, the colour under it does not count
                if got != want_px:
                    wrong += 1
        if wrong:
            bad.append('icon1.ico: the %dx%d entry differs in %d pixels from '
                       'data/window.png at %dx with %d pixels of margin - '
                       'run Tools/make_ico.py' % (size, size, wrong, scale, margin))
    return bad


@check('font_metrics')
def check_font_metrics():
    """A font must say where its letters sit, or the keycap frame misses them.

    The frame around a <k>...</k> is drawn on exactly rows capTop..capBottom
    of a glyph cell, and those default to the line box - lineHeight and offset
    - which is where a font's ink normally sits. A font is free to sit
    elsewhere: the note's font ends its writing five rows above the foot of
    its line box, and the tooltip font's key names are taller than its line in
    both directions. The two attributes say where the ink really is, and this
    is what stops them from going stale when the art is redrawn, since nothing
    else reads them and no compiler can see the picture.

    Reported when the frame would cut into the letters, or sit off to one side
    of them by more than half a row. The rows come out of the image: for every
    printable character the first and the last inked row of its cell, and then
    the most common of each - the top of a capital and the line the writing
    sits on. The mode and not the extremes, because a brace reaches higher and
    a comma lower than anything a key is called."""
    sys.path.insert(0, os.path.join(ROOT, 'WebBuild'))
    from make_icon import read_png

    fonts = []
    for pattern in ('Blocks5/data/*.xml', 'Blocks5/levels/skins/*/*.xml'):
        for path in sorted(glob.glob(os.path.join(ROOT, pattern))):
            # The root element, not a <Font>somewhere.xml</Font> naming one
            # inside a dialog.
            if re.match(r'\s*(<\?xml[^>]*\?>\s*)?<Font\b', read(path)):
                fonts.append(path)

    bad = []
    for path in fonts:
        rel = os.path.relpath(path, ROOT).replace(os.sep, '/')
        text = read(path)

        def number(name, fallback=None):
            m = re.search(r'\b%s="(-?\d+)"' % name, text)
            return int(m.group(1)) if m else fallback

        image = re.search(r'image="([^"]+)"', text)
        lineHeight, offset = number('lineHeight'), number('offset')
        if not image or lineHeight is None or offset is None:
            bad.append('%s: no image, lineHeight or offset' % rel)
            continue

        imagePath = os.path.join(os.path.dirname(path), image.group(1))
        if not os.path.exists(imagePath):
            bad.append('%s: the image "%s" is missing' % (rel, image.group(1)))
            continue

        width, height, pixels = read_png(imagePath)

        # The first and the last inked row of every printable character, and
        # the most common of each.
        tops, bottoms = {}, {}
        for m in re.finditer(r'code="(\d+)" x="(\d+)" y="(\d+)" w="(\d+)" h="(\d+)"', text):
            code, x, y, w, h = [int(g) for g in m.groups()]
            if not 32 <= code < 127:
                continue
            first = last = None
            for row in range(h):
                if y + row >= height:
                    break
                line = (y + row) * width * 4
                if any(pixels[line + (x + column) * 4 + 3] for column in range(w) if x + column < width):
                    first = row if first is None else first
                    last = row
            if first is not None:
                tops[first] = tops.get(first, 0) + 1
                bottoms[last] = bottoms.get(last, 0) + 1
        if not tops:
            bad.append('%s: no inked character in the image' % rel)
            continue

        inkTop = max(sorted(tops), key=lambda row: tops[row])
        inkBottom = max(sorted(bottoms), key=lambda row: bottoms[row])

        # What the game will do with it - Font::getKeyBoxRows(), which draws
        # the frame on exactly these two rows.
        capTop = number('capTop', -offset)
        capBottom = number('capBottom', -offset + lineHeight - 1)
        top, bottom = capTop, capBottom

        fix = ('add capTop="%d" capBottom="%d" to <Font>' % (inkTop, inkBottom)
               if number('capTop') is None else
               'capTop="%d" capBottom="%d" no longer describe the image'
               % (capTop, capBottom))
        if top > inkTop or bottom < inkBottom:
            bad.append('%s: the letters sit in rows %d..%d of a cell, the keycap frame '
                       'in rows %d..%d - it cuts into them; %s'
                       % (rel, inkTop, inkBottom, top, bottom, fix))
        elif abs((top + bottom) - (inkTop + inkBottom)) > 1:
            bad.append('%s: the letters sit in rows %d..%d of a cell, the keycap frame '
                       'in rows %d..%d - it is not centred on them; %s'
                       % (rel, inkTop, inkBottom, top, bottom, fix))
    return bad


@check('comments')
def check_comments():
    """English comments - a German line among them is always a leftover.

    The tree was German until the 1.2.0 sweep and the two languages read alike
    at a glance, so the two word lists are counted against each other rather
    than one being searched for: "the particles die" is English even though
    "die" is on the German list, and "so weit kommt das nicht" is German even
    though "so" is on the English one. A line is reported when the German words
    outnumber the English ones.

    The density is a second, unrelated guard: the tree sits at 5 to 7 per cent,
    but a few files explain other people's bugs and are allowed to be denser.
    What gets reported is a file that has turned into an essay."""
    en = re.compile(r'\b(the|is|are|and|that|which|with|this|for|not|but|from|'
                    r'when|would|can|does|it|its|of|to|be|has|have|so)\b', re.I)
    de = re.compile(r'\b(der|die|das|und|nicht|ist|sind|ein|eine|einen|dem|den|'
                    r'wird|werden|wenn|aber|auch|nur|noch|hier|dann|dass|sich|'
                    r'von|zu|es|im|bei|fuer|als|ueber|schon|kein|keine|man)\b', re.I)

    # The list above is counted against the English one because every word on
    # it has an English twin. These have none, so one hit is enough - and one
    # is all there ever is: what the 1.2.0 sweep left behind was not a German
    # sentence but a single German noun inside an English one, which no
    # majority rule can see.
    #
    # Nouns and verbs only, and only where there is genuinely no English
    # reading. The first draft of this list held the German for "key", which
    # fired on four English lines about a matter of taste before it had been
    # in the tree a minute; a symbol would be as bad, since rand() would put
    # rand on it.
    deWord = re.compile(r'\b(funkel|spuren|verwischen|schein|aufloesung|abstand|'
                        r'anzahl|breite|hoehe|groesse|farbe|zeiger|bild|ebene|'
                        r'massiv|uebersetzen|aendern|loeschen|pruefen|zeichnen|'
                        r'erzeugen|berechnen|richtung|zeile|spalte|maus|'
                        r'fenster|speicher|datei|laenge|flimmern|schatten|kante|'
                        r'gegner|spieler|geschwindigkeit|einstellung)\b', re.I)
    bad = []
    for p in source_files():
        rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
        lines = read(p).split('\n')
        comment = sum(1 for l in lines if l.strip().startswith('//'))
        code = sum(1 for l in lines if l.strip() and not l.strip().startswith('//'))
        if code >= 100 and comment * 100.0 / code > 50.0:
            bad.append('%s: %d%% comment (%d/%d) - that is no longer an explanation'
                       % (rel, round(comment * 100.0 / code), comment, code))

    # The language half reaches further than the density half: a script or a
    # page has no ratio worth judging, but its comments are prose like any
    # other and are exactly where a German line survives unseen.
    seen = set(os.path.relpath(p, ROOT) for p in source_files())
    files = [(p, '//') for p in source_files()]
    files += [(p, m) for p, m in prose_files() if os.path.relpath(p, ROOT) not in seen]
    for p, mark in files:
        rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
        for i, line in enumerate(read(p).split('\n')):
            s = line.strip()
            if not s.startswith(mark):
                continue
            body = s[len(mark):]
            deHits = len(de.findall(body))
            if len(body.split()) >= 5 and deHits >= 2 and deHits > len(en.findall(body)):
                bad.append('%s:%d: German comment - "%s"' % (rel, i + 1, body.strip()[:60]))
            else:
                m = deWord.search(body)
                if m:
                    bad.append('%s:%d: German word "%s" - "%s"'
                               % (rel, i + 1, m.group(0), body.strip()[:60]))
    return bad


# ---------------------------------------------------------------------------


def main(argv):
    only = None
    quiet = False
    for i, a in enumerate(argv):
        if a == '--list':
            for name, fn in CHECKS:
                print('%-15s %s' % (name, (fn.__doc__ or '').split('\n')[0]))
            return 0
        if a == '--only':
            only = argv[i + 1]
        if a == '--quiet':
            quiet = True

    # A name nothing matches used to run no check and then say "all clear",
    # which is the answer a passing run gives - so a check renamed out from
    # under selftest.py, or a typo on the command line, read as success.
    if only and only not in [name for name, fn in CHECKS]:
        print('no such check: %s (--list names them)' % only)
        return 2

    total = 0
    for name, fn in CHECKS:
        if only and name != only:
            continue
        try:
            problems = fn()
        except Exception as e:                                  # noqa: BLE001
            problems = ['the check itself failed: %r' % (e,)]
        total += len(problems)
        if problems:
            print('[%s] %d' % (name, len(problems)))
            for p in problems:
                print('    ' + p)
        elif not quiet:
            print('[%s] ok' % name)

    print('')
    print('%d finding(s)' % total if total else 'all clear')
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
