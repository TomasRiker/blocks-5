#!/bin/sh
# compile_db.sh - write compile_commands.json for the clang tools.
#
# clang-rename and clang-change-namespace rename a symbol by parsing the
# translation unit and following the AST, which is the difference between a
# rename and a text substitution: a blanket sed over "GLState::" also renames
# the constructor GLState::GLState, and a member called the same thing as a
# local, and the word inside a comment or a string.
#
# The flags are the native build's own, asked of LinuxBuild/build.sh rather
# than copied, so the two cannot drift apart. The file is a build product and
# is gitignored; nothing in the three builds reads it.
#
#   sh Tools/compile_db.sh                      writes ./compile_commands.json
#   clang-rename-18 -i --qualified-name=A::b --new-name=c Blocks5/src/*.cpp

set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"

# Ask build.sh for its own variables instead of duplicating them: it is a
# shell script, so sourcing the first fifty lines gets INC and CXXFLAGS
# exactly as the compiler sees them.
CXXFLAGS="$("$ROOT/LinuxBuild/build.sh" flags)"

python3 - "$ROOT" "$CXXFLAGS" <<'PY'
import json, os, shlex, sys
root, flags = sys.argv[1], ' '.join(sys.argv[2].split())
# INC is written over several lines in build.sh, and a command with a
# newline in it is not one a compilation database consumer has to accept.
entries = []
for d in ('Blocks5/src', 'WebBuild', 'PWEncrypt', 'ShowUserDir'):
    full = os.path.join(root, d)
    if not os.path.isdir(full):
        continue
    for name in sorted(os.listdir(full)):
        if not name.endswith(('.cpp', '.c')):
            continue
        # stackwalker is Windows-only and pch.cpp exists to create the
        # precompiled header; neither parses here, the same two the native
        # build leaves out.
        if name in ('stackwalker.cpp', 'pch.cpp'):
            continue
        entries.append({
            'directory': root,
            'file': os.path.join(d, name),
            'command': 'clang++ %s -c %s' % (flags, os.path.join(d, name)),
        })
out = os.path.join(root, 'compile_commands.json')
json.dump(entries, open(out, 'w'), indent=1)
print('%s: %d translation units' % (out, len(entries)))
PY
