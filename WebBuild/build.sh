#!/bin/bash
# build.sh - build Blocks 5 for the browser with Emscripten.
#
# Run from anywhere; paths are resolved relative to this script.
#   ./build.sh            incremental
#   ./build.sh clean      from scratch
#   ./build.sh hooks      plus the test hooks from test_hooks.cpp
#
# "hooks" compiles test_hooks.cpp with -DBLOCKS5_TEST_HOOKS and builds into
# build-test/ instead of build/, keeping a build with hooks from ever becoming
# the shipped one by accident. Without the word the translation unit is empty.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME="$HERE/../Blocks5"
ZLIB="$GAME/libs/zlib-1.3.1"
OUT="$HERE/build"
HOOKS=""
if [ "${1:-}" = "hooks" ]; then HOOKS="-DBLOCKS5_TEST_HOOKS"; OUT="$HERE/build-test"; fi
source /home/user/emsdk/emsdk_env.sh >/dev/null 2>&1

[ "${1:-}" = "clean" ] && rm -rf "$OUT"
mkdir -p "$OUT/obj"

INC="-I$GAME/src -I$HERE
     -I$GAME/libs/tinyxml-2.6.2 -I$GAME/libs/sigslot -I$GAME/libs/mtrand-1.1
     -I$GAME/libs/openal-soft-1.25.2/include -I$GAME/libs/openal-soft-1.25.2/include/AL
     -I$GAME/libs/libvorbis-1.3.4/include -I$GAME/libs/libvorbis-1.3.4/lib
     -I$GAME/libs/libogg-1.3.2/include -I$GAME/libs/zlib-1.3.1 -I$GAME/libs/stb
     -I$GAME/libs/zlib-1.3.1/contrib/minizip"

CFLAGS="-O2 -DTIXML_USE_STL -sUSE_SDL=1 $INC"
CXXFLAGS="$CFLAGS -std=c++14 -Wno-register -include $HERE/compat.h"

# The game's sources, minus the three that cannot come along:
#   stackwalker  - the SEH crash handler, Win32 only
#   videorecorder- encodes in a thread of its own, and there is none here;
#                  no audio either (replaced by videorecorder_stub.cpp)
#   pch          - the translation unit that creates the PCH under MSVC
SRCS=$(ls "$GAME"/src/*.cpp | grep -vE '/(stackwalker|videorecorder|pch)\.cpp$')
SRCS="$SRCS $HERE/gl_compat.cpp $HERE/gl_immediate.cpp $HERE/videorecorder_stub.cpp $HERE/platform_stubs.cpp $HERE/web_transfer.cpp $HERE/web_audio.cpp $HERE/web_bluescreen.cpp $HERE/test_hooks.cpp"
CSRCS="$GAME/libs/zlib-1.3.1/contrib/minizip/ioapi.c
       $GAME/libs/zlib-1.3.1/contrib/minizip/unzip.c
       $GAME/libs/zlib-1.3.1/contrib/minizip/zip.c
       $ZLIB/adler32.c $ZLIB/compress.c $ZLIB/crc32.c $ZLIB/deflate.c
       $ZLIB/infback.c $ZLIB/inffast.c $ZLIB/inflate.c $ZLIB/inftrees.c
       $ZLIB/trees.c $ZLIB/uncompr.c $ZLIB/zutil.c
       $GAME/libs/libogg-1.3.2/src/bitwise.c $GAME/libs/libogg-1.3.2/src/framing.c"
for f in analysis bitrate block codebook envelope floor0 floor1 info lookup lpc lsp \
         mapping0 mdct psy registry res0 sharedbook smallft synthesis vorbisenc \
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
for f in $SRCS;  do
  n=$((n+1))
  # Only test_hooks.cpp sees the define. It is not in CXXFLAGS, or switching
  # between the two kinds of build would recompile all 160 units - the two
  # output directories keep them apart anyway.
  extra=""
  case "$f" in */test_hooks.cpp|*/testhooks.cpp) extra="$HOOKS";; esac
  o=$(compile "$f" "$CXXFLAGS $extra") || { fail=1; continue; }
  OBJS="$OBJS $o"
done
[ $fail -ne 0 ] && { echo "### COMPILE FAILED ###"; exit 1; }
echo "### compiled $total translation units OK ###"

