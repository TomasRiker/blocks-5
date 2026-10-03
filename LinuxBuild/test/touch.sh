#!/bin/bash
# touch.sh - a finger is not a point: GUI::pickTouchTarget, which moves a
# finger's press that just missed onto what it meant, and the hold that keeps
# it there until the finger lets go.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/touch.sh
#
# There is no finger here, so the harness lends the game one: B5_FINGER makes
# every press of a hooks build a finger's, and its value is the game pixels
# per reference pixel - 1 gives a reach of 16 game pixels and a margin of 4,
# whatever the size of the window.
#
# Two kinds of check. Sweeps ask the picker about every few pixels of a screen
# ("touchsweep") and hold every answer against rules that must never break:
# a hit on a control stays a hit on it, a press on the level stays on the
# level, and a moved press lands on what it picked, within reach and never
# behind a pane. Real presses through SDL then try what the rules are for - a
# near miss, a finger that wobbles and one that slides away, a slider dragged
# beside its bar, a near miss of a greyed-out button, a finger landing under a
# title bar, a tap between two buttons, a tap on the level beside one, a held
# button covered by a menu. A list is dragged, tapped, flicked and caught, and
# a multi-line edit box dragged and tapped. And a last start without the
# finger shows that a mouse is as exact as it ever was, and still selects on
# the press and by dragging over a text.
set -u
B5_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export XDG_DATA_HOME="${B5_TOUCH_XDG:-/tmp/blocks5-touch-xdg}"
B5_PRIVATE_HOME="$XDG_DATA_HOME/blocks5"
export B5_DISPLAY="${B5_DISPLAY:-:86}"
SHOTS="${B5_SHOTS:-/tmp/blocks5-touch}"

source "$B5_HERE/harness.sh"
rm -rf "$SHOTS"
mkdir -p "$SHOTS"
trap b5_stop EXIT

# --- starting and stopping ---------------------------------------------------
start()   # $1 the name of the run, [$2 a function that writes its levels]
{
	echo
	echo "=== $1"
	rm -rf "$XDG_DATA_HOME"
	mkdir -p "$B5_PRIVATE_HOME/levels"
	printf 1.2.0 > "$B5_PRIVATE_HOME/.initialized"
	[ -n "${2:-}" ] && "$2"
	B5_OUT="$SHOTS/$1"
	b5_start
	b5_waitForState GS_Menu
	settle
}
# A transition moves everything it shows, and a sweep or a press during one
# would be about the transition.
settle()
{
	local i
	for i in $(seq 1 40); do
		b5_dump && [ "$(b5_json "d['crossfade']")" = "-1" ] && return 0
		sleep 0.5
	done
}

# --- pressing ------------------------------------------------------------------
# A press and release at a game coordinate with a rest between, as b5_clickAt
# does, with the button to use.
pressAt()   # x y [button]
{
	b5_mouseAt "$1" "$2"
	xdotool mousedown "${3:-1}"; sleep 0.4; xdotool mouseup "${3:-1}"; sleep 1.5
}
# A press at one point, moved to a second and released there.
pressMoveRelease()   # x0 y0 x1 y1
{
	b5_mouseAt "$1" "$2"; xdotool mousedown 1; sleep 0.4
	b5_mouseAt "$3" "$4"; sleep 0.4; xdotool mouseup 1; sleep 1.5
}
# What the picker says a finger at x y presses, as "name x y moved|stays",
# and where in it when that is given.
expectTouch()   # x y expected-name moved|stays what [landing-x landing-y]
{
	local got name lx ly how
	got=$(b5_ask "touch $1 $2")
	read -r name lx ly how <<< "$got"
	if [ -z "$how" ]; then
		b5_note "$5: no answer from the picker"
	elif [ "$name" != "$3" ] || [ "$how" != "$4" ]; then
		b5_note "$5: the finger presses $name ($how), not $3 ($4)"
	elif [ -n "${6:-}" ] && { [ "$lx" != "$6" ] || [ "$ly" != "$7" ]; }; then
		b5_note "$5: the finger presses $name at ($lx,$ly), not at ($6,$7)"
	else
		b5_ok "$5: the finger presses $3 ($4${6:+ at $6,$7})"
	fi
}
# A finger lands where it lands, its press bringing the position along: the
# move and the press in one xdotool call reach the game in one tick, as a
# touch's do. pressAt moves first and waits, which no finger does.
landAt()   # x y
{
	local wx wy
	b5_dump || b5_hookFailed
	b5_clientOrigin
	wx=$(b5_json "d['present'][0] + int(($1 + 0.5) * d['present'][2] / d['screen'][2])")
	wy=$(b5_json "d['present'][1] + int(($2 + 0.5) * d['present'][3] / d['screen'][3])")
	xdotool mousemove $((B5_CX + wx)) $((B5_CY + wy)) mousedown 1; sleep 0.4; xdotool mouseup 1; sleep 1
}
rect() { b5_dump; b5_json "' '.join(map(str, el('$1')['rect']))"; }
shown() { b5_dump; b5_json "el('$1')['shown']"; }
# Escape closes the options only where they are open; in the bare menu it
# would quit the game.
closeOptions() { [ "$(shown OptionsPane.Options)" = True ] && b5_key Escape; }

