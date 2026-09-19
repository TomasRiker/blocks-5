#!/bin/bash
# frames.sh - one named scene, one 640x480 PNG, meant to be byte-reproducible.
#
# Proved both ways, which is what makes it worth trusting. Two runs at one
# seed: all nineteen scenes byte-identical. A run at another seed moves
# fifteen of them, so the seed really does reach the draws a tick and a
# rendered frame make; editor-connect, hint, loading and plain stay, because
# nothing in those four is random. An oracle that can only answer
# "identical" is one that measures nothing. Getting there took five changes
# to the game itself - .claude/rules/testing.md lists them - and each
# was a frame that came out differently on a machine that bunched its ticks
# differently, which is to say on any two machines.
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
# It is also the run that puts the renderer's record through its paces, which
# is why it ends by reading the log: a hooks build reads the real binding,
# blend, program, buffers, mask, stencil and scissor back after every draw and
# reports a record that disagrees, and every onRender in the tree goes past
# here. A line there is a problem and the exit code says so.
#
#   frames.sh <outdir> [scene ...]     default: every scene
#   frames.sh --list

B5_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

SCENES="menu options crt manager star editor help editbox editor-select editor-connect select cube night plain lava toxic hint loading credits credits-plain"

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
write_level()   # $1 title, $2 nightVision, $3 objects, [$4 raining, $5 thunderstorm, $6 clouds]
{
	python3 - "$1" "$2" "$3" "${4:-0}" "${5:-0}" "${6:-0}" "$B5_PRIVATE_HOME/levels/$1.xml" <<'PY'
import sys
title, night, objects, raining, thunderstorm, clouds, path = sys.argv[1:8]
W, H = 40, 25
rows = lambda f: ''.join('<Row>%s</Row>' % f(y) for y in range(H))
empty = rows(lambda y: ' ' * W)
walls = rows(lambda y: 'M' * W if y in (0, H - 1) else 'M' + ' ' * (W - 2) + 'M')
head = ('<?xml version="1.0" ?><Level title="%s" '
        'skin0="" skin1="" skin2="" skin3="" skin4="" skin5="" skin6="" '
        'skin7="" skin8="" skin9="" skin10="" width="40" height="25" '
        'numLayers="2" numDiamondsNeeded="0" electricityOn="1" '
        'nightVision="%s" raining="%s" clouds="%s" snowing="0" thunderstorm="%s" '
        'lightColorR="255" lightColorG="255" lightColorB="255" '
        'musicFilename="">' % (title, night, raining, clouds, thunderstorm))
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
# The electronics pair is here for the wire pass, and it is the reason this
# scene has objects that have nothing to do with each other. Electronics sets
# RL_WIRE in its own constructor and every part used to replace it, so
# renderObjects skipped all of them and no connection was drawn anywhere in the
# game - silently, for as long as the layers have had names, because none of
# the five scenes had a wire in it. The editor palettes could not catch this
# one either: cat4 holds the parts and nothing in it is connected.
write_level '!2plain' 0 \
	'<Object type="Player" x="3" y="3" character="0" active="1"/><Object type="Diamond" x="10" y="6"/><Object type="Block" x="12" y="6"/><Object type="E_Clock" x="10" y="14" dir="0" value="0"><OutputConnections><Connection sourcePinID="10" targetX="25" targetY="14" targetPinID="0" /></OutputConnections></Object><Object type="E_LightBulb" x="25" y="14" dir="0"><OutputConnections /></Object><Object type="Exit" x="36" y="21"/>'
write_level '!1night' 1 \
	'<Object type="Player" x="3" y="3" character="0" active="1"/><Object type="Laser" x="20" y="21" dir="3"/><Object type="LightBarrierSender" x="2" y="12" dir="1"/><Object type="LightBarrierSender" x="34" y="4" dir="2"/><Object type="Fire" x="8" y="16"/>'

# A pool of lava under rain, thunder and clouds: the three lava passes with
# their stencil, the weather's scrolling textures, the lightning's two passes
# and the flash, none of which the two levels above draw. The pool's rows
# take the three shapes of lava the shipped levels use most, straight flows
# (0, 3) and a two-pass one (8).
lava_pool=$(python3 -c "
print(''.join('<Object type=\"Lava\" x=\"%d\" y=\"%d\" dir=\"%d\"/>' % (x, y, {14: 0, 15: 8, 16: 3}[y]) for y in (14, 15, 16) for x in range(14, 20)))")
write_level '!3lava' 0 \
	"<Object type=\"Player\" x=\"3\" y=\"3\" character=\"0\" active=\"1\"/>$lava_pool<Object type=\"Fire\" x=\"8\" y=\"16\"/><Object type=\"Fire\" x=\"30\" y=\"10\"/><Object type=\"Laser\" x=\"20\" y=\"21\" dir=\"3\"/><Object type=\"Diamond\" x=\"10\" y=\"6\"/><Object type=\"Exit\" x=\"36\" y=\"21\"/>" \
	1 1 1

# Two of the levels Tools/testlevels keeps: Bob standing in toxic gas, which
# is the one way to put the contamination effect on the screen, and a hint
# note with keycaps under Bob's feet, which opens by itself. Retitled so that
# they sort behind the levels above - the campaign lists by title, and every
# scene below counts on the order.
retitle()   # $1 source, $2 title
{
	python3 - "$1" "$2" "$B5_PRIVATE_HOME/levels/$2.xml" <<'PY'
import re, sys
src, title, path = sys.argv[1:4]
text = open(src, encoding='latin-1').read()
text = re.sub(r'(<Level\b[^>]*?\btitle=")[^"]*(")', lambda m: m.group(1) + title + m.group(2), text, count=1)
open(path, 'w', encoding='latin-1').write(text)
PY
}
retitle "$B5_HERE/../../Tools/testlevels/contamination.xml" '!4toxic'
retitle "$B5_HERE/../../Tools/testlevels/keycaps.xml" '!5hint'

. "$B5_HERE/harness.sh"

# However this script leaves - every FAILED above exits from inside a
# function - the game and the X server it started go with it. Left behind,
# they are what the next run finds attached to the display and refuses to
# start over.
trap 'b5_stop' EXIT

# Let the clock run again. ~0u is the "never" the freeze starts at.
b5_release() { b5_ask "freeze 4294967295" >/dev/null; }

# Freeze at a tick of the running scene and write its frame. "now" instead of
# a tick freezes at the next tick whatever it is, for a screen without a
# clock of its own - the editor's level does not tick - whose frame is
# reproducible for the weaker reason that nothing in it moves. The wait is on the reported state and never on an
# interval: under llvmpipe a frame is a fifth of a second, so a guessed sleep
# is either wrong or slow.
b5_frame()   # $1 name, $2 tick or "now"
{
	local name=$1 tick=$2
	b5_ask resetstats >/dev/null
	if [ "$tick" = now ]; then b5_ask "freeze 0" >/dev/null; tick=""
	else b5_ask "freeze $tick" >/dev/null; fi
	b5_takeFrozen "$name" "$tick"
}

# A crossfade's clock moves once per iteration of the main loop and the
# screen under it once per tick, and how many ticks an iteration bunches is
# up to the machine: the level's load alone is a backlog of several. Left
# to itself the fade therefore reaches its 400 ms at a different tick of the
# new screen on every run. Lockstep - one tick per iteration - pins the two
# clocks to each other, so the fade's 400 ms is always the same tick of the
# screen behind it. On from before the click that starts the transition,
# off once the frame is taken.
b5_releaseLockstep()
{
	b5_release
	b5_ask "lockstep 0" >/dev/null
}

# A transition started by the hook from a frozen screen: the old image is
# that screen at a named tick - a click that lands on a tick, which no real
# click can - and under lockstep the fade's 400 ms is a fixed number of
# frames later. Both screens a crossfade is taken from here tick: the menu's
# title demo and the select screen's preview level.
b5_transition()   # $1 button, $2 tick to freeze the old screen on, $3 frame name
{
	b5_ask resetstats >/dev/null
	b5_ask "freeze $2" >/dev/null
	b5_waitFrozen "$3" "$2"
	b5_ask "lockstep 1" >/dev/null
	[ "$(b5_ask "click $1")" = "ok" ] || { echo "FAILED: the hook found no button $1"; exit 1; }
	b5_ask "freeze fade 400" >/dev/null
	b5_takeFrozen "$3"
	b5_releaseLockstep
}

b5_waitFrozen()   # $1 name, [$2 the tick it must have stopped on]
{
	local name=$1 tick=${2:-} i
	for i in $(seq 1 120); do
		b5_alive || { echo "FAILED: the game exited while waiting for $name to freeze"; b5_diagnose; exit 2; }
		b5_dump || { sleep 1; continue; }
		[ "$(b5_json "d['frozen']")" = "True" ] && break
		sleep 0.5
	done
	[ "$(b5_json "d['frozen']")" = "True" ] || { echo "FAILED: $name never froze"; exit 1; }

	# The clock stopped somewhere else than it was asked to: the tick had
	# passed before the request arrived, and the frame belongs to whatever
	# tick the harness's own timing reached.
	if [ -n "$tick" ]; then
		local at
		at=$(b5_json "d['scene']")
		if [ "$at" != "$tick" ]; then
			echo "FAILED: $name froze at tick $at, not at $tick - the tick had passed before the freeze was asked for"
			exit 1
		fi
	fi
}

b5_takeFrozen()   # $1 name, [$2 tick]
{
	local name=$1
	b5_waitFrozen "$@"
	if [ "$(b5_ask "shot $B5_OUTDIR/$name.png")" != "ok" ]; then
		echo "FAILED: $name could not be written"; exit 1
	fi
	b5_checkNotDoubled "$B5_OUTDIR/$name.png" "$name"
	# Per frame and per draw, not the raw counters: those accumulate over
	# however many frames the machine managed between the reset and the freeze,
	# so only a ratio is comparable between two runs.
	#
	# "draw calls" is every draw render() made, counted at the link (see the
	# foot of testhooks.cpp) - the number the renderer redesign is measured
	# by. "batch draws" beside it is the renderer's own flushes that drew
	# something, and the reasons say what ended each batch.
	b5_ok "$name.png  (scene tick $(b5_json "d['scene']"), $(b5_json "'%.1f draw calls/frame' % (d['draws']['calls'] / max(d['draws']['frames'], 1))"), batch $(b5_json "'%.1f draws/frame, %.1f quads/draw' % (d['batch']['draws'] / max(d['draws']['frames'], 1), d['batch']['quads'] / max(d['batch']['draws'], 1))"))"
	b5_ok "  batch flushes by reason: $(b5_json "', '.join('%s %d' % (k, v) for k, v in d['batch']['byReason'].items() if v) or 'none'")"
	# The font cache beside them: what it is holding, and whether it is
	# earning it. quads * 64 bytes is the geometry; the measures are what
	# measureText() was asked, and the walks are the ones it had to answer by
	# reading the string.
	b5_ok "  font cache: $(b5_json "'%d entries, %d quads (%.0f KB), %.0f%% of %d lookups hit, %d evictions' % (d['fontcache']['entries'], d['fontcache']['quads'], d['fontcache']['quads'] * 64.0 / 1024.0, 100.0 * d['fontcache']['hits'] / max(d['fontcache']['hits'] + d['fontcache']['misses'], 1), d['fontcache']['hits'] + d['fontcache']['misses'], d['fontcache']['evictions'])")"
	b5_ok "  measures:   $(b5_json "'%d asked, %d by a laid-out string, %d by the %d dimensions kept (%.0f KB, %d evictions), %d walks' % (d['fontcache']['measures'], d['fontcache']['measureHits'], d['fontcache']['dimHits'], d['fontcache']['dimEntries'], d['fontcache']['dimBytes'] / 1024.0, d['fontcache']['dimEvictions'], d['fontcache']['measures'] - d['fontcache']['measureHits'] - d['fontcache']['dimHits'])")"
}

# Is the picture really the game's 640x480 frame, or a magnified corner of it?
# encodeFrame reads GL_COLOR_ATTACHMENT0, so a caller that has not bound the
# game's own framebuffer gets whatever else is - and what came back then was
# the frame rasterized at 2x under the window's viewport, of which a 640x480
# read takes one quarter. Every pixel was its own neighbour and the oracle
# compared a doubled quarter-screen for as long as it existed, silently,
# because a doubled frame is still perfectly reproducible.
#
# The test is that shape and not the bug: a frame whose every row pair AND
# every column pair is identical has been through an integer upscale, whatever
# caused it. Real art at 640x480 has thousands of pairs that differ.
b5_checkNotDoubled()
{
	local png=$1 name=$2 verdict
	verdict=$(B5_PNG="$png" B5_WEB="$B5_HERE/../../WebBuild" python3 - <<'PYEND'
import os, sys
sys.path.insert(0, os.environ['B5_WEB'])
from make_icon import read_png
w, h, px = read_png(os.environ['B5_PNG'])

def row(y):
    return px[(y * w) * 4:((y + 1) * w) * 4]

def column(x):
    return [px[(y * w + x) * 4:(y * w + x) * 4 + 3] for y in range(h)]

# Stop at the first pair that differs: a healthy frame answers in two
# comparisons, and only a doubled one pays for the whole image.
doubled = all(row(y) == row(y + 1) for y in range(0, h - 1, 2)) and \
          all(column(x) == column(x + 1) for x in range(0, w - 1, 2))
print('doubled' if doubled else 'ok')
PYEND
	)
	if [ "$verdict" = "doubled" ]; then
		echo "FAILED: $name is a 2x upscale of a quarter frame - every row and column pair is a copy"
		exit 1
	fi
}

# Every scene starts from the menu, so one game serves the lot: the frames are
# taken in one run and the startup is paid once.
b5_start
b5_waitForState GS_Menu

wanted() { case " $WANT " in *" $1 "*) return 0;; esac; return 1; }
for scene in $WANT; do
	case " $SCENES " in
	*" $scene "*) ;;
	*) echo "unknown scene \"$scene\" - try --list"; exit 2;;
	esac
