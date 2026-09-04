# harness.sh - das Spiel starten und ueber Elementnamen bedienen.
#
# Wird von einem Test mit "source" eingebunden; siehe smoke.sh.
#
# Geklickt wird auf einen Namen ("Menu.Options") und nicht auf eine Koordinate.
# Die liefert der Testhaken aus Blocks5/src/testhooks.cpp, der im Build mit
# LinuxBuild/build.sh hooks steckt: er legt den GUI-Baum mit den
# Fensterkoordinaten jedes Elements als JSON hin und beantwortet die Frage, wer
# einen Klick auf einen Punkt bekaeme. Der Klick selbst bleibt ein gewoehnlicher
# Mausklick und geht denselben Weg durch SDL, Engine und GUI wie im Spiel.
#
# Ohne das war jeder Klick geraten. Beim ersten Start liegt zum Beispiel
# Menu.CrtPane ueber dem ganzen Bild, und ein Klick auf die Mitte von
# Menu.Options landet dort - zu sehen ist das einem Bildschirmfoto nicht.

B5_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
B5_GAME="$B5_HERE/../../Blocks5"
B5_EXE="$B5_HERE/../build-test/blocks5"
B5_OUT="${B5_SHOTS:-/tmp/blocks5-smoke}"
B5_DISP="${B5_DISPLAY:-:99}"
B5_SCREEN_W=1600
B5_SCREEN_H=1200

b5_problems=0
b5_note() { echo "  ! $*"; b5_problems=$((b5_problems + 1)); }
b5_ok()   { echo "  . $*"; }

b5_start()
{
	[ -x "$B5_EXE" ] || { echo "$B5_EXE fehlt - erst LinuxBuild/build.sh hooks laufen lassen."; exit 2; }
	for t in Xvfb xdotool ffmpeg python3; do
		command -v $t >/dev/null 2>&1 || { echo "$t fehlt."; exit 2; }
	done

	rm -rf "$B5_OUT"; mkdir -p "$B5_OUT"
	B5_TEST_DIR="$B5_OUT/hook"; mkdir -p "$B5_TEST_DIR"; export B5_TEST_DIR

	Xvfb "$B5_DISP" -screen 0 ${B5_SCREEN_W}x${B5_SCREEN_H}x24 >"$B5_OUT/xvfb.log" 2>&1 &
	B5_XVFB_PID=$!
	sleep 2
	export DISPLAY="$B5_DISP"
	if command -v openbox >/dev/null 2>&1; then
		openbox >"$B5_OUT/wm.log" 2>&1 &
		B5_WM_PID=$!
		sleep 2
	else
		B5_WM_PID=""
		echo "  (kein Fenstermanager - der Vollbildtest faellt aus)"
	fi

	# ALSOFT_DRIVERS=null: auf einer Maschine ohne Tonausgabe braeche das Spiel
	# sonst schon beim Start ab, und darum geht es hier nicht.
	( cd "$B5_GAME" && ALSOFT_DRIVERS=null "$B5_EXE" -windowed >"$B5_OUT/run.log" 2>&1 ) &
	B5_GAME_PID=$!

	# Auf das Fenster warten statt eine Zeit zu raten: unter llvmpipe braucht
	# der Start eine halbe Minute, auf richtiger Hardware einen Augenblick.
	echo "Warte auf das Fenster ..."
	B5_WIN=""
	local i
	for i in $(seq 1 60); do
		B5_WIN=$(xdotool search --name "Blocks 5" 2>/dev/null | head -1)
		[ -n "$B5_WIN" ] && break
		sleep 1
	done
	[ -n "$B5_WIN" ] || { echo "FEHLGESCHLAGEN: kein Fenster nach 60 s"; tail -20 "$B5_OUT/run.log"; exit 1; }

	# Und danach darauf, dass der Haken antwortet und etwas zu berichten hat:
	# das Fenster steht lange, bevor die Oberflaeche darin steht.
	for i in $(seq 1 90); do
		if b5_dump 2>/dev/null && [ "$(b5_json "d['state']")" != "" ]; then break; fi
		sleep 1
	done
	[ "$(b5_json "d['state']")" != "" ] || { echo "FEHLGESCHLAGEN: der Testhaken antwortet nicht"; exit 1; }

	xdotool windowactivate "$B5_WIN" 2>/dev/null
	sleep 1
	b5_geometry
	echo "Fenster $B5_W x $B5_H bei ($B5_X, $B5_Y)"
}