# --- the sweep ---------------------------------------------------------------
# "touchsweep" answers a line a point: x, y, what a mouse would hit there, what
# the finger presses, the point it presses at, and what a mouse would hit at
# that point. Held against the tree in the dump. A press is moved where the
# point changes, also within one element - off a window's body onto its title
# bar. Optional: a pane nothing behind may be picked from, and an element
# whose own area above a given y keeps every press - a level.
sweep()   # name "x0 y0 x1 y1 step" [pane] [element:y]
{
	local name=$1 grid=$2 pane=${3:-} keeper=${4:-} verdict counts
	b5_dump || b5_hookFailed
	# Thousands of picks in one tick: the options' 9945 points take five to
	# six seconds on a slow machine, past what an ask waits by default.
	b5_ask "touchsweep $grid" 30 > "$B5_OUT/sweep-$name.txt"
	verdict=$(python3 - "$B5_OUT/dump.json" "$B5_OUT/sweep-$name.txt" "$pane" "$keeper" "$grid" <<'PY'
import json, math, sys
d = json.load(open(sys.argv[1]))
pane, keeper = sys.argv[3], sys.argv[4]
keeperName, keeperBelow = (keeper.split(':')[0], int(keeper.split(':')[1])) if keeper else ('', 0)
els = {e['path']: e for e in d['elements']}
controls = {'GUI_Button', 'GUI_CheckBox', 'GUI_RadioButton', 'GUI_ListBox',
            'GUI_EditBox', 'GUI_MultiLineEditBox', 'GUI_ScrollBar'}
reach = 16 + 0.75    # a ring's points are rounded to whole pixels
points = moved = 0
problems = []
for line in open(sys.argv[2]):
    f = line.split()
    if len(f) != 7:
        continue
    x, y, under, picked, lx, ly, landed = int(f[0]), int(f[1]), f[2], f[3], int(f[4]), int(f[5]), f[6]
    points += 1
    at = '(%d,%d)' % (x, y)
    # A press moves to another element, or within one: off a window's body
    # onto its title bar, off the editor's toolbar onto its level.
    if picked == under and (lx, ly) == (x, y):
        continue
    moved += 1
    if els.get(under, {}).get('type') in controls:
        problems.append('%s is on the control %s and was moved to %s' % (at, under, picked))
    if keeperName and under == keeperName and y < keeperBelow:
        problems.append('%s is on %s and was moved to %s' % (at, under, picked))
    e = els.get(picked)
    if e is None or not e['shown'] or not e['active']:
        problems.append('%s was moved to %s, not an enabled element on screen' % (at, picked))
    if landed != picked:
        problems.append('%s was moved to %s but is pressed on %s' % (at, picked, landed))
    if math.hypot(lx - x, ly - y) > reach:
        problems.append('%s was moved %.1f pixels, out of reach' % (at, math.hypot(lx - x, ly - y)))
    if pane and not picked.startswith(pane):
        problems.append('%s was moved to %s, behind %s' % (at, picked, pane))
x0, y0, x1, y1, step = map(int, sys.argv[5].split())
expected = ((x1 - x0) // step + 1) * ((y1 - y0) // step + 1)
if points != expected:
    problems.append('%d of %d points answered' % (points, expected))
print(points, moved, len(problems))
for p in problems[:8]:
    print('    ' + p)
PY
)
	counts=($(echo "$verdict" | head -1))
	if [ "${counts[0]:-0}" -gt 0 ] && [ "${counts[2]:-1}" -eq 0 ]; then
		b5_ok "sweep $name: ${counts[0]} points, ${counts[1]} moved, every rule kept"
	else
		b5_note "sweep $name: ${counts[*]:-no answer} (points, moved, broken rules)"
		echo "$verdict" | tail -n +2
	fi
}

# --- 1. a finger -----------------------------------------------------------------
export B5_FINGER=1
start finger

sweep menu "0 0 639 479 8"

# The menu's round buttons are hit on the square their disc fills, inset 8 in
# an 80x80 cell, so the cell's corner is a near miss.
expectTouch 471 55 Menu.Options moved "a corner of the options button's cell"
pressAt 471 55
[ "$(shown OptionsPane.Options)" = True ] && b5_ok "a near miss opens the options" \
	|| b5_note "a near miss did not open the options"
closeOptions

# A finger that wobbles while it presses keeps its button; one that slides out
# of reach lets go of it, as a mouse dragged off a button does.
pressMoveRelease 471 55 466 57
[ "$(shown OptionsPane.Options)" = True ] && b5_ok "a press that wobbles within reach still opens them" \
	|| b5_note "a wobble within reach lost the press"
closeOptions
pressMoveRelease 471 55 420 55
[ "$(shown OptionsPane.Options)" = False ] && b5_ok "a press that slides away does not" \
	|| b5_note "a press that slid out of reach still opened the options"
closeOptions

# --- the options dialog: small controls, labels, a greyed-out button ---
b5_click Menu.Options
b5_expectShown OptionsPane.Options
read ox oy ow oh <<< "$(b5_json "' '.join(map(str, el('OptionsPane.Options')['rect']))")"
sweep options "$ox $oy $((ox + ow - 1)) $((oy + oh - 1)) 4"

# A window's body takes no press and its title bar does, so a finger just
# under the bar is moved onto it - within one element, which the sweep's
# rules cannot tell from a press left where it was.
expectTouch $((ox + 160)) $((oy + 24)) OptionsPane.Options moved "4 pixels under the options' title bar"

read bx by bw bh <<< "$(b5_json "' '.join(map(str, el('OptionsPane.Options.UpdateCheck')['rect']))")"
before=$(b5_json "el('OptionsPane.Options.UpdateCheck')['checked']")
expectTouch $((bx + bw / 2)) $((by + bh + 3)) OptionsPane.Options.UpdateCheck moved "4 pixels under the update check's box"
pressAt $((bx + bw / 2)) $((by + bh + 3))
b5_dump
[ "$(b5_json "el('OptionsPane.Options.UpdateCheck')['checked']")" != "$before" ] \
	&& b5_ok "and a press there ticks it" || b5_note "a press beside the box did not tick it"

# Left of the box the greyed-out Reset selected is 7 pixels off, within the
# margin of the box's 4: a greyed-out target counts as near as it is, so that
# finger is torn between two and gets nothing.
expectTouch $((bx - 4)) $((by + bh / 2)) OptionsPane.Options stays "4 pixels left of the box, 7 right of the greyed-out Reset selected"

# A greyed-out button under the finger keeps the press: nothing beside it gets
# a tap that was on it.
read rx ry rw rh <<< "$(b5_json "' '.join(map(str, el('OptionsPane.Options.ResetSelected')['rect']))")"
[ "$(b5_json "el('OptionsPane.Options.ResetSelected')['active']")" = False ] || b5_note "ResetSelected is not greyed out"
expectTouch $((rx + 2)) $((ry + rh / 2)) OptionsPane.Options.ResetSelected stays "a greyed-out button's edge"

# The nearest target is what the finger meant, greyed out or not: a near miss
# of a greyed-out button does nothing, as a press on it does, and never slides
# onto the button beside it - here Reset all, which would reset every key.
expectTouch $((rx + rw / 2)) $((ry + rh)) OptionsPane.Options stays "1 pixel under the greyed-out button, 5 above Reset all"

# A slider taken hold of from beside its bar ends where one taken on it does:
# the press lands right across from the finger, and the gesture after it is
# moved by as much, so a finger drifting up and down beside the bar does not
# move the value. Each drag starts with the knob sent to the far end, then
# presses the track at x+80 - which centres the knob there - and goes on
# through x+100 to x+120.
volume() { b5_dump; b5_json "el('OptionsPane.Options.SoundVolume')['scroll']"; }
read sx sy sw sh <<< "$(b5_json "' '.join(map(str, el('OptionsPane.Options.SoundVolume')['rect']))")"
dragAlong()   # y at x+80, at x+100, at x+120
{
	pressAt $((sx + sw - 20)) $((sy + sh / 2))
	b5_mouseAt $((sx + 80)) "$1"; xdotool mousedown 1; sleep 0.4
	b5_mouseAt $((sx + 100)) "$2"; sleep 0.4
	b5_mouseAt $((sx + 120)) "$3"; sleep 0.4
	xdotool mouseup 1; sleep 1
}
expectTouch $((sx + 80)) $((sy - 5)) OptionsPane.Options.SoundVolume moved "5 pixels above the sound volume's track"
dragAlong $((sy + sh / 2)) $((sy + sh / 2)) $((sy + sh / 2)); on=$(volume)
dragAlong $((sy - 5)) $((sy - 2)) $((sy - 9)); beside=$(volume)
[ "$beside" = "$on" ] && b5_ok "a slider dragged beside its bar ends where one dragged on it does ($on)" \
	|| b5_note "a slider dragged beside its bar ended at $beside, one dragged on it at $on"

# A finger lands with no move before it, so its press arrives together with the
# jump from wherever the last one lifted. A window taking that press on its
# title bar must not take the jump for a drag. Escape and not Cancel after it,
# which a window that did jump may have taken off the screen.
before=$(rect OptionsPane.Options)
landAt $((ox + 160)) $((oy + 24))
[ "$(rect OptionsPane.Options)" = "$before" ] && b5_ok "a finger landing under the title bar leaves the window where it stood" \
	|| b5_note "a finger landing under the title bar moved the window from $before to $(rect OptionsPane.Options)"
closeOptions

# --- the Manager: a pane over the menu, buttons in rows with narrow gaps ---
b5_click Menu.Manager
b5_expectShown Menu.ManagerPane.Manager
settle
sweep manager "0 0 639 479 8" Menu.ManagerPane

kind() { b5_dump; b5_json "[e['path'].split('.')[-1] for e in d['elements'] if e['path'].startswith('Menu.ManagerPane.Manager.Kind') and e.get('checked')]"; }
read kx ky kw kh <<< "$(b5_json "' '.join(map(str, el('Menu.ManagerPane.Manager.KindLevel')['rect']))")"
read cx cy cw ch <<< "$(b5_json "' '.join(map(str, el('Menu.ManagerPane.Manager.KindCampaign')['rect']))")"
before=$(kind)
gap=$((kx + kw + (cx - kx - kw) / 2))
expectTouch $gap $((ky + kh / 2)) Menu.ManagerPane.Manager stays "the middle of the gap between two kinds"
pressAt $gap $((ky + kh / 2))
[ "$(kind)" = "$before" ] && b5_ok "a tap between two kinds picks neither" \
	|| b5_note "a tap between two kinds changed $before to $(kind)"
expectTouch $((cx + cw / 2)) $((cy + ch + 3)) Menu.ManagerPane.Manager.KindCampaign moved "3 pixels under the campaigns"
pressAt $((cx + cw / 2)) $((cy + ch + 3))
[ "$(kind)" = "['KindCampaign']" ] && b5_ok "and a press there picks the campaigns" \
	|| b5_note "a near miss under the campaigns picked $(kind)"

b5_dump
read lx ly lw lh <<< "$(b5_json "' '.join(map(str, el('Menu.ManagerPane.Manager.Close')['rect']))")"
expectTouch $((lx + lw / 2)) $((ly + lh + 4)) Menu.ManagerPane.Manager.Close moved "4 pixels under Close"
pressAt $((lx + lw / 2)) $((ly + lh + 4))
[ "$(shown Menu.ManagerPane)" = False ] && b5_ok "and a press there closes the Manager" \
	|| b5_note "a near miss under Close left the Manager open"

# --- the level editor: a level right above a row of buttons ---
b5_click Menu.LevelEditor
b5_waitForState GS_LevelEditor
settle
sweep editor "0 376 639 479 3" "" LevelEditor:400

depth() { b5_dump; b5_json "d['undo']"; }
steps=$(depth)
# On the level, six pixels over the undo button: the level keeps it, and the
# right button there rubs out the frame's wall.
expectTouch 399 398 LevelEditor stays "the level's last row over the undo button"
pressAt 399 398 3
[ "$(depth)" -eq $((steps + 1)) ] && b5_ok "a press there works the level, not the undo button" \
	|| b5_note "a press on the level over the undo button left $(depth) undo steps, from $steps"

# The strip of toolbar between the level and its buttons takes no press, so
# a finger on it with no button near is moved up onto the level.
steps=$(depth)
expectTouch 120 402 LevelEditor moved "the toolbar 3 pixels under the level, no button near" 120 399
pressAt 120 402 3
[ "$(depth)" -eq $((steps + 1)) ] && b5_ok "and a press there works the level" \
	|| b5_note "a press on the toolbar under the level left $(depth) undo steps, from $steps"

# Between the level, the minus and the plus, all about as near: nobody's.
b5_click LevelEditor.NumDiamondsNeeded+
steps=$(depth)
read mx my mw mh <<< "$(b5_json "' '.join(map(str, el('LevelEditor.NumDiamondsNeeded+')['rect']))")"
expectTouch $mx $((my - 3)) LevelEditor stays "the corner between the level, minus and plus"
pressAt $mx $((my - 3))
[ "$(depth)" -eq "$steps" ] && b5_ok "a tap there changes nothing" \
	|| b5_note "a tap between the level, minus and plus left $(depth) undo steps, from $steps"

# The rest of a moved press's gesture is moved by as much (GUI::pointFor): a
# stroke started on the toolbar under the level is moved up onto its bottom
# row, and rubs that row out as the finger slides along the toolbar, where
# the finger's own points lie off the level and would paint nothing after the
# first cell. Read off the game's own frame, cells 8 to 11 of row 24.
b5_ask "shot $B5_OUT/stroke-before.png" > /dev/null
b5_mouseAt 100 402; xdotool mousedown 3; sleep 0.4
b5_mouseAt 140 402; sleep 0.3
b5_mouseAt 180 402; sleep 0.4
xdotool mouseup 3; sleep 1
b5_ask "shot $B5_OUT/stroke-after.png" > /dev/null
rubbed=$(python3 - "$B5_HERE/../../WebBuild" "$B5_OUT/stroke-before.png" "$B5_OUT/stroke-after.png" <<'PY2'
import sys
sys.path.insert(0, sys.argv[1])
from make_icon import read_png
(w, h, a), (_, _, b) = read_png(sys.argv[2]), read_png(sys.argv[3])
def cell(px, cx):
    return [px[((24 * 16 + y) * w + cx * 16 + x) * 4:((24 * 16 + y) * w + cx * 16 + x) * 4 + 3]
            for y in range(2, 14) for x in range(2, 14)]
print(' '.join(str(cx) for cx in range(8, 12) if cell(a, cx) != cell(b, cx)))
PY2
)
[ "$rubbed" = "8 9 10 11" ] && b5_ok "a stroke started under the level rubs out its bottom row along the finger" \
	|| b5_note "a stroke started under the level changed cells '$rubbed' of 8 to 11 on its bottom row"

# A button held under a still finger lets go once something covers it, as it
# does for a mouse: Escape opens the editor's menu over the toolbar while the
# finger rests on Undo, and lifting it then must undo nothing.
steps=$(depth)
read ux uy uw uh <<< "$(rect LevelEditor.Undo)"
b5_mouseAt $((ux + uw / 2)) $((uy + uh / 2)); xdotool mousedown 1; sleep 0.4
# Not b5_key: its --clearmodifiers lets go of a held mouse button for the
# length of the key and presses it again after, a release the game takes for
# the finger's own.
xdotool keydown Escape; sleep 0.06; xdotool keyup Escape; sleep 0.6
covered=$(shown LevelEditor.MenuPane)
xdotool mouseup 1; sleep 1
[ "$covered" = True ] || b5_note "Escape did not open the editor's menu over the held button"
[ "$(depth)" -eq "$steps" ] && b5_ok "a held undo button the menu then covers does not fire" \
	|| b5_note "a held undo button the menu then covered fired: $(depth) undo steps, from $steps"
b5_stop

# --- 2. the game: a level is a target, and a press moved onto it acts there ---
# A level of its own, as drag.sh writes one: the second character stands on
# the bottom row, where the frame is open for it, right above the status bar.
writeLevel()
{
	python3 - "$B5_PRIVATE_HOME/levels/touch.xml" <<'PY2'
import sys
W, H = 40, 25
def tiles(y):
    row = ['M' if (x in (0, W - 1) or y in (0, H - 1)) else ' ' for x in range(W)]
    if y == H - 1: row[10] = ' '
    return ''.join(row)
rows = lambda f: ''.join('<Row>%s</Row>' % f(y) for y in range(H))
objects = ('<Object type="Player" x="5" y="5" character="0" active="1"/>'
           '<Object type="Player" x="10" y="24" character="1" active="0"/>')
head = ('<?xml version="1.0" ?><Level title="!touch" '
        'skin0="" skin1="" skin2="" skin3="" skin4="" skin5="" skin6="" '
        'skin7="" skin8="" skin9="" skin10="" width="40" height="25" '
        'numLayers="2" numDiamondsNeeded="0" electricityOn="1" '
        'nightVision="0" raining="0" clouds="0" snowing="0" thunderstorm="0" '
        'lightColorR="255" lightColorG="255" lightColorB="255" '
        'musicFilename="">')
open(sys.argv[1], 'w', encoding='latin-1').write(
    head + '<Layer>' + rows(lambda y: ' ' * W) + '</Layer>'
         + '<Layer>' + rows(tiles) + '</Layer>' + objects + '</Level>')
PY2
}
start game writeLevel
b5_click Menu.StartGame
b5_waitForState GS_SelectLevel
b5_click SelectLevel.Campaigns
b5_key End
b5_click SelectLevel.PlayLevel
b5_waitForState GS_Game
sleep 2
cell() { b5_dump; b5_json "d['player']"; }
[ "$(cell)" = "[5, 5]" ] || b5_note "the level did not start as written: $(cell)"
# Three pixels under the second character, on the status bar: the press lands
# on the level's bottom row, and the game acts where it landed - a finger's
# own cell would be row 25, outside the level, where nothing is.
expectTouch 168 402 Game moved "the status bar 3 pixels under the second character" 168 399
pressAt 168 402
[ "$(cell)" = "[10, 24]" ] && b5_ok "and a press there takes hold of that character" \
	|| b5_note "a press under the second character left $(cell) the active one"
b5_stop

# --- 3. a list and a text: a finger drags them -------------------------------
# A finger has no wheel, so it scrolls a list by dragging it: the list follows
# once the finger has moved further than a tap may (8 game pixels here),
# selects nothing on the way, and glides on when let go of while moving. Its
# press selects on release, as a tap. The campaign editor's list of levels,
# made long with levels written for it, so that a glide has room to run, and
# a double tap there adds a level, which the other list counts.
writeLevels()
{
	local i
	for i in $(seq -w 1 160); do
		cp "$B5_HERE/../../Blocks5/levels/example01.xml" "$B5_PRIVATE_HOME/levels/list$i.xml"
	done
}
start list writeLevels
b5_click Menu.CampaignEditor
b5_waitForState GS_CampaignEditor
settle
L=CampaignEditor.AvailableLevels
list() { b5_dump; b5_json "'%d %d %d' % tuple(el('$L')[k] for k in ('scroll', 'selection', 'items'))"; }
# How often the selection has changed: a gesture that selected on the press
# and put the selection back would end where it began, and show only here.
changes() { b5_dump; b5_json "el('$L')['changes']"; }
read ax ay aw ah <<< "$(rect $L)"
line=$(b5_json "el('$L')['lineHeight']")
x=$((ax + 60))
# The item a press at y means, with the list scrolled by s.
itemAt() { echo $(( ($1 - ay - 2 + $2) / line )); }
# A finger put down at x y0, slid through the given ys and lifted after a rest
# there, so that it lifts still and nothing glides.
slide()   # y0 y...
{
	b5_mouseAt $x "$1"; xdotool mousedown 1; sleep 0.4
	shift
	local y
	for y in "$@"; do b5_mouseAt $x "$y"; done
	sleep 0.6; xdotool mouseup 1; sleep 1
}

read s sel n <<< "$(list)"
[ "$s $sel" = "0 -1" ] || b5_note "the list did not start at its top with nothing selected: $s $sel"
[ "$n" -gt 200 ] || b5_note "the list holds $n levels, not the 160 written and the shipped ones"
ch=$(changes)
slide $((ay + 150)) $((ay + 125)) $((ay + 100)) $((ay + 75)) $((ay + 50))
read s sel n <<< "$(list)"
[ "$s" -eq 92 ] && b5_ok "a finger dragged up 100 pixels scrolls the list 92, from the edge of the slop" \
	|| b5_note "a finger dragged up 100 pixels scrolled the list to $s, not 92"
[ "$sel $(changes)" = "-1 $ch" ] && b5_ok "and selects nothing, not even for a moment" \
	|| b5_note "a drag left item $sel selected, the selection having changed $(( $(changes) - ch )) times"

# Clamped at an end, the list follows the finger back at once, not after it
# has made up the way it went past: 92 down past the top, then 30 back.
slide $((ay + 40)) $((ay + 77)) $((ay + 115)) $((ay + 152)) $((ay + 190)) $((ay + 175)) $((ay + 160))
read s sel n <<< "$(list)"
[ "$s" -eq 30 ] && b5_ok "dragged down past the top and back up 30, the list comes back 30" \
	|| b5_note "dragged down past the top and back up 30, the list stands at $s"

want=$(itemAt $((ay + 60)) "$s")
pressAt $x $((ay + 60))
read s sel n <<< "$(list)"
[ "$sel" -eq "$want" ] && b5_ok "a tap selects the item under the finger ($want)" \
	|| b5_note "a tap selected item $sel, not $want"

# Within the slop a finger still taps; slid past it sideways, it neither taps
# nor drags, the list being one that scrolls up and down.
want=$(itemAt $((ay + 100)) "$s")
b5_mouseAt $x $((ay + 100)); xdotool mousedown 1; sleep 0.4
b5_mouseAt $((x + 5)) $((ay + 103)); sleep 0.4; xdotool mouseup 1; sleep 1
read s2 sel n <<< "$(list)"
[ "$sel $s2" = "$want $s" ] && b5_ok "one that wobbles 6 pixels, within the slop, still taps ($want)" \
	|| b5_note "a wobble within the slop left item $sel selected at $s2, not $want at $s"
ch=$(changes)
b5_mouseAt $x $((ay + 140)); xdotool mousedown 1; sleep 0.4
b5_mouseAt $((x + 30)) $((ay + 140)); sleep 0.6; xdotool mouseup 1; sleep 1
read s2 sel2 n <<< "$(list)"
[ "$sel2 $s2 $(changes)" = "$sel $s $ch" ] && b5_ok "one slid 30 pixels sideways neither taps nor scrolls" \
	|| b5_note "a slide sideways left item $sel2 selected at $s2, not $sel at $s"

# The list keeps the gesture when the finger leaves it: 80 up, the last 50
# above its top edge, out of the finger's reach.
read s sel n <<< "$(list)"
slide $((ay + 30)) $((ay + 10)) $((ay - 10)) $((ay - 30)) $((ay - 50))
read s2 sel2 n <<< "$(list)"
[ "$s2" -eq $((s + 72)) ] && b5_ok "a finger dragged on past the list's top edge keeps scrolling it" \
	|| b5_note "a finger dragged past the list's top edge scrolled it from $s to $s2, not $((s + 72))"

# Let go of while moving, the list glides on and comes to rest by itself; a
# press stops it where it is and selects nothing - it only caught the list.
# The flick goes through xdotool alone, a twentieth of a second a step, with
# the release on the last move as a flick lifts, and one logic tick a frame
# (lockstep), so that every tick of it sees where the finger is: in a frame
# that runs several, only the first does, and a finger read still for three
# ticks has stopped before it lifted.
b5_ask "lockstep 1" > /dev/null
b5_dump; b5_clientOrigin
pt() { b5_json "'%d %d' % ($B5_CX + d['present'][0] + int(($1 + 0.5) * d['present'][2] / d['screen'][2]), $B5_CY + d['present'][1] + int(($2 + 0.5) * d['present'][3] / d['screen'][3]))"; }
p0=$(pt $x $((ay + 170))); p1=$(pt $x $((ay + 150))); p2=$(pt $x $((ay + 130)))
p3=$(pt $x $((ay + 110))); p4=$(pt $x $((ay + 90)))
# 80 up in four steps; the list follows 72 of it before it is let go.
flick()
{
	xdotool mousemove $p0 mousedown 1; sleep 0.4
	xdotool mousemove $p1; sleep 0.05; xdotool mousemove $p2; sleep 0.05; xdotool mousemove $p3; sleep 0.05
	xdotool mousemove $p4 mouseup 1
}
read s sel n <<< "$(list)"
flick
sleep 0.2; read g1 x1 x2 <<< "$(list)"
sleep 0.2; read g2 x1 x2 <<< "$(list)"
[ "$g1" -gt $((s + 72)) ] && [ "$g2" -gt "$g1" ] \
	&& b5_ok "let go while moving, the list glides on past where the finger left it ($((s + 72)), $g1, $g2)" \
	|| b5_note "let go while moving, the list stood at $g1 and $g2, the finger having left it at $((s + 72))"
# Under lockstep a tick is a frame, so how long the glide takes is the
# renderer's business: asked until two answers half a second apart agree.
r2=-1
for i in $(seq 1 40); do
	sleep 0.5; r1=$r2; read r2 x1 x2 <<< "$(list)"
	[ "$r1" = "$r2" ] && break
done
[ "$r1" = "$r2" ] && [ "$r2" -gt "$g2" ] && b5_ok "and comes to rest by itself ($r2)" \
	|| b5_note "the gliding list stood at $r1 and then $r2, having been at $g2"
# The same flick again, caught a tenth of a second after it lifts: the list
# holds still under the finger, short of where the first came to rest by
# itself - by half that glide at least, or nothing showed it was still going.
glide=$((r2 - s - 72))
read s sel n <<< "$(list)"
ch=$(changes)
flick
sleep 0.1; xdotool mousedown 1
sleep 0.2; read h1 x1 x2 <<< "$(list)"
sleep 0.4; read h2 x1 x2 <<< "$(list)"
xdotool mouseup 1; sleep 1
read c csel x2 <<< "$(list)"
[ "$h1" -gt $((s + 72)) ] && [ "$h1 $c" = "$h2 $h2" ] && [ $((c - s - 72)) -lt $((glide / 2)) ] \
	&& b5_ok "a finger put on the gliding list stops it there ($((c - s - 72)) of a $glide-pixel glide) and holds it" \
	|| b5_note "a finger put on the gliding list held it at $h1, then $h2, and it stood at $c after, $((c - s - 72)) of a $glide-pixel glide"
[ "$csel $(changes)" = "$sel $ch" ] && b5_ok "and selects nothing when it lifts" \
	|| b5_note "the press that stopped the glide selected item $csel, where $sel was"
b5_ask "lockstep 0" > /dev/null

# Two quick taps on an item are a double click: the list's submit button,
# here Add, which moves the level into the campaign's list.
added() { b5_dump; b5_json "el('CampaignEditor.CampaignLevels')['items']"; }
before=$(added)
b5_mouseAt $x $((ay + 40))
xdotool mousedown 1; sleep 0.1; xdotool mouseup 1; sleep 0.1
xdotool mousedown 1; sleep 0.1; xdotool mouseup 1; sleep 1.5
[ "$(added)" -eq $((before + 1)) ] && b5_ok "a double tap adds the level, as a double click does" \
	|| b5_note "a double tap left the campaign with $(added) levels, from $before"

# On a slow frame the first tap's lift and the second's touch arrive in one
# tick, the lift first. Sent in one xdotool call, they do here.
before=$(added)
xdotool mousedown 1; sleep 0.1; xdotool mouseup 1 mousedown 1; sleep 0.1; xdotool mouseup 1; sleep 1.5
[ "$(added)" -eq $((before + 1)) ] && b5_ok "so does one whose first lift and second touch come in one tick" \
	|| b5_note "a double tap whose first lift and second touch came in one tick left $(added) levels, from $before"

# A tap held on the list while a pane opens over it taps nothing when it
# lifts, as a held button lets go once covered: the campaign has changed, so
# Escape asks whether to quit. Escape without b5_key, whose --clearmodifiers
# lets go of the held button.
read s sel n <<< "$(list)"
ch=$(changes)
b5_mouseAt $x $((ay + 120)); xdotool mousedown 1; sleep 0.4
xdotool keydown Escape; sleep 0.06; xdotool keyup Escape; sleep 0.6
covered=$(shown CampaignEditor.MessageBoxPane)
xdotool mouseup 1; sleep 1
read s2 sel2 n <<< "$(list)"
[ "$covered" = True ] || b5_note "Escape did not ask whether to quit over the list"
[ "$sel2 $(changes)" = "$sel $ch" ] && b5_ok "a tap held on the list while a pane covers it selects nothing" \
	|| b5_note "a tap held on the list while a pane covered it selected item $sel2, where $sel was"
b5_click CampaignEditor.MessageBoxPane.MessageBox.No

# A multi-line edit box pans the same way, up and down: the campaign's
# description, given twelve lines of seven characters each - "lineNN" and
# the break - and its caret put at the top. Dragged up 100 it scrolls 92,
# its caret stays and nothing is selected; a tap then puts the caret at the
# start of the line under the finger and moves nothing.
fillText()   # element
{
	local i
	b5_click "$1"
	b5_chord ctrl a
	# Line by line, the break as a key of its own: xdotool types a newline
	# in its text as nothing the game takes for Return.
	for i in $(seq -w 1 12); do
		xdotool type --delay 120 "line$i"
		[ "$i" = 12 ] || { xdotool keydown Return; sleep 0.06; xdotool keyup Return; sleep 0.2; }
	done
	sleep 1
	b5_chord ctrl Home
}
text() { b5_dump; b5_json "'%d %d %d %d' % (el('$1')['scroll'][1], el('$1')['caret'], el('$1')['selected'][0], el('$1')['selected'][1])"; }
D=CampaignEditor.Description
fillText $D
read tx ty tw th <<< "$(rect $D)"
read s caret s0 s1 <<< "$(text $D)"
[ "$s $caret $s0 $s1" = "0 0 0 0" ] || b5_note "the description did not stand at its top with the caret there: $s $caret $s0 $s1"
# From above the horizontal scroll bar, which takes the box's bottom 16 rows.
b5_mouseAt $((tx + 40)) $((ty + 60)); xdotool mousedown 1; sleep 0.4
for y in 35 10 -15 -40; do b5_mouseAt $((tx + 40)) $((ty + y)); done
sleep 0.6; xdotool mouseup 1; sleep 1
read s caret s0 s1 <<< "$(text $D)"
[ "$s" -eq 92 ] && b5_ok "a finger dragged up 100 pixels in a multi-line edit box scrolls it 92" \
	|| b5_note "a finger dragged up 100 pixels in a multi-line edit box scrolled it to $s, not 92"
[ "$caret $s0 $s1" = "0 0 0" ] && b5_ok "and moves no caret and selects no text" \
	|| b5_note "a finger's drag in a multi-line edit box left the caret at $caret and $s0..$s1 selected"
lh=$(b5_json "el('$D')['lineHeight']")
k=$(( (16 - 2 + s + lh - 1) / lh ))
pressAt $((tx + 3)) $((ty + 2 + k * lh - s + lh / 2))
read s2 caret s0 s1 <<< "$(text $D)"
[ "$caret $s2" = "$((7 * k)) $s" ] && b5_ok "a tap puts the caret at the start of the line tapped ($caret)" \
	|| b5_note "a tap on line $k put the caret at $caret with the text at $s2, not $((7 * k)) at $s"

# A finger's tap on a text field asks for the touch keyboard, which Windows
# shows (TouchKeyboard) and the dump reports everywhere as touchKeyboard. The
# focus leaving the text fields sends it away, and a drag asks for none,
# though it takes the focus along.
kb() { b5_dump; b5_json "d['touchKeyboard']"; }
read ex ey ew eh <<< "$(rect CampaignEditor.Title)"
pressAt $((ex + 20)) $((ey + eh / 2))
[ "$(kb)" = True ] && b5_ok "a finger's tap on the title asks for the touch keyboard" \
	|| b5_note "a finger's tap on the title did not ask for the touch keyboard"
pressAt $((ax + 60)) $((ay + 10))
[ "$(kb)" = False ] && b5_ok "a tap on the list, which takes no text, sends it away" \
	|| b5_note "a tap on the list left the touch keyboard asked for"
# Sized to its text, so the middle of that and not of its rect.
read lx ly s s <<< "$(rect CampaignEditor.Static4)"
read lw lh <<< "$(b5_json "' '.join(map(str, el('CampaignEditor.Static4')['text']))")"
pressAt $((lx + lw / 2)) $((ly + lh / 2))
b5_dump
[ "$(b5_json "d['touchKeyboard']") $(b5_json "d['focus']")" = "True CampaignEditor.Title" ] \
	&& b5_ok "a tap on the title's label asks for it as well" \
	|| b5_note "a tap on the title's label: touch keyboard $(b5_json "d['touchKeyboard']"), the focus on $(b5_json "d['focus']")"
pressAt $((ax + 60)) $((ay + 10))
b5_mouseAt $((tx + 40)) $((ty + 50)); xdotool mousedown 1; sleep 0.4
for y in 35 20 5; do b5_mouseAt $((tx + 40)) $((ty + y)); done
sleep 0.6; xdotool mouseup 1; sleep 1
b5_dump
[ "$(b5_json "d['touchKeyboard']") $(b5_json "d['focus']")" = "False $D" ] \
	&& b5_ok "a drag on the description asks for none, though the focus went there" \
	|| b5_note "after a drag on the description the touch keyboard is $(b5_json "d['touchKeyboard']"), the focus on $(b5_json "d['focus']")"
pressAt $((tx + 40)) $((ty + 20))
[ "$(kb)" = True ] && b5_ok "and a tap on it does" \
	|| b5_note "a tap on the description did not ask for the touch keyboard"
b5_stop

# --- 4. a mouse ------------------------------------------------------------------
unset B5_FINGER
start mouse
pressAt 471 55
[ "$(shown OptionsPane.Options)" = False ] && b5_ok "a mouse in the corner of the options button's cell misses it" \
	|| b5_note "a mouse that missed the options button opened them"
# Not vacuous: the mouse was there, and a press of it on the button does open
# them.
b5_dump
[ "$(b5_json "d['cursor']")" = "[471, 55]" ] || b5_note "the mouse was at $(b5_json "d['cursor']"), not at 471,55"
read px py pw ph <<< "$(rect Menu.Options)"
pressAt $((px + pw / 2)) $((py + ph / 2))
[ "$(shown OptionsPane.Options)" = True ] && b5_ok "and a mouse on the button opens them, so its presses arrive" \
	|| b5_note "a mouse on the options button did not open them"

# A mouse's press on a list selects at once, and a mouse dragged over the list
# does not scroll it: the gesture that does is a finger's.
A=OptionsPane.Options.Actions
read qx qy qw qh <<< "$(rect $A)"
want=$(( (20 - 2) / $(b5_json "el('$A')['lineHeight']") ))
b5_mouseAt $((qx + 40)) $((qy + 20)); xdotool mousedown 1; sleep 0.4
b5_dump; pressed=$(b5_json "el('$A')['selection']")
b5_mouseAt $((qx + 40)) $((qy + 60)); sleep 0.4; xdotool mouseup 1; sleep 1
b5_dump
[ "$pressed" -eq "$want" ] && b5_ok "a mouse's press on a list selects item $want while still held" \
	|| b5_note "a mouse's press on a list had item $pressed selected while held, not $want"
[ "$(b5_json "el('$A')['scroll']")" -eq 0 ] && b5_ok "and dragging the mouse over the list does not scroll it" \
	|| b5_note "dragging the mouse over the list scrolled it to $(b5_json "el('$A')['scroll']")"
closeOptions

# And a mouse dragged over the lines of a multi-line edit box selects them, as
# it always did, and scrolls nothing.
b5_click Menu.CampaignEditor
b5_waitForState GS_CampaignEditor
settle
fillText $D
[ "$(kb)" = False ] && b5_ok "a mouse's click on a text field asks for no touch keyboard" \
	|| b5_note "a mouse's click on a text field asked for the touch keyboard"
read tx ty tw th <<< "$(rect $D)"
b5_mouseAt $((tx + 40)) $((ty + 10)); xdotool mousedown 1; sleep 0.4
b5_mouseAt $((tx + 40)) $((ty + 40)); sleep 0.4; xdotool mouseup 1; sleep 1
read s caret s0 s1 <<< "$(text $D)"
[ "$s0" -lt "$s1" ] && [ "$s" -eq 0 ] && b5_ok "a mouse dragged over a multi-line edit box selects $s0..$s1 and scrolls nothing" \
	|| b5_note "a mouse dragged over a multi-line edit box selected $s0..$s1 with the text at $s"
b5_stop

b5_finish