done
needs() { local s; for s in "$@"; do wanted "$s" && return 0; done; return 1; }

# The menu and its dialogs come first, on the menu's first visit. The select
# screen, the editors and a played level are pushed on top of the menu, and
# the menu's own clock - which the title demo's recording and the clouds run
# on - carries on across that while the restored title level's starts again;
# after a pop the two therefore stand apart by whatever the harness's timing
# made of the visit, and the clouds at a level tick are a different picture on
# every run. On the first visit both start at zero. The ticks asked for leave
# the navigation room to arrive first.
if wanted menu; then
	b5_frame menu 4000
	b5_release
fi
if needs options crt; then
	# The CRT settings button switches the filter on there and then, and
	# Cancel takes that back through loadConfig() - which on this run's
	# fresh home has no config.xml to load and so takes back nothing. Left
	# on, the CRT's curvature warps every later click off its element; the
	# filter that was on is therefore clicked back by its own radio button,
	# named as the dump names the filter.
	b5_dump
	FILTER=$(b5_json "d['filter']")
	b5_click Menu.Options
	if wanted options; then b5_frame options 10000; b5_release; fi
	if wanted crt; then
		b5_click OptionsPane.Options.CrtSettings
		b5_frame crt 16000
		b5_release
		b5_click OptionsPane.CrtOptions.Close
		b5_click "OptionsPane.Options.$FILTER"
	fi
	b5_click OptionsPane.Options.Cancel
