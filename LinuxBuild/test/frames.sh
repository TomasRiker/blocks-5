#!/bin/bash
# frames.sh - one named scene, one 640x480 PNG, meant to be byte-reproducible.
#
# Proved both ways, which is what makes it worth trusting. Two runs at one
# seed: all five scenes byte-identical. Two runs at different seeds: menu,
# select and night differ, so the seed really does reach the draws a rendered
# frame makes; editor and plain do not, because nothing in either is random.
# An oracle that can only answer "identical" is one that measures nothing.
#
# This is the oracle the rest of the render work is checked against: a change
# that should move no pixel is proved by running this before and after and
# comparing the bytes, rather than by arguing that it cannot.
#
# Three things make a frame reproducible, and all three are needed:
#
#   B5_SEED       seeds the one generator every random() draws from, keyed on
#                 the scene's tick - see Engine::render(). Without it the
#                 night vision's noise offsets alone move two thirds of the
#                 pixels between two runs of the same binary.
#   freeze <t>    stops the logic clock at a named tick of the running level,
#                 so the picture belongs to that tick however many ticks the
#                 machine caught up with inside one rendered frame.
#   shot <path>   writes what the game read out of its own framebuffer, at
#                 640x480 whatever the window is doing - not a screen grab of
#                 a scaled window.
#
# XDG_DATA_HOME points the game at a private home directory per run, so a
# scene starts from a clean config and the developer's own levels, saves and
# progress are neither read nor written.
#
# It is also the run that puts the GL state layer through its paces, which is
# why it ends by reading the log: a hooks build reads the real binding, texture
# matrix and enable back on every GL:: call and reports a record that
# disagrees, and every onRender in the tree goes past here. A line there is a
# problem and the exit code says so.
#
#   frames.sh <outdir> [scene ...]     default: every scene
#   frames.sh --list

B5_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

SCENES="menu select plain night editor"

if [ "$1" = "--list" ]; then echo $SCENES; exit 0; fi

B5_OUTDIR="$1"; shift
[ -n "$B5_OUTDIR" ] || { echo "usage: frames.sh <outdir> [scene ...]"; exit 2; }
WANT="${*:-$SCENES}"

mkdir -p "$B5_OUTDIR"

# A private home, and a private display and hook directory so two runs of this
# can go at once - see b5_clearDisplay in harness.sh.
export XDG_DATA_HOME="${B5_FRAMES_XDG:-/tmp/blocks5-frames-xdg}"
# getAppHomeDirectory() appends "/blocks5/" to XDG_DATA_HOME, so this and not
# XDG_DATA_HOME itself is where the game's own files go. Writing a level one
# directory too high is silent: the single-levels list simply does not contain
# it, and the select screen then plays whatever sorts first among the game
# folder's own forty-two.
B5_PRIVATE_HOME="$XDG_DATA_HOME/blocks5"
export B5_DISPLAY="${B5_DISPLAY:-:88}"
export B5_SHOTS="${B5_SHOTS:-/tmp/blocks5-frames}"
export B5_SEED="${B5_SEED:-12345}"
rm -rf "$XDG_DATA_HOME"
mkdir -p "$B5_PRIVATE_HOME/levels" "$B5_PRIVATE_HOME/screenshots"

# Written before the first start rather than left to it, because each of them
# is a way the picture or the run could depend on something outside the game.
# .update_checker asks the website for the current version, which is a network
# round trip in front of the window - and on a machine without the network it
# is a stall of unknown length. .initialized skips the version-migration
# branch, which has nothing to do on a fresh directory but does list it.
printf 0 > "$B5_PRIVATE_HOME/.update_checker"
printf 1.2.0 > "$B5_PRIVATE_HOME/.initialized"

