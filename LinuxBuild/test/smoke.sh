#!/bin/bash
# smoke.sh - den Linux-Build starten und durch die Oberflaeche fuehren.
#
#   LinuxBuild/build.sh && LinuxBuild/test/smoke.sh
#
# Anders als im Browser gibt es hier keine Testhaken: der GUI-Baum ist von
# aussen nicht zu sehen, und geklickt wird auf Koordinaten. Das geht nur, weil
# das Spiel immer in 640x480 zeichnet und das Fenster ein ganzzahliges
# Vielfaches davon ist - die Umrechnung steht in click() und liest den
# Fensterursprung bei xdotool nach, statt ihn zu raten.
#
# Gebraucht werden Xvfb, ein Fenstermanager (openbox), xdotool und ffmpeg:
#
#   sudo apt install xvfb openbox xdotool ffmpeg x11-utils
#
# Ohne Fenstermanager laeuft alles ausser dem Vollbildwechsel: darum bittet das
# Spiel nach EWMH, und ohne Fenstermanager hoert das niemand.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME="$HERE/../../Blocks5"
EXE="$HERE/../build/blocks5"
OUT="${B5_SHOTS:-/tmp/blocks5-smoke}"
DISP="${B5_DISPLAY:-:99}"
SCREEN_W=1600
SCREEN_H=1200

[ -x "$EXE" ] || { echo "$EXE fehlt - erst LinuxBuild/build.sh laufen lassen."; exit 2; }
for t in Xvfb xdotool ffmpeg; do
    command -v $t >/dev/null 2>&1 || { echo "$t fehlt."; exit 2; }
done

problems=0
note() { echo "  ! $*"; problems=$((problems + 1)); }
ok()   { echo "  . $*"; }

rm -rf "$OUT"; mkdir -p "$OUT"

# Eigene Anzeige, damit der Lauf nichts anfasst, was sonst noch offen ist.
Xvfb "$DISP" -screen 0 ${SCREEN_W}x${SCREEN_H}x24 >"$OUT/xvfb.log" 2>&1 &
XVFB_PID=$!
sleep 2
export DISPLAY="$DISP"
if command -v openbox >/dev/null 2>&1; then
    openbox >"$OUT/wm.log" 2>&1 &
    WM_PID=$!
    sleep 2
else
    WM_PID=""
    echo "  (kein Fenstermanager - der Vollbildtest faellt aus)"
fi

cleanup() {
    kill "${GAME_PID:-}" 2>/dev/null
    [ -n "$WM_PID" ] && kill "$WM_PID" 2>/dev/null
    kill "$XVFB_PID" 2>/dev/null
    wait 2>/dev/null
}
trap cleanup EXIT

# ALSOFT_DRIVERS=null: auf einer Maschine ohne Tonausgabe bricht das Spiel
# sonst schon beim Start ab, und darum geht es hier nicht.
( cd "$GAME" && ALSOFT_DRIVERS=null "$EXE" -windowed >"$OUT/run.log" 2>&1 ) &
GAME_PID=$!

win() { xdotool search --name "Blocks 5" 2>/dev/null | head -1; }
geom() { xdotool getwindowgeometry --shell "$1" 2>/dev/null; }
shot() { ffmpeg -loglevel error -f x11grab -video_size ${SCREEN_W}x${SCREEN_H} -i "$DISP" -frames:v 1 "$OUT/$1.png" -y; }

# Auf das Fenster warten statt eine Zeit zu raten: unter llvmpipe braucht der
# Start eine halbe Minute, auf richtiger Hardware einen Augenblick.
echo "Warte auf das Fenster ..."
W=""
for i in $(seq 1 60); do
    W=$(win); [ -n "$W" ] && break
    sleep 1
done
[ -n "$W" ] || { echo "FEHLGESCHLAGEN: kein Fenster nach 60 s"; tail -20 "$OUT/run.log"; exit 1; }

