#!/bin/bash
# volume.sh - the music's two volumes (ROADMAP 31): the menu's music, which the
# level selection and both editors play as well, follows a slider of its own
# and a level's music the other, each as soon as it moves, both written to
# config.xml by OK, taken back by Cancel and read again at the next start. A
# config.xml from before the menu had its own hands the menu the one music
# volume it held, and with no config.xml at all both start at the same default.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/volume.sh
#
# What the music plays at is read off the dump: the track, whether it is the
# menu's, its fade and the gain OpenAL has for it, which must be the fade times
# the slider of its kind. A check waits for the fade-in to end, so that the
# gain is the slider's value itself. A slider is set by pressing its track,
# which centres the knob there, and its value is read back off the dump rather
# than worked out from where the press went.
set -u
B5_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# A private home, as drag.sh keeps one: the script writes config.xml, and the
# developer's own is neither read nor overwritten.
export XDG_DATA_HOME="${B5_VOLUME_XDG:-/tmp/blocks5-volume-xdg}"
B5_PRIVATE_HOME="$XDG_DATA_HOME/blocks5"
rm -rf "$XDG_DATA_HOME"
mkdir -p "$B5_PRIVATE_HOME"
printf 1.2.0 > "$B5_PRIVATE_HOME/.initialized"

# A config.xml as 1.2.0 wrote it: one music volume, for the levels and the menu
# alike.
cat > "$B5_PRIVATE_HOME/config.xml" <<'XML'
<?xml version="1.0" ?>
<Config>
    <Language>en</Language>
    <Details>2</Details>
    <SoundVolume>0.500000</SoundVolume>
    <MusicVolume>0.300000</MusicVolume>
</Config>
XML

source "$B5_HERE/harness.sh"
trap b5_stop EXIT

# Is the music playing $1 (a part of its file name), the menu's if $2 is True,
# at the slider value $3? Waits up to 15 s for it to have faded in; prints ok,
# or what is playing instead.
b5_musicAt()   # $1 file, $2 True or False, $3 0..100
{
	local i
	for i in $(seq 1 30); do
		b5_dump
		[ "$(b5_json "bool(d['music']) and '$1' in d['music']['file'] and d['music']['volume'] >= 0.999")" = True ] && break
		sleep 0.5
	done
	b5_json "'ok' if d['music'] and '$1' in d['music']['file'] and d['music']['menu'] == $2 and abs(d['music']['gain'] - $3 / 100.0) < 0.0015 else 'playing: %s' % d['music']"
}
b5_expectMusic()   # $1 file, $2 True or False, $3 0..100, $4 what that means
{
	local answer
	answer=$(b5_musicAt "$1" "$2" "$3")
	[ "$answer" = ok ] && b5_ok "$4" || b5_note "$4 - $answer"
}

# Press a slider of the options' track at a fraction of its length and print
# the value it then shows. A press on the knob moves nothing, so a fraction has
# to stay clear of where the knob stands: it is a sixth of the track wide.
b5_setSlider()   # $1 the slider's name in Options, $2 0..1
{
	local x y w h
	b5_dump
	read x y w h <<< "$(b5_json "' '.join(map(str, el('OptionsPane.Options.$1')['rect']))")"
	b5_clickAt "$(python3 -c "print($x + 16 + int($2 * ($w - 32)))")" $((y + h / 2))
	b5_dump
	b5_json "el('OptionsPane.Options.$1')['scroll']"
}
b5_sliders() { b5_dump; b5_json "'%s %s' % (el('OptionsPane.Options.MusicVolume')['scroll'], el('OptionsPane.Options.MenuMusicVolume')['scroll'])"; }

# A volume as config.xml holds it, 0..100, or nothing where it is missing.
b5_config()   # $1 the element
{
	python3 - "$B5_PRIVATE_HOME/config.xml" "$1" <<'PY'
import sys, xml.etree.ElementTree as E
e = E.parse(sys.argv[1]).getroot().find(sys.argv[2])
print('' if e is None or e.text is None else round(float(e.text) * 100))
PY
}

b5_start
b5_waitForState GS_Menu
[ "$(b5_json "d['appActive']")" = True ] || b5_note "the game is not the active window, so every volume reads 0"

# --- 1. A config.xml from before: the menu keeps the one music volume ----------
b5_expectMusic menu.ogg True 30 "the menu's music plays at the old config.xml's music volume, 30"
b5_click Menu.Options
b5_expectShown OptionsPane.Options
[ "$(b5_sliders)" = "30 30" ] && b5_ok "and both music sliders show it" \
	|| b5_note "the game and menu music sliders show $(b5_sliders), not 30 30"

