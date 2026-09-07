#!/bin/sh
# syntax.sh - compile every source of the game with mingw, without linking.
#
# The Windows build cannot be built under Linux, but mingw-w64 knows the Win32
# headers: with -fsyntax-only it finds everything a compiler can find at all -
# typos, wrong signatures, forgotten declarations. It is the only way to put a
# compiler over the Windows code from here, and it costs half a minute.
#
#     sh Tools/syntax.sh            all source files
#     sh Tools/syntax.sh engine.cpp only this one
#
# Output only on an error; exit code 1 as soon as one file does not go through.
#
# Three files are left out, and always have been: main.cpp (WinMain and the
# update checker pull in wininet and things mingw declares differently),
# videorecorder.cpp (the three vendored encoders) and stackwalker.cpp
# (dbghelp, third-party). They are left out of the web build as well, see
# WebBuild/build.sh.

set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
SRC="$ROOT/Blocks5/src"
LIBS="$ROOT/Blocks5/libs"

command -v i686-w64-mingw32-g++ >/dev/null 2>&1 || {
    echo "i686-w64-mingw32-g++ nicht gefunden - kein mingw-w64 installiert."
    exit 2
}

# The game writes <Windows.h>, <Shellapi.h>, <Shlobj.h> and <al.h>; mingw and
# OpenAL Soft file them under other names, and Linux is strict about case. A
# handful of forwarding headers in a throwaway directory are enough - nothing
# has to be checked in for it.
SHIM=$(mktemp -d)
trap 'rm -rf "$SHIM"' EXIT
for h in Windows:windows Shellapi:shellapi Shlobj:shlobj VersionHelpers:versionhelpers; do
    echo "#include <$(echo "$h" | cut -d: -f2).h>" > "$SHIM/$(echo "$h" | cut -d: -f1).h"
done
echo '#include <AL/al.h>'  > "$SHIM/al.h"
echo '#include <AL/alc.h>' > "$SHIM/alc.h"

INC="-I$SRC -I$SHIM
     -I$LIBS/SDL-1.2.15/include -I$LIBS/tinyxml-2.6.2
     -I$LIBS/libogg-1.3.2/include -I$LIBS/libvorbis-1.3.4/include
     -I$LIBS/stb -I$LIBS/openal-soft-1.25.2/include
     -I$LIBS/minih264 -I$LIBS/minimp4 -I$LIBS/shine/src/lib
     -I$LIBS/zlib-1.3.1 -I$LIBS/zlib-1.3.1/contrib/minizip
     -I$LIBS/sigslot -I$LIBS/mtrand-1.1"

# -w, not -Wall: the tree is ten years old and emits thousands of warnings
# that were all there in 2015. What is wanted here is errors. For a warning
# sweep: swap this -w for -Wall -Wextra and compare the output against the
# same sweep before the change.
FLAGS="-fsyntax-only -std=c++14 -DTIXML_USE_STL -DDECLSPEC= -w"

if [ $# -gt 0 ]; then
    FILES=$*
else
    FILES=$(cd "$SRC" && ls *.cpp | grep -vE '^(main|videorecorder|stackwalker)\.cpp$')
fi

fail=0
n=0
for f in $FILES; do
    n=$((n + 1))
    out=$(cd "$SRC" && i686-w64-mingw32-g++ $FLAGS $INC "$f" 2>&1)
    if [ -n "$out" ]; then
        echo "### $f"
        echo "$out"
        fail=1
    fi
done

if [ $fail -eq 0 ]; then
    echo "$n Quelldateien uebersetzen fehlerfrei"
else
    echo "### FEHLER ###"
fi
exit $fail
