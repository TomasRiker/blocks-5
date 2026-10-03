#!/bin/bash
# drag.sh - the two mouse gestures on the field: dragging a character to the
# tile it should walk to, and clicking on what it is standing next to; and
# what one press does while the game is paused or a hint note is open.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/drag.sh
#
# The one test that reads the level rather than the GUI: these gestures steer
# the field, so no widget can be asked whether they worked. The hook reports
# the active character's cell, whether the lights are out, whether a note is
# open on that cell and whether the game is paused, and every assertion below
# is one of those.
#
# It plays two levels of its own, for the same reason frames.sh writes its
# scenes: the geometry *is* the test. A wall the drag has to walk round, a
# switch beside where the character starts and another out of reach, a panel
# under its feet, and in the second level a note beside the start with a switch
# next to it - in a shipped level all of that would be whatever happened to be
# near the start, and an assertion about it would be a statement about level 1
# rather than about the gesture.
set -u
B5_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# A private home like frames.sh's: the level below has to be the first entry
# in the single-levels list, and the developer's own levels, saves and
# progress are neither read nor written.
export XDG_DATA_HOME="${B5_DRAG_XDG:-/tmp/blocks5-drag-xdg}"
B5_PRIVATE_HOME="$XDG_DATA_HOME/blocks5"
rm -rf "$XDG_DATA_HOME"
mkdir -p "$B5_PRIVATE_HOME/levels"
printf 1.2.0 > "$B5_PRIVATE_HOME/.initialized"

#  row 3:      #    ##M      <- a loose block, then two against a wall
#  row 4:  MMMMMMMMMMM        <- the floor they stand on, or they would fall
#
#  7         10 11   13        <- the column the wall stands in, then the
#  M                              character, the switch it can reach and the
#  M          B  S     S          one it cannot; P is the panel below it.
#  M          P
#
# The wall is five rows tall and open above and below, so a leg walking west
# is stopped by it while a leg walking north is not - which is the whole
# point: the drag has to notice the first and hand over to the second. The lane
# up at rows 3 and 4 asks the same question of objects rather than tiles: one
# block gives way, a chain of them against a wall does not, and only a drag
# that walks the chain can tell those apart before it leans on it. Pushing the
# loose block east is what builds that chain, so the first test sets up the
# second. The floor under them is not decoration: a block with nothing beneath
# it falls out of the lane before the character ever arrives, and the test then
# passes on an empty row - which is how the first draft of it lied.
python3 - "$B5_PRIVATE_HOME/levels/drag.xml" <<'PY'
import sys
path = sys.argv[1]
W, H = 40, 25
def tiles(y):
    row = ['M' if (x in (0, W - 1) or y in (0, H - 1)) else ' ' for x in range(W)]
    if 8 <= y <= 12: row[7] = 'M'
    if y == 4: row[14:25] = ['M'] * 11
    if y == 3: row[23] = 'M'
    return ''.join(row)
rows = lambda f: ''.join('<Row>%s</Row>' % f(y) for y in range(H))
objects = ('<Object type="Player" x="10" y="10" character="0" active="1"/>'
           # A second character, parked in a corner well away from everything
           # the drag assertions touch. It is here for the Tab check at the
           # foot of this file, which needs somebody to switch to.
           '<Object type="Player" x="35" y="20" character="1" active="0"/>'
           '<Object type="LightSwitch" x="11" y="10"/>'
           '<Object type="LightSwitch" x="13" y="10"/>'
           '<Object type="LightPanel" x="10" y="11" subType="0"/>'
           '<Object type="Block" x="16" y="3"/>'
           '<Object type="Block" x="21" y="3"/>'
           '<Object type="Block" x="22" y="3"/>')
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