# Assemble exactly the runtime tree the game expects, mirroring stage.bat.
# Preloading Blocks5/ wholesale would drag in 40MB+ of .psd and .wav sources.
WEBROOT="$OUT/webroot"
rm -rf "$WEBROOT"; mkdir -p "$WEBROOT/levels/campaigns" "$WEBROOT/levels/skins" "$WEBROOT/screenshots" "$WEBROOT/videos"
cp "$GAME/data.zip"                    "$WEBROOT/"           2>/dev/null
cp "$GAME/.update_checker"             "$WEBROOT/"           2>/dev/null
cp "$GAME"/update_checker_*.bat        "$WEBROOT/"           2>/dev/null
# Exactly the three files stage.bat puts into levels/. A levels/*.xml plus
# levels/*.ogg glob would reach into the author's working directory: the 42
# source levels and the 10 music tracks blocks.zip is built from, all 52 of them
# a second time in the package and byte-identical to a member of the archive -
# 8.3 of the 21 MiB the browser loads, for nothing. None of them is needed:
# gs_game.cpp fetches the campaign music out of blocks.zip itself, and the two
# example levels name none at all.
cp "$GAME"/levels/example0*.xml        "$WEBROOT/levels/"    2>/dev/null
cp "$GAME/levels/readme.txt"           "$WEBROOT/levels/"    2>/dev/null
# These two are templates like the other readmes: main.cpp copies them into the
# user directory on a first start, and nothing ever reads them.
cp "$GAME/levels/campaigns/readme.txt" "$WEBROOT/levels/campaigns/" 2>/dev/null
cp "$GAME/levels/skins/readme.txt"     "$WEBROOT/levels/skins/"     2>/dev/null
cp "$GAME"/levels/campaigns/*.zip      "$WEBROOT/levels/campaigns/" 2>/dev/null
cp "$GAME"/levels/skins/*.zip          "$WEBROOT/levels/skins/"     2>/dev/null
cp "$GAME/screenshots/readme.txt"      "$WEBROOT/screenshots/" 2>/dev/null
cp "$GAME/videos/readme.txt"           "$WEBROOT/videos/"      2>/dev/null
PRELOAD="--preload-file $WEBROOT@/"
[ -f "$GAME/data.zip" ] || echo "(warning: data.zip missing - run zip_data.bat or the zip -P equivalent)"
echo "webroot: $(du -sh "$WEBROOT" | cut -f1)"

# -sINITIAL_MEMORY: 48 MiB, measured and not guessed. Started at 16 MiB the heap
# grows exactly once, to 40 MiB, and stays there - through the loading screen,
# the menu, the options, the manager, the level editor, the level select and
# half a minute of a played level. Reserving more generously costs a tab on a
# phone before the menu is up. ALLOW_MEMORY_GROWTH stays on; an unusually large
# level therefore has room.
#
# -sSTACK_SIZE: minizip's zipOpen3 puts a zip64_internal on the stack, and that
# struct embeds a 64 KiB compression buffer (zip.c:150, Z_BUFSIZE). Emscripten's
# default 64 KiB stack is exactly consumed by it, so every zip WRITE - saving a
# campaign, saving progress - clobbered the stack and trapped with "table index
# is out of bounds". Reads were unaffected, which is why it stayed hidden.
em++ $OBJS -o "$OUT/blocks5.html" \
  -O2 -sASSERTIONS=1 -sUSE_SDL=1 -lopenal \
  -sLEGACY_GL_EMULATION=1 -sGL_UNSAFE_OPTS=0 \
  -Wl,--wrap=SDL_CreateRGBSurface \
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=50331648 \
  -sEXIT_RUNTIME=0 -sSTACK_SIZE=4194304 -lidbfs.js --pre-js $HERE/pre.js \
  --shell-file $HERE/shell.html \
  $PRELOAD \
  2>&1 | tail -30
# The pipe's exit code is tail's and therefore always 0. What is wanted is
# em++'s: otherwise a failed link reports the previous run's blocks5.wasm as a
# success.
linkStatus=${PIPESTATUS[0]}
[ $linkStatus -ne 0 ] && { echo "### LINK FAILED ###"; exit 1; }
# The four files that get uploaded are blocks5.{js,wasm,data} plus the page, and
# the page has to be called index.html so that the directory it is dropped into
# serves it by itself. Only the HTML is renamed: em++ derives the js/wasm/data
# names from its -o argument, and the page refers to blocks5.js by name, so
# giving em++ index.html would rename all four and buy nothing.
[ -f "$OUT/blocks5.html" ] && cp "$OUT/blocks5.html" "$OUT/index.html"

# The three payload files carry the build's stamp in their names. They belong
# together - blocks5.js holds a table of byte offsets into blocks5.data, and the
# EM_ASM fragments sit at addresses that fit this one wasm and no other - and
# separately they can be cached anywhere: in the browser, in a proxy, in
# mod_pagespeed. Served in such a mixture the game aborts with "No EM_ASM
# constant found at address ...".
#
# With the stamp in the name every URL is immutable. An old state can then only
# be entirely old, and that is harmless.
version=$(cat "$OUT/blocks5.js" "$OUT/blocks5.wasm" "$OUT/blocks5.data" | md5sum | cut -c1-12)
rm -f "$OUT"/blocks5-*.js "$OUT"/blocks5-*.wasm "$OUT"/blocks5-*.data
mv "$OUT/blocks5.js"   "$OUT/blocks5-$version.js"
mv "$OUT/blocks5.wasm" "$OUT/blocks5-$version.wasm"
mv "$OUT/blocks5.data" "$OUT/blocks5-$version.data"

# The loading screen's line, in the game's own font. The page stands before
# data.zip and before any GL context and cannot draw that font itself, hence it
# is drawn here and stamped into the page as a data URI: no extra request,
# nothing that could be missing from the cache, and there with the first paint.
# It is $LOADING from data/languages.txt, the very line the game itself puts up
# a moment later.
loadtext=$(python3 "$HERE/make_text.py" --js "$GAME/data/font.xml" '$LOADING')

# Three places in the page: the script tag em++ inserted, the stamp from which
# Module.locateFile builds the names of the other two, and the two images of the
# loading line. The last of these is substituted by python3 and not by sed,
# because base64 contains slashes and plus signs.
for page in "$OUT/blocks5.html" "$OUT/index.html"; do
  sed -i -e "s/blocks5\.js/blocks5-$version.js/g" -e "s/%%BUILD%%/$version/g" "$page"
  python3 - "$page" "$loadtext" <<'PYEOF'
import io, sys
path, text = sys.argv[1], sys.argv[2]
page = io.open(path, encoding='utf-8').read()
if '%%LOADTEXT%%' not in page:
    raise SystemExit('%s: no %%LOADTEXT%% in the page' % path)
io.open(path, 'w', encoding='utf-8', newline='\n').write(page.replace('%%LOADTEXT%%', text))
PYEOF
done

# Everything for the installable page. It belongs beside index.html and not in
# the webroot: the webroot is packed into the virtual filesystem, while this
# directory here is what gets served over HTTP.
#
# The cache name is a hash of the three payload files. It therefore changes
# exactly when the payload changes and never otherwise - and blocks5.js can
# never end up beside a blocks5.data from another build. See the header of
# sw.js and ROADMAP.md, item 20.
cp "$HERE/manifest.json" "$OUT/manifest.json"
cp "$HERE/touch_controls.js" "$OUT/touch_controls.js"
# The headers for Apache. index.html is the only file carrying no stamp in its
# name and is therefore the one that must not be cached - otherwise nobody
# learns of a new build.
cp "$HERE/htaccess" "$OUT/.htaccess"
# The icon is the same one the game window carries - 32x32, and therefore too
# small for a home screen. A phone would otherwise scale it up itself and smooth
# it in the process; pixel replication at a whole factor keeps every edge hard,
# which suits the game. make_icon.py gets by with the standard library and
# therefore costs no dependency.
#
# Four of them, because they are used differently:
#   192/512 "any"   full-bleed and with transparency, shown unchanged.
#   512 "maskable"  The launcher crops a shape of its own out of it, and only a
#                   circle of 80% of the edge is safe. The drawing is full-bleed
#                   round and reaches far beyond that, hence 10x (320px) instead
#                   of 16x, centred on opaque black - a transparent pixel would
#                   be a hole under the mask.
#   apple-touch     iOS reads no transparency and only rounds the corners,
#                   where nothing stands anyway. Full-bleed, but opaque.
# Get rid of them first: $OUT is not emptied, and an icon that once had another
# name would otherwise lie in the shipped directory for ever.
rm -f "$OUT"/icon*.png "$OUT"/apple-touch-icon.png
python3 "$HERE/make_icon.py" "$GAME/data/window.png" "$OUT/icon-192.png" --scale 6 >/dev/null
python3 "$HERE/make_icon.py" "$GAME/data/window.png" "$OUT/icon-512.png" --scale 16 >/dev/null
python3 "$HERE/make_icon.py" "$GAME/data/window.png" "$OUT/icon-maskable-512.png" \
        --scale 10 --canvas 512 --background 000000 >/dev/null
python3 "$HERE/make_icon.py" "$GAME/data/window.png" "$OUT/apple-touch-icon.png" \
        --scale 16 --canvas 512 --background 000000 >/dev/null
sed "s/%%VERSION%%/$version/" "$HERE/sw.js" > "$OUT/sw.js"
echo "### PWA: manifest.json, 4 icons, sw.js (cache blocks5-$version) ###"

[ -f "$OUT/blocks5-$version.wasm" ] || { echo "### LINK FAILED ###"; exit 1; }
echo "### LINK OK -> $OUT/blocks5-$version.wasm ($(du -h "$OUT/blocks5-$version.wasm" | cut -f1)) ###"