fi
if wanted manager; then
	b5_click Menu.Manager
	b5_frame manager 30000
	b5_release
	b5_click Menu.ManagerPane.Manager.Close
fi

# The editor scenes. The star is the crossfade from the menu into the editor,
# and for the reason above it is started by the hook while the menu stands
# frozen at 36000 - a click on a named tick, which no real click can be - so
# the old image is the menu at that tick and the new one the editor, which
# has no clock. Under lockstep the fade's 400 ms is then a fixed number of
# frames later.
if needs editor help editbox editor-select editor-connect star; then
	if wanted star; then
		b5_transition Menu.LevelEditor 36000 star
	else
		b5_click Menu.LevelEditor
	fi
	b5_waitForState GS_LevelEditor
	# The palette's category is in every frame of this group, so the click
	# that picks it is not the editor scene's own: a run asked for editbox
	# alone has to show the palette the full run shows.
	b5_click LevelEditor.Cat1
	if wanted editor; then
		b5_frame editor now
		b5_release
	fi
	if wanted help; then
		b5_click LevelEditor.ShowMenu
		b5_click LevelEditor.MenuPane.Menu.Help
		b5_frame help now
		b5_release
		b5_click LevelEditor.HelpPane.Help.OK
		b5_click LevelEditor.MenuPane.Menu.OK
	fi
	if wanted editbox; then
		# A selection and the caret in the title box; the caret's pulse reads
		# the engine clock, which the frozen frame pins. The title is
		# replaced first: a click lands the caret wherever it was, and the
		# level's default title is a localized string of both languages.
		b5_click LevelEditor.ShowSettings
		b5_click LevelEditor.SettingsPane.Settings.Title
		b5_chord ctrl a
		b5_type "Selected text"
		b5_chord ctrl a
		b5_frame editbox now
		b5_release
		b5_click LevelEditor.SettingsPane.Settings.Cancel
	fi
	if wanted editor-select; then
		# Select mode, a rectangle dragged over tiles (5,5) to (12,9), the
		# cursor left inside the level: the marching ants and the smoothed
		# tile highlight, the two lines the renderer redesign changes.
		b5_click LevelEditor.Mode4
		b5_drag 88 88 200 152
		b5_frame editor-select now
		b5_release
		b5_click LevelEditor.Mode0
	fi
	if wanted editor-connect; then
		# Connection mode over a clock and a light bulb placed from the
		# electronics palette: the clock's output pin clicked as the start of
		# a wire and the bulb's input pin under the cursor, which draws the
		# two pin frames - unsmoothed one-pixel loops at a half-pixel offset,
		# the case the redesign has to reproduce exactly. The palette is
		# drawn, not GUI, so its entries are clicked by coordinate: it starts
		# at (245,428) in 16-pixel cells, and cat4.xml puts the clock at cell
		# (1,2) and the bulb at (4,0). A pin is found within three pixels of
		# its own spot in the part's cell, (15,8) for the clock's output and
		# (7,15) for the bulb's input.
		b5_click LevelEditor.Cat4
		b5_clickAt 269 468
		b5_clickAt 168 232
		b5_clickAt 317 436
		b5_clickAt 408 232
		b5_click LevelEditor.Mode6
		b5_clickAt 175 232
		b5_mouseAt 407 239
		b5_frame editor-connect now
		b5_release
		b5_click LevelEditor.Mode0
	fi
	# The editor asks whether to throw a modified level away.
	b5_click LevelEditor.ShowMenu
	b5_click LevelEditor.MenuPane.Menu.Quit
	b5_dump
	if [ "$(b5_json "el('LevelEditor.MessageBoxPane.MessageBox.Yes')['shown']")" = "True" ]; then
		b5_click LevelEditor.MessageBoxPane.MessageBox.Yes
	fi
	b5_waitForState GS_Menu
