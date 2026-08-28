#!/bin/bash
# build.sh - build Blocks 5 for the browser with Emscripten.
#
# Run from anywhere; paths are resolved relative to this script.
#   ./build.sh            incremental
#   ./build.sh clean      from scratch
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME="$HERE/../Blocks5"
DEPS="${BLOCKS5_DEPS:-/home/user/deps}"
OUT="$HERE/build-asan"
source /home/user/emsdk/emsdk_env.sh >/dev/null 2>&1

[ "${1:-}" = "clean" ] && rm -rf "$OUT"
mkdir -p "$OUT/obj"

INC="-I$GAME/src -I$HERE
     -I$GAME/libs/tinyxml-2.6.2 -I$GAME/libs/sigslot -I$GAME/libs/mtrand-1.1
     -I$GAME/libs/OpenAL-1.1/include
     -I$DEPS/vorbis/include -I$DEPS/vorbis/lib -I$DEPS/ogg/include -I$DEPS/zlib -I$DEPS/stb
     -I$GAME/libs/zlib-1.2.8/contrib/minizip"

CFLAGS="-O1 -fsanitize=address -DTIXML_USE_STL -DBLOCKS5_NO_FFMPEG -sUSE_SDL=1 $INC"
CXXFLAGS="$CFLAGS -std=c++14 -Wno-register -include $HERE/compat.h"

# Game sources, minus the four that cannot come along:
#   stackwalker  - Win32 SEH crash handler
#   videorecorder- ffmpeg (replaced by videorecorder_stub.cpp)
#   hq2x         - links a prebuilt x86 .obj
#   pch          - the Create-PCH translation unit, unused here
SRCS=$(ls "$GAME"/src/*.cpp | grep -vE '/(stackwalker|videorecorder|hq2x|pch)\.cpp$')
SRCS="$SRCS $HERE/gl_compat.cpp $HERE/gl_immediate.cpp $HERE/videorecorder_stub.cpp $HERE/platform_stubs.cpp $HERE/img_load.cpp"
CSRCS="$GAME/libs/zlib-1.2.8/contrib/minizip/ioapi.c
       $GAME/libs/zlib-1.2.8/contrib/minizip/unzip.c
       $GAME/libs/zlib-1.2.8/contrib/minizip/zip.c
       $DEPS/zlib/adler32.c $DEPS/zlib/compress.c $DEPS/zlib/crc32.c $DEPS/zlib/deflate.c
       $DEPS/zlib/infback.c $DEPS/zlib/inffast.c $DEPS/zlib/inflate.c $DEPS/zlib/inftrees.c
       $DEPS/zlib/trees.c $DEPS/zlib/uncompr.c $DEPS/zlib/zutil.c
       $DEPS/ogg/src/bitwise.c $DEPS/ogg/src/framing.c"
for f in analysis bitrate block codebook envelope floor0 floor1 info lookup lpc lsp \
         mapping0 mdct misc psy registry res0 sharedbook smallft synthesis vorbisenc \
         vorbisfile window; do CSRCS="$CSRCS $DEPS/vorbis/lib/$f.c"; done
# TinyXML 1 is linked as a prebuilt .lib on Windows, so its sources are not vendored.
for f in tinyxml tinyxmlparser tinyxmlerror tinystr; do SRCS="$SRCS $DEPS/tinyxml1/$f.cpp"; done

fail=0; n=0; total=$(echo $SRCS $CSRCS | wc -w)
compile() { # $1=file $2=flags
  local o="$OUT/obj/$(echo "$1" | md5sum | cut -c1-12)-$(basename "$1").o"
  if [ -f "$o" ] && [ "$o" -nt "$1" ]; then echo "$o"; return 0; fi
  if ! emcc -c "$1" -o "$o" $2 2> "$o.log"; then
      echo "FAILED: $1" >&2; head -20 "$o.log" >&2; return 1
  fi
  echo "$o"
}
OBJS=""
for f in $CSRCS; do n=$((n+1)); o=$(compile "$f" "$CFLAGS") || { fail=1; continue; }; OBJS="$OBJS $o"; done
for f in $SRCS;  do n=$((n+1)); o=$(compile "$f" "$CXXFLAGS") || { fail=1; continue; }; OBJS="$OBJS $o"; done
[ $fail -ne 0 ] && { echo "### COMPILE FAILED ###"; exit 1; }
echo "### compiled $total translation units OK ###"

# Assemble exactly the runtime tree the game expects, mirroring stage.bat.
# Preloading Blocks5/ wholesale would drag in 40MB+ of .psd and .wav sources.
WEBROOT="$OUT/webroot"
rm -rf "$WEBROOT"; mkdir -p "$WEBROOT/levels/campaigns" "$WEBROOT/levels/skins" "$WEBROOT/screenshots" "$WEBROOT/videos"
cp "$GAME/data.zip"                    "$WEBROOT/"           2>/dev/null
cp "$GAME/config.xml"                  "$WEBROOT/"           2>/dev/null
cp "$GAME/.update_checker"             "$WEBROOT/"           2>/dev/null
cp "$GAME"/update_checker_*.bat        "$WEBROOT/"           2>/dev/null
cp "$GAME"/levels/*.xml                "$WEBROOT/levels/"    2>/dev/null
cp "$GAME"/levels/*.ogg                "$WEBROOT/levels/"    2>/dev/null
cp "$GAME/levels/readme.txt"           "$WEBROOT/levels/"    2>/dev/null
cp "$GAME"/levels/campaigns/*.zip      "$WEBROOT/levels/campaigns/" 2>/dev/null
cp "$GAME"/levels/skins/*.zip          "$WEBROOT/levels/skins/"     2>/dev/null
cp "$GAME/screenshots/readme.txt"      "$WEBROOT/screenshots/" 2>/dev/null
cp "$GAME/videos/readme.txt"           "$WEBROOT/videos/"      2>/dev/null
PRELOAD="--preload-file $WEBROOT@/"
[ -f "$GAME/data.zip" ] || echo "(warning: data.zip missing - run zip_data.bat or the zip -P equivalent)"
echo "webroot: $(du -sh "$WEBROOT" | cut -f1)"

em++ $OBJS -o "$OUT/blocks5.html" \
  -O1 -g2 -fsanitize=address -sASSERTIONS=2 -sUSE_SDL=1 -lopenal \
  -sLEGACY_GL_EMULATION=1 -sGL_UNSAFE_OPTS=0 \
  -Wl,--wrap=SDL_CreateRGBSurface \
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=536870912 -sSTACK_SIZE=5242880 \
  -sEXIT_RUNTIME=0 -lidbfs.js --pre-js $HERE/pre.js \
  $PRELOAD \
  2>&1 | tail -30
[ -f "$OUT/blocks5.wasm" ] && echo "### LINK OK -> $OUT/blocks5.wasm ($(du -h "$OUT/blocks5.wasm" | cut -f1)) ###" || echo "### LINK FAILED ###"
