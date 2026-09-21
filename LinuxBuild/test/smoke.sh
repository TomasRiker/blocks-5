#!/bin/bash
# smoke.sh - one round through the Linux build's GUI.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/smoke.sh
#
# Clicks go to element names, not to coordinates; harness.sh says how. It needs
# Xvfb, a window manager (openbox), xdotool, ffmpeg and xdpyinfo (x11-utils),
# which is what it waits on for the server to come up:
#
#   sudo apt install xvfb openbox xdotool ffmpeg x11-utils
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

# --- Help: every page still fits its box ------------------------------------
# The frame around a help page is a fixed 580x370, and what has to fit in it is
# written by hand in languages.txt, wrapped at display time, and carries
# %BINDING markers that expand to whatever the player rebound them to. So "it
# fits" is not a property anybody can read off the file, and when it stopped
# being true the only symptom was a row sliced in half by the frame's bottom
# edge - which is what this screen looked like until the box grew by a line.
#
# The height is the game's own answer, not a second implementation of the wrap:
# a dump reports every static text's laid-out size, measured through the
# element's own font. The text sits 5 px inside the box, so it has to end 5 px
# short of the other edge.
b5_helpPagesFit()   # $1 what to call the language in the message
{
	local p textH boxH room worst=0
	b5_click Menu.Help
	for p in 1 2 3 4 5 6; do
		b5_dump || b5_hookFailed
		textH=$(b5_json "el('HelpPane.Help.Page.Text')['text'][1]")
		boxH=$(b5_json "el('HelpPane.Help.Page')['rect'][3]")
		room=$((boxH - 10))
		if [ "$textH" -gt "$room" ]; then
			b5_note "$1 help page $p is ${textH}px of text in ${room}px of box - $((textH - room))px past the frame"
		fi
		[ "$textH" -gt "$worst" ] && worst=$textH
		[ "$p" = 6 ] || b5_click HelpPane.Help.NextPage
	done
	b5_click HelpPane.Help.OK
	b5_expectShown HelpPane.Help false
	b5_ok "$1 help: six pages, the fullest ${worst}px of $((boxH - 10))px"
}
b5_helpPagesFit English

# And again in the other language, which is the half nobody has in front of
# them: German is the longer of the two everywhere else in this file. The radio
# applies at once (Options::handleClick calls setLanguage), so there is nothing
# to confirm but the dialog itself.
b5_click Menu.Options
b5_click OptionsPane.Options.German
b5_click OptionsPane.Options.OK
b5_helpPagesFit German
b5_click Menu.Options
b5_click OptionsPane.Options.English
b5_click OptionsPane.Options.OK

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

# Restarting the level five times over must leave one transition running, not
# five queued behind each other. $A_RESTART_LEVEL used to carry a delay and an
# interval of a second without turning the repeat off, which made it an
# auto-fire action: a second press inside that second went into the repeat's
# five-deep buffer and was played out a second later, so mashing F5 restarted
# the level - and began the transition again over the one still running - for
# seconds after the last press.
#
# What is asked is not how long it takes, which would be a timing assertion
# under whatever load the machine is under, but whether the transition's own
# clock ever runs backwards afterwards. It can only do that if a second one
# began, which is the bug exactly.
#
# F5 is read off the key state once a tick, so it has to be held past a
# rendered frame - a fifth of a second under llvmpipe.
b5_mashRestart()
{
	local i last now
	for i in 1 2 3 4 5; do
		xdotool keydown F5; sleep 0.3; xdotool keyup F5; sleep 0.2
	done
	last=-1
	for i in $(seq 1 40); do
		b5_dump || b5_hookFailed
		now=$(b5_json "d['crossfade']")
		[ "$now" = "-1" ] && break
		if [ "$last" != "-1" ] && [ "$now" -lt "$last" ]; then
			b5_note "the restart transition went back from ${last}ms to ${now}ms - a second one began"
			return
		fi
		last=$now
		sleep 0.15
	done
	b5_ok "five restarts ran one transition through, ending at ${last}ms"
}
b5_mashRestart
b5_expectState GS_Game