# Und danach auf das erste gezeichnete Bild: das Fenster steht lange, bevor
# etwas darin ist.
for i in $(seq 1 60); do
    shot warmup
    python3 - "$OUT/warmup.png" <<'PY' && break
import sys
from PIL import Image
import numpy as np
a = np.array(Image.open(sys.argv[1]).convert('RGB'))
sys.exit(0 if a.mean() > 20 else 1)
PY
    sleep 1
done

eval "$(geom "$W")"
ORIGIN_X=$X; ORIGIN_Y=$Y; SCALE=$((WIDTH / 640))
echo "Fenster $WIDTH x $HEIGHT bei ($ORIGIN_X, $ORIGIN_Y), Massstab ${SCALE}x"
[ "$SCALE" -ge 1 ] || { echo "FEHLGESCHLAGEN: Fenster schmaler als 640"; exit 1; }

# Spiel-Pixel zu Bildschirm-Pixel. Ein Klick, den das Spiel sieht: es liest die
# Maus einmal je Logiktakt von 20 ms, ein Druck und Loslassen in derselben
# Millisekunde faellt dazwischen durch.
click() {
    xdotool mousemove $((ORIGIN_X + $1 * SCALE)) $((ORIGIN_Y + $2 * SCALE))
    sleep 0.4; xdotool mousedown 1; sleep 0.4; xdotool mouseup 1; sleep 1.5
}
# Tasten kommen im Spiel auf zwei Wegen an, und die beiden wollen genau das
# Gegenteil voneinander:
#
#   key()     fuer alles, was ueber SDL_KEYDOWN laeuft - Escape, Alt+Return und
#             was sonst die GUI liest. Ereignisse werden gepuffert, ein Tippen
#             genuegt also. Halten waere hier falsch: engine.cpp:210 setzt
#             SDL_EnableKeyRepeat(140, 60), und ein Escape, das 400 ms liegt,
#             kommt sechsmal an - das erste schliesst den Dialog, das zweite
#             beendet das Spiel.
#
#   holdKey() fuer die benannten Aktionen ($A_CAPTURE_SCREENSHOT und die
#             uebrigen). Engine::updateVKs liest die mit SDL_GetKeyState, einer
#             Momentaufnahme, einmal je Logiktakt von 20 ms. Ein Druck und
#             Loslassen in derselben Millisekunde faellt zwischen zwei Aufnahmen
#             durch - unter llvmpipe, wo ein Bild eine Fuenftelsekunde braucht,
#             jedesmal.
key()     { xdotool key --clearmodifiers "$1"; sleep 1.5; }
holdKey() { xdotool keydown --clearmodifiers "$1"; sleep 0.4; xdotool keyup --clearmodifiers "$1"; sleep 1.5; }

# Ist an dieser Stelle im Fenster ueberhaupt etwas? Mehr sagt ein Foto nicht,
# solange es keine Testhaken gibt.
brightness() {
    shot "$3"
    python3 - "$OUT/$3.png" "$((ORIGIN_X + $1 * SCALE))" "$((ORIGIN_Y + $2 * SCALE))" "$SCALE" <<'PY'
import sys
from PIL import Image
import numpy as np
a = np.array(Image.open(sys.argv[1]).convert('RGB')).astype(float)
x, y, s = int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
print(round(a[y:y + 40 * s, x:x + 120 * s].mean(), 1))
PY
}

echo "Klicke durch die Oberflaeche ..."
xdotool windowactivate "$W" 2>/dev/null; sleep 1

# Beim allerersten Start fragt das Spiel nach dem Roehrenfilter. "Nein, danke"
# liegt bei etwa (190, 322); steht die Frage nicht, trifft der Klick den
# Hintergrund und schadet nichts.
click 190 322
shot 1-menu

