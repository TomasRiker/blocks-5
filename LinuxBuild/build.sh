#!/bin/bash
# build.sh - compile Blocks 5 for Linux.
#
# Callable from anywhere; every path hangs off this script.
#   ./build.sh            incremental
#   ./build.sh clean      from scratch
#   ./build.sh hooks      with the test hooks, into build-test/
#   ./build.sh run [...]  build and start, everything after it goes to the game
#
# "hooks" compiles engine.cpp, testhooks.cpp and renderer.cpp with
# -DBLOCKS5_TEST_HOOKS and builds into build-test/ instead of build/, which
# keeps a build with hooks from ever being the shipped one by accident. Without
# the word, testhooks.cpp is an empty translation unit and the other two lose
# the readback checks that say the renderer's record is being lied to.
#
# Needed: g++, SDL 1.2 (sdl12-compat everywhere today, hence SDL 2 underneath),
# OpenAL and OpenGL. On Debian and Ubuntu:
#
#   sudo apt install build-essential libsdl1.2-dev libopenal-dev \
#                    libglu1-mesa-dev libgl1-mesa-dev
#
# Everything else - zlib, minizip, libogg, libvorbis, TinyXML, stb, minih264,
# shine, minimp4 - comes out of Blocks5/libs, exactly as in the Windows and the
# browser build. All three therefore compile the same code.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME="$HERE/../Blocks5"
LIBS="$GAME/libs"
ZLIB="$LIBS/zlib-1.3.1"
OUT="$HERE/build"
HOOKS=""; WRAP=""
if [ "${1:-}" = "hooks" ]; then
  HOOKS="-DBLOCKS5_TEST_HOOKS"; OUT="$HERE/build-test"
  # The test hook's draw-call count: every call to these three from the
  # game's objects lands on the __wrap_ versions at the foot of testhooks.cpp,
  # which count and go on to the real one. At the link rather than through a
  # macro, so no header and no other translation unit sees the define.
  WRAP="-Wl,--wrap=glBegin,--wrap=glDrawArrays,--wrap=glDrawElements"
fi

[ "${1:-}" = "clean" ] && rm -rf "$OUT"
mkdir -p "$OUT/obj"

# libogg's os_types.h ends in "#include <ogg/config_types.h>" for every
# compiler it does not know by name, GCC on Linux among them, and that header
# is one configure writes: it is not in the tarball and so not in the tree.
# It is generated here, as syntax.sh generates its forwarding headers, so that
# the build does not depend on a system libogg-dev happening to be installed.
# Written only when its content changes: the objects depend on it, and a fresh
# mtime on every run would recompile the whole tree every time.
GEN="$OUT/gen"
mkdir -p "$GEN/ogg"
cat > "$GEN/ogg/config_types.h.new" <<'EOF'
#ifndef __CONFIG_TYPES_H__
#define __CONFIG_TYPES_H__
#include <stdint.h>
typedef int16_t ogg_int16_t;
typedef uint16_t ogg_uint16_t;
typedef int32_t ogg_int32_t;
typedef uint32_t ogg_uint32_t;
typedef int64_t ogg_int64_t;
#endif
EOF
if cmp -s "$GEN/ogg/config_types.h.new" "$GEN/ogg/config_types.h"; then
    rm "$GEN/ogg/config_types.h.new"
else
    mv "$GEN/ogg/config_types.h.new" "$GEN/ogg/config_types.h"
fi

command -v sdl-config >/dev/null 2>&1 || {
    echo "sdl-config not found - libsdl1.2-dev is missing."; exit 2; }

INC="-I$GAME/src -I$HERE -I$GEN
     -I$LIBS/tinyxml-2.6.2 -I$LIBS/sigslot -I$LIBS/mtrand-1.1
     -I$LIBS/openal-soft-1.25.2/include -I$LIBS/openal-soft-1.25.2/include/AL
     -I$LIBS/libvorbis-1.3.4/include -I$LIBS/libvorbis-1.3.4/lib
     -I$LIBS/libogg-1.3.2/include -I$ZLIB -I$LIBS/stb
     -I$ZLIB/contrib/minizip
     -I$LIBS/minih264 -I$LIBS/minimp4 -I$LIBS/shine
     $(sdl-config --cflags)"

# -DTIXML_USE_STL as in both other builds. -fno-strict-aliasing, because the
# tree reads across pointer types in several places (the vendored encoders do
# it too) and GCC would otherwise be allowed to optimise that away.
CFLAGS="-O2 -fno-strict-aliasing -DTIXML_USE_STL $INC"
CXXFLAGS="$CFLAGS -std=c++14 -Wno-register"

