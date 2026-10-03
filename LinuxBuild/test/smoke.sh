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

# The version: where curl or wget is, the button with this version on its
# first line, enabled unless a check is running just now - the developer's own
# config.xml may have started one; where neither is, the plain label in its
# place. Looked at and not clicked, since that would ask the website;
# update.sh clicks it against a server of its own.
b5_dump
version=$(sed -n 's/.*p_localVersion = "\([^"]*\)".*/\1/p' "$B5_GAME/src/main.cpp")
if command -v curl >/dev/null 2>&1 || command -v wget >/dev/null 2>&1; then
	shown=Menu.VersionButton; hidden=Menu.Version
else
	shown=Menu.Version; hidden=Menu.VersionButton
fi
[ "$(b5_json "el('$shown')['shown'] and not el('$hidden')['shown']")" = True ] \
	&& b5_ok "$shown is shown and $hidden is not" \
	|| b5_note "$shown is shown: $(b5_json "el('$shown')['shown']"), $hidden: $(b5_json "el('$hidden')['shown']")"
if [ "$shown" = Menu.VersionButton ]; then
	[ "$(b5_json "el('Menu.VersionButton')['title'].split('\u00b6')[0] == 'v$version'")" = True ] \
		&& b5_ok "the version button names $version" \
		|| b5_note "the version button says $(b5_json "repr(el('Menu.VersionButton')['title'])")"
	want=$(b5_json "d['updateCheck'] != 1")
	[ "$(b5_json "el('Menu.VersionButton')['active']")" = "$want" ] \
		&& b5_ok "the version button's active is $want, as the check says" \
		|| b5_note "the version button's active is not $want"
else
	[ "$(b5_json "el('Menu.Version')['text'][0] > 0")" = True ] \
		&& b5_ok "the label holds the version" \
		|| b5_note "the label is empty"
fi

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

# The update check's box and its label are there exactly where curl or wget
# is, which is what the check asks. Looked at and not clicked: a click and OK
# write config.xml, and this may be the developer's own user directory.
# update.sh clicks it, in a home of its own.
box="el('OptionsPane.Options.UpdateCheck')"; label="el('OptionsPane.Options.UpdateCheckLabel')"
if command -v curl >/dev/null 2>&1 || command -v wget >/dev/null 2>&1; then
	expect="$box['shown'] and $box['active'] and $label['shown']"
	state="shown and enabled"; why="curl or wget is installed"
else
	expect="not $box['shown'] and not $label['shown']"
	state=hidden; why="neither curl nor wget is installed"
fi
[ "$(b5_json "$expect")" = True ] \
	&& b5_ok "the update check box and its label are $state: $why" \
	|| b5_note "the update check box and its label are not $state, although $why"

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
for slider in Scan Curve Bloom Flicker ScanFlicker Converge Rewind; do
	b5_expectShown "OptionsPane.CrtOptions.$slider"
done
b5_shot 3b-crt

# Closed again through OK. The button is simply called Close -
# Options::handleClick compares the name, and that is the one string here that
# no other check looks at.
b5_click OptionsPane.CrtOptions.Close
b5_expectShown OptionsPane.CrtOptions false

# And put the filter back explicitly and save it, not through Cancel: that
# restores whatever config.xml holds, or the defaults where there is none yet,
# and the next run has to find a flat picture whatever the developer had set -
# the test hook does not account for the curvature, so every click would land
# in the wrong place.
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

# --- Wrapping: no keycap broken across two lines, no line over its width -----
# adjustText() breaks a line at the last space it may, and a key name can hold
# one - "Num Enter", "Page Up" - which must not be it: the frame would be cut in
# two, one half at the end of a line and the other at the start of the next
# (ROADMAP 48). The hook wraps a text at every width from 1 to 600 in one tick
# ("wrap"), in both fonts the GUI wraps with, upright and in the slant of <h> and
# the speech balloons, and no line of any answer may stand inside <k>...</k>.
# Against adjustText without its keycap guard, 1450 of these 7200 answers did.
# Nor may a line be wider than the width it was wrapped to, measured the way it
# is drawn, slant included - unless all it holds is one keycap or one character,
# which no width makes narrower. Against adjustText counting no slant, 624
# answers held one.
# The keycaps stand where a sentence puts them, glued to a word and to
# punctuation, and paired by half spaces as getBindingMarkup writes a binding.
H=$'\xb7'
wrapTexts=(
	"Press <k>Num Enter</k> to save in the hotel, <k>Page Up</k> and <k>Page Down</k> to scroll, and <k>Num Enter</k> once more."
	"x<k>Num Enter</k>,y<k>Page Down</k>.<k>Num Enter</k>${H}/${H}<k>Page Up</k>${H}+${H}<k>Num Enter</k>"
	"Hotel: <h>in italics</h> <k>Num Enter</k>${H}/${H}<k>Right Ctrl</k>, then <k>Page Up</k>, or <k>Num Enter</k>."
)
# One sweep: the hook's answers for a font, a slant and a range of widths,
# counted into answers, split and over. Not called in a subshell, so that a
# hook that does not answer ends the script.
b5_wrapSweep()   # $1 font, $2 italic, $3 from, $4 to, $5 text
{
	b5_ask "wrap $1 $2 $3 $4 $5" 20 > "$B5_OUT/wrap.jsonl" || b5_hookFailed
	read -r answers split over <<< "$(python3 - "$B5_OUT/wrap.jsonl" <<'PY'
import json, re, sys
answers = split = over = 0
atom = re.compile(r'<k>[^<]*(</k>)?|.')
for row in open(sys.argv[1]):
    try:
        answer = json.loads(row)
        width, lines, widths = answer['width'], answer['lines'], answer['widths']
    except (ValueError, KeyError, TypeError): continue
    answers += 1
    if any(l.count('<k>') != l.count('</k>') for l in lines): split += 1
    if any(w > width and not atom.fullmatch(re.sub('</?h>', '', l)) for l, w in zip(lines, widths)): over += 1
print(answers, split, over)
PY
)"
}
wrapAnswers=0; wrapSplit=0; wrapOver=0
for font in font.xml tooltip_font.xml; do
	for italic in 0 4; do
		for text in "${wrapTexts[@]}"; do
			b5_wrapSweep "$font" "$italic" 1 600 "$text"
			wrapAnswers=$((wrapAnswers + answers)); wrapSplit=$((wrapSplit + split)); wrapOver=$((wrapOver + over))
			[ "$split" -eq 0 ] || b5_note "$font, italic $italic: $split widths break inside a keycap of \"$text\""
			[ "$over" -eq 0 ] || b5_note "$font, italic $italic: $over widths hold a line wider than the width of \"$text\""
		done
	done