b5_stop()
{
	kill "${B5_GAME_PID:-}" 2>/dev/null
	[ -n "${B5_WM_PID:-}" ] && kill "$B5_WM_PID" 2>/dev/null
	kill "${B5_XVFB_PID:-}" 2>/dev/null
	wait 2>/dev/null
}

b5_geometry()
{
	eval "$(xdotool getwindowgeometry --shell "$B5_WIN" 2>/dev/null)"
	B5_X=$X; B5_Y=$Y; B5_W=$WIDTH; B5_H=$HEIGHT
}

# Wo die Zeichenflaeche wirklich anfaengt - und das ist nicht, was
# b5_geometry liefert. Unter einem umhaengenden Fenstermanager steckt das
# Fenster des Spiels in einem Rahmen, und xdotool meldet dessen Ecke; der
# Titelbalken verschiebt damit jeden Klick um seine Hoehe. Bei einem
# fingergrossen Menueknopf faellt das nie auf, bei einem 18 Pixel hohen Knopf
# geht jeder Klick daneben. xwininfo nennt die absolute Ecke des Inhalts
# selbst. Fehlt es, bleibt es beim Rahmen - dann ist es wie vorher.
b5_clientOrigin()
{
	local info
	info=$(xwininfo -id "$B5_WIN" 2>/dev/null)
	B5_CX=$(printf '%s\n' "$info" | sed -n 's/.*Absolute upper-left X: *\(-\?[0-9]*\).*/\1/p')
	B5_CY=$(printf '%s\n' "$info" | sed -n 's/.*Absolute upper-left Y: *\(-\?[0-9]*\).*/\1/p')
	if [ -z "$B5_CX" ] || [ -z "$B5_CY" ]; then
		b5_geometry
		B5_CX=$B5_X; B5_CY=$B5_Y
	fi
}

# Eine Anfrage an den Haken stellen und die Antwort ausgeben.
b5_ask()
{
	rm -f "$B5_TEST_DIR/response"
	echo "$1" > "$B5_TEST_DIR/request"
	local i
	for i in $(seq 1 100); do
		[ -f "$B5_TEST_DIR/response" ] && { cat "$B5_TEST_DIR/response"; return 0; }
		sleep 0.2
	done
	return 1
}

# Den Baum holen und ablegen; b5_json fragt ihn danach ab.
b5_dump() { b5_ask dump > "$B5_OUT/dump.json"; [ -s "$B5_OUT/dump.json" ]; }

# Einen Ausdruck ueber dem zuletzt geholten Baum auswerten. "d" ist der Baum,
# "el(name)" ein Element daraus.
b5_json()
{
	python3 - "$B5_OUT/dump.json" "$1" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
byName = {e['path']: e for e in d['elements']}
def el(name):
    if name not in byName: raise SystemExit('kein Element "%s"' % name)
    return byName[name]
try:
    value = eval(sys.argv[2])
except SystemExit as e:
    print('', end=''); sys.exit(0)
print(value if value is not None else '')
PY
}

b5_shot() { ffmpeg -loglevel error -f x11grab -video_size ${B5_SCREEN_W}x${B5_SCREEN_H} -i "$B5_DISP" -frames:v 1 "$B5_OUT/$1.png" -y; }

# Tasten kommen im Spiel auf zwei Wegen an, und die beiden wollen genau das
# Gegenteil voneinander:
#
#   b5_key   fuer alles, was ueber SDL_KEYDOWN laeuft - Escape, Alt+Return und
#            was sonst die GUI liest. Ereignisse werden gepuffert, ein Tippen
#            genuegt also.
#
#   b5_hold  fuer die benannten Aktionen ($A_CAPTURE_SCREENSHOT und die
#            uebrigen). Engine::updateVKs liest die mit SDL_GetKeyState, einer
#            Momentaufnahme, einmal je Logiktakt von 20 ms. Ein Druck und
#            Loslassen in derselben Millisekunde faellt zwischen zwei Aufnahmen
#            durch - unter llvmpipe, wo ein Bild eine Fuenftelsekunde braucht,
#            jedesmal.
b5_key()  { xdotool key --clearmodifiers "$1"; sleep 1.5; }
b5_hold() { xdotool keydown --clearmodifiers "$1"; sleep 0.4; xdotool keyup --clearmodifiers "$1"; sleep 1.5; }

