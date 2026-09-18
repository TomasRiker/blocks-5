#!/bin/bash
# drag.sh - the two mouse gestures on the field: dragging a character to the
# tile it should walk to, and clicking on what it is standing next to.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/drag.sh
#
# The one test that reads the level rather than the GUI: these gestures steer
# the field, so no widget can be asked whether they worked. The hook reports
# the active character's cell and whether the lights are out, and every
# assertion below is one of those two.
#
# It plays a level of its own, for the same reason frames.sh writes its
# scenes: the geometry *is* the test. A wall the drag has to walk round, a
# switch beside where the character starts and another out of reach, and a
# panel under its feet - in a shipped level all of that would be whatever
# happened to be near the start, and an assertion about it would be a
# statement about level 1 rather than about the gesture.
set -u
B5_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# A private home like frames.sh's: the level below has to be the first entry
# in the single-levels list, and the developer's own levels, saves and
# progress are neither read nor written.
export XDG_DATA_HOME="${B5_DRAG_XDG:-/tmp/blocks5-drag-xdg}"
B5_PRIVATE_HOME="$XDG_DATA_HOME/blocks5"
rm -rf "$XDG_DATA_HOME"
mkdir -p "$B5_PRIVATE_HOME/levels"
printf 0 > "$B5_PRIVATE_HOME/.update_checker"
printf 1.2.0 > "$B5_PRIVATE_HOME/.initialized"

#  7         10 11   13        <- the column the wall stands in, then the
#  M                              character, the switch it can reach and the
#  M          B  S     S          one it cannot; P is the panel below it.
#  M          P
#
# The wall is five rows tall and open above and below, so a leg walking west
# is stopped by it while a leg walking north is not - which is the whole
# point: the drag has to notice the first and hand over to the second.
python3 - "$B5_PRIVATE_HOME/levels/drag.xml" <<'PY'
import sys
path = sys.argv[1]
W, H = 40, 25
def tiles(y):
    row = ['M' if (x in (0, W - 1) or y in (0, H - 1)) else ' ' for x in range(W)]
    if 8 <= y <= 12: row[7] = 'M'
    return ''.join(row)
rows = lambda f: ''.join('<Row>%s</Row>' % f(y) for y in range(H))
objects = ('<Object type="Player" x="10" y="10" character="0" active="1"/>'
           '<Object type="LightSwitch" x="11" y="10"/>'
           '<Object type="LightSwitch" x="13" y="10"/>'
           '<Object type="LightPanel" x="10" y="11" subType="0"/>')
head = ('<?xml version="1.0" ?><Level title="!drag" '
        'skin0="" skin1="" skin2="" skin3="" skin4="" skin5="" skin6="" '
        'skin7="" skin8="" skin9="" skin10="" width="40" height="25" '
        'numLayers="2" numDiamondsNeeded="0" electricityOn="1" '
        'nightVision="0" raining="0" clouds="0" snowing="0" thunderstorm="0" '
        'lightColorR="255" lightColorG="255" lightColorB="255" '
        'musicFilename="">')
open(path, 'w', encoding='latin-1').write(
    head + '<Layer>' + rows(lambda y: ' ' * W) + '</Layer>'
         + '<Layer>' + rows(tiles) + '</Layer>' + objects + '</Level>')
PY

source "$B5_HERE/harness.sh"

b5_cell()  { b5_dump; b5_json "d['player']"; }
b5_dark()  { b5_dump; b5_json "d['nightVision']"; }
b5_pixel() { echo $(( $1 * 16 + 8 )); }

# Press, pull the cursor to a cell and hold it there. The character walks
# towards that cell for as long as the button is down, so the hold is the
# gesture and not a wait for it: a step is 80 ms once the repeat is going.
b5_dragTo()   # $1 from cell x, $2 from cell y, $3 to cell x, $4 to cell y, $5 hold
{
	b5_mouseAt "$(b5_pixel "$1")" "$(b5_pixel "$2")"
	xdotool mousedown 1
	sleep 0.3
	b5_mouseAt "$(b5_pixel "$3")" "$(b5_pixel "$4")"
	sleep "$5"
}

trap b5_stop EXIT
b5_start
b5_waitForState GS_Menu

b5_dump
if [ "$(b5_json "el('Menu.CrtPane.Crt.NoThanks')['shown']")" = "True" ]; then
	b5_click Menu.CrtPane.Crt.NoThanks
fi