# The second level, played after the first: a character one step west of a
# hint note, with a switch north of the note, so that a click with the note
# open has something it would work.
#
#  row  9:            S         <- the switch, beside the note
#  row 10:           BN        <- the character and the note it walks onto
#
# Its title sorts straight after the first one's, which is how NextLevel finds
# it on the select screen.
python3 - "$B5_PRIVATE_HOME/levels/note.xml" <<'PY'
import sys
path = sys.argv[1]
W, H = 40, 25
border = lambda y: ''.join('M' if (x in (0, W - 1) or y in (0, H - 1)) else ' ' for x in range(W))
rows = lambda f: ''.join('<Row>%s</Row>' % f(y) for y in range(H))
objects = ('<Object type="Player" x="9" y="10" character="0" active="1"/>'
           '<Object type="Hint" x="10" y="10"><Text><![CDATA[A note to be put away.]]></Text></Object>'
           '<Object type="LightSwitch" x="10" y="9"/>')
head = ('<?xml version="1.0" ?><Level title="!note" '
        'skin0="" skin1="" skin2="" skin3="" skin4="" skin5="" skin6="" '
        'skin7="" skin8="" skin9="" skin10="" width="40" height="25" '
        'numLayers="2" numDiamondsNeeded="0" electricityOn="1" '
        'nightVision="0" raining="0" clouds="0" snowing="0" thunderstorm="0" '
        'lightColorR="255" lightColorG="255" lightColorB="255" '
        'musicFilename="">')
open(path, 'w', encoding='latin-1').write(
    head + '<Layer>' + rows(lambda y: ' ' * W) + '</Layer>'
         + '<Layer>' + rows(border) + '</Layer>' + objects + '</Level>')
PY

source "$B5_HERE/harness.sh"

b5_cell()  { b5_dump; b5_json "d['player']"; }
b5_dark()  { b5_dump; b5_json "d['nightVision']"; }
b5_pixel() { echo $(( $1 * 16 + 8 )); }
b5_noteOpen() { b5_dump; b5_json "d['note']"; }
b5_paused()   { b5_dump; b5_json "d['paused']"; }
b5_menuOpen() { b5_dump; b5_json "el('Game.MenuPane')['shown']"; }

# Tap a key bound to an action: held long enough for a tick's snapshot of the
# keyboard to see it, and let go well inside the 240 ms after which a held
# movement key starts to repeat.
b5_tapKey() { xdotool keydown "$1"; sleep 0.15; xdotool keyup "$1"; sleep 1.0; }

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
# Up into the block lane, and then east: the loose block has room behind it, so
# the character pushes it along and walks on. A chain test that broke an
# ordinary push would be no use.
b5_dragTo 10 11 14 3 3.5
xdotool mouseup 1; sleep 1.0
atLane=$(b5_cell)
[ "$atLane" = "[14, 3]" ] || b5_note "could not reach the block lane: $atLane"
b5_dragTo 14 3 19 3 3.5
xdotool mouseup 1; sleep 1.0
pushed=$(b5_cell)
if [ "$pushed" = "[19, 3]" ]; then
	b5_ok "it pushed the loose block ahead of it and walked on to $pushed"
else
	b5_note "the push did not carry the character along: $pushed"
fi

# That push left three blocks in a row against the wall at x = 23, and no
# character can shift those. The leg east is over before it starts, so the leg
# north takes it round - where a drag that only looks one cell deep sees a
# block that "can be pushed" and leans on it until the button comes up.
b5_dragTo 19 3 24 1 3.5
xdotool mouseup 1; sleep 1.0
around=$(b5_cell)
if [ "$around" = "[24, 1]" ]; then
	b5_ok "a chain of blocks with a wall behind it ended the leg, and it got round to $around"
else
	b5_note "the blocked chain did not hand over to the other axis: $around"
fi
# --- Switching characters: once per press ------------------------------------
# $A_SWITCH_CHARACTER repeated on the action defaults - 240 ms and then every
# 80 - so holding Tab cycled the active character for as long as it was held,
# about two and a half times a second once GS_Game's own 400 ms throttle had
# had its say. With two characters that is a flicker; with three it is a
# lottery. The throttle is gone with the repeat, and with it a hardcoded
# SDLK_TAB in the key handler that cleared it - which read the key and not the
# action, so it did nothing at all for anybody who had rebound the switch.
#
# Held, the switch must happen once and stay. Tapped, it must happen every
# time, which is what the throttle's removal has to not have broken: three
# taps from the first character have to end on the second.
# Compared against where the other character stands and not against a cell
# written down here: the first one has walked a long way by this point in the
# file, and an assertion naming its starting cell would pass whatever Tab did.
before=$(b5_cell)
xdotool keydown Tab; sleep 0.4
held=$(b5_cell)
steady=yes
for i in 1 2 3 4; do sleep 0.3; [ "$(b5_cell)" = "$held" ] || steady=no; done
xdotool keyup Tab; sleep 0.5
if [ "$held" != "$before" ] && [ "$steady" = yes ]; then
	b5_ok "Tab held switched once, from $before to $held, and stayed there"