# The oracle levels. Written here rather than committed, because their whole
# content is "one screen holding the objects this scene is about" and a reader
# of the scene list should be able to see what that is.
write_level()   # $1 title, $2 nightVision, $3 objects
{
	python3 - "$1" "$2" "$3" "$B5_PRIVATE_HOME/levels/$1.xml" <<'PY'
import sys
title, night, objects, path = sys.argv[1:5]
W, H = 40, 25
rows = lambda f: ''.join('<Row>%s</Row>' % f(y) for y in range(H))
empty = rows(lambda y: ' ' * W)
walls = rows(lambda y: 'M' * W if y in (0, H - 1) else 'M' + ' ' * (W - 2) + 'M')
head = ('<?xml version="1.0" ?><Level title="%s" '
        'skin0="" skin1="" skin2="" skin3="" skin4="" skin5="" skin6="" '
        'skin7="" skin8="" skin9="" skin10="" width="40" height="25" '
        'numLayers="2" numDiamondsNeeded="0" electricityOn="1" '
        'nightVision="%s" raining="0" clouds="0" snowing="0" thunderstorm="0" '
        'lightColorR="255" lightColorG="255" lightColorB="255" '
        'musicFilename="">' % (title, night))
open(path, 'w', encoding='latin-1').write(
    head + '<Layer>' + empty + '</Layer><Layer>' + walls + '</Layer>' + objects + '</Level>')
PY
}

# The single-levels campaign holds every loose level in BOTH roots, so the
# game folder's forty-two campaign sources are in the list whatever the
# private home contains. It is sorted by localized title, and every one of
# those titles begins with a letter - so a title beginning with "!" sorts in
# front of the lot and is the entry a freshly selected campaign already has.
# That is what makes these reachable with no level-navigation click at all,
# which is the part that went wrong: the select screen's list keeps the
# keyboard focus, and a click on a level button is one more thing to get
# right for no benefit.
write_level '!2plain' 0 \
	'<Object type="Player" x="3" y="3" character="0" active="1"/><Object type="Diamond" x="10" y="6"/><Object type="Block" x="12" y="6"/><Object type="Exit" x="36" y="21"/>'
write_level '!1night' 1 \
	'<Object type="Player" x="3" y="3" character="0" active="1"/><Object type="Laser" x="20" y="21" dir="3"/><Object type="LightBarrierSender" x="2" y="12" dir="1"/><Object type="LightBarrierSender" x="34" y="4" dir="2"/><Object type="Fire" x="8" y="16"/>'

. "$B5_HERE/harness.sh"

# Let the clock run again. ~0u is the "never" the freeze starts at.
b5_release() { b5_ask "freeze 4294967295" >/dev/null; }

# Freeze at a tick of the running level and write its frame. The wait is on
# the reported state and never on an interval: under llvmpipe a frame is a
# fifth of a second, so a guessed sleep is either wrong or slow.
b5_frame()
{
	local name=$1 tick=$2 i
	b5_ask resetstats >/dev/null
	b5_ask "freeze $tick" >/dev/null
	for i in $(seq 1 120); do
		b5_alive || { echo "FAILED: the game exited while waiting for tick $tick"; b5_diagnose; exit 2; }
		b5_dump || { sleep 1; continue; }
		[ "$(b5_json "d['frozen']")" = "True" ] && break
		sleep 0.5
	done
	[ "$(b5_json "d['frozen']")" = "True" ] || { echo "FAILED: $name never froze"; exit 1; }

	# Say so when the clock stopped somewhere else than it was asked to. It
	# means the scene has no level that ticks - the editor's does not - so the
	# freeze fired on whatever the last one left behind, and the frame is only
	# reproducible for as long as nothing in that scene is random.
	local at
	at=$(b5_json "d['scene']")
	# A note and not a problem: the frame is still reproducible, it is only
	# reproducible for a weaker reason - nothing in that scene is random - and
	# a tool that exits nonzero on every run is one nobody reads the exit code
	# of.
	[ "$at" = "$tick" ] || echo "    (note) froze at $at, not at $tick - this scene has no clock of its own"
	if [ "$(b5_ask "shot $B5_OUTDIR/$name.png")" != "ok" ]; then
		echo "FAILED: $name could not be written"; exit 1
	fi
	# Per frame and per draw, not the raw counters: those accumulate over
	# however many frames the machine managed between the reset and the freeze,
	# so only a ratio is comparable between two runs.
	#
	# "batch draws" is sprite-batch flushes that drew something, not the
	# frame's GL draw calls - every glBegin block and every drawQuadArray is
	# outside this count. What it is good for is the quads-per-draw beside it,
	# which is how much each flush carried.
	b5_ok "$name.png  (scene tick $(b5_json "d['scene']"), $(b5_json "'%.1f batch draws/frame, %.1f quads/draw' % (d['batch']['draws'] / max(d['frames']['count'], 1), d['batch']['quads'] / max(d['batch']['draws'], 1))"), state $(b5_json "'%.0f%% of %d calls skipped' % (100.0 * d['glstate']['skipped'] / max(d['glstate']['issued'] + d['glstate']['skipped'], 1), d['glstate']['issued'] + d['glstate']['skipped'])"))"
}

