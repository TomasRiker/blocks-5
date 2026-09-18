#!/bin/bash
# drag.sh - does dragging a character to a tile walk it there, and does it
# stop the moment the button comes up?
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/drag.sh
#
# The one test that reads the level rather than the GUI: a drag steers the
# field, so no widget can be asked whether it worked, and the picture cannot
# be asked either - it rains in level 1, so two frames differ by a million
# pixels whether anybody walked or not. The hook reports the active
# character's cell instead.
set -u
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/harness.sh"

b5_cell() { b5_dump; b5_json "d['player']"; }

trap b5_stop EXIT
b5_start
b5_waitForState GS_Menu

b5_dump
if [ "$(b5_json "el('Menu.CrtPane.Crt.NoThanks')['shown']")" = "True" ]; then
	b5_click Menu.CrtPane.Crt.NoThanks
fi

b5_click Menu.StartGame
b5_waitForState GS_SelectLevel
b5_click SelectLevel.PlayLevel
b5_waitForState GS_Game
sleep 2

start=$(b5_cell)
b5_ok "the active character is at $start"
[ "$start" = "[-1, -1]" ] && b5_note "nobody is active - the drag has nothing to steer"

# Press on the character's own cell, so the drag begins with nowhere to go,
# then pull the cursor to a tile well to the left and hold it there. The
# character should walk towards that tile for as long as the button is down.
cx=$(b5_json "d['player'][0] * 16 + 8")
cy=$(b5_json "d['player'][1] * 16 + 8")
b5_mouseAt "$cx" "$cy"
xdotool mousedown 1
sleep 0.3
b5_mouseAt $((cx - 96)) "$cy"
sleep 1.5

held=$(b5_cell)
b5_ok "while the button is held it has reached $held"
[ "$held" = "$start" ] && b5_note "the drag did not move the character at all"

# Let go, and look twice. Anything still queued in the action buffer would be
# played out over the next second, which is exactly what a pulsed key rather
# than a held one would leave behind.
xdotool mouseup 1
atRelease=$(b5_cell)
sleep 1.5
settled=$(b5_cell)

if [ "$atRelease" = "$settled" ]; then
	b5_ok "it stopped where the button came up, at $settled"
else
	b5_note "it walked on after the release: $atRelease then $settled"
fi

# The walk is one axis at a time, so a leg along x may not drift in y.
if [ "$(b5_json "d['player'][1]")" = "$(echo "$start" | sed 's/.*, //; s/\]//')" ]; then
	b5_ok "the row never changed - the leg stayed on one axis"
else
	b5_note "the row changed during a leg along x"
fi
b5_finish
