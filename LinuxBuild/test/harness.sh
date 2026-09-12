# harness.sh - start the game and drive it by element names.
#
# Pulled into a test with "source"; see smoke.sh.
#
# Clicks go to a name ("Menu.Options") and not to a coordinate. The coordinate
# is supplied by the test hook in Blocks5/src/testhooks.cpp, which sits in the
# build made by LinuxBuild/build.sh hooks: it puts the GUI tree with every
# element's window coordinates down as JSON and answers who would get a click
# on a point. The click itself stays an ordinary mouse click and travels the
# same way through SDL, Engine and GUI as in the game.
#
# Without it every click is guesswork. On a first start Menu.CrtPane covers
# everything, and a click on the middle of Menu.Options lands on the pane
# instead - which a screenshot cannot show.

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
	[ -x "$B5_EXE" ] || { echo "$B5_EXE is missing - run LinuxBuild/build.sh hooks first."; exit 2; }

	# The test drives build-test/, but development usually builds only build/ -
	# a change would then not be in it at all and the run would come out green,
	# because it asks the program from the day before yesterday. The same holds
	# for data.zip: the game reads the archive, not the loose files beside it.
	b5_stale()
	{
		local newer
		newer="$(find "$2" -type f -newer "$1" -print 2>/dev/null | head -3)"
		[ -z "$newer" ] && return 0
		echo "$(basename "$1") is older than:"
		echo "$newer" | sed 's|.*/||; s|^|  |'
		echo "$3"
		return 1
	}
	b5_stale "$B5_EXE" "$B5_GAME/src" \
		"Run 'LinuxBuild/build.sh hooks' first." || exit 2
	b5_stale "$B5_GAME/data.zip" "$B5_GAME/data" \
		"Run 'Blocks5/pack.sh data' first." || exit 2

	for t in Xvfb xdotool ffmpeg python3; do
		command -v $t >/dev/null 2>&1 || { echo "$t is missing."; exit 2; }
	done

	# Before the first destructive step, not beside the Xvfb further down:
	# B5_OUT is the other run's screenshots and its hook directory as much as
	# this one's, and a refusal after it is gone has refused nothing.
	b5_clearDisplay || exit 2

	rm -rf "$B5_OUT"; mkdir -p "$B5_OUT"
	B5_TEST_DIR="$B5_OUT/hook"; mkdir -p "$B5_TEST_DIR"; export B5_TEST_DIR

	# Two windows open themselves over the menu, and both come of their own
	# accord: the CRT offer on a first start and the donation window once enough
	# time has been played - which a machine that has run the tests often enough
	# reaches by itself. Both are one-shot, so no test touching the menu can be
	# repeatable while they may appear. The markers are exactly the ones the
	# game itself writes.
	B5_HOME="${XDG_DATA_HOME:-$HOME/.local/share}/blocks5"
	mkdir -p "$B5_HOME"
	[ -f "$B5_HOME/.crt_offered" ]   || echo -n "1"       > "$B5_HOME/.crt_offered"
	[ -f "$B5_HOME/.donation_asked" ] || echo -n "disable" > "$B5_HOME/.donation_asked"

	# A run that was killed - a timeout, a Ctrl-C - leaves its Xvfb and its
	# lock file standing. The next Xvfb then exits at once because the display
	# is taken, and the game attaches to the old server instead, where it hangs
	# in SDL_SetVideoMode with nothing to say. Clearing it first is what makes
	# a rerun after an aborted run mean anything.
	Xvfb "$B5_DISP" -screen 0 ${B5_SCREEN_W}x${B5_SCREEN_H}x24 >"$B5_OUT/xvfb.log" 2>&1 &
	B5_XVFB_PID=$!

	# Wait for the server itself rather than guessing: everything below talks
	# to it, and a dead one costs a minute of timeouts to find out about.
	local x
	for x in $(seq 1 40); do
		xdpyinfo -display "$B5_DISP" >/dev/null 2>&1 && break
		kill -0 "$B5_XVFB_PID" 2>/dev/null || { echo "FAILED: Xvfb died - $(tail -1 "$B5_OUT/xvfb.log")"; exit 2; }
		sleep 0.5
	done
	xdpyinfo -display "$B5_DISP" >/dev/null 2>&1 || { echo "FAILED: no X server on $B5_DISP"; exit 2; }
	export DISPLAY="$B5_DISP"
	if command -v openbox >/dev/null 2>&1; then
		openbox >"$B5_OUT/wm.log" 2>&1 &
		B5_WM_PID=$!
		sleep 2
	else
		B5_WM_PID=""
		echo "  (no window manager - the fullscreen test is skipped)"
	fi

	# ALSOFT_DRIVERS=null: on a machine with no audio output the game would
	# otherwise abort at startup, and that is not what this is about.
	# B5_ARGS appends further switches - -nofbo and -noshader force the two
	# fallback paths that otherwise do not exist on this machine.
	( cd "$B5_GAME" && ALSOFT_DRIVERS=null "$B5_EXE" -windowed ${B5_ARGS:-} >"$B5_OUT/run.log" 2>&1 ) &
	B5_GAME_PID=$!

	# Wait for the window rather than guessing an interval: under llvmpipe the
	# start takes half a minute, on real hardware a moment. Both waits below
	# give up rather than hang, and both watch the process while they wait: a
	# game that has died or wedged in SDL_SetVideoMode is the common case, and
	# waiting the whole deadline out for it wastes an afternoon.
	echo "Waiting for the window ..."
	B5_WIN=""
	local i
	for i in $(seq 1 60); do
		b5_alive || { echo "FAILED: the game exited before its window appeared"; b5_diagnose; exit 2; }
		B5_WIN=$(xdotool search --name "Blocks 5" 2>/dev/null | head -1)
		[ -n "$B5_WIN" ] && break
		sleep 1
	done
	[ -n "$B5_WIN" ] || { echo "FAILED: no window after 60 s"; b5_diagnose; exit 2; }

	# And then for the hook to answer and have something to report: the window
	# is up long before the GUI inside it is.
	for i in $(seq 1 60); do
		b5_alive || { echo "FAILED: the game exited during startup"; b5_diagnose; exit 2; }
		if b5_dump 2>/dev/null && [ "$(b5_json "d['state']")" != "" ]; then break; fi
		sleep 1
	done
	[ "$(b5_json "d['state']")" != "" ] || b5_hookFailed

	xdotool windowactivate "$B5_WIN" 2>/dev/null
	sleep 1
	b5_geometry
	echo "Window $B5_W x $B5_H at ($B5_X, $B5_Y)"
}

