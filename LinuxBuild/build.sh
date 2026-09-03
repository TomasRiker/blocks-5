#!/bin/bash
# build.sh - Blocks 5 fuer Linux uebersetzen.
#
# Von ueberall aufrufbar; alle Pfade haengen an diesem Skript.
#   ./build.sh            inkrementell
#   ./build.sh clean      von vorn
#   ./build.sh hooks      mit den Testhaken, nach build-test/
#   ./build.sh run [...]  bauen und starten, alles danach geht ans Spiel
#
# "hooks" uebersetzt engine.cpp und testhooks.cpp mit -DBLOCKS5_TEST_HOOKS und
# baut nach build-test/ statt build/, damit ein Build mit Haken nie versehentlich
# der ausgelieferte ist. Ohne das Wort ist testhooks.cpp eine leere
# Uebersetzungseinheit.
#
# Gebraucht werden: g++, SDL 1.2 (heute ueberall sdl12-compat, also SDL 2
# darunter), OpenAL, OpenGL und GLU. Auf Debian und Ubuntu:
#
#   sudo apt install build-essential libsdl1.2-dev libopenal-dev \
#                    libglu1-mesa-dev libgl1-mesa-dev
#
# Alles andere - zlib, minizip, libogg, libvorbis, TinyXML, stb, minih264,
# shine, minimp4 - kommt aus Blocks5/libs, genau wie beim Windows- und beim
# Browser-Build. Damit uebersetzen alle drei denselben Code.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME="$HERE/../Blocks5"
LIBS="$GAME/libs"
ZLIB="$LIBS/zlib-1.3.1"
OUT="$HERE/build"
HOOKS=""
if [ "${1:-}" = "hooks" ]; then HOOKS="-DBLOCKS5_TEST_HOOKS"; OUT="$HERE/build-test"; fi

[ "${1:-}" = "clean" ] && rm -rf "$OUT"
mkdir -p "$OUT/obj"

command -v sdl-config >/dev/null 2>&1 || {
    echo "sdl-config nicht gefunden - libsdl1.2-dev fehlt."; exit 2; }

INC="-I$GAME/src -I$HERE
     -I$LIBS/tinyxml-2.6.2 -I$LIBS/sigslot -I$LIBS/mtrand-1.1
     -I$LIBS/openal-soft-1.25.2/include -I$LIBS/openal-soft-1.25.2/include/AL
     -I$LIBS/libvorbis-1.3.4/include -I$LIBS/libvorbis-1.3.4/lib
     -I$LIBS/libogg-1.3.2/include -I$ZLIB -I$LIBS/stb
     -I$ZLIB/contrib/minizip
     -I$LIBS/minih264 -I$LIBS/minimp4 -I$LIBS/shine
     $(sdl-config --cflags)"

# -DTIXML_USE_STL wie in beiden anderen Builds. -fno-strict-aliasing, weil der
# Baum an mehreren Stellen ueber Zeigertypen hinweg liest (die vendorierten
# Encoder tun es auch) und GCC das sonst wegoptimieren darf.
CFLAGS="-O2 -fno-strict-aliasing -DTIXML_USE_STL $INC"
CXXFLAGS="$CFLAGS -std=c++14 -Wno-register"

