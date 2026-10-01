#!/bin/bash
# update.sh - the update check: the menu's version button in every state and
# both languages, the plain label in its place where nothing can ask, the
# switch in config.xml, and how a .update_checker left by the installer or by
# a version before 1.2.0 is taken in.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/update.sh
#
# The website is never asked. A hooks build takes the address from
# B5_UPDATE_URL (updatecheck.cpp), and a server of this script's own answers
# there with whatever a step needs: the same version, a newer one, garbage, an
# error, or nothing for a while. PATH is the other lever: directories of links
# to everything but curl, wget or both, and stand-ins for a curl that hangs and
# for an xdg-open that only writes down what it was asked to open.
#
# Eleven starts of the game, so a few minutes under llvmpipe.
set -u
B5_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# A private home like drag.sh's: config.xml and .update_checker are what is
# tested, and the developer's own must be neither read nor written.
export XDG_DATA_HOME="${B5_UPDATE_XDG:-/tmp/blocks5-update-xdg}"
B5_PRIVATE_HOME="$XDG_DATA_HOME/blocks5"
export B5_DISPLAY="${B5_DISPLAY:-:87}"
SHOTS="${B5_SHOTS:-/tmp/blocks5-update}"
WORK="$SHOTS/work"

source "$B5_HERE/harness.sh"

GAME_SIGNAL="$B5_GAME/.update_checker"
rm -rf "$SHOTS"
mkdir -p "$WORK"

# --- the server --------------------------------------------------------------
# "answer" is the body, "mode" what to do first: nothing, "404", or
# "sleep <seconds>". Every request is logged with its user agent.
cat > "$WORK/server.py" <<'PY'
import http.server, os, sys, time
work = sys.argv[1]
class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        with open(os.path.join(work, 'requests'), 'a') as f:
            f.write('%s %s\n' % (self.path, self.headers.get('User-Agent', '')))
        mode = open(os.path.join(work, 'mode')).read().split()
        if mode[:1] == ['sleep']:
            time.sleep(float(mode[1]))
            mode = mode[2:]
        if mode[:1] == ['404']:
            self.send_error(404)
            return
        body = open(os.path.join(work, 'answer'), 'rb').read()
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def log_message(self, *args):
        pass
server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
open(os.path.join(work, 'port'), 'w').write(str(server.server_address[1]))
server.serve_forever()
PY
: > "$WORK/requests"
printf '1.2.0\n' > "$WORK/answer"
: > "$WORK/mode"
# Started from a subshell, so that it is no job of this one: b5_stop ends with
# a bare wait, which would otherwise wait for the server for ever.
( python3 "$WORK/server.py" "$WORK" & echo $! > "$WORK/server.pid" )
SERVER_PID=$(cat "$WORK/server.pid")
for i in $(seq 1 50); do [ -s "$WORK/port" ] && break; sleep 0.1; done
[ -s "$WORK/port" ] || { echo "FAILED: the test server did not start"; exit 2; }
export B5_UPDATE_URL="http://127.0.0.1:$(cat "$WORK/port")/version.txt"

cleanup()
{
	b5_stop
	kill "$SERVER_PID" 2>/dev/null
	# The game folder is the working tree; nothing of a test may stay there.
	rm -f "$GAME_SIGNAL"
	[ -f "$WORK/hang.pids" ] && kill $(cat "$WORK/hang.pids") 2>/dev/null
}
trap cleanup EXIT

serve()   # $1 the body, escapes such as \n allowed; [$2 the mode]
{
	printf '%b' "$1" > "$WORK/answer"
	printf '%s' "${2:-}" > "$WORK/mode"
}
requests() { wc -l < "$WORK/requests"; }