fi

# The level scenes. The single-level campaign lists the levels this script
# wrote by title, so index 0 is !1night, 1 is !2plain, 2 is !3lava, 3 is
# !4toxic and 4 is !5hint; the select screen stays on the level that was
# played last, and LEVEL_AT tracks that.
LEVEL_AT=0
goSelect()
{
	b5_click Menu.StartGame
	b5_waitForState GS_SelectLevel
	b5_click SelectLevel.Campaigns
	b5_key End
	LEVEL_AT=0
}
playLevel()   # $1 index, [$2 name of a frame of the crossfade into it]
{
	while [ "$LEVEL_AT" -lt "$1" ]; do
		b5_click SelectLevel.NextLevel
		LEVEL_AT=$((LEVEL_AT + 1))
	done
	# The select screen's preview level ticks, so a crossfade out of it
	# starts from a frozen tick like the star does.
	if [ -n "${2:-}" ]; then b5_transition SelectLevel.PlayLevel 6000 "$2"
	else b5_click SelectLevel.PlayLevel; fi
	b5_waitForState GS_Game
}
quitLevel()
{
	# Escape closes an open hint note before it opens the menu, so a level
	# whose note is up needs a second one.
	b5_key Escape
	b5_dump
	if [ "$(b5_json "el('Game.MenuPane.Menu.Quit')['shown']")" != "True" ]; then b5_key Escape; fi
	b5_click Game.MenuPane.Menu.Quit
	b5_waitForState GS_SelectLevel
}

