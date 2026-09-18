#!/bin/bash
# drag.sh - does a mouse drag actually walk a character?
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/drag.sh
#
# The one thing the GUI harness cannot answer by asking: a drag steers the
# level, not a widget, so what it proves has to be read off the picture. It
# drags the active character a long way to the right and compares the two
# screenshots - a character that walked moves its own pixels, and one whose
# binding never reached the action layer moves none.
set -u
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/harness.sh"

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

# Where the character stands before and after. Read as a number and not off
# the picture: it rains in this level, so a frame differs from the one before
# it by a million pixels whether anybody walked or not.
b5_dump
from=$(b5_json "d['player']")
b5_ok "the active character is at $from"
[ "$from" = "[-1, -1]" ] && b5_note "no character is active - the drag has nothing to steer"

# Press on open ground and pull left, well past the six-pixel threshold and
# far enough that the direction cannot be read as the other axis. It may start
# anywhere: the drag steers whoever is active, as the keyboard does.
b5_drag 320 200 140 200
b5_dump
to=$(b5_json "d['player']")
b5_ok "and afterwards at $to"

if [ "$from" = "$to" ]; then
	b5_note "the drag did not move the character at all"
else
	b5_ok "the drag walked the character"
fi
b5_finish