# --- PATH --------------------------------------------------------------------
# A directory of links to every program on PATH but the ones named, so that a
# machine without them can be had without uninstalling anything.
pathWithout()   # $1 directory, $2... the programs left out
{
	local dir=$1 d f name
	shift
	# Joined before IFS changes, which would join them with colons instead.
	local skip=" $* "
	mkdir -p "$dir"
	local IFS=:
	for d in $PATH; do
		[ -d "$d" ] || continue
		for f in "$d"/*; do
			name=${f##*/}
			case "$skip" in *" $name "*) continue;; esac
			[ -x "$f" ] && [ ! -e "$dir/$name" ] && ln -s "$f" "$dir/$name"
		done
	done
}
ORIGINAL_PATH=$PATH
pathWithout "$WORK/notools" curl wget
pathWithout "$WORK/nocurl" curl

# xdg-open, which openURL() runs, writing down its argument instead.
mkdir -p "$WORK/stubs"
cat > "$WORK/stubs/xdg-open" <<EOF
#!/bin/sh
printf '%s\n' "\$1" >> "$WORK/opened"
EOF
# A curl that never answers: exec, so that the pid written down is the one
# the game has to kill.
mkdir -p "$WORK/hang"
cat > "$WORK/hang/curl" <<EOF
#!/bin/sh
echo \$\$ >> "$WORK/hang.pids"
exec sleep 600
EOF
chmod +x "$WORK/stubs/xdg-open" "$WORK/hang/curl"

# --- a start of the game -----------------------------------------------------
# A fresh home every time, holding what the step says it holds.
freshHome()   # $1 the version .initialized names
{
	rm -rf "$XDG_DATA_HOME"
	mkdir -p "$B5_PRIVATE_HOME/levels"
	printf '%s' "$1" > "$B5_PRIVATE_HOME/.initialized"
}
writeConfig()   # $1 the elements inside <Config>
{
	printf '<?xml version="1.0" ?>\n<Config>%s</Config>\n' "$1" > "$B5_PRIVATE_HOME/config.xml"
}
start()   # $1 the name of the run, for its screenshots
{
	echo
	echo "=== $1"
	B5_OUT="$SHOTS/$1"
	b5_start
	b5_waitForState GS_Menu
	# The menu comes in through a transition, and a picture of the button
	# taken during it is a picture of the transition.
	local i
	for i in $(seq 1 40); do
		b5_dump && [ "$(b5_json "d['crossfade']")" = "-1" ] && break
		sleep 0.5
	done
}
quit()
{
	# Escape in the menu quits, as a player does, so that Engine::exit() and
	# the abort of a running check both happen.
	b5_key Escape
	local i
	for i in $(seq 1 30); do b5_alive || break; sleep 0.5; done
	b5_alive && { b5_note "the game did not quit on Escape"; b5_stop; }
	wait "$B5_GAME_PID" 2>/dev/null
	b5_stop
}
config()   # what config.xml says about the check, or "none"
{
	sed -n 's:.*<CheckForUpdates>\(.*\)</CheckForUpdates>.*:\1:p' "$B5_PRIVATE_HOME/config.xml" 2>/dev/null | head -1 |
		grep . || echo none
}

# --- assertions --------------------------------------------------------------
# UpdateCheck::State, as the dump reports it.
IDLE=0; CHECKING=1; UP_TO_DATE=2; AVAILABLE=3; FAILED=4

waitUpdate()   # $1 state, $2 its name, [$3 seconds]
{
	local i
	for i in $(seq 1 $((2 * ${3:-20}))); do
		b5_dump || b5_hookFailed
		[ "$(b5_json "d['updateCheck']")" = "$1" ] && return 0
		sleep 0.5
	done
	b5_note "the check did not come to $2 (state $(b5_json "d['updateCheck']"))"
	return 1
}

