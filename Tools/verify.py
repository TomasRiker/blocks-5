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


def strip_comments(text):
    """Empty out the comments and keep the strings - so that a pattern does
    not fire inside a commented-out leftover."""
    out = []
    i, n = 0, len(text)
    while i < n:
        if text.startswith('//', i):
            j = text.find('\n', i)
            i = n if j < 0 else j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            i = n if j < 0 else j + 2
        elif text[i] == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == '\\' else 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(text[i])
            i += 1
    return ''.join(out)


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
