#!/usr/bin/env python3
# -*- coding: ascii -*-
"""verify.py - static checks over the Blocks 5 tree.

Ein Durchlauf ohne Uebersetzer, in wenigen Sekunden. Die Pruefungen suchen
genau die Sorte Fehler, die beim Bearbeiten stillschweigend entsteht und die
weder der Compiler noch ein Blick auf den Diff findet: eine Zeichenkette, die
ein Umbenennen mitgenommen hat, ein GUI-Pfad, den es nicht mehr gibt, ein
Attribut, das geschrieben und nirgends gelesen wird, eine neue Membervariable
ohne Anfangswert, eine Quelldatei, die im Projekt fehlt.

    python3 tools/verify.py             alle Pruefungen
    python3 tools/verify.py --list      die Namen
    python3 tools/verify.py --only gui_paths
    python3 tools/verify.py --quiet     nur die Zusammenfassung

Rueckgabewert 1, sobald irgendetwas beanstandet wird.

Zwei langsame Pruefungen stehen daneben und laufen nur auf Wunsch:
    tools/syntax.sh    uebersetzt jede Quelldatei mit mingw (-fsyntax-only)
    WebBuild/build.sh  baut den Browser-Build
"""

import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'Blocks5', 'src')
DATA = os.path.join(ROOT, 'Blocks5', 'data')
WEB = os.path.join(ROOT, 'WebBuild')

# Vendored, nicht unseres. stackwalker ist zugekauft und CRLF.
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
    """Jede Quelldatei, die uns gehoert - ohne libs/ und ohne die Bauergebnisse."""
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


BASELINE = '95660bb'      # letzter Stand vor dieser Zusammenarbeit
_baseline_cache = {}


def original_lines(rel):
    """Die Datei, wie sie im Original stand - fuer Pruefungen, die nur ueber
    Neues urteilen sollen. Leere Menge, wenn es die Datei damals nicht gab oder
    git nicht zu erreichen ist."""
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
    """Kommentare und Zeichenketten stehen lassen, aber Kommentare leeren -
    damit ein Muster nicht in einem auskommentierten Rest anschlaegt."""
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
    """Reines ASCII und LF im Quellcode; CRLF und Latin-1 in den mitgelieferten
    Textdateien. Ein einziger Umlaut in einem Kommentar macht die Kodierung des
    Baums wieder zu einer Frage."""
    bad = []
    for p in source_files():
        data = open(p, 'rb').read()
        rel = os.path.relpath(p, ROOT)
        try:
            data.decode('ascii')
        except UnicodeDecodeError as e:
            bad.append('%s: nicht ASCII (%s)' % (rel, e))
        if b'\r' in data:
            bad.append('%s: CRLF, erwartet LF' % rel)

    for rel in ('Blocks5/readme.txt', 'Blocks5/levels/readme.txt', 'Blocks5/data/languages.txt'):
        p = os.path.join(ROOT, rel)
        if not os.path.exists(p):
            bad.append('%s: fehlt' % rel)
            continue
        data = open(p, 'rb').read()
        if b'\r\n' not in data:
            bad.append('%s: kein CRLF - diese Datei wird so ausgeliefert' % rel)
        if data.count(b'\n') != data.count(b'\r\n'):
            bad.append('%s: gemischte Zeilenenden' % rel)
    return bad


@check('project_files')
def check_project_files():
    """Es gibt keinen Glob-Build: eine neue Quelldatei muss in Blocks5.vcxproj
    *und* in Blocks5.vcxproj.filters stehen, sonst uebersetzt Visual Studio sie
    nicht und niemand merkt es unter Linux."""
    proj = os.path.join(ROOT, 'Blocks5', 'Blocks5.vcxproj')
    filt = proj + '.filters'
    if not os.path.exists(proj) or not os.path.exists(filt):
        return ['Blocks5.vcxproj oder .filters fehlt']

    ptext, ftext = read(proj), read(filt)
    bad = []
    for f in sorted(os.listdir(SRC)):
        if not f.endswith(('.cpp', '.h')):
            continue
        if f == 'pch.cpp':          # die Create-PCH-Einheit, steht mit eigener Regel drin
            pass
        for name, text in (('Blocks5.vcxproj', ptext), ('Blocks5.vcxproj.filters', ftext)):
            if ('src\\' + f) not in text and ('src/' + f) not in text:
                bad.append('%s fehlt in %s' % (f, name))
    return bad


