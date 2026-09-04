#!/bin/bash
# build.sh - build Blocks 5 for the browser with Emscripten.
#
# Run from anywhere; paths are resolved relative to this script.
#   ./build.sh            incremental
#   ./build.sh clean      from scratch
#   ./build.sh hooks      plus die Testhaken aus test_hooks.cpp
#
# "hooks" uebersetzt test_hooks.cpp mit -DBLOCKS5_TEST_HOOKS und baut nach
# build-test/ statt build/, damit ein Build mit Haken nie versehentlich der
# ausgelieferte ist. Ohne das Wort ist die Uebersetzungseinheit leer.
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

# Game sources, minus the four that cannot come along:
#   stackwalker  - Win32 SEH crash handler
#   videorecorder- portable now, but nothing here captures audio (replaced by
#                  videorecorder_stub.cpp)
#   pch          - the Create-PCH translation unit, unused here
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
  # Nur test_hooks.cpp sieht das Define. Es steht nicht in CXXFLAGS, damit ein
  # Wechsel zwischen den beiden Buildarten nicht jede der 160 Einheiten neu
  # uebersetzt - die beiden Ausgabeverzeichnisse trennen sie ohnehin.
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
# Genau die drei Dateien, die stage.bat nach levels/ legt. Frueher stand hier
# levels/*.xml und levels/*.ogg, und das griff in das Arbeitsverzeichnis des
# Autors: die 42 Quell-Level und die 10 Musikstuecke, aus denen blocks.zip
# gebaut wird. Alle 52 lagen damit ein zweites Mal im Paket, byte-identisch zu
# einem Mitglied des Archivs - 8,3 der 21 MiB, die der Browser laedt, fuer
# nichts. Gebraucht wird keine davon: die Kampagnenmusik holt gs_game.cpp aus
# blocks.zip selbst, und die beiden Beispiel-Level nennen gar keine.
cp "$GAME"/levels/example0*.xml        "$WEBROOT/levels/"    2>/dev/null
cp "$GAME/levels/readme.txt"           "$WEBROOT/levels/"    2>/dev/null
cp "$GAME"/levels/campaigns/*.zip      "$WEBROOT/levels/campaigns/" 2>/dev/null
cp "$GAME"/levels/skins/*.zip          "$WEBROOT/levels/skins/"     2>/dev/null
cp "$GAME/screenshots/readme.txt"      "$WEBROOT/screenshots/" 2>/dev/null
cp "$GAME/videos/readme.txt"           "$WEBROOT/videos/"      2>/dev/null
PRELOAD="--preload-file $WEBROOT@/"
[ -f "$GAME/data.zip" ] || echo "(warning: data.zip missing - run zip_data.bat or the zip -P equivalent)"
echo "webroot: $(du -sh "$WEBROOT" | cut -f1)"

# -sINITIAL_MEMORY: 48 MiB, gemessen und nicht geraten. Von 16 MiB aus waechst
# der Heap genau einmal auf 40 MiB und bleibt dort - durch Ladebild, Menue,
# Optionen, Manager, Leveleditor, Levelauswahl und eine halbe Minute gespieltes
# Level. Grosszuegiger vorzulegen kostet auf einem Telefon den Tab, bevor das
# Menue steht. ALLOW_MEMORY_GROWTH bleibt an, ein ungewoehnlich grosses Level
# hat also Luft.
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
# Der Rueckgabewert der Pipe ist der von tail und damit immer 0. Gefragt ist
# der von em++, sonst meldet ein fehlgeschlagenes Linken den blocks5.wasm des
# vorigen Laufs als Erfolg - und genau das hat einen Linkfehler eine Weile
# verdeckt.
linkStatus=${PIPESTATUS[0]}
[ $linkStatus -ne 0 ] && { echo "### LINK FAILED ###"; exit 1; }
# The four files that get uploaded are blocks5.{js,wasm,data} plus the page, and
# the page has to be called index.html so that the directory it is dropped into
# serves it by itself. Only the HTML is renamed: em++ derives the js/wasm/data
# names from its -o argument, and the page refers to blocks5.js by name, so
# giving em++ index.html would rename all four and buy nothing.
[ -f "$OUT/blocks5.html" ] && cp "$OUT/blocks5.html" "$OUT/index.html"

# Die drei Nutzlastdateien bekommen die Kennung des Baus in den Namen. Sie
# gehoeren zusammen - blocks5.js traegt eine Tabelle mit Byteabstaenden in
# blocks5.data, und die EM_ASM-Bausteine liegen an Adressen, die nur zu diesem
# einen wasm passen -, und getrennt zwischengespeichert werden koennen sie
# ueberall: im Browser, in einem Proxy, in mod_pagespeed. Genau das ist
# passiert: eine von PageSpeed unter eigenem Namen aufbewahrte alte
# blocks5.js neben einer frischen blocks5.wasm, und das Spiel brach mit
# "No EM_ASM constant found at address ..." ab.
#
# Mit der Kennung im Namen ist jede URL unveraenderlich. Ein alter Stand kann
# dann nur noch vollstaendig alt sein, und das ist harmlos.
version=$(cat "$OUT/blocks5.js" "$OUT/blocks5.wasm" "$OUT/blocks5.data" | md5sum | cut -c1-12)
rm -f "$OUT"/blocks5-*.js "$OUT"/blocks5-*.wasm "$OUT"/blocks5-*.data
mv "$OUT/blocks5.js"   "$OUT/blocks5-$version.js"
mv "$OUT/blocks5.wasm" "$OUT/blocks5-$version.wasm"
mv "$OUT/blocks5.data" "$OUT/blocks5-$version.data"

# Die Zeile des Ladebildschirms, in der Schrift des Spiels. Die Seite steht vor
# data.zip und vor jedem GL-Kontext und kann diese Schrift nicht selbst
# zeichnen, also wird sie hier gezeichnet und als Daten-URI in die Seite
# gestempelt: keine zusaetzliche Anfrage, nichts, was im Zwischenspeicher
# fehlen koennte, und beim ersten Bild schon da. Es ist $LOADING aus
# data/languages.txt, also dieselbe Zeile, die das Spiel gleich darauf selbst
# anzeigt.
loadtext=$(python3 "$HERE/make_text.py" --js "$GAME/data/font.xml" '$LOADING')

# In der Seite drei Stellen: das Skript-Tag, das em++ eingesetzt hat, der
# Stempel, aus dem Module.locateFile die Namen der beiden anderen bildet, und
# die beiden Bilder der Ladezeile. Das Einsetzen des letzten macht python3 und
# nicht sed, weil base64 Schraegstriche und Pluszeichen enthaelt.
for page in "$OUT/blocks5.html" "$OUT/index.html"; do
  sed -i -e "s/blocks5\.js/blocks5-$version.js/g" -e "s/%%BUILD%%/$version/g" "$page"
  python3 - "$page" "$loadtext" <<'PYEOF'
import io, sys
path, text = sys.argv[1], sys.argv[2]
page = io.open(path, encoding='utf-8').read()
if '%%LOADTEXT%%' not in page:
    raise SystemExit('%s: kein %%LOADTEXT%% in der Seite' % path)
io.open(path, 'w', encoding='utf-8', newline='\n').write(page.replace('%%LOADTEXT%%', text))
PYEOF
done

# Alles fuer die installierbare Seite. Das gehoert neben index.html und nicht in
# den webroot: der wird ins virtuelle Dateisystem gepackt, ueber HTTP
# ausgeliefert wird dieses Verzeichnis hier.
#
# Der Name der Zwischenspeicherung ist ein Hash der drei Nutzlastdateien. Damit
# wechselt sie genau dann, wenn sich die Nutzlast aendert, und nie sonst - und
# blocks5.js kann nie neben einem blocks5.data eines anderen Baus landen. Siehe
# den Kopf von sw.js und ROADMAP.md, Punkt 20.
cp "$HERE/manifest.json" "$OUT/manifest.json"
cp "$HERE/touch_controls.js" "$OUT/touch_controls.js"
# Die Kopfzeilen fuer Apache. index.html traegt als einzige Datei keine
# Kennung im Namen und ist deshalb die eine, die nicht zwischengespeichert
# werden darf - sonst erfaehrt niemand von einem neuen Bau.
cp "$HERE/htaccess" "$OUT/.htaccess"
# Das Symbol ist dasselbe, das das Spielfenster traegt - 32x32, und damit zu
# klein fuer einen Startbildschirm. Ein Telefon vergroessert es sonst selbst und
# glaettet dabei; ganzzahlig pixelvervielfacht bleibt jede Kante hart, was zum
# Spiel passt. make_icon.py kommt mit der Standardbibliothek aus, das kostet
# also keine Abhaengigkeit.
#
# Vier Stueck, weil sie verschieden benutzt werden:
#   192/512 "any"   randlos und mit Transparenz, wird unveraendert angezeigt.
#   512 "maskable"  Der Startbildschirm schneidet sich eine eigene Form heraus,
#                   sicher ist nur ein Kreis von 80% der Kante. Das Bild ist
#                   randlos rund und ragt weit darueber hinaus, also 10fach
#                   (320px) statt 16fach, mittig auf deckendem Schwarz - ein
#                   durchsichtiges Pixel waere beim Maskieren ein Loch.
#   apple-touch     iOS wertet keine Transparenz aus und rundet nur die Ecken,
#                   wo ohnehin nichts steht. Also randlos, aber deckend.
# Erst weg damit: $OUT wird nicht geleert, und ein Symbol, das einmal anders
# hiess, laege sonst fuer immer im ausgelieferten Verzeichnis.
rm -f "$OUT"/icon*.png "$OUT"/apple-touch-icon.png
python3 "$HERE/make_icon.py" "$GAME/data/window.png" "$OUT/icon-192.png" --scale 6 >/dev/null
python3 "$HERE/make_icon.py" "$GAME/data/window.png" "$OUT/icon-512.png" --scale 16 >/dev/null
python3 "$HERE/make_icon.py" "$GAME/data/window.png" "$OUT/icon-maskable-512.png" \
        --scale 10 --canvas 512 --background 000000 >/dev/null
python3 "$HERE/make_icon.py" "$GAME/data/window.png" "$OUT/apple-touch-icon.png" \
        --scale 16 --canvas 512 --background 000000 >/dev/null
sed "s/%%VERSION%%/$version/" "$HERE/sw.js" > "$OUT/sw.js"
echo "### PWA: manifest.json, 4 Symbole, sw.js (cache blocks5-$version) ###"

[ -f "$OUT/blocks5-$version.wasm" ] || { echo "### LINK FAILED ###"; exit 1; }
echo "### LINK OK -> $OUT/blocks5-$version.wasm ($(du -h "$OUT/blocks5-$version.wasm" | cut -f1)) ###"