# "flags" prints them and stops, which is how Tools/compile_db.sh builds the
# compilation database the clang tools need. Asking rather than copying is the
# point: a compile_commands.json with its own idea of the include paths is a
# refactoring tool parsing a different program from the one that ships.
if [ "${1:-}" = "flags" ]; then echo "$CXXFLAGS"; exit 0; fi

# The game's sources without the three that do not come along here:
#   stackwalker  - Win32 SEH, exists only there
#   audiocapture - the #else branch is a stub, but it does come along
#   pch          - the translation unit that creates the PCH under MSVC
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
compile() { # $1=file $2=flags
  # The flags go into the name, not only the path: otherwise, changing the
  # flags leaves the old object lying there, because it is newer than the
  # source.
  local o="$OUT/obj/$(echo "$1 $2" | md5sum | cut -c1-12)-$(basename "$1").o"
  local d="$o.d"
  # Reuse the object only if it is newer than the source AND than every header
  # the source included last time. Without the second condition, a changed
  # header recompiles only the files that changed themselves - the rest stays
  # built against the old class layout, and the singletons then lie on top of
  # each other in memory.
  if [ -f "$o" ] && [ -f "$d" ] && [ "$o" -nt "$1" ]; then
      local stale=0 dep
      for dep in $(sed -e 's/^[^:]*://' -e 's/\\$//' "$d"); do
          [ -e "$dep" ] && [ "$dep" -nt "$o" ] && { stale=1; break; }
      done
      [ $stale -eq 0 ] && { echo "$o"; return 0; }
  fi
  # gcc for .c, g++ for .cpp: emcc picks the language by extension, the two GNU
  # drivers do not - g++ compiles a .c as C++ as well, and the vendored
  # libraries are C and will not compile that way.
  local cc=g++
  case "$1" in *.c) cc=gcc;; esac
  if ! $cc -c "$1" -o "$o" -MMD -MF "$d" $2 2> "$o.log"; then
      echo "FAILED: $1" >&2; head -30 "$o.log" >&2; return 1
  fi
  echo "$o"
}

OBJS=""
total=$(echo $SRCS $CSRCS | wc -w)
for f in $CSRCS; do o=$(compile "$f" "$CFLAGS")   || { fail=1; continue; }; OBJS="$OBJS $o"; done
for f in $SRCS
do
  # Only the three that get anything out of it. It is not in CXXFLAGS:
  # otherwise switching between the build kinds would recompile every unit -
  # the two output directories separate them anyway.
  extra=""
  case "$f" in */engine.cpp|*/testhooks.cpp|*/renderer.cpp) extra="$HOOKS";; esac
  o=$(compile "$f" "$CXXFLAGS $extra") || { fail=1; continue; }
  OBJS="$OBJS $o"
done
[ $fail -ne 0 ] && { echo "### COMPILE FAILED ###"; exit 1; }
echo "### compiled $total translation units OK ###"

# -lX11 for the fullscreen switch in linux_window.cpp. SDL brings it along
# itself, but that cannot be relied on: under sdl12-compat there is SDL 2
# underneath, and that loads its video drivers only at runtime.
g++ $OBJS $WRAP -o "$OUT/blocks5" $(sdl-config --libs) -lopenal -lGL -lX11 -lm -lpthread || {
    echo "### LINK FAILED ###"; exit 1; }
echo "### LINK OK -> $OUT/blocks5 ($(du -h "$OUT/blocks5" | cut -f1)) ###"

# data.zip is a build product and is not in Git. Without it the game does not
# get past the loading screen, which looks like a fault in the build although
# only one step is missing.
[ -f "$GAME/data.zip" ] || echo "(warning: data.zip missing - Blocks5/pack.sh builds it)"
# So is the campaign, and its absence is quieter still: the game comes up
# normally and the level selection simply has nothing shipped in it.
[ -f "$GAME/levels/campaigns/blocks.zip" ] || echo "(warning: blocks.zip missing - Blocks5/pack.sh campaign builds it)"
[ -f "$GAME/levels/skins/blocks_01.zip" ] || echo "(warning: the skin archives are missing - Blocks5/pack.sh skins builds them)"

# The game opens data.zip relative to the working directory and therefore has
# to run out of Blocks5/ - exactly as under Windows.
if [ "${1:-}" = "run" ]; then
    shift
    cd "$GAME" && exec "$OUT/blocks5" "$@"
fi