# Auf ein Element klicken. Bricht ab, wenn es das nicht gibt, wenn es
# unsichtbar oder abgeschaltet ist, oder wenn etwas darueber liegt - dann liegt
# der Fehler nicht an einer verrutschten Koordinate, sondern im Spiel.
b5_click()
{
	local path=$1
	b5_dump || { echo "FEHLGESCHLAGEN: der Testhaken antwortet nicht"; exit 1; }

	local shown active
	shown=$(b5_json "el('$path')['shown']")
	[ -n "$shown" ] || { echo "FEHLGESCHLAGEN: kein Element \"$path\""; exit 1; }
	[ "$shown" = "True" ] || { echo "FEHLGESCHLAGEN: $path ist nicht sichtbar"; exit 1; }
	active=$(b5_json "el('$path')['active']")
	[ "$active" = "True" ] || { echo "FEHLGESCHLAGEN: $path ist abgeschaltet"; exit 1; }

	# Wuerde der Klick wirklich hier ankommen? getElementAt() geht denselben
	# Weg wie GUI::update().
	local game hit
	game=$(b5_json "'%d %d' % (el('$path')['rect'][0] + el('$path')['rect'][2]//2, el('$path')['rect'][1] + el('$path')['rect'][3]//2)")
	hit=$(b5_ask "hit $game")
	if [ "$hit" != "$path" ]; then
		echo "FEHLGESCHLAGEN: ein Klick auf die Mitte von $path ginge an \"${hit:-nichts}\" - es liegt etwas darueber"
		exit 1
	fi

	# Und nun die Fensterkoordinate, dieselbe Rechnung wie in presentFrame().
	local wx wy
	b5_clientOrigin
	wx=$(b5_json "el('$path')['win'][0] + el('$path')['win'][2]//2")
	wy=$(b5_json "el('$path')['win'][1] + el('$path')['win'][3]//2")

	# Bewegen, ruhen lassen, druecken, halten, loslassen: das Spiel liest die
	# Maus einmal je Logiktakt, ein Klick in einer Millisekunde faellt durch.
	xdotool mousemove $((B5_CX + wx)) $((B5_CY + wy))
	sleep 0.4; xdotool mousedown 1; sleep 0.4; xdotool mouseup 1; sleep 1.5
}

# Ist dieses Element sichtbar (oder ausdruecklich nicht)?
b5_expectShown()
{
	local path=$1 want=${2:-true} shown
	b5_dump
	shown=$(b5_json "el('$path')['shown']")
	[ "$shown" = "True" ] && shown=true || shown=false
	[ "$shown" = "$want" ] && b5_ok "$path ist $( [ "$want" = true ] && echo sichtbar || echo verschwunden)" \
	                       || b5_note "$path: sichtbar=$shown, erwartet $want"
}

# Auf einen Spielzustand warten, statt eine Zeit zu raten. Der Haken antwortet
# schon in GS_Loading, also lange bevor das Menue steht, und unter llvmpipe
# dauert das Laden eine halbe Minute.
b5_waitForState()
{
	local want=$1 seconds=${2:-90} i
	for i in $(seq 1 "$seconds"); do
		b5_dump || { sleep 1; continue; }
		[ "$(b5_json "d['state']")" = "$want" ] && { b5_ok "Spielzustand $want"; return 0; }
		sleep 1
	done
	echo "FEHLGESCHLAGEN: $want nicht erreicht (zuletzt: $(b5_json "d['state']"))"
	exit 1
}

b5_expectState()
{
	local want=$1 have
	b5_dump
	have=$(b5_json "d['state']")
	[ "$have" = "$want" ] && b5_ok "Spielzustand $have" || b5_note "Spielzustand $have, erwartet $want"
}

b5_finish()
{
	echo
	if [ "$b5_problems" -eq 0 ]; then echo "IN ORDNUNG (Bilder in $B5_OUT)"; return 0
	else echo "$b5_problems Beanstandung(en) (Bilder in $B5_OUT)"; return 1; fi
}