# Is the game still running? Everything that waits asks this, so that a run
# ends in seconds when the game is gone instead of at the far end of a
# deadline.
b5_alive() { [ -n "${B5_GAME_PID:-}" ] && kill -0 "$B5_GAME_PID" 2>/dev/null; }

# Why the hook went quiet: the game is gone, or it is standing there and not
# answering. The two want different things looked at, so they are not one
# message.
b5_hookFailed()
{
	b5_alive && echo "FAILED: the test hook does not answer" \
			 || echo "FAILED: the game is no longer running"
	b5_diagnose
	exit 2
}

# What to print when a wait gives up. The log's last lines say where it got to
# - "Initializing SDL ..." with nothing after it is the wedged-X server case.
b5_diagnose()
{
	echo "--- the last lines of $B5_OUT/run.log:"
	tail -12 "$B5_OUT/run.log" 2>/dev/null | sed 's/^/    /'
	b5_alive && echo "--- the game is still running (pid $B5_GAME_PID); killing it."
	b5_stop
}

# Take down an X server left behind by a run that did not get to b5_stop. Its
# own pid, never a pattern that could match this script.
#
# A server with the game still attached to it belongs to a run that is not over,
# and killing that one takes the other run's game down mid-frame - which reads
# as a rendering fault rather than as a collision. Two harnesses on one display
# is the shape that produces it, so this stops instead and says which two
# variables separate them - the display and the shots directory, since the hook
# file both games poll lives in the second.
b5_clearDisplay()
{
	local stale p
	stale=$(pgrep -x Xvfb 2>/dev/null | while read -r p; do
		tr '\0' ' ' 2>/dev/null < "/proc/$p/cmdline" | grep -q -- "$B5_DISP " && echo "$p"
	done)
	if [ -n "$stale" ]; then
		# The game's own environment, not a global pgrep: a run on another
		# display is none of this one's business.
		for p in $(pgrep -x blocks5 2>/dev/null); do
			# The redirections are applied left to right, so the 2>/dev/null has
			# to come first or the shell's own "no such file" for a process that
			# has just exited reaches the terminal.
			if tr '\0' '\n' 2>/dev/null < "/proc/$p/environ" |
			   grep -qx -- "DISPLAY=$B5_DISP"; then
				echo "  ! a blocks5 is still attached to $B5_DISP, so another run is"
				echo "    using it. Give this one its own B5_DISPLAY *and* its own"
				echo "    B5_SHOTS: the display separates the two X servers, and the"
				echo "    shots directory the hook file both games poll."
				return 1
			fi
		done
		echo "  (clearing an X server left behind on $B5_DISP)"
		kill $stale 2>/dev/null
		sleep 1
	fi
	rm -f "/tmp/.X${B5_DISP#:}-lock"
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

# Where the client area really begins - and that is not what b5_geometry
# reports. Under a reparenting window manager the game's window sits inside a
# frame and xdotool reports that frame's corner; the title bar therefore shifts
# every click by its own height. On a finger-sized menu button that never
# shows; on an 18-pixel-high button every click misses. xwininfo names the
# absolute corner of the content itself. Where it is missing, the frame corner
# has to do.
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

# Put a request to the hook and print the answer.
# The hook answers once per logic tick, so a second is already generous and
# five is the outside of any frame rate this runs at. Giving up matters more
# than the number: b5_waitForState asks sixty times over, and at twenty
# seconds a piece an unanswering game costs twenty minutes to notice.
b5_ask()
{
	rm -f "$B5_TEST_DIR/response"
	echo "$1" > "$B5_TEST_DIR/request"
	local i
	for i in $(seq 1 25); do
		# The request has to be gone before the answer counts. The game
		# deletes it and writes the response in the same pass, so a response
		# appearing while the request is still lying there belongs to an
		# earlier ask that gave up - and reading it answers the wrong
		# question. What that looks like is not a timeout but a wrong answer:
		# a click reporting that a whole dump is "on top" of the button,
		# because that is what the previous ask was going to return.
		if [ ! -f "$B5_TEST_DIR/request" ] && [ -f "$B5_TEST_DIR/response" ]; then
			cat "$B5_TEST_DIR/response"
			return 0
		fi
		b5_alive || return 1
		sleep 0.2
	done
	return 1
}

# Fetch the tree and put it down; b5_json queries it afterwards.
b5_dump() { b5_ask dump > "$B5_OUT/dump.json"; [ -s "$B5_OUT/dump.json" ]; }

# Evaluate an expression over the tree last fetched. "d" is the tree,
# "el(name)" one element out of it.
b5_json()
{
	python3 - "$B5_OUT/dump.json" "$1" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
byName = {e['path']: e for e in d['elements']}
def el(name):
    if name not in byName: raise SystemExit('no element "%s"' % name)
    return byName[name]
try:
    value = eval(sys.argv[2])
except SystemExit as e:
    print('', end=''); sys.exit(0)
print(value if value is not None else '')
PY
}

b5_shot() { ffmpeg -loglevel error -f x11grab -video_size ${B5_SCREEN_W}x${B5_SCREEN_H} -i "$B5_DISP" -frames:v 1 "$B5_OUT/$1.png" -y; }

# Keys reach the game by two routes, and the two want the exact opposite of
# each other:
#
#   b5_key   for everything that goes through SDL_KEYDOWN - Escape, Alt+Return
#            and whatever else the GUI reads. Events are buffered; a tap is
#            enough.
#
#   b5_hold  for the named actions ($A_CAPTURE_SCREENSHOT and the rest).
#            Engine::updateVKs reads those with SDL_GetKeyState, a snapshot
#            taken once per 20 ms logic tick. A press and release in the same
#            millisecond falls between two snapshots - under llvmpipe, where a
#            frame takes a fifth of a second, every time.
#
# Both hold the key briefly, and b5_key for a second reason: SDL_PollEvent runs
# only once per frame, hence every 200 ms. If a run falls between the press and
# the release of an "xdotool key" that lets go again at once, the game sees only
# the press - and at the next run SDL_EnableKeyRepeat(140, 60) posts a repeat
# that arrives as a second key press. Measured, every fifth press arrived twice.
# 60 ms is long enough for one run to see both events and short enough for the
# repeat not to bite.
b5_key()  { xdotool keydown --clearmodifiers "$1"; sleep 0.06; xdotool keyup --clearmodifiers "$1"; sleep 1.5; }
b5_hold() { xdotool keydown --clearmodifiers "$1"; sleep 0.4; xdotool keyup --clearmodifiers "$1"; sleep 1.5; }

# Click on an element. Bails out if there is no such element, if it is
# invisible or disabled, or if something lies over it - then the fault is not a
# slipped coordinate but the game itself.
b5_click()
{
	local path=$1
	b5_dump || b5_hookFailed

	local shown active
	shown=$(b5_json "el('$path')['shown']")
	[ -n "$shown" ] || { echo "FAILED: no element \"$path\""; exit 1; }
	[ "$shown" = "True" ] || { echo "FAILED: $path is not visible"; exit 1; }
	active=$(b5_json "el('$path')['active']")
	[ "$active" = "True" ] || { echo "FAILED: $path is disabled"; exit 1; }

	# Would the click really land here? getElementAt() goes the same way as
	# GUI::update().
	local game hit
	game=$(b5_json "'%d %d' % (el('$path')['rect'][0] + el('$path')['rect'][2]//2, el('$path')['rect'][1] + el('$path')['rect'][3]//2)")
	hit=$(b5_ask "hit $game")
	if [ "$hit" != "$path" ]; then
		echo "FAILED: a click on the middle of $path would go to \"${hit:-nothing}\" - something is on top"
		exit 1
	fi

	# And now the window coordinate, the same arithmetic as in presentFrame().
	local wx wy
	b5_clientOrigin
	wx=$(b5_json "el('$path')['win'][0] + el('$path')['win'][2]//2")
	wy=$(b5_json "el('$path')['win'][1] + el('$path')['win'][3]//2")

	# Move, settle, press, hold, release: the game reads the mouse once per
	# logic tick, and a click lasting one millisecond falls through.
	xdotool mousemove $((B5_CX + wx)) $((B5_CY + wy))
	sleep 0.4; xdotool mousedown 1; sleep 0.4; xdotool mouseup 1; sleep 1.5
}

# Is this element visible (or explicitly not)?
b5_expectShown()
{
	local path=$1 want=${2:-true} shown
	b5_dump
	shown=$(b5_json "el('$path')['shown']")
	[ "$shown" = "True" ] && shown=true || shown=false
	[ "$shown" = "$want" ] && b5_ok "$path is $( [ "$want" = true ] && echo visible || echo gone)" \
	                       || b5_note "$path: visible=$shown, expected $want"
}

# Wait for a game state rather than guessing an interval. The hook answers as
# early as GS_Loading, long before the menu is up, and under llvmpipe loading
# takes half a minute.
b5_waitForState()
{
	local want=$1 seconds=${2:-90} i
	for i in $(seq 1 "$seconds"); do
		b5_alive || { echo "FAILED: the game exited while waiting for $want"; b5_diagnose; exit 2; }
		b5_dump || { sleep 1; continue; }
		[ "$(b5_json "d['state']")" = "$want" ] && { b5_ok "game state $want"; return 0; }
		sleep 1
	done
	echo "FAILED: $want not reached (last: $(b5_json "d['state']"))"
	b5_diagnose
	exit 1
}

b5_expectState()
{
	local want=$1 have
	b5_dump
	have=$(b5_json "d['state']")
	[ "$have" = "$want" ] && b5_ok "game state $have" || b5_note "game state is $have, expected $want"
}

b5_finish()
{
	echo
	if [ "$b5_problems" -eq 0 ]; then echo "OK (screenshots in $B5_OUT)"; return 0
	else echo "$b5_problems problem(s) (screenshots in $B5_OUT)"; return 1; fi
}