else
	b5_note "Tab held did not switch once and stay: $before -> $held, steady=$steady"
fi

# Three taps is an odd number of switches, so with two characters the active
# one has to be the other one again at the end.
for i in 1 2 3; do xdotool keydown Tab; sleep 0.3; xdotool keyup Tab; sleep 0.3; done
after=$(b5_cell)
if [ "$after" != "$held" ]; then
	b5_ok "and three taps each counted, ending back on $after"
else
	b5_note "three taps did not all count: still on $after"
fi

# --- The hint note and the pause: one press, one thing -----------------------
# Paused, any key or click only ends the pause. With a note open, a click or
# Return, Escape or Space only puts the note away; any other key is the game's,
# and an arrow walks off the field, which closes the note on the way. The next
# press acts. The engine's own keys never count - F11 photographs the pause and
# leaves it - and nor does a modifier nothing is bound to, Alt being half of
# Alt+Return and Alt+Tab. Every check reads the dump's note, paused and player,
# and the second level's switch, which a click from the note's field would work.
b5_key Escape
b5_dump
[ "$(b5_json "el('Game.MenuPane.Menu.Quit')['shown']")" = True ] || b5_key Escape
b5_click Game.MenuPane.Menu.Quit
b5_waitForState GS_SelectLevel
b5_click SelectLevel.NextLevel
b5_click SelectLevel.PlayLevel
b5_waitForState GS_Game
sleep 2

# Off the note's field and back onto it by drag: a key could take a second step
# on a machine slow enough to see it held past the repeat delay, where the drag
# stops on the cell under the cursor. Leaving counts as much as arriving, since
# a note put away stays away until the character has left its field.
b5_reopenNote()
{
	local x y
	[ "$(b5_noteOpen)" = True ] && b5_key Escape
	b5_dump
	x=$(b5_json "d['player'][0]"); y=$(b5_json "d['player'][1]")
	if [ "$x" = 10 ] && [ "$y" = 10 ]; then
		b5_dragTo 10 10 12 10 1.5; xdotool mouseup 1; sleep 0.5
		x=12; y=10
	fi
	b5_dragTo "$x" "$y" 10 10 2.0; xdotool mouseup 1; sleep 1.5
	if [ "$(b5_cell)" != "[10, 10]" ] || [ "$(b5_noteOpen)" != True ]; then
		b5_note "the note did not open again: $(b5_cell), note $(b5_noteOpen)"
	fi
}

# What a check that failed may have left open is closed again, so that the
# checks after it still start from the field.
b5_closeMenu()
{
	[ "$(b5_menuOpen)" = True ] && b5_click Game.MenuPane.Menu.Continue
	return 0
}