# Every scene starts from the menu, so one game serves the lot: the frames are
# taken in one run and the startup is paid once.
b5_start
b5_waitForState GS_Menu

wanted() { case " $WANT " in *" $1 "*) return 0;; esac; return 1; }

for scene in $WANT; do
	case "$scene" in
	menu|select|plain|night|editor) ;;
	*) echo "unknown scene \"$scene\" - try --list"; exit 2;;
	esac
done

# One game for the lot: the startup costs half a minute under llvmpipe, and
# the navigation between scenes is the same walk a player makes. The clock is
# released after each frame, or nothing would move again.

if wanted menu; then
	# The title demo, which is a level like any other and so has the clock
	# everything here hangs on.
	b5_frame menu 4000
	b5_release
fi

if wanted select || wanted plain || wanted night; then
	b5_click Menu.StartGame
	b5_waitForState GS_SelectLevel

	# The single levels are the last campaign, and the list keeps the keyboard
	# focus after a click - which is what makes End reach them. The two oracle
	# levels sort last of all by their leading tildes, "~~night" behind
	# "~plain", so the last level of the last campaign is the night-vision one.
	# End on the campaign list, which keeps the keyboard focus after a click -
	# that is what reaches the single levels, listed last. The level index is
	# then 0, which is "!1night" by the sort above.
	b5_click SelectLevel.Campaigns
	b5_key End

	# The preview is a loaded level, so the select screen has a clock too -
	# and the frame carries the 39x39 status stamp, which is one of only two
	# odd-sized sprites in the game.
	if wanted select; then b5_frame select 2000; b5_release; fi

	if wanted night; then
		b5_click SelectLevel.PlayLevel
		b5_waitForState GS_Game
		# Far enough in that the laser has ramped up and both light barriers
		# are steady; the fire and its particles are still animating, which is
		# the point of taking it here rather than at tick 0.
		b5_frame night 6000
		b5_release
		b5_key Escape
		b5_click Game.MenuPane.Menu.Quit
		b5_waitForState GS_SelectLevel
	fi

	if wanted plain; then
		b5_click SelectLevel.NextLevel
		b5_click SelectLevel.PlayLevel
		b5_waitForState GS_Game
		b5_frame plain 3000
		b5_release
		b5_key Escape
		b5_click Game.MenuPane.Menu.Quit
		b5_waitForState GS_SelectLevel
	fi

	b5_key Escape
	b5_waitForState GS_Menu
fi

if wanted editor; then
	# The editor draws its object palette under a glTranslated of its own, so
	# a sprite drawn under the wrong matrix leaves the screen there and nowhere
	# else - which is how the batch's first two bugs showed. This scene opens
	# on the first palette tab, and cat0.xml holds no objects at all, so what
	# it watches for now is the rest of the editor: the tile grid, the panes
	# and the cursor. Catching the palette again means clicking a tab that has
	# something in it, which moves this scene's picture.
	b5_click Menu.LevelEditor
	b5_waitForState GS_LevelEditor
	b5_frame editor 2000
	b5_release
fi

# Every "+ ERROR" the run logged, which is where GL:: reports a record that
# disagrees with what OpenGL is really holding - a hooks build reads the state
# back on every call. A wrong record is a wrong picture somewhere nobody was
# looking, and a message in a log nobody reads is not a check.
echo
grep -q "ERROR" "$B5_OUT/run.log" \
	&& b5_note "ERROR in the log: $(grep -m3 ERROR "$B5_OUT/run.log" | tr '\n' ' ')" \
	|| b5_ok "no error line in the log"

echo "frames in $B5_OUTDIR"
b5_stop
b5_finish
