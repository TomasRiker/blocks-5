#!/bin/sh
# syntax.sh - jede Quelldatei des Spiels mit mingw uebersetzen, ohne zu linken.
#
# Der Windows-Build laesst sich unter Linux nicht bauen, aber mingw-w64 kennt
# die Win32-Kopfdateien: -fsyntax-only findet damit alles, was ein Uebersetzer
# ueberhaupt finden kann - Tippfehler, falsche Signaturen, vergessene
# Deklarationen. Das ist die einzige Gelegenheit, den Windows-Code hier zu
# pruefen, und sie kostet eine halbe Minute.
#
#     sh Tools/syntax.sh            alle Quelldateien
#     sh Tools/syntax.sh engine.cpp nur diese
#
# Ausgabe nur bei einem Fehler; Rueckgabewert 1, sobald eine Datei nicht
# durchgeht.
#
# Drei Dateien bleiben aussen vor, und zwar schon immer: main.cpp (WinMain und
# der Aktualisierungspruefer ziehen wininet und Dinge, die mingw anders
# deklariert), videorecorder.cpp (die drei vendorierten Encoder) und
# stackwalker.cpp (dbghelp, zugekauft). Sie fallen im Web-Build ebenfalls
# heraus, siehe WebBuild/build.sh.

set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
SRC="$ROOT/Blocks5/src"
LIBS="$ROOT/Blocks5/libs"

command -v i686-w64-mingw32-g++ >/dev/null 2>&1 || {
    echo "i686-w64-mingw32-g++ nicht gefunden - kein mingw-w64 installiert."
    exit 2
}

# Das Spiel schreibt <Windows.h>, <Shellapi.h>, <Shlobj.h> und <al.h>; mingw
# und OpenAL Soft legen sie unter anderen Namen ab, und Linux nimmt es mit der
# Gross- und Kleinschreibung genau. Ein paar Weiterleitungen in einem
# Wegwerfverzeichnis reichen - eingecheckt werden muss dafuer nichts.
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

# -w, nicht -Wall: der Baum ist zehn Jahre alt und meldet tausende Warnungen,
# die alle schon 2015 dastanden. Gesucht sind hier Fehler. Fuer eine
# Warnungsrunde: dieses -w gegen -Wall -Wextra tauschen und die Ausgabe mit
# dem Stand vor der Aenderung vergleichen.
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