# Optionen auf und wieder zu. Das Zahnrad sitzt rechts oben.
before=$(brightness 100 100 before-options)
click 497 84
after=$(brightness 100 100 2-options)
python3 -c "import sys; sys.exit(0 if abs($after - $before) > 4 else 1)" \
    && ok "Optionen: das Bild hat sich geaendert ($before -> $after)" \
    || note "Optionen: das Bild ist gleich geblieben ($before -> $after)"
key Escape
shot 3-back

# Der Manager, derselbe Nachweis.
click 287 110
mgr=$(brightness 100 100 4-manager)
python3 -c "import sys; sys.exit(0 if abs($mgr - $before) > 4 else 1)" \
    && ok "Manager: das Bild hat sich geaendert ($before -> $mgr)" \
    || note "Manager: das Bild ist gleich geblieben ($before -> $mgr)"
key Escape
shot 5-back

# Vollbild und zurueck. Das laeuft ueber den Fenstermanager, nicht ueber SDL -
# siehe LinuxBuild/linux_window.cpp.
if [ -n "$WM_PID" ]; then
    key alt+Return; sleep 3
    eval "$(geom "$W")"
    shot 6-fullscreen
    if [ "$WIDTH" -eq "$SCREEN_W" ] && [ "$HEIGHT" -eq "$SCREEN_H" ] && [ "$X" -eq 0 ] && [ "$Y" -eq 0 ]; then
        ok "Vollbild: $WIDTH x $HEIGHT bei (0, 0)"
    else
        note "Vollbild: $WIDTH x $HEIGHT bei ($X, $Y), erwartet $SCREEN_W x $SCREEN_H bei (0, 0)"
    fi
    key alt+Return; sleep 3
    eval "$(geom "$W")"
    shot 7-windowed
    if [ "$X" -eq "$ORIGIN_X" ] && [ "$Y" -eq "$ORIGIN_Y" ]; then
        ok "Zurueck ins Fenster an dieselbe Stelle"
    else
        note "Zurueck ins Fenster bei ($X, $Y) statt ($ORIGIN_X, $ORIGIN_Y)"
    fi
fi

# F11 schreibt ein Bildschirmfoto ins Benutzerverzeichnis. Das ist der einzige
# Weg von hier, den Bildpuffer selbst zu sehen - alles andere ist das Fenster.
# Die Dateien heissen .bmp, nicht .png - SDL_SaveBMP schreibt sie.
HOME_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/blocks5"
before_count=$(ls "$HOME_DIR/screenshots"/*.bmp 2>/dev/null | wc -l)
holdKey F11
sleep 2
after_count=$(ls "$HOME_DIR/screenshots"/*.bmp 2>/dev/null | wc -l)
if [ "$after_count" -gt "$before_count" ]; then
    ok "F11 hat ein Bildschirmfoto geschrieben ($HOME_DIR/screenshots)"
else
    note "F11 hat kein Bildschirmfoto geschrieben"
fi

# Ueber das Spiel beenden und nicht ueber das Fenster: "xdotool windowclose"
# ruft XDestroyWindow, und SDL faellt danach ueber ein Fenster, das es noch fuer
# seines haelt. Escape im Menue ist der Weg, den auch ein Spieler nimmt, und nur
# darueber laeuft Engine::exit() und schreibt die config.xml.
key Escape
for i in $(seq 1 25); do kill -0 "$GAME_PID" 2>/dev/null || break; sleep 1; done
kill -0 "$GAME_PID" 2>/dev/null && note "Escape im Menue hat das Spiel nicht beendet"

[ -f "$HOME_DIR/config.xml" ] && ok "config.xml angelegt" || note "config.xml fehlt"
grep -q "ERROR" "$OUT/run.log" && note "ERROR im Protokoll: $(grep -m3 ERROR "$OUT/run.log" | tr '\n' ' ')" \
                               || ok "keine Fehlerzeile im Protokoll"

echo
if [ "$problems" -eq 0 ]; then echo "IN ORDNUNG (Bilder in $OUT)"; exit 0
else echo "$problems Beanstandung(en) (Bilder in $OUT)"; exit 1; fi
