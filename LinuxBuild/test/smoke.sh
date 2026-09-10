#!/bin/bash
# smoke.sh - one round through the Linux build's GUI.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/smoke.sh
#
# Clicks go to element names, not to coordinates; harness.sh says how. It needs
# Xvfb, a window manager (openbox), xdotool and ffmpeg:
#
#   sudo apt install xvfb openbox xdotool ffmpeg
#
# Without a window manager everything runs except the fullscreen switch: the
# game asks for that through EWMH, and with no window manager nobody hears it.
set -u
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/harness.sh"

trap b5_stop EXIT
b5_start
b5_waitForState GS_Menu

# On the very first start the CRT filter offer lies over everything else.
b5_dump
if [ "$(b5_json "el('Menu.CrtPane.Crt.NoThanks')['shown']")" = "True" ]; then
	b5_ok "the CRT offer is up (first start)"
	b5_click Menu.CrtPane.Crt.NoThanks
	b5_expectShown Menu.CrtPane false
fi
b5_shot 1-menu

# --- Options: open, look, close again ---------------------------------------
b5_click Menu.Options
b5_expectShown OptionsPane.Options
b5_shot 2-options

# Without a selection in the list the buttons under it are disabled. From
# outside the only other sign of that is that they stay grey.
b5_dump
for name in OptionsPane.Options.PrimaryKey OptionsPane.Options.SecondaryKey OptionsPane.Options.ResetSelected; do
	[ "$(b5_json "el('$name')['active']")" = "True" ] \
		&& b5_note "$name is enabled with no selection" \
		|| b5_ok "$name is disabled with no selection"
done

# Escape belongs to the dialog, not to the menu under it - otherwise it quits
# the game instead of closing the dialog.
b5_key Escape
b5_expectShown OptionsPane.Options false
b5_expectState GS_Menu
b5_shot 3-back

# And the same with the key held. SDL_EnableKeyRepeat(140, 60) turns 400 ms of
# Escape into six events: the first closes the dialog, and the repeats behind
# it must not quit the game as well.
b5_click Menu.Options
b5_expectShown OptionsPane.Options
b5_hold Escape
if kill -0 "$B5_GAME_PID" 2>/dev/null; then
	b5_ok "a held Escape did not quit the game"
	b5_expectShown OptionsPane.Options false
	b5_expectState GS_Menu
else
	b5_note "a held Escape in the options dialog quit the game"
fi

# --- The CRT filter's sliders -----------------------------------------------
# The only dialog whose elements are all addressed through getChild() and that
# no other test opens. "CRT settings ..." deliberately switches the filter on
# as it goes; that belongs to the button.
b5_click Menu.Options
b5_expectShown OptionsPane.Options
b5_click OptionsPane.Options.CrtSettings
b5_expectShown OptionsPane.CrtOptions
for slider in Scan Curve Bloom Flicker ScanFlicker Converge; do
	b5_expectShown "OptionsPane.CrtOptions.$slider"
done
b5_shot 3b-crt

# Closed again through OK. The button is simply called Close -
# Options::handleClick compares the name, and that is the one string here that
# no other check looks at.
b5_click OptionsPane.CrtOptions.Close
b5_expectShown OptionsPane.CrtOptions false

# And put the filter back explicitly, not through Cancel: that calls
# loadConfig(), and on a first run there is no config.xml yet that would have
# anything to take back. With the CRT filter left on, the next run would see a
# curved picture - and the test hook does not account for the curvature, so
# every click would land in the wrong place.
b5_click OptionsPane.Options.SharpFit
b5_click OptionsPane.Options.OK
b5_expectShown OptionsPane.Options false

# --- Manager: step through the five kinds -----------------------------------
b5_click Menu.Manager
b5_expectShown Menu.ManagerPane.Manager
for kind in KindLevel KindCampaign KindMusic KindSkin KindProgress; do
	b5_click "Menu.ManagerPane.Manager.$kind"
done
b5_shot 4-manager