done
[ "$wrapAnswers" -eq 7200 ] && [ "$wrapSplit" -eq 0 ] \
	&& b5_ok "wrapped at every width from 1 to 600, no line stops inside a keycap ($wrapAnswers answers)" \
	|| b5_note "of $wrapAnswers wrapped answers (7200 asked), $wrapSplit break inside a keycap"
[ "$wrapAnswers" -eq 7200 ] && [ "$wrapOver" -eq 0 ] \
	&& b5_ok "and no line is wider than its width, slant included, but a keycap or a character on its own" \
	|| b5_note "of $wrapAnswers wrapped answers (7200 asked), $wrapOver hold a line wider than the width"

# A tab runs to the next stop of the line it stands on, so what follows the
# last space moves down by another amount where it holds one, and is measured
# where it lands. A row of the help table - a label, two tabs and the rest -
# from 160 pixels, where its two stops fit: below that a tab stops past the
# width, and a tab is no place to break. Against adjustText counting a tab
# that moved down as wide as its glyph, and the slant not at all, 319 of these
# 1764 answers ran over.
tabText="Move via mouse		Drag character with left mouse button, then <k>Page Up</k>	and more words here"
tabAnswers=0; tabOver=0
for font in font.xml tooltip_font.xml; do
	for italic in 0 4; do
		b5_wrapSweep "$font" "$italic" 160 600 "$tabText"
		tabAnswers=$((tabAnswers + answers)); tabOver=$((tabOver + over))
		[ "$over" -eq 0 ] || b5_note "$font, italic $italic: $over widths hold a line of the tabbed row wider than the width"
	done
done
[ "$tabAnswers" -eq 1764 ] && [ "$tabOver" -eq 0 ] \
	&& b5_ok "nor in a row with tabs, from 160 to 600 ($tabAnswers answers)" \
	|| b5_note "of $tabAnswers answers for the row with tabs (1764 asked), $tabOver hold a line wider than the width"

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

# Export and Delete as for the levels above.
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
# Only a fresh press ends the pause, and the hold makes none - so what this
# reads is the hold, not a second press that "any key leaves the pause" would
# have taken.
xdotool keydown Pause; sleep 1.5; xdotool keyup Pause; sleep 1
b5_dump || b5_hookFailed
if [ "$(b5_json "d['paused']")" = "True" ]; then
	b5_ok "the pause key held for a second and a half left the game paused"
else
	b5_note "the pause key held for a second and a half toggled itself back off"
fi
# Any key ends the pause and does nothing else (GS_Game::takeInput), so the
# Escapes below each open or close the menu as counted. drag.sh checks Escape,
# a click and the Menu button the same way.
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

# The hook's shot, and not F11, takes only an absolute path. The game resolves a
# relative one through its own file system, against the mounted data.zip, and
# the picture would become a member of that archive - which the browser build
# ships - while the hook answered ok.
zipMembers() { python3 -c 'import sys, zipfile; print(len(zipfile.ZipFile(sys.argv[1]).namelist()))' "$B5_GAME/data.zip"; }
membersBefore=$(zipMembers)
answer=$(b5_ask "shot smoke-relative.png") || b5_hookFailed
[ "${answer%%:*}" = refused ] && [ "$(zipMembers)" = "$membersBefore" ] \
	&& b5_ok "a shot to a relative path is refused, and data.zip keeps its $membersBefore members" \
	|| b5_note "a shot to a relative path answered \"$answer\", and data.zip holds $(zipMembers) members, not $membersBefore"

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