# The button as the dump has it: the caption below the version, whether it
# can be clicked and whether it flashes. And that the caption fits the frame
# with a pixel to spare, as menu.xml works it out: a frame a pixel thick on the
# left and at the top and two on the right and at the bottom, a line drawn from
# (w - W) / 2 to a pixel past its end, and the block a pixel high and reaching a
# row below its last line with a descender. The caption is a Python literal,
# so that a German letter can be written as an escape.
expectButton()   # $1 the lines below the version, $2 active, $3 flashing, $4 what this is
{
	local got
	got=$(b5_json "el('Menu.VersionButton')['title'] == 'v1.2.0\u00b6' + $1")
	[ "$got" = "True" ] && b5_ok "$4: the caption says $1" \
		|| b5_note "$4: the caption is $(b5_json "repr(el('Menu.VersionButton')['title'])"), not $1"
	[ "$(b5_json "len(el('Menu.VersionButton')['title'].split('\u00b6')) == 3")" = True ] \
		&& b5_ok "$4: three lines, the version on the first" || b5_note "$4: not three lines"
	[ "$(b5_json "el('Menu.VersionButton')['active']")" = "$2" ] \
		&& b5_ok "$4: active is $2" || b5_note "$4: active is not $2"
	[ "$(b5_json "el('Menu.VersionButton')['flashing']")" = "$3" ] \
		&& b5_ok "$4: flashing is $3" || b5_note "$4: flashing is not $3"
	got=$(b5_json "(lambda w, h, W, H: (w - W) // 2 >= 2 and (w - W) // 2 + W <= w - 4 and (h - H) // 2 - 1 >= 2 and (h - H) // 2 + H <= h - 3)(el('Menu.VersionButton')['rect'][2], el('Menu.VersionButton')['rect'][3], el('Menu.VersionButton')['titleSize'][0], el('Menu.VersionButton')['titleSize'][1])")
	[ "$got" = "True" ] && b5_ok "$4: the caption fits ($(b5_json "el('Menu.VersionButton')['titleSize']") in $(b5_json "el('Menu.VersionButton')['rect'][2:]"))" \
		|| b5_note "$4: the caption, $(b5_json "el('Menu.VersionButton')['titleSize']"), does not fit $(b5_json "el('Menu.VersionButton')['rect'][2:]")"
}

# The button's pixels in a screenshot, as raw RGB beside it.
buttonShot()   # $1 shot name
{
	b5_shot "$1"
	local crop
	crop=$(b5_json "'%d:%d:%d:%d' % (el('Menu.VersionButton')['win'][2], el('Menu.VersionButton')['win'][3], el('Menu.VersionButton')['win'][0] + $B5_CX, el('Menu.VersionButton')['win'][1] + $B5_CY)")
	ffmpeg -loglevel error -y -i "$B5_OUT/$1.png" -vf "crop=$crop" -f rawvideo -pix_fmt rgb24 "$B5_OUT/$1.rgb"
}

# Over a set of those, the most colour values one pair of them differs in by
# more than a shade. A shade, because the frame's lower edge is soft and the
# clouds moving behind it change two or three levels there; a flash changes
# a hundred, across the whole frame.
mostChanged()   # $@ shot names
{
	python3 - "$B5_OUT" "$@" <<'PY'
import sys, itertools
shots = [open('%s/%s.rgb' % (sys.argv[1], n), 'rb').read() for n in sys.argv[2:]]
print(max(sum(1 for x, y in zip(a, b) if abs(x - y) > 16) for a, b in itertools.combinations(shots, 2)))
PY
}

# A click on the version button by coordinate rather than by name, for the
# two cases b5_click refuses: the button while it is disabled, and where it is
# hidden.
clickVersionAnyway()
{
	b5_clickAt "$(b5_json "el('Menu.VersionButton')['rect'][0] + el('Menu.VersionButton')['rect'][2] // 2")" \
	           "$(b5_json "el('Menu.VersionButton')['rect'][1] + el('Menu.VersionButton')['rect'][3] // 2")"
}