if needs select cube night plain lava toxic hint; then
	goSelect
	# The select screen has a clock after all: its preview is a level, and
	# the level ticks - with the night vision's noise and the fire's
	# particles in it.
	if wanted select; then b5_frame select 4000; b5_release; fi
	if needs cube night; then
		# The cube is the crossfade into a level from the select screen, frozen
		# 400 ms into its 850: the old image is the select screen with its
		# preview at 6000, and the new one the level at a fixed early tick.
		playLevel 0 $(wanted cube && echo cube)
		if wanted night; then b5_frame night 6000; b5_release; fi
		quitLevel
	fi
	if wanted plain; then playLevel 1; b5_frame plain 3000; b5_release; quitLevel; fi
	# The bolt: under this seed the thunderstorm's flash is at about 15050
	# and the bolt it announces is generated 200 ms later, so at 15300 the
	# bolt stands bright and the flash has all but faded. Probed by shots
	# without a freeze, then frozen at four ticks around it; the counters
	# behind both are drawn from the seeded streams and the bolt fades per
	# tick, so this is the same tick on every run - and another seed moves
	# it, which is what the scene is worth as an oracle.
	if wanted lava; then playLevel 2; b5_frame lava 15300; b5_release; quitLevel; fi
	# Bob stands in the gas from the first tick, so the contamination has
	# long settled at its ceiling by now; earlier than about the second
	# second the harness's own latency has not delivered the freeze yet.
	if wanted toxic; then playLevel 3; b5_frame toxic 3000; b5_release; quitLevel; fi
	# The note opens between the twentieth and the fortieth tick and settles
	# on whole pixels once its ease has run out; 3000 is well past both.
	if wanted hint; then playLevel 4; b5_frame hint 3000; b5_release; quitLevel; fi
	b5_key Escape
	b5_waitForState GS_Menu
