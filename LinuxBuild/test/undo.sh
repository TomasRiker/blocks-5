#!/bin/bash
# undo.sh - the level editor's undo list: whatever changes the level is one
# step, and whatever changes nothing is none.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/undo.sh
#
# The hook reports the editor's undo and redo depths, which no picture shows,
# and every assertion below is that pair, bar the two that the note's editor
# opened. A step that changes nothing is the failure worth a test: Ctrl+Z then
# visibly does nothing, and the step clears the redo list on its way in. The
# tools, keys and dialogs that change the level are worked here, most of them
# once changing it and once not.
set -u
B5_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# A private home like drag.sh's, so that no configuration of the developer's
# - a CRT filter warping the clicks - reaches the editor.
export XDG_DATA_HOME="${B5_UNDO_XDG:-/tmp/blocks5-undo-xdg}"
B5_PRIVATE_HOME="$XDG_DATA_HOME/blocks5"
rm -rf "$XDG_DATA_HOME"
mkdir -p "$B5_PRIVATE_HOME/levels"
printf 0 > "$B5_PRIVATE_HOME/.update_checker"
printf 1.2.0 > "$B5_PRIVATE_HOME/.initialized"

source "$B5_HERE/harness.sh"

b5_expectDepths()   # $1 undo, $2 redo, $3 what was done
{
	b5_dump
	local u r
	u=$(b5_json "d['undo']"); r=$(b5_json "d['redo']")
	if [ "$u" = "$1" ] && [ "$r" = "$2" ]; then b5_ok "$3: undo $u, redo $r"
	else b5_note "$3: undo $u, redo $r where $1 and $2 were expected"; fi
}

# The field is 16-pixel cells from the top left; the palette is drawn rather
# than GUI and starts at (245,428) in cells of the same size.
b5_palette() { b5_clickAt $((245 + $1 * 16 + 8)) $((428 + $2 * 16 + 8)); }
b5_rightClickAt() { b5_mouseAt "$1" "$2"; xdotool mousedown 3; sleep 0.4; xdotool mouseup 3; sleep 1.5; }
b5_rightDrag()
{
	b5_mouseAt "$1" "$2"; xdotool mousedown 3; sleep 0.4
	b5_mouseAt "$3" "$4"; sleep 0.4; xdotool mouseup 3; sleep 1.5
}

trap b5_stop EXIT
b5_start
b5_waitForState GS_Menu
b5_click Menu.LevelEditor
b5_waitForState GS_LevelEditor
b5_expectDepths 0 0 "a fresh level"

# Nothing changes in any of these, and a redo is left standing to show that
# none of them clears the list either.
b5_clickAt 168 168
b5_expectDepths 1 0 "the pen on an empty cell"
b5_clickAt 168 168
b5_expectDepths 1 0 "the pen again on the same cell"
b5_chord ctrl z
b5_expectDepths 0 1 "Ctrl+Z"
b5_click LevelEditor.ShowSettings
b5_click LevelEditor.SettingsPane.Settings.Cancel
b5_expectDepths 0 1 "the settings opened and cancelled"
b5_click LevelEditor.ShowSettings
b5_click LevelEditor.SettingsPane.Settings.OK
b5_expectDepths 0 1 "the settings opened and confirmed unchanged"
b5_rightClickAt 168 168
b5_expectDepths 0 1 "the eraser on an empty cell"
b5_key Delete
b5_expectDepths 0 1 "Delete with nothing selected"
b5_click LevelEditor.Mode1
b5_clickAt 168 168
b5_expectDepths 0 1 "modify on an empty cell"
b5_click LevelEditor.Mode3
b5_clickAt 168 168
b5_expectDepths 0 1 "the transition tool on an empty cell"
# A stroke the other button cancels puts back what it had drawn.
b5_click LevelEditor.Mode0
b5_mouseAt 200 168; xdotool mousedown 1; sleep 0.4
b5_mouseAt 264 168; sleep 0.4
xdotool mousedown 3; sleep 0.4; xdotool mouseup 3; sleep 0.2; xdotool mouseup 1; sleep 1.5
b5_expectDepths 0 1 "a stroke cancelled by the other button"
b5_chord ctrl y
b5_expectDepths 1 0 "Ctrl+Y"

# Each of these changes the level, and each is one step, however many cells
# it touched.
b5_drag 104 104 168 104
b5_expectDepths 2 0 "a pen stroke across five cells"
b5_rightDrag 104 104 168 104
b5_expectDepths 3 0 "the eraser across them"
b5_click LevelEditor.Mode2
b5_drag 104 200 168 232
b5_expectDepths 4 0 "a rectangle"