# --- 2. Each slider turns its own music, while it is dragged -------------------
menu=$(b5_setSlider MenuMusicVolume 0.75)
b5_expectMusic menu.ogg True "$menu" "the menu music slider, pressed to $menu, turns the menu's music to it before OK"
game=$(b5_setSlider MusicVolume 0.55)
b5_expectMusic menu.ogg True "$menu" "the game music slider, pressed to $game, leaves the menu's at $menu"
[ "$menu" != 30 ] && [ "$game" != 30 ] && [ "$menu" != "$game" ] \
	|| b5_note "the presses left the sliders at $game and $menu, which tell nothing apart"
b5_click OptionsPane.Options.OK
[ "$(b5_config MenuMusicVolume) $(b5_config MusicVolume)" = "$menu $game" ] \
	&& b5_ok "OK writes <MenuMusicVolume> $menu and <MusicVolume> $game" \
	|| b5_note "after OK config.xml holds <MenuMusicVolume> '$(b5_config MenuMusicVolume)' and <MusicVolume> '$(b5_config MusicVolume)'"

# --- 3. Cancel takes a dragged slider back -------------------------------------
b5_click Menu.Options
other=$(b5_setSlider MenuMusicVolume 0.4)
b5_expectMusic menu.ogg True "$other" "pressed again, to $other, the menu's music follows"
b5_click OptionsPane.Options.Cancel
b5_expectMusic menu.ogg True "$menu" "and Cancel puts it back to $menu"

# --- 4. The menu's music wherever it plays, a level's at the game's ------------
b5_click Menu.StartGame
b5_waitForState GS_SelectLevel
b5_expectMusic menu.ogg True "$menu" "the level selection plays the menu's music at $menu"
b5_click SelectLevel.PlayLevel
b5_waitForState GS_Game
b5_expectMusic music2.ogg False "$game" "the first level plays its own music at the game's $game"
# Escape closes an open hint note before it opens the menu.
b5_key Escape
b5_dump
[ "$(b5_json "el('Game.MenuPane.Menu.Quit')['shown']")" = True ] || b5_key Escape
b5_click Game.MenuPane.Menu.Quit
b5_waitForState GS_SelectLevel
b5_expectMusic menu.ogg True "$menu" "back at the selection, the menu's music plays at $menu again"
b5_click SelectLevel.Quit
b5_waitForState GS_Menu
b5_click Menu.LevelEditor
b5_waitForState GS_LevelEditor
b5_expectMusic menu.ogg True "$menu" "the level editor plays the menu's music at $menu"
b5_click LevelEditor.ShowMenu
b5_click LevelEditor.MenuPane.Menu.Quit
b5_waitForState GS_Menu

# --- 5. The mute key silences the menu's music as well --------------------------
# An action, so held rather than tapped.
b5_hold F1
b5_expectMusic menu.ogg True 0 "the mute key silences the menu's music"
b5_hold F1
b5_expectMusic menu.ogg True "$menu" "and brings it back at $menu"

# --- 6. Read again at the next start -------------------------------------------
# Escape in the menu quits the way a player does, and Engine::exit() writes
# config.xml once more.
b5_key Escape
for i in $(seq 1 25); do b5_alive || break; sleep 1; done
b5_alive && b5_note "Escape in the menu did not quit the game"
b5_stop
b5_start
b5_waitForState GS_Menu
b5_expectMusic menu.ogg True "$menu" "started again, the menu's music plays at $menu"
b5_click Menu.Options
[ "$(b5_sliders)" = "$game $menu" ] && b5_ok "and the sliders show $game and $menu" \
	|| b5_note "started again, the game and menu music sliders show $(b5_sliders), not $game $menu"
b5_click OptionsPane.Options.Cancel

# --- 7. With no config.xml at all, both start at the same default -------------
b5_key Escape
for i in $(seq 1 25); do b5_alive || break; sleep 1; done
b5_alive && b5_note "Escape in the menu did not quit the game"
b5_stop
rm -f "$B5_PRIVATE_HOME/config.xml"
b5_start
b5_waitForState GS_Menu
b5_expectMusic menu.ogg True 100 "with no config.xml, the menu's music plays at the default, 100"
b5_click Menu.Options
[ "$(b5_sliders)" = "100 100" ] && b5_ok "and both music sliders show 100" \
	|| b5_note "with no config.xml, the game and menu music sliders show $(b5_sliders), not 100 100"
b5_click OptionsPane.Options.Cancel

b5_finish