@check('version')
def check_version():
    """Die Versionsnummer steht an vier Stellen. Die .rc ist schon einmal
    uebersehen worden und blieb eine ganze Fassung lang falsch - das ist die
    Nummer, die der Explorer zeigt und die in einem Absturzbericht steht."""
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
        return ['keine Versionsnummer gefunden - die Muster stimmen nicht mehr']
    values = set(found.values())
    if len(values) == 1:
        return []
    return ['Versionsnummern gehen auseinander:'] + \
           ['    %-34s %s' % (k, v) for k, v in sorted(found.items())]


@check('gui_paths')
def check_gui_paths():
    """Jeder Elementpfad im Code muss in einem Dialog-XML stehen. Ein
    umbenanntes Element faellt sonst nur dadurch auf, dass der Knopf nichts
    mehr tut - gui[...] liefert dann einen Nullzeiger."""
    known = set()
    for f in sorted(os.listdir(DATA)):
        if not f.endswith('.xml'):
            continue
        for m in re.finditer(r'name="([^"]+)"', read(os.path.join(DATA, f))):
            known.add(m.group(1))

    # Im Code stehen ganze Pfade ("Menu.ManagerPane.Manager.Close"); geprueft
    # wird jedes Glied, denn die XML nennt nur die einzelnen Namen.
    allowed_missing = {
        'Game',                                   # zur Laufzeit erzeugt
        'LevelEditor.EditHintPane.EditHint.Text',  # dito
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
                    bad.append('%s:%d: "%s" - kein Element namens "%s" in data/*.xml'
                               % (rel, line, full, part))
                    break
    return bad


@check('strings')
def check_strings():
    """Jede $ID aus Code und XML muss in languages.txt stehen, und jede dortige
    Zeichenkette braucht einen englischen und einen deutschen Text."""
    langs = os.path.join(DATA, 'languages.txt')
    if not os.path.exists(langs):
        return ['data/languages.txt fehlt']

    text = read(langs)
    ids, bodies, current = set(), {}, None
    for line in text.split('\r\n' if '\r\n' in text else '\n'):
        if line.startswith('$'):
            # Ein '#' am Ende heisst "Zeilen nicht zusammenziehen" und gehoert
            # nicht zur Kennung - loadStringDB() schneidet es genauso ab.
            current = line.strip().rstrip('#')
            ids.add(current)
            bodies[current] = set()
        elif line.startswith('\xa7') and current:
            bodies[current].add(line[1:3])

    bad = []
    for i in sorted(ids):
        if i == '$END_OF_FILE':      # Schlussmarke, kein Text
            continue
        for want in ('en', 'de'):
            if want not in bodies.get(i, set()):
                bad.append('languages.txt: %s hat keinen %s-Text' % (i, want))

    used = set()
    for p in source_files(('.cpp', '.h')):
        for m in re.finditer(r'"(\$[A-Z][A-Z0-9_]*)"', read(p)):
            used.add(m.group(1))
    for f in sorted(os.listdir(DATA)):
        if f.endswith('.xml'):
            for m in re.finditer(r'>(\$[A-Z][A-Z0-9_]*)<', read(os.path.join(DATA, f))):
                used.add(m.group(1))

    for i in sorted(used - ids):
        bad.append('%s wird benutzt, steht aber nicht in languages.txt' % i)
    return bad


@check('xml_attrs')
def check_xml_attrs():
    """Ein Attribut, das geschrieben und nirgends gelesen wird, ist entweder
    tote Last oder ein Tippfehler - so ist "numLayers" beim Umbenennen einer
    Konstanten einmal zu "NUM_LAYERS" geworden und die Groessenpruefung des
    Levels damit still ausgefallen."""
    written, readd = {}, set()
    # Der Aufrufpfeil gehoert ins Muster: sonst passt "Attribute(" auch auf das
    # Ende von "SetAttribute(", jedes geschriebene Attribut gilt als gelesen,
    # und die Pruefung findet nie etwas.
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

    # Die GUI liest ihre Attribute ueber readAttributes(), nicht ueber diese
    # Namen; das XML wird dort geschrieben, nicht gelesen.
    return ['%s: Attribut "%s" wird geschrieben, aber nirgends gelesen' % (where, name)
            for name, where in sorted(written.items()) if name not in readd]


@check('config')
def check_config():
    """Was Engine::saveConfig schreibt, muss Engine::loadConfig auch wieder
    lesen - sonst verliert die config.xml bei jedem Start eine Einstellung."""
    text = read(os.path.join(SRC, 'engine.cpp'))

    def section(name):
        m = re.search(r'void Engine::' + name + r'\(\)\s*\{', text)
        if not m:
            return ''
        depth, i = 0, m.end() - 1
        while i < len(text):
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
                if depth == 0:
                    return text[m.end():i]
            i += 1
        return ''

    load, save = section('loadConfig'), section('saveConfig')
    if not load or not save:
        return ['loadConfig() oder saveConfig() nicht gefunden']

    written = set(re.findall(r'new TiXmlElement\(\s*"([A-Za-z][\w]*)"', save))
    read_names = set(re.findall(r'FirstChildElement\(\s*"([A-Za-z][\w]*)"', load))
    read_names |= set(re.findall(r'NextSiblingElement\(\s*"([A-Za-z][\w]*)"', load))

    bad = []
    for name in sorted(written - read_names - {'Config'}):
        bad.append('config.xml: <%s> wird geschrieben, aber nicht gelesen' % name)
    for name in sorted(read_names - written - {'Config'}):
        bad.append('config.xml: <%s> wird gelesen, aber nicht geschrieben' % name)
    return bad


@check('ctor_init')
def check_ctor_init():
    """Skalare Member, die der Konstruktor nicht setzt. Genau daran hing
    presentVertexBuffer: ohne Bildpuffer las Engine::exit() einen zufaelligen
    Wert und hielt ihn fuer einen GL-Namen.

    Gesucht wird je Klasse, die im .cpp einen eigenen Konstruktor hat, und nur
    in dessen Rumpf. Laesst er die Mehrzahl der Member aus, folgt die Klasse
    einer anderen Regel - Objekte bekommen ihre Felder aus readAttributes() -
    und die Pruefung schweigt.

    Beurteilt wird nur, was seit dem Stand vor dieser Zusammenarbeit dazukam.
    Ein Member, den es damals schon gab, wird irgendwo vor dem ersten Lesen
    gesetzt - init(), loadConfig(), setLogicRate() - und das seit zehn Jahren."""
    scalar = re.compile(
        r'^\s*(?:unsigned\s+|signed\s+)?'
        r'(bool|char|short|int|long|float|double|uint|uchar|ushort|ulong|size_t)\s+'
        r'([a-z_]\w*)\s*;\s*(?://.*)?$')

    def classes(htext):
        """(Name, Member) je Klasse. Member einer verschachtelten Struktur
        gehoeren dem, der sie anlegt, und bleiben aussen vor - sie stehen eine
        Klammerebene tiefer."""
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
        if re.search(r'[:,]\s*%s\s*\(' % re.escape(name), body):        # Initialisierungsliste
            return True
        if re.search(r'&%s\b' % re.escape(name), body):                  # per Adresse gefuellt
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
            # Setzt der Konstruktor weniger als die Haelfte, folgt die Klasse
            # einer anderen Regel und die Pruefung sagt nichts ueber sie.
            if len(missing) * 2 > len(members):
                continue
            was = original_lines(rel)
            missing = [m for m in missing
                       if not any(re.search(r'\b%s\s*(\[|;)' % re.escape(m), l) for l in was)]
            for m in missing:
                bad.append('%s: %s::%s wird im Konstruktor nicht gesetzt' % (rel, cls, m))
    return bad


@check('assets')
def check_assets():
    """Dateinamen, die im Code stehen, muessen es auch auf der Platte geben."""
    have = set()
    for base in ('Blocks5/data', 'Blocks5/levels', 'Blocks5'):
        for root, dirs, files in os.walk(os.path.join(ROOT, base)):
            if 'libs' in root.split(os.sep):
                continue
            for f in files:
                have.add(f.lower())

    # Bruchstuecke, Formatzeichenketten und zur Laufzeit erzeugte Dateien.
    runtime = re.compile(r'%|^\.|^/|:|\*|,|\s')
    generated = {'config.xml', 'progress.zip', 'crash_log.txt', 'log.txt',
                 'keyboard.dat', 'campaign.xml', '~campaignsave.zip'}
    bad = []
    lit = re.compile(r'"([A-Za-z0-9_][A-Za-z0-9_.\- ]*\.(?:png|xml|ogg|wav|txt|zip|dat))"')
    for p in source_files(('.cpp', '.h')):
        rel = os.path.relpath(p, ROOT)
        for m in lit.finditer(read(p)):
            name = m.group(1)
            if runtime.search(name) or name.lower() in generated:
                continue
            if name.lower() not in have:
                bad.append('%s: "%s" gibt es nicht' % (rel, name))
    return bad


@check('style')
def check_style():
    """Tabulatoren, kein Leerzeichen zwischen Schluesselwort und Klammer, keine
    Leerzeichen am Zeilenende - so haelt es der uebrige Baum.

    Beurteilt wird nur, was seit dem Stand vor dieser Zusammenarbeit
    dazugekommen ist (BASELINE oben). Eine Zeile, die dort schon woertlich so
    stand, bleibt unbehelligt: sie jedesmal zu melden hiesse, bei jedem Lauf
    dieselben vierzehn Stellen zu lesen, bis niemand mehr hinsieht."""
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
            bad.append('%s: mit Leerzeichen eingerueckt (%d Zeilen)' % (rel, spaces))

        # In EM_ASM steht JavaScript, und dort ist "if (" richtig. Der Rumpf
        # wird zeilenweise ausgeblendet, damit die Zeilennummern stimmen.
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

        for i, line in enumerate(masked):
            if kw.search(line) and not line.strip().startswith('//') and line not in was:
                bad.append('%s:%d: "%s" - der Baum schreibt "if(" ohne Leerzeichen'
                           % (rel, i + 1, line.strip()[:60]))
        for i, line in enumerate(lines):
            if re.search(r'[ \t]+$', line) and line not in was:
                bad.append('%s:%d: Leerzeichen am Zeilenende' % (rel, i + 1))
    return bad


@check('comments')
def check_comments():
    """Deutsche Kommentare - ein englischer Block zwischen den anderen ist
    immer ein Rest aus einer Bearbeitung.

    Die Dichte steht nur als Notbremse dabei: der Baum liegt bei 5 bis 7
    Prozent, aber ein paar Dateien erklaeren fast nur fremde Fehler und duerfen
    dichter sein. Gemeldet wird erst, was zur Abhandlung geworden ist."""
    en = re.compile(r'\b(the|is|are|and|that|which|with|this|for|not|but|from|'
                    r'when|would|can|does|it|its|of|to|be|has|have|so)\b', re.I)
    de = re.compile(r'\b(der|die|das|und|nicht|ist|sind|ein|eine|einen|dem|den|'
                    r'wird|werden|wenn|aber|auch|nur|noch|hier|dann|dass|sich|'
                    r'von|zu|es|im|bei|fuer|als|ueber|schon|kein|keine|man)\b', re.I)
    bad = []
    for p in source_files():
        rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
        lines = read(p).split('\n')
        comment = sum(1 for l in lines if l.strip().startswith('//'))
        code = sum(1 for l in lines if l.strip() and not l.strip().startswith('//'))
        if code >= 100 and comment * 100.0 / code > 50.0:
            bad.append('%s: %d%% Kommentar (%d/%d) - das ist keine Erlaeuterung mehr'
                       % (rel, round(comment * 100.0 / code), comment, code))
        for i, line in enumerate(lines):
            s = line.strip()
            if not s.startswith('//'):
                continue
            body = s[2:]
            if len(body.split()) >= 5 and len(en.findall(body)) >= 2 and not de.search(body):
                bad.append('%s:%d: englischer Kommentar - "%s"' % (rel, i + 1, body.strip()[:60]))
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
            problems = ['die Pruefung selbst ist gescheitert: %r' % (e,)]
        total += len(problems)
        if problems:
            print('[%s] %d' % (name, len(problems)))
            for p in problems:
                print('    ' + p)
        elif not quiet:
            print('[%s] ok' % name)

    print('')
    print('%d Beanstandung(en)' % total if total else 'alles in Ordnung')
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