b5_click LevelEditor.Mode4
b5_drag 104 200 168 232
b5_expectDepths 4 0 "the rectangle selected"
b5_key Delete
b5_expectDepths 5 0 "Delete"
b5_key Delete
b5_expectDepths 5 0 "Delete again, over nothing"
b5_chord ctrl z
b5_chord ctrl c
b5_chord ctrl v
b5_expectDepths 4 1 "Ctrl+Z, then Ctrl+C and Ctrl+V onto the copy's own place"
b5_key Right
b5_expectDepths 5 0 "the selection moved by an arrow"
b5_chord ctrl x
b5_expectDepths 6 0 "Ctrl+X"

b5_click LevelEditor.ShowSettings
b5_click LevelEditor.SettingsPane.Settings.Rain
b5_click LevelEditor.SettingsPane.Settings.OK
b5_expectDepths 7 0 "rain switched on in the settings"
b5_click LevelEditor.ShowSettings
b5_click LevelEditor.SettingsPane.Settings.Rain
b5_click LevelEditor.SettingsPane.Settings.Cancel
b5_expectDepths 7 0 "rain switched off and cancelled"

b5_click LevelEditor.NumDiamondsNeeded+
b5_expectDepths 8 0 "one more diamond needed"
b5_click LevelEditor.NumDiamondsNeeded-
b5_expectDepths 9 0 "one fewer"
b5_click LevelEditor.NumDiamondsNeeded-
b5_expectDepths 9 0 "one fewer than none"
b5_click LevelEditor.ElectricityOn
b5_expectDepths 10 0 "the electricity switched"

b5_click LevelEditor.Mode0
b5_click LevelEditor.Cat1
b5_palette 10 0
b5_clickAt 296 104
b5_expectDepths 11 0 "an arrow placed"
b5_click LevelEditor.Mode1
b5_clickAt 296 104
b5_expectDepths 12 0 "the arrow turned"

# Placing a teleporter or a note is a step of its own, and aiming the one or
# writing the other the next: the note's Cancel takes back the writing and
# leaves the note.
b5_palette 9 0
b5_clickAt 328 104
b5_expectDepths 13 0 "a teleporter placed and not aimed"
b5_drag 360 104 424 168
b5_expectDepths 15 0 "a teleporter placed and aimed in one drag"
b5_palette 21 1
b5_clickAt 328 296
b5_expectShown LevelEditor.EditHintPane true
b5_click LevelEditor.EditHintPane.EditHint.Cancel
b5_expectDepths 16 0 "a note placed and its editor cancelled"
b5_clickAt 360 296
b5_expectShown LevelEditor.EditHintPane true
b5_click LevelEditor.EditHintPane.EditHint.OK
b5_expectDepths 17 0 "a note placed and its editor confirmed unchanged"
b5_click LevelEditor.Mode1
b5_clickAt 360 296
b5_type hello
b5_click LevelEditor.EditHintPane.EditHint.OK
b5_expectDepths 18 0 "the note's text changed"
b5_clickAt 360 296
b5_type again
b5_click LevelEditor.EditHintPane.EditHint.Cancel
b5_expectDepths 18 0 "the note written and cancelled"

# A wire from the clock's output pin, (15,8) in its cell, to the bulb's
# input, (7,15) in its own - the pins frames.sh's editor-connect scene uses.
b5_click LevelEditor.Cat4
b5_clickAt 269 468
b5_clickAt 168 232
b5_clickAt 317 436
b5_clickAt 408 232
b5_expectDepths 20 0 "a clock and a bulb placed"
b5_click LevelEditor.Mode6
b5_clickAt 175 232
b5_clickAt 407 239
b5_expectDepths 21 0 "the wire connected"
b5_clickAt 175 232
b5_clickAt 407 239
b5_expectDepths 21 0 "the same wire again, refused"
b5_rightClickAt 175 232
b5_expectDepths 22 0 "the pin disconnected"
b5_rightClickAt 175 232
b5_expectDepths 22 0 "the pin disconnected again, with nothing on it"

b5_chord ctrl z
b5_chord ctrl z
b5_expectDepths 20 2 "two steps undone"
b5_click LevelEditor.ShowMenu
b5_click LevelEditor.MenuPane.Menu.Clear
b5_expectDepths 21 0 "the level cleared from the menu"
b5_click LevelEditor.ShowMenu
b5_click LevelEditor.MenuPane.Menu.Clear
b5_expectDepths 21 0 "the empty level cleared again"

b5_finish