# The switch north of the note, and the lights put back on after a check that
# may have put them out. The pause between two clicks is the one the first
# level's checks explain: a click goes through move(), and its lockout has to
# have run out.
b5_clickSwitch() { b5_clickAt "$(b5_pixel 10)" "$(b5_pixel 9)"; }
b5_lightsOn()    { sleep 0.6; [ "$(b5_dark)" = True ] && b5_clickSwitch; return 0; }
b5_shots()       { ls "$B5_PRIVATE_HOME/screenshots"/*.png 2>/dev/null | wc -l; }

if [ "$(b5_cell)" = "[9, 10]" ] && [ "$(b5_noteOpen)" = False ]; then
	b5_ok "the second level started beside the note, with the note shut"
else
	b5_note "the second level did not start as written: $(b5_cell), note $(b5_noteOpen)"
fi

b5_tapKey Right; sleep 0.5
if [ "$(b5_cell)" = "[10, 10]" ] && [ "$(b5_noteOpen)" = True ]; then
	b5_ok "with nothing open a key walks: onto the note, which opened"
else
	b5_note "walking onto the note: $(b5_cell), note $(b5_noteOpen)"
fi

b5_tapKey Right
if [ "$(b5_cell)" = "[11, 10]" ]; then
	b5_ok "an arrow with the note open walked off the field, which put the note away"
else
	b5_note "an arrow with the note open: $(b5_cell) - expected a step to [11, 10]"
fi

for key in space Return; do
	b5_reopenNote
	b5_tapKey "$key"
	if [ "$(b5_cell)" = "[10, 10]" ] && [ "$(b5_noteOpen)" = False ]; then
		b5_ok "$key put the note away"
	else
		b5_note "$key with the note open: $(b5_cell), note $(b5_noteOpen) - expected [10, 10] and shut"
	fi
done

# A click on the switch beside the note: the note goes, the switch stays as it
# is, and only the next click works it.
b5_reopenNote
b5_clickSwitch
if [ "$(b5_noteOpen)" = False ] && [ "$(b5_dark)" = False ]; then
	b5_ok "a click with the note open put it away and worked nothing"
else
	b5_note "a click on the switch with the note open: note $(b5_noteOpen), dark $(b5_dark) - expected shut and lit"
fi
sleep 0.6
b5_clickSwitch
if [ "$(b5_dark)" = True ]; then
	b5_ok "and the next click worked the switch"
else
	b5_note "the click after the note was put away did not work the switch"
fi
b5_lightsOn

# A press on the character with the note open takes no hold of it.
b5_reopenNote
b5_dragTo 10 10 20 10 1.5
xdotool mouseup 1; sleep 1.0
if [ "$(b5_cell)" = "[10, 10]" ] && [ "$(b5_noteOpen)" = False ]; then
	b5_ok "a press on the character with the note open put it away and dragged nobody"
else
	b5_note "a drag begun with the note open: $(b5_cell), note $(b5_noteOpen) - expected [10, 10] and shut"
fi

# Escape puts it away and opens no menu. F11 before it photographs the note and
# leaves it open.
b5_reopenNote
shots=$(b5_shots)
b5_hold F11
if [ "$(b5_shots)" -gt "$shots" ] && [ "$(b5_noteOpen)" = True ]; then
	b5_ok "F11 photographed the open note and left it open"
else
	b5_note "F11 over the note: note $(b5_noteOpen), screenshots $shots before, $(b5_shots) after"
fi
b5_key Escape
if [ "$(b5_noteOpen)" = False ] && [ "$(b5_menuOpen)" = False ]; then
	b5_ok "Escape put the note away and opened no menu"
else
	b5_note "Escape with the note open: note $(b5_noteOpen), menu $(b5_menuOpen)"
fi
b5_closeMenu

# Paused with the note open, which is where one press used to do three things
# at once. focusblip pauses as coming back from another window does. A key
# only ends the pause, and the next is the game's again.
b5_reopenNote
b5_ask focusblip > /dev/null || b5_hookFailed
sleep 0.5
if [ "$(b5_paused)" = True ] && [ "$(b5_noteOpen)" = True ]; then
	b5_ok "paused with the note open"
else
	b5_note "focusblip did not pause over the open note: paused $(b5_paused), note $(b5_noteOpen)"
fi
b5_tapKey Right
if [ "$(b5_paused)" = False ] && [ "$(b5_noteOpen)" = True ] && [ "$(b5_cell)" = "[10, 10]" ]; then
	b5_ok "the first key only ended the pause"
else
	b5_note "the first key after the pause: paused $(b5_paused), note $(b5_noteOpen), at $(b5_cell)"
fi
b5_tapKey Right
if [ "$(b5_cell)" = "[11, 10]" ]; then
	b5_ok "and the next walked off the field"
else
	b5_note "the key after the pause: at $(b5_cell) - expected a step to [11, 10]"
fi

# A click, in the order the three come in: the pause ends, then the note goes,
# then the switch is worked.
b5_reopenNote
b5_ask focusblip > /dev/null || b5_hookFailed
sleep 0.5
b5_clickSwitch
if [ "$(b5_paused)" = False ] && [ "$(b5_noteOpen)" = True ] && [ "$(b5_dark)" = False ]; then
	b5_ok "the first click only ended the pause"
else
	b5_note "the first click after the pause: paused $(b5_paused), note $(b5_noteOpen), dark $(b5_dark)"
fi
b5_clickSwitch
if [ "$(b5_noteOpen)" = False ] && [ "$(b5_dark)" = False ]; then
	b5_ok "the second only put the note away"
else
	b5_note "the second click: note $(b5_noteOpen), dark $(b5_dark)"
fi
sleep 0.6
b5_clickSwitch
if [ "$(b5_dark)" = True ]; then
	b5_ok "and the third worked the switch"
else
	b5_note "the third click did not work the switch"
fi
b5_lightsOn

# The pause by itself: the engine's keys and Alt leave it, Escape and the Menu
# button only end it, and a press on the character takes no hold. Off the
# note's field first: on it, an open note could be what kept the menu shut,
# and the check would not be about the pause.
b5_dragTo 10 10 12 10 1.5; xdotool mouseup 1; sleep 1.0
[ "$(b5_cell)" = "[12, 10]" ] || b5_note "could not step off the note's field: $(b5_cell)"
b5_ask focusblip > /dev/null || b5_hookFailed
sleep 0.5
shots=$(b5_shots)
b5_hold F11
if [ "$(b5_paused)" = True ] && [ "$(b5_shots)" -gt "$shots" ]; then
	b5_ok "F11 photographed the pause and left it"
else
	b5_note "F11 while paused: paused $(b5_paused), screenshots $shots before, $(b5_shots) after"
fi
xdotool keydown Alt_L; sleep 0.06; xdotool keyup Alt_L; sleep 1.0
if [ "$(b5_paused)" = True ]; then
	b5_ok "so did Alt, which nothing is bound to"
else
	b5_note "Alt on its own ended the pause"
fi
b5_ask focusblip > /dev/null || b5_hookFailed
sleep 0.5
b5_key Escape
if [ "$(b5_paused)" = False ] && [ "$(b5_menuOpen)" = False ]; then
	b5_ok "Escape only ended the pause"
else
	b5_note "Escape while paused: paused $(b5_paused), menu $(b5_menuOpen)"
fi
b5_closeMenu

b5_ask focusblip > /dev/null || b5_hookFailed
sleep 0.5
b5_click Game.ShowMenu
if [ "$(b5_paused)" = False ] && [ "$(b5_menuOpen)" = False ]; then
	b5_ok "a click on the Menu button only ended the pause"
else
	b5_note "the Menu button while paused: paused $(b5_paused), menu $(b5_menuOpen)"
fi
b5_closeMenu

b5_ask focusblip > /dev/null || b5_hookFailed
sleep 0.5
b5_dump
x=$(b5_json "d['player'][0]"); y=$(b5_json "d['player'][1]"); before=$(b5_cell)
b5_dragTo "$x" "$y" $((x + 8)) "$y" 1.5
xdotool mouseup 1; sleep 1.0
if [ "$(b5_paused)" = False ] && [ "$(b5_cell)" = "$before" ]; then
	b5_ok "a press on the character only ended the pause, and dragged nobody"
else
	b5_note "a drag begun while paused: paused $(b5_paused), from $before to $(b5_cell)"
fi

# Held on, the key that ended the pause goes into a walk once the repeat delay
# is over, as a held key does after any first press.
b5_ask focusblip > /dev/null || b5_hookFailed
sleep 0.5
b5_dump
x=$(b5_json "d['player'][0]")
xdotool keydown Right; sleep 1.2; xdotool keyup Right; sleep 1.0
if [ "$(b5_paused)" = False ] && [ "$(b5_json "d['player'][0]")" -ge $((x + 2)) ]; then
	b5_ok "a key held on after the pause ended it and then walked, to $(b5_cell)"
else
	b5_note "a key held after the pause: paused $(b5_paused), from x $x to $(b5_cell)"
fi

b5_finish