# Every state there is to see, in the language the home is set to: before any
# question, asking (the server waits four seconds), the same version, garbage,
# an answer too long to be a number, an error, and a newer version.
allStates()   # $1 the language's lines as Python literals: check, checking, up to date, error, available
{
	local check=$1 checking=$2 upToDate=$3 failed=$4 available=$5 before

	b5_dump
	[ "$(b5_json "d['updateCheck']")" = "$IDLE" ] && b5_ok "nothing asked at the start" \
		|| b5_note "the check ran at the start although it is off"
	[ "$(requests)" -eq 0 ] && b5_ok "the server has not been asked" \
		|| b5_note "the server was asked $(requests) times before any click"
	expectButton "$check" True False "before any question"

	serve '1.2.0\n' 'sleep 4'
	b5_click Menu.VersionButton
	b5_dump
	expectButton "$checking" False False "while asking"
	before=$(requests)
	clickVersionAnyway
	waitUpdate "$UP_TO_DATE" "up to date"
	[ "$(requests)" -eq "$before" ] && b5_ok "a click while asking asks nothing" \
		|| b5_note "a click on the disabled button asked again"
	expectButton "$upToDate" True False "the same version"

	serve '<html>no</html>'
	b5_click Menu.VersionButton
	waitUpdate "$FAILED" "failed"
	expectButton "$failed" True False "garbage"

	serve '1.3.0.1.2.3.4.5.6.7.8.9\n'
	b5_click Menu.VersionButton
	waitUpdate "$FAILED" "failed"
	expectButton "$failed" True False "an answer too long"

	serve '' 404
	b5_click Menu.VersionButton
	waitUpdate "$FAILED" "failed"
	expectButton "$failed" True False "an HTTP error"

	serve '1.3.0\r\n'
	b5_click Menu.VersionButton
	waitUpdate "$AVAILABLE" "available"
	expectButton "$available" True True "a newer version"
}

# --- 1. English, every state -------------------------------------------------
export PATH="$WORK/stubs:$ORIGINAL_PATH"
freshHome 1.2.0
start english
grep -q "Not checking for updates" "$B5_OUT/run.log" && b5_ok "the log says the check is off" \
	|| b5_note "the log does not say the check is off"

# The button where the check can ask, and not the label that stands in for it
# where nothing can. It stays clear of the logo, which begins at x 86 in
# menu.png.
b5_dump
[ "$(b5_json "el('Menu.VersionButton')['shown'] and not el('Menu.Version')['shown']")" = True ] \
	&& b5_ok "the button is shown and the label is not" \
	|| b5_note "the button is shown: $(b5_json "el('Menu.VersionButton')['shown']"), the label: $(b5_json "el('Menu.Version')['shown']")"
[ "$(b5_json "el('Menu.VersionButton')['rect'][0] + el('Menu.VersionButton')['rect'][2] <= 83")" = True ] \
	&& b5_ok "the button ends at $(b5_json "el('Menu.VersionButton')['rect'][0] + el('Menu.VersionButton')['rect'][2] - 1"), clear of the logo" \
	|| b5_note "the button reaches $(b5_json "el('Menu.VersionButton')['rect'][0] + el('Menu.VersionButton')['rect'][2] - 1"), into the logo at 86"

# The flashing as drawn, measured below against this: the same button, not
# flashing, the same in every shot - the frame hides the clouds moving behind.
b5_clientOrigin
b5_dump
for i in 1 2 3; do buttonShot "still$i"; sleep 0.2; done
changed=$(mostChanged still1 still2 still3)
[ "$changed" -eq 0 ] && b5_ok "the button not flashing stands still" \
	|| b5_note "the button not flashing changes $changed colour values from shot to shot"
allStates "'Check for\u00b6Update'" "'\u00b6Checking ...'" "'\u00b6Up to date'" "'Error\u00b6Retry'" "'Update\u00b6available!'"

# The new version is in the tooltip, and the agent string names this one.
b5_dump
[ "$(b5_json "el('Menu.VersionButton')['toolTip'] == 'New version: 1.3.0\u00b6A click opens the download page.'")" = True ] \
	&& b5_ok "the tooltip names the new version" \
	|| b5_note "the tooltip is $(b5_json "repr(el('Menu.VersionButton').get('toolTip'))")"
grep -q "Scherfgen-Software Blocks 5 (1.2.0)" "$WORK/requests" && b5_ok "the agent string names the version" \
	|| b5_note "the agent string is wrong: $(tail -1 "$WORK/requests")"

