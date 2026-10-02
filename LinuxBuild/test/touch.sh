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
# beside its bar, a tap between two buttons, a tap on the level beside one -
# and a second start without the finger shows that a mouse is as exact as it
# ever was.
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
start()   # $1 the name of the run
{
	echo
	echo "=== $1"
	rm -rf "$XDG_DATA_HOME"
	mkdir -p "$B5_PRIVATE_HOME/levels"
	printf 1.2.0 > "$B5_PRIVATE_HOME/.initialized"
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
# What the picker says a finger at x y presses, as "name x y moved|stays".
expectTouch()   # x y expected-name moved|stays what
{
	local got
	got=$(b5_ask "touch $1 $2")
	set -- "$1" "$2" "$3" "$4" "$5" $got
	[ "$6" = "$3" ] && [ "$9" = "$4" ] && b5_ok "$5: the finger presses $3 ($4)" \
		|| b5_note "$5: the finger presses $6 ($9), not $3 ($4)"
}
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
	b5_ask "touchsweep $grid" > "$B5_OUT/sweep-$name.txt"
	verdict=$(python3 - "$B5_OUT/dump.json" "$B5_OUT/sweep-$name.txt" "$pane" "$keeper" <<'PY'
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

# The menu's round buttons are hit inside their disc only (inset 8 of an 80x80
# cell), so the cell's corner is a near miss.
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
expectTouch $((bx - 4)) $((by + bh / 2)) OptionsPane.Options.UpdateCheck moved "4 pixels left of the update check's box"
pressAt $((bx - 4)) $((by + bh / 2))
b5_dump
[ "$(b5_json "el('OptionsPane.Options.UpdateCheck')['checked']")" != "$before" ] \
	&& b5_ok "and a press there ticks it" || b5_note "a press beside the box did not tick it"

# A greyed-out button under the finger keeps the press: nothing beside it gets
# a tap that was on it.
read rx ry rw rh <<< "$(b5_json "' '.join(map(str, el('OptionsPane.Options.ResetSelected')['rect']))")"
[ "$(b5_json "el('OptionsPane.Options.ResetSelected')['active']")" = False ] || b5_note "ResetSelected is not greyed out"
expectTouch $((rx + 2)) $((ry + rh / 2)) OptionsPane.Options.ResetSelected stays "a greyed-out button's edge"

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
b5_click OptionsPane.Options.Cancel

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
expectTouch 120 402 LevelEditor moved "the toolbar 3 pixels under the level, no button near"
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
b5_stop

# --- 2. a mouse ------------------------------------------------------------------
unset B5_FINGER
start mouse
pressAt 471 55
[ "$(shown OptionsPane.Options)" = False ] && b5_ok "a mouse in the corner of the options button's cell misses it" \
	|| b5_note "a mouse that missed the options button opened them"
b5_stop

b5_finish
