#!/bin/bash
# build.sh - build Blocks 5 for the browser with Emscripten.
#
# Run from anywhere; paths are resolved relative to this script.
#   ./build.sh            incremental
#   ./build.sh clean      from scratch
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME="$HERE/../Blocks5"
ZLIB="$GAME/libs/zlib-1.3.1"
OUT="$HERE/build"
source /home/user/emsdk/emsdk_env.sh >/dev/null 2>&1

[ "${1:-}" = "clean" ] && rm -rf "$OUT"
mkdir -p "$OUT/obj"

INC="-I$GAME/src -I$HERE
     -I$GAME/libs/tinyxml-2.6.2 -I$GAME/libs/sigslot -I$GAME/libs/mtrand-1.1
     -I$GAME/libs/openal-soft-1.25.2/include -I$GAME/libs/openal-soft-1.25.2/include/AL
     -I$GAME/libs/libvorbis-1.3.4/include -I$GAME/libs/libvorbis-1.3.4/lib
     -I$GAME/libs/libogg-1.3.2/include -I$GAME/libs/zlib-1.3.1 -I$GAME/libs/stb
     -I$GAME/libs/zlib-1.3.1/contrib/minizip"

CFLAGS="-O2 -DTIXML_USE_STL -DBLOCKS5_NO_FFMPEG -sUSE_SDL=1 $INC"
CXXFLAGS="$CFLAGS -std=c++14 -Wno-register -include $HERE/compat.h"

# Game sources, minus the four that cannot come along:
#   stackwalker  - Win32 SEH crash handler
#   videorecorder- ffmpeg (replaced by videorecorder_stub.cpp)
#   hq2x         - links a prebuilt x86 .obj
#   pch          - the Create-PCH translation unit, unused here
SRCS=$(ls "$GAME"/src/*.cpp | grep -vE '/(stackwalker|videorecorder|hq2x|pch)\.cpp$')
SRCS="$SRCS $HERE/gl_compat.cpp $HERE/gl_immediate.cpp $HERE/videorecorder_stub.cpp $HERE/platform_stubs.cpp $HERE/img_load.cpp $HERE/web_transfer.cpp $HERE/web_audio.cpp"
CSRCS="$GAME/libs/zlib-1.3.1/contrib/minizip/ioapi.c
       $GAME/libs/zlib-1.3.1/contrib/minizip/unzip.c
       $GAME/libs/zlib-1.3.1/contrib/minizip/zip.c
       $ZLIB/adler32.c $ZLIB/compress.c $ZLIB/crc32.c $ZLIB/deflate.c
       $ZLIB/infback.c $ZLIB/inffast.c $ZLIB/inflate.c $ZLIB/inftrees.c
       $ZLIB/trees.c $ZLIB/uncompr.c $ZLIB/zutil.c
       $GAME/libs/libogg-1.3.2/src/bitwise.c $GAME/libs/libogg-1.3.2/src/framing.c"
for f in analysis bitrate block codebook envelope floor0 floor1 info lookup lpc lsp \
         mapping0 mdct misc psy registry res0 sharedbook smallft synthesis vorbisenc \
         vorbisfile window; do CSRCS="$CSRCS $GAME/libs/libvorbis-1.3.4/lib/$f.c"; done
# TinyXML 2.6.2 is vendored in the tree and compiled here exactly as the Visual
# Studio project compiles it, so both builds run the same parser.
for f in tinyxml tinyxmlparser tinyxmlerror tinystr; do SRCS="$SRCS $GAME/libs/tinyxml-2.6.2/$f.cpp"; done

fail=0; n=0; total=$(echo $SRCS $CSRCS | wc -w)
compile() { # $1=file $2=flags
  local o="$OUT/obj/$(echo "$1" | md5sum | cut -c1-12)-$(basename "$1").o"
  local d="$o.d"
  # Reuse the object only if it is newer than the source AND every header the
  # source pulled in last time. Without the header check, editing a header that
  # changes a class layout (engine.h's key tables, say) leaves every unmodified
  # .cpp compiled against the old layout: the link then merges vague-linkage
  # statics at two different sizes and the singletons overlap in memory. That
  # bug looks like random corruption a long way from its cause.
  if [ -f "$o" ] && [ -f "$d" ] && [ "$o" -nt "$1" ]; then
      local stale=0 dep
      for dep in $(sed -e 's/^[^:]*://' -e 's/\\$//' "$d"); do
          [ -e "$dep" ] && [ "$dep" -nt "$o" ] && { stale=1; break; }
      done
      [ $stale -eq 0 ] && { echo "$o"; return 0; }
  fi
  if ! emcc -c "$1" -o "$o" -MMD -MF "$d" $2 2> "$o.log"; then
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

# -sSTACK_SIZE: minizip's zipOpen3 puts a zip64_internal on the stack, and that
# struct embeds a 64 KiB compression buffer (zip.c:150, Z_BUFSIZE). Emscripten's
# default 64 KiB stack is exactly consumed by it, so every zip WRITE - saving a
# campaign, saving progress - clobbered the stack and trapped with "table index
# is out of bounds". Reads were unaffected, which is why it stayed hidden.
em++ $OBJS -o "$OUT/blocks5.html" \
  -O2 -sASSERTIONS=1 -sUSE_SDL=1 -lopenal \
  -sLEGACY_GL_EMULATION=1 -sGL_UNSAFE_OPTS=0 \
  -Wl,--wrap=SDL_CreateRGBSurface \
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 \
  -sEXIT_RUNTIME=0 -sSTACK_SIZE=4194304 -lidbfs.js --pre-js $HERE/pre.js \
  $PRELOAD \
  2>&1 | tail -30
[ -f "$OUT/blocks5.wasm" ] && echo "### LINK OK -> $OUT/blocks5.wasm ($(du -h "$OUT/blocks5.wasm" | cut -f1)) ###" || echo "### LINK FAILED ###"