fi

# The logo screen, entered again from the menu: the intro's one textured
# quad at its settled size, with the loading line that appears at 2900 under
# it. Released, the state loads what is already loaded and runs on into the
# menu by itself.
if wanted loading; then
	b5_ask "state GS_Loading" >/dev/null
	b5_waitForState GS_Loading
	b5_frame loading 2920
	b5_release
	b5_waitForState GS_Menu
fi

# The ending. "full" asks for it outright, because GS_Credits otherwise reads
# a progress file and the private home has none - so without the word this
# would be the plain version below and the star field would go untested.
#
# The stars draw their own last frame back into the next one, so the picture
# depends on how many frames were rendered and not only on the tick: lockstep
# makes those the same number. The clock starts two seconds before zero and
# the oracle's tick counts from there.
#
# 6000 and not the 3000 this started at, which was the lead-in: the gradient
# and the stars with no text over them at all, so the font laid nothing out
# and the block table was walked for nothing. At 6000 the thanks is up and
# fading in, under the animated charScaling that is the one text in the game
# nothing caches.
if wanted credits; then
	b5_ask "lockstep 1" >/dev/null
	b5_ask "state GS_Credits full" >/dev/null
	b5_waitForState GS_Credits
	b5_frame credits 6000
	b5_release
	b5_ask "lockstep 0" >/dev/null
	b5_ask "state GS_Menu" >/dev/null
	b5_waitForState GS_Menu
fi

# The plain version, which is what a player who has not finished the shipped
# campaign gets: no word, so GS_Credits asks the database and the private home
# answers no. Text on black - the thanks dropped, so this is the programming
# credit, at a charScaling held at 1 and therefore cached.
#
# 4500 where the ending's frame is 6000, and the two are not comparable: this
# version's clock starts at zero rather than two seconds before it, since it
# has no star field to fade up and nothing to establish, so sceneTick begins
# at 2000 and 4500 is two and a half seconds in - the programming credit four
# fifths of the way through its fade, which begins half a second in. Clear of
# the point the fade turns round on, where the frame would sit on a branch.
#
# No lockstep, and that is the point rather than an omission: with nothing
# drawn back out of the last frame the picture belongs to the tick alone, so
# the scene is reproducible without pinning the frame count, and it costs
# seconds where the ending's takes a minute. It also enters the credits a
# second time in the same run, which is what proves onEnter starts from
# nothing.
if wanted credits-plain; then
	b5_ask "state GS_Credits" >/dev/null
	b5_waitForState GS_Credits
	b5_frame credits-plain 4500
	b5_release
	b5_ask "state GS_Menu" >/dev/null
	b5_waitForState GS_Menu
fi

# Every "+ ERROR" the run logged, which is where the renderer reports a record
# that disagrees with what OpenGL is really holding - a hooks build reads the
# state back after every draw. A wrong record is a wrong picture somewhere
# nobody was looking, and a message in a log nobody reads is not a check.
echo
grep -q "ERROR" "$B5_OUT/run.log" \
	&& b5_note "ERROR in the log: $(grep -m3 ERROR "$B5_OUT/run.log" | tr '\n' ' ')" \
	|| b5_ok "no error line in the log"

echo "frames in $B5_OUTDIR"
b5_stop
b5_finish