# The flashing as drawn: the button's pixels go on changing, where with the
# same picture standing still they did not. Five shots a fifth of a second
# apart cover the one-second pulse.
b5_clientOrigin
for i in 1 2 3 4 5; do buttonShot "flash$i"; sleep 0.2; done
changed=$(mostChanged flash1 flash2 flash3 flash4 flash5)
[ "$changed" -ge 1000 ] && b5_ok "the flashing button changes as it is drawn ($changed colour values)" \
	|| b5_note "the flashing button barely changes as it is drawn ($changed colour values)"

# With an update out, a click opens the download page and asks nothing.
before=$(requests)
b5_click Menu.VersionButton
sleep 1
[ "$(cat "$WORK/opened" 2>/dev/null)" = "https://www.david-scherfgen.de/meine-spiele/blocks-5/" ] \
	&& b5_ok "a click opens the download page" \
	|| b5_note "a click opened \"$(cat "$WORK/opened" 2>/dev/null)\""
[ "$(requests)" -eq "$before" ] && b5_ok "and does not ask again" || b5_note "the click asked again"

# The box in the options: OK writes it to config.xml, Cancel takes a tick
# back even when nothing else was touched.
b5_click Menu.Options
b5_dump
[ "$(b5_json "el('OptionsPane.Options.UpdateCheck')['checked']")" = False ] \
	&& b5_ok "the box is clear, as the check is off" || b5_note "the box is ticked although the check is off"
b5_click OptionsPane.Options.UpdateCheckLabel
b5_click OptionsPane.Options.OK
[ "$(config)" = 1 ] && b5_ok "OK writes the tick to config.xml" || b5_note "config.xml says $(config) after a tick and OK"
b5_click Menu.Options
b5_click OptionsPane.Options.UpdateCheck
b5_click OptionsPane.Options.Cancel
b5_click Menu.Options
b5_dump
[ "$(b5_json "el('OptionsPane.Options.UpdateCheck')['checked']")" = True ] \
	&& b5_ok "Cancel takes back a click on the box alone" || b5_note "Cancel left the box cleared"
b5_click OptionsPane.Options.UpdateCheck
b5_click OptionsPane.Options.OK
[ "$(config)" = 0 ] && b5_ok "clearing it and OK writes 0" || b5_note "config.xml says $(config) after clearing and OK"
quit

# --- 2. German, every state --------------------------------------------------
: > "$WORK/requests"
freshHome 1.2.0
writeConfig '<Language>de</Language>'
start german
allStates "'Auf Update\u00b6pr\u00fcfen'" "'\u00b6Pr\u00fcfe ...'" "'\u00b6Aktuell'" "'Fehler\u00b6Wiederholen'" "'Update\u00b6verf\u00fcgbar!'"
quit

# --- 3. an old version's switch in the user directory ------------------------
# Taken into config.xml over what that says and deleted, and the check runs at
# once.
: > "$WORK/requests"
serve '1.3.0\n'
freshHome 1.2.0
writeConfig '<CheckForUpdates>0</CheckForUpdates>'
printf '1 \r\n' > "$B5_PRIVATE_HOME/.update_checker"
start adopt-home
[ -e "$B5_PRIVATE_HOME/.update_checker" ] && b5_note "the user directory's .update_checker is still there" \
	|| b5_ok "the user directory's .update_checker is gone"
[ "$(config)" = 1 ] && b5_ok "config.xml has taken it in" || b5_note "config.xml says $(config)"
waitUpdate "$AVAILABLE" "available"
[ "$(requests)" -eq 1 ] && b5_ok "the check ran at the start, once" || b5_note "$(requests) requests at the start"
expectButton "'Update\u00b6available!'" True True "after a check at the start"
quit

# And from config.xml alone at the next start.
: > "$WORK/requests"
serve '1.2.0\n'
start config-on
waitUpdate "$UP_TO_DATE" "up to date"
[ "$(requests)" -eq 1 ] && b5_ok "config.xml switches the check on by itself" || b5_note "$(requests) requests at the start"
quit