# Export and Delete follow different rules, because the list unites both roots:
# everything that stands there can be exported, only what has its own version
# in the user directory can be deleted. The first entry of the sorted union is
# selected - if it lies only in the game folder, Delete stays grey. Not assumed
# "on a fresh profile" but looked up every time: an example level moves from
# one side to the other the moment somebody saves it.
b5_managerRoots() { # $1=kind  ->  "<subfolder> <extension>"
	case "$1" in
		KindLevel)    echo "levels .xml" ;;
		KindCampaign) echo "levels/campaigns .zip" ;;
		KindSkin)     echo "levels/skins .zip" ;;
	esac
}
b5_home="${XDG_DATA_HOME:-$HOME/.local/share}/blocks5"
for kind in KindLevel KindCampaign KindSkin; do
	set -- $(b5_managerRoots "$kind")
	sub=$1; ext=$2
	first=$(ls "$B5_GAME/$sub"/*$ext "$b5_home/$sub"/*$ext 2>/dev/null \
			| sed 's|.*/||' | sort -u | head -1)
	[ -n "$first" ] && [ -f "$b5_home/$sub/$first" ] && wantDelete=True || wantDelete=False

	b5_click "Menu.ManagerPane.Manager.$kind"
	b5_dump
	have=$(b5_json "el('Menu.ManagerPane.Manager.Delete')['active']")
	[ "$have" = "$wantDelete" ] \
		&& b5_ok "$kind: Delete enabled=$have, matching \"$first\"" \
		|| b5_note "$kind: Delete enabled=$have, expected $wantDelete for \"$first\""
	[ "$(b5_json "el('Menu.ManagerPane.Manager.Export')['active']")" = "True" ] \
		|| b5_note "$kind: Export is disabled although something is selected"
done

# The progress database is not in that table, and cannot be: it has no
# subfolder to look in, and the game folder's own root holds data.zip. One
# file, in the user directory itself, never shipped - so the list has an entry
# exactly when the player has played, and Delete follows the same answer.
b5_click Menu.ManagerPane.Manager.KindProgress
b5_dump
[ -f "$b5_home/progress.zip" ] && wantProgress=True || wantProgress=False
have=$(b5_json "el('Menu.ManagerPane.Manager.Delete')['active']")
[ "$have" = "$wantProgress" ] \
	&& b5_ok "KindProgress: Delete enabled=$have, matching the user directory" \
	|| b5_note "KindProgress: Delete enabled=$have, expected $wantProgress"

# Export and Delete both hang on the selection, but not on the same condition:
# everything in the list can be exported, only what belongs to the player can
# be deleted. The list is the union of both roots and alphabetically sorted,
# and the first entry is selected - if that one lies in the game folder, Delete
# stays grey.
#
# Whether there is any music there at all depends on where the game is run
# from. stage.bat says what ships, and it puts only the two example levels into
# levels/. Out of the working directory - which is how this test runs - the ten
# music tracks blocks.zip is built from lie there too.
b5_click Menu.ManagerPane.Manager.KindMusic
b5_dump
musicGame="$B5_GAME/levels"
musicHome="${XDG_DATA_HOME:-$HOME/.local/share}/blocks5/levels"
first=$(ls "$musicGame"/*.ogg "$musicHome"/*.ogg 2>/dev/null | sed 's|.*/||' | sort -u | head -1)
if [ -z "$first" ]; then
	wantExport=False; wantDelete=False
else
	wantExport=True
	if [ -f "$musicGame/$first" ]; then wantDelete=False; else wantDelete=True; fi
fi
for name in Menu.ManagerPane.Manager.Export:$wantExport Menu.ManagerPane.Manager.Delete:$wantDelete; do
	want=${name##*:}
	name=${name%%:*}
	have=$(b5_json "el('$name')['active']")
	[ "$have" = "$want" ] \
		&& b5_ok "$name: enabled=$have, matching the music list" \
		|| b5_note "$name: enabled=$have, expected $want"
done

b5_key Escape
b5_expectShown Menu.ManagerPane.Manager false
b5_expectState GS_Menu
b5_shot 5-back

# --- In the game: Escape open and closed again ------------------------------
# The game menu is the only place where Escape means two things, and that can
# only be checked in a running level. It is likewise the place where the
# repeat hurts: SDL_EnableKeyRepeat(140, 60) turns a held key into half a dozen
# events, and without the guard in GS_Game the menu would flap open and shut
# for as long as the finger rests.
b5_click Menu.StartGame
b5_waitForState GS_SelectLevel
b5_click SelectLevel.PlayLevel
b5_waitForState GS_Game
sleep 2

b5_key Escape
b5_expectShown Game.MenuPane.Menu
b5_expectState GS_Game
b5_shot 5b-ingamemenu

b5_key Escape
b5_expectShown Game.MenuPane.Menu false
b5_expectState GS_Game

# And held: open once, and it stays that way. Done wrong it flickers.
b5_hold Escape
b5_expectShown Game.MenuPane.Menu
b5_expectState GS_Game

# Back through the game's own menu, leaving the rest in the main menu again.
b5_click Game.MenuPane.Menu.Quit
b5_waitForState GS_SelectLevel
b5_key Escape
b5_expectState GS_Menu

# --- Fullscreen and back ----------------------------------------------------
# That goes through the window manager, not through SDL - see
# LinuxBuild/linux_window.cpp.
if [ -n "$B5_WM_PID" ]; then
	origin_x=$B5_X; origin_y=$B5_Y
	b5_key alt+Return; sleep 3
	b5_geometry
	b5_shot 6-fullscreen
	if [ "$B5_W" -eq "$B5_SCREEN_W" ] && [ "$B5_H" -eq "$B5_SCREEN_H" ] && [ "$B5_X" -eq 0 ] && [ "$B5_Y" -eq 0 ]; then
		b5_ok "fullscreen: $B5_W x $B5_H at (0, 0)"
	else
		b5_note "fullscreen: $B5_W x $B5_H at ($B5_X, $B5_Y), expected $B5_SCREEN_W x $B5_SCREEN_H at (0, 0)"
	fi
	b5_key alt+Return; sleep 3
	b5_geometry
	b5_shot 7-windowed
	# The size first, then the position. A window still in fullscreen sits in
	# the corner and looks like a lost position - the report would name the
	# position while the broken thing is the toggle. Under X11 the position
	# itself is restored by the window manager, not by the game.
	if [ "$B5_W" -ge "$B5_SCREEN_W" ] && [ "$B5_H" -ge "$B5_SCREEN_H" ]; then
		b5_note "still $B5_W x $B5_H after Alt+Return - fullscreen was not left"
	elif [ "$B5_X" -eq "$origin_x" ] && [ "$B5_Y" -eq "$origin_y" ]; then
		b5_ok "back in a window at the same position"
	else
		b5_note "back in a window at ($B5_X, $B5_Y) instead of ($origin_x, $origin_y)"
	fi
fi

# --- Screenshot -------------------------------------------------------------
# F11 writes one into the user directory. That is the only way from here to see
# the framebuffer itself - everything else is the window.
#
# It checks not only that a file appeared but that it is a PNG: the signature
# at the front and the IEND chunk at the back. The encoder is the game's own
# (src/img_save.cpp), and a truncated file could not be spotted from its size
# alone.
HOME_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/blocks5"
before=$(ls "$HOME_DIR/screenshots"/*.png 2>/dev/null | wc -l)
b5_hold F11
sleep 2
after=$(ls "$HOME_DIR/screenshots"/*.png 2>/dev/null | wc -l)
if [ "$after" -gt "$before" ]; then
	shot=$(ls -t "$HOME_DIR/screenshots"/*.png | head -1)
	magic=$(od -An -tx1 -N8 "$shot" | tr -d ' \n')
	ending=$(tail -c 12 "$shot" | od -An -tx1 | tr -d ' \n')
	if [ "$magic" = "89504e470d0a1a0a" ] && [ "${ending#*49454e44}" != "$ending" ]; then
		b5_ok "F11 wrote a valid PNG ($(wc -c < "$shot") bytes)"
	else
		b5_note "F11 wrote a file, but not a valid PNG"
	fi
else
	b5_note "F11 wrote no screenshot"
fi

# --- Quitting ---------------------------------------------------------------
# Through the game and not through the window: "xdotool windowclose" calls
# XDestroyWindow, and SDL then trips over a window it still believes is its
# own. Escape in the menu is the way a player takes too, and only that way does
# Engine::exit() run and write config.xml.
b5_key Escape
for i in $(seq 1 25); do kill -0 "$B5_GAME_PID" 2>/dev/null || break; sleep 1; done
kill -0 "$B5_GAME_PID" 2>/dev/null && b5_note "Escape in the menu did not quit the game"

[ -f "$HOME_DIR/config.xml" ] && b5_ok "config.xml written" || b5_note "config.xml is missing"
grep -q "ERROR" "$B5_OUT/run.log" && b5_note "ERROR in the log: $(grep -m3 ERROR "$B5_OUT/run.log" | tr '\n' ' ')" \
                                  || b5_ok "no error line in the log"

b5_finish