# The single-levels campaign is the last one, and this level's title sorts in
# front of the forty-two campaign sources the game folder also contributes.
b5_click Menu.StartGame
b5_waitForState GS_SelectLevel
b5_click SelectLevel.Campaigns
b5_key End
b5_click SelectLevel.PlayLevel
b5_waitForState GS_Game
sleep 2

start=$(b5_cell)
if [ "$start" = "[10, 10]" ] && [ "$(b5_dark)" = "False" ]; then
	b5_ok "the character is at $start with the lights on"
else
	b5_note "the level did not start as written: $start, dark $(b5_dark)"
fi

# A switch three cells away is not within reach of a walk, so a click on it
# is not within reach either.
b5_clickAt "$(b5_pixel 13)" "$(b5_pixel 10)"
if [ "$(b5_dark)" = "False" ]; then
	b5_ok "a click on a switch out of reach did nothing"
else
	b5_note "a switch three cells away was worked by a click"
fi

# A panel is walked onto, never into: the character can go there, so the
# click is not a step and works nothing.
b5_clickAt "$(b5_pixel 10)" "$(b5_pixel 11)"
if [ "$(b5_dark)" = "False" ]; then
	b5_ok "a click on the panel beside the character did nothing"
else
	b5_note "the panel was triggered from beside it"
fi

# The switch next door is exactly what a walk into it would reach.
b5_clickAt "$(b5_pixel 11)" "$(b5_pixel 10)"
if [ "$(b5_dark)" = "True" ]; then
	b5_ok "a click on the switch next door put the lights out"
else
	b5_note "the switch next door was not worked by a click"
fi
[ "$(b5_cell)" = "[10, 10]" ] || b5_note "the click moved the character to $(b5_cell)"

# And again, which only works because the click goes through move(): the
# 20-tick lockout it sets has to have run out first.
sleep 0.6
b5_clickAt "$(b5_pixel 11)" "$(b5_pixel 10)"
if [ "$(b5_dark)" = "False" ]; then
	b5_ok "clicking it again brought them back"
else
	b5_note "the second click did not reach the switch"
fi

# A drag that does not begin on a character steers nobody.
b5_dragTo 20 4 20 12 1.5
xdotool mouseup 1; sleep 1.0
if [ "$(b5_cell)" = "[10, 10]" ]; then
	b5_ok "a drag from empty ground moved nobody"
else
	b5_note "a drag from empty ground walked the character to $(b5_cell)"
fi

# West into the wall, with nothing to the north or south of the cursor to go
# on with: the character walks up to the wall and stops there. Were the key
# commanded anyway it would lean on the wall for as long as the button is
# held, which is what the second leg below could never then get past.
b5_dragTo 10 10 2 10 2.0
xdotool mouseup 1; sleep 1.0
blocked=$(b5_cell)
if [ "$blocked" = "[8, 10]" ]; then
	b5_ok "it walked up to the wall and stopped at $blocked"
else
	b5_note "expected the wall to stop it at [8, 10], not $blocked"
fi

# Now north-west, from right in front of the wall. The leg along x is blocked
# in its first step, so the leg along y takes over, and when that has run out
# the way west is open above the wall. Two straight legs and a corner.
b5_dragTo 8 10 4 6 3.0
held=$(b5_cell)
xdotool mouseup 1
atRelease=$(b5_cell)
sleep 1.5
settled=$(b5_cell)

if [ "$held" = "[4, 6]" ]; then
	b5_ok "a blocked leg handed over to the other axis and it arrived at $held"
else
	b5_note "it did not get round the wall to [4, 6]: $held"
fi

# Anything still queued in the action buffer would be played out over the next
# second, which is what a pulsed key rather than a held one leaves behind.
if [ "$atRelease" = "$settled" ]; then
	b5_ok "it stopped where the button came up, at $settled"
else
	b5_note "it walked on after the release: $atRelease then $settled"
fi

# The panel again, this time stood on: it is what makes the click on it above
# a real negative rather than a panel that does nothing at all.
b5_dragTo 4 6 10 11 3.0
xdotool mouseup 1; sleep 1.0
if [ "$(b5_cell)" = "[10, 11]" ] && [ "$(b5_dark)" = "True" ]; then
	b5_ok "walked onto the panel, and standing on it put the lights out"
else
	b5_note "the panel was not reached or not triggered: $(b5_cell), dark $(b5_dark)"
fi
b5_finish