# --- 4. the installation's default -----------------------------------------
# The installer writes its box beside the game, and a player starts with it -
# from the first start until config.xml says otherwise. The game folder is the
# working tree here, so the trap deletes the file whatever happens.
: > "$WORK/requests"
serve '1.2.0\n'
freshHome 1.2.0
printf '1' > "$GAME_SIGNAL"
start default-new
waitUpdate "$UP_TO_DATE" "up to date"
[ "$(requests)" -eq 1 ] && b5_ok "a new player starts with the installation's on" \
	|| b5_note "$(requests) requests at a new player's start"
quit
[ "$(config)" = 1 ] && b5_ok "and keeps it in config.xml from the first exit on" || b5_note "config.xml says $(config)"
[ -e "$GAME_SIGNAL" ] && b5_ok "the installation's default stays for the next player" \
	|| b5_note "the installation's default was deleted"

# A player's own setting is theirs, whatever the installation says.
: > "$WORK/requests"
freshHome 1.2.0
writeConfig '<CheckForUpdates>0</CheckForUpdates>'
start default-own
b5_dump
[ "$(b5_json "d['updateCheck']")" = "$IDLE" ] && [ "$(requests)" -eq 0 ] \
	&& b5_ok "a player's own off stands against the installation's on" \
	|| b5_note "the installation's default overruled the player's own setting"
quit
rm -f "$GAME_SIGNAL"

# An old version's switch is deleted only once config.xml holds it. A
# config.xml that points into a folder that is not there cannot be written,
# not even by root, and reads as missing.
freshHome 1.2.0
ln -s "$B5_PRIVATE_HOME/missing/config.xml" "$B5_PRIVATE_HOME/config.xml"
printf '1' > "$B5_PRIVATE_HOME/.update_checker"
start unwritable
[ -e "$B5_PRIVATE_HOME/.update_checker" ] && b5_ok "the old switch stays where config.xml cannot be written" \
	|| b5_note "the old switch was deleted although config.xml could not be written"
grep -q "config.xml could not be written" "$B5_OUT/run.log" && b5_ok "and the log says why" \
	|| b5_note "the log does not say why the old switch stays"
quit

# --- 5. no curl, no wget -----------------------------------------------------
# Nothing to ask with: the version alone in the corner, the label of before
# 1.2.0, and the options' box greyed out. Nothing says what is missing.
export PATH="$WORK/notools"
: > "$WORK/requests"
freshHome 1.2.0
start no-tools
b5_dump
[ "$(b5_json "el('Menu.Version')['shown'] and not el('Menu.VersionButton')['shown']")" = True ] \
	&& b5_ok "the label stands in the button's place" \
	|| b5_note "the label is shown: $(b5_json "el('Menu.Version')['shown']"), the button: $(b5_json "el('Menu.VersionButton')['shown']")"
[ "$(b5_json "el('Menu.Version')['text'][0] > 0")" = True ] \
	&& b5_ok "the label holds the version" || b5_note "the label is empty"
# Where the hidden button lies, a click starts nothing. Even a check that could
# not run would show, as an immediate failure.
clickVersionAnyway
sleep 1
b5_dump
[ "$(b5_json "d['updateCheck']")" = "$IDLE" ] && b5_ok "a click where the button would be starts nothing" \
	|| b5_note "a click on the hidden button started a check (state $(b5_json "d['updateCheck']"))"
b5_click Menu.Options
b5_dump
[ "$(b5_json "el('OptionsPane.Options.UpdateCheck')['active']")" = False ] \
	&& b5_ok "the options' box is disabled" || b5_note "the options' box is enabled without curl and wget"
[ "$(b5_json "[e['path'] for e in d['elements'] if e.get('toolTip') and e['path'].startswith('OptionsPane.Options.UpdateCheck')]")" = "[]" ] \
	&& b5_ok "and has no tooltip, there being nothing it would take effect on" \
	|| b5_note "the greyed box has a tooltip: $(b5_json "repr(el('OptionsPane.Options.UpdateCheck').get('toolTip'))")"