# Die Spielquellen ohne die drei, die hier nicht mitkommen:
#   stackwalker  - Win32-SEH, gibt es nur dort
#   audiocapture - der #else-Zweig ist ein Stummel, kommt aber mit
#   pch          - die Uebersetzungseinheit, die unter MSVC den PCH erzeugt
SRCS=$(ls "$GAME"/src/*.cpp | grep -vE '/(stackwalker|pch)\.cpp$')
SRCS="$SRCS $HERE/linux_window.cpp"
for f in tinyxml tinyxmlparser tinyxmlerror tinystr; do SRCS="$SRCS $LIBS/tinyxml-2.6.2/$f.cpp"; done

CSRCS="$ZLIB/contrib/minizip/ioapi.c $ZLIB/contrib/minizip/unzip.c $ZLIB/contrib/minizip/zip.c
       $ZLIB/adler32.c $ZLIB/compress.c $ZLIB/crc32.c $ZLIB/deflate.c
       $ZLIB/infback.c $ZLIB/inffast.c $ZLIB/inflate.c $ZLIB/inftrees.c
       $ZLIB/trees.c $ZLIB/uncompr.c $ZLIB/zutil.c
       $LIBS/libogg-1.3.2/src/bitwise.c $LIBS/libogg-1.3.2/src/framing.c
       $LIBS/minih264/minih264e_impl.c $LIBS/minimp4/minimp4_impl.c"
for f in analysis bitrate block codebook envelope floor0 floor1 info lookup lpc lsp \
         mapping0 mdct psy registry res0 sharedbook smallft synthesis vorbisenc \
         vorbisfile window; do CSRCS="$CSRCS $LIBS/libvorbis-1.3.4/lib/$f.c"; done
for f in bitstream huffman l3bitstream l3loop l3mdct l3subband layer3 reservoir \
         tables; do CSRCS="$CSRCS $LIBS/shine/$f.c"; done

fail=0
compile() { # $1=Datei $2=Schalter
  # Die Schalter gehen in den Namen ein, nicht nur der Pfad: sonst bleibt beim
  # Aendern der Schalter das alte Objekt liegen, weil es neuer ist als die
  # Quelle. Genau das ist beim ersten Mal passiert, als noch alles durch g++
  # ging - eine .c, die sich zufaellig auch als C++ uebersetzen liess, lag
  # danach mit verstuemmelten Namen da und fehlte beim Linken.
  local o="$OUT/obj/$(echo "$1 $2" | md5sum | cut -c1-12)-$(basename "$1").o"
  local d="$o.d"
  # Das Objekt nur wiederverwenden, wenn es neuer ist als die Quelle UND als
  # jede Kopfdatei, die sie beim letzten Mal eingebunden hat. Ohne die zweite
  # Bedingung uebersetzt eine geaenderte Kopfdatei nur die Dateien neu, die
  # sich selbst geaendert haben - der Rest bleibt gegen das alte Klassenbild
  # gebaut, und die Singletons liegen danach uebereinander im Speicher.
  if [ -f "$o" ] && [ -f "$d" ] && [ "$o" -nt "$1" ]; then
      local stale=0 dep
      for dep in $(sed -e 's/^[^:]*://' -e 's/\\$//' "$d"); do
          [ -e "$dep" ] && [ "$dep" -nt "$o" ] && { stale=1; break; }
      done
      [ $stale -eq 0 ] && { echo "$o"; return 0; }
  fi
  # gcc fuer .c, g++ fuer .cpp: emcc waehlt die Sprache nach der Endung, die
  # beiden GNU-Treiber nicht - g++ uebersetzt auch eine .c als C++, und die
  # vendorierten Bibliotheken sind C und lassen sich so nicht uebersetzen.
  local cc=g++
  case "$1" in *.c) cc=gcc;; esac
  if ! $cc -c "$1" -o "$o" -MMD -MF "$d" $2 2> "$o.log"; then
      echo "FEHLGESCHLAGEN: $1" >&2; head -30 "$o.log" >&2; return 1
  fi
  echo "$o"
}

OBJS=""
total=$(echo $SRCS $CSRCS | wc -w)
for f in $CSRCS; do o=$(compile "$f" "$CFLAGS")   || { fail=1; continue; }; OBJS="$OBJS $o"; done
for f in $SRCS
do
  # Nur die beiden, die etwas davon haben. Es steht nicht in CXXFLAGS, damit
  # ein Wechsel zwischen den Buildarten nicht alle 167 Einheiten neu
  # uebersetzt - die beiden Ausgabeverzeichnisse trennen sie ohnehin.
  extra=""
  case "$f" in */engine.cpp|*/testhooks.cpp) extra="$HOOKS";; esac
  o=$(compile "$f" "$CXXFLAGS $extra") || { fail=1; continue; }
  OBJS="$OBJS $o"
done
[ $fail -ne 0 ] && { echo "### UEBERSETZEN FEHLGESCHLAGEN ###"; exit 1; }
echo "### $total Uebersetzungseinheiten in Ordnung ###"

# -lX11 fuer den Vollbildwechsel in linux_window.cpp. SDL bringt es selbst mit,
# aber verlassen darf man sich darauf nicht: unter sdl12-compat steckt SDL 2
# darunter, und das laedt seine Videotreiber erst zur Laufzeit.
g++ $OBJS -o "$OUT/blocks5" $(sdl-config --libs) -lopenal -lGL -lGLU -lX11 -lm -lpthread || {
    echo "### LINKEN FEHLGESCHLAGEN ###"; exit 1; }
echo "### LINK OK -> $OUT/blocks5 ($(du -h "$OUT/blocks5" | cut -f1)) ###"

# data.zip ist ein Bauergebnis und liegt nicht im Git. Ohne es kommt das Spiel
# nicht ueber den Ladebildschirm hinaus, und das sieht nach einem Fehler im
# Build aus, obwohl nur ein Schritt fehlt.
[ -f "$GAME/data.zip" ] || echo "(Achtung: data.zip fehlt - Blocks5/pack.sh baut es)"

# Das Spiel oeffnet data.zip relativ zum Arbeitsverzeichnis, muss also aus
# Blocks5/ heraus laufen - genau wie unter Windows.
if [ "${1:-}" = "run" ]; then
    shift
    cd "$GAME" && exec "$OUT/blocks5" "$@"
fi