# The pause was the same shape - 200 and 500 with the repeat left on - so
# holding the key toggled it every half second and a hold ended wherever the
# arithmetic landed. Held for a second and a half it must simply be paused:
# under the old numbers that hold fired at 0, 200, 700 and 1200 ms and came
# out the other side switched off.
#
# It is the one key the "any key leaves the pause" rule cannot resume with,
# since the resume sits in front of the action chain and spends the press - so
# what this reads is the hold, not a second press.
xdotool keydown Pause; sleep 1.5; xdotool keyup Pause; sleep 1
b5_dump || b5_hookFailed
if [ "$(b5_json "d['paused']")" = "True" ]; then
	b5_ok "the pause key held for a second and a half left the game paused"
else
	b5_note "the pause key held for a second and a half toggled itself back off"
fi
# Space and not Escape, although both resume: the resume spends the press for
# the action chain alone, and the game menu hangs off Escape in GameGUI's own
# key handler, which runs whatever onUpdate did with the press. So Escape
# resumes and opens the menu in one go - right for a player, since the menu is
# a pause of its own, but it would leave the menu standing and put every
# Escape below this out by one.
b5_key space
b5_dump || b5_hookFailed
[ "$(b5_json "d['paused']")" = "False" ] \
	&& b5_ok "and any key resumed it" \
	|| b5_note "the game is still paused after a keypress"
b5_expectShown Game.MenuPane.Menu false
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

# --- The credits, both versions ---------------------------------------------
# Which one runs is the whole of the feature and the frame oracle cannot see
# it: the picture at a named tick proves each version draws what it should,
# not that the key asked for that one. So this drives the two chords and reads
# the answer off the one behaviour that tells them apart - a click or Escape
# leaves the plain version, where the ending takes neither as an exit.
#
# The modifiers are held across the key, because GS_Menu::onUpdate reads those
# with SDL_GetKeyState - a level, answered from the last pump. The key itself
# it reads with wasKeyPressed(), the edge an SDL_KEYDOWN sets, so a short press
# inside the hold is seen however long a frame is taking.
credits_chord()   # $1 the function key, F2 (plain) or F3 (the ending)
{
	xdotool keydown ctrl; sleep 0.1
	xdotool keydown shift; sleep 0.1
	xdotool keydown "$1"; sleep 0.2; xdotool keyup "$1"; sleep 0.1
	xdotool keyup shift; xdotool keyup ctrl
	sleep 1.5
}

credits_chord F2
b5_waitForState GS_Credits
b5_clickAt 320 240
b5_expectState GS_Menu
b5_ok "Ctrl+Shift+F2 ran the plain credits and a click left them"

credits_chord F2
b5_waitForState GS_Credits
b5_key Escape
b5_expectState GS_Menu
b5_ok "and so does a key"

credits_chord F3
b5_waitForState GS_Credits
b5_clickAt 320 240
b5_key Escape
b5_expectState GS_Credits
b5_ok "Ctrl+Shift+F3 ran the ending, which neither a click nor a key cuts off"
# Out through the hook rather than the keyboard: Escape there only
# fast-forwards, and the ending is fifty-eight seconds long even at five times
# speed.
b5_ask "state GS_Menu" >/dev/null
b5_waitForState GS_Menu

# And the way in that a player takes, which is the one the chords above are not:
# an invisible button over the Credits line in menu.png. It passes no parameter,
# so which version runs is Campaign::isBuiltInCompleted()'s answer - the plain
# one here, this home having solved nothing. b5_click is the whole of the check
# for the button itself: it refuses a name the tree does not have, and it asks
# the hook whether a click on the middle would really land there before it
# clicks, so a button covered by something else fails loudly rather than
# silently doing nothing.
b5_click Menu.Credits
b5_waitForState GS_Credits
b5_clickAt 320 240
b5_expectState GS_Menu
b5_ok "the Credits button ran the version the player has earned"

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