[ "$(b5_json "[e['path'] for e in d['elements'] if 'curl' in e.get('toolTip', '') or 'wget' in e.get('toolTip', '')]")" = "[]" ] \
	&& b5_ok "no tooltip names curl or wget" \
	|| b5_note "tooltips name curl or wget: $(b5_json "[e['path'] for e in d['elements'] if 'curl' in e.get('toolTip', '') or 'wget' in e.get('toolTip', '')]")"
b5_key Escape
quit

# --- 6. a curl that never answers -------------------------------------------
# The game gives up after twelve seconds and kills it, and a check still
# running when the game quits ends with it.
export PATH="$WORK/hang:$ORIGINAL_PATH"
: > "$WORK/hang.pids"
freshHome 1.2.0
writeConfig '<CheckForUpdates>1</CheckForUpdates>'
start hang
first=$(head -1 "$WORK/hang.pids")
[ -n "$first" ] && b5_ok "the check started with the game" || b5_note "no check started"
# Given up on wherever the twelve seconds ran out - during the loading screen,
# which does not poll, the menu's first poll finds them over.
waitUpdate "$FAILED" "failed" 20
expectButton "'Error\u00b6Retry'" True False "given up on"
kill -0 "$first" 2>/dev/null && b5_note "the hanging curl was not killed" || b5_ok "the hanging curl was killed and reaped"
b5_click Menu.VersionButton
b5_dump
expectButton "'\u00b6Checking ...'" False False "asking again"
second=$(tail -1 "$WORK/hang.pids")
[ "$second" != "$first" ] && b5_ok "Retry started another" || b5_note "Retry started nothing"
quit
kill -0 "$second" 2>/dev/null && b5_note "a check running at the end outlived the game" \
	|| b5_ok "a check running at the end ended with the game"

# --- 7. wget alone ------------------------------------------------------------
export PATH="$WORK/nocurl"
: > "$WORK/requests"
serve '1.2.0\n'
freshHome 1.2.0
start wget
b5_click Menu.VersionButton
waitUpdate "$UP_TO_DATE" "up to date"
grep -qi "wget" "$WORK/requests" && b5_note "wget sent its own agent string" \
	|| b5_ok "wget asks, with the game's agent string"
quit
export PATH="$ORIGINAL_PATH"

# --- 8. the author's -updatecheckversion ---------------------------------------
# The update check takes the version given for the one running, and nothing
# else does. Given the very version .initialized names, a version check that
# took it too would see no change: the migration must run all the same and
# .initialized must get the real version.
: > "$WORK/requests"
serve '1.2.0\n'
freshHome 1.1.2
writeConfig '<CheckForUpdates>1</CheckForUpdates>'
B5_ARGS="-updatecheckversion 1.1.2"
start pretend
B5_ARGS=""
waitUpdate "$AVAILABLE" "available"
[ "$(b5_json "el('Menu.VersionButton')['title'] == 'v1.1.2\u00b6Update\u00b6available!'")" = True ] \
	&& b5_ok "the real version offered as an update to the one given" \
	|| b5_note "the button says $(b5_json "repr(el('Menu.VersionButton')['title'])")"
grep -q "Scherfgen-Software Blocks 5 (1.1.2)" "$WORK/requests" && b5_ok "the agent string names the version given" \
	|| b5_note "the agent string is $(tail -1 "$WORK/requests")"
grep -q "Initializing/Updating" "$B5_OUT/run.log" && b5_ok "the migration ran by the real version" \
	|| b5_note "the migration did not run"
[ "$(cat "$B5_PRIVATE_HOME/.initialized")" = 1.2.0 ] && b5_ok ".initialized has the real version" \
	|| b5_note ".initialized says $(cat "$B5_PRIVATE_HOME/.initialized")"
quit

b5_finish
