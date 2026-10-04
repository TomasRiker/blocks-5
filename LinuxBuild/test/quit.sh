#!/bin/bash
# quit.sh - the round Quit in the level editor's menu and in the menu of a
# level played from the editor. Where the editor's level holds changes not
# saved, it asks first, as Leave Editor does; No, and Escape in the level's
# menu, leave everything as it stood, and Yes quits. With nothing to lose it
# quits at once.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/quit.sh
#
# A quit ends the run, so the four that do are a start each: Yes, and Quit
# with nothing to lose, from the editor's menu and from a level played from
# it. That the game quit is that its process ended having written config.xml,
# which Engine::exit() does on the way out and nothing else does in a home
# that starts without one.
set -u
B5_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# A private home, as drag.sh keeps one: the config.xml looked for is this
# run's, and a level of the developer's does not stand in the editor's way.
export XDG_DATA_HOME="${B5_QUIT_XDG:-/tmp/blocks5-quit-xdg}"
B5_PRIVATE_HOME="$XDG_DATA_HOME/blocks5"

source "$B5_HERE/harness.sh"
trap b5_stop EXIT

# A fresh home and the editor on a new level, which nothing has changed yet.
b5_freshEditor()
{
	b5_stop
	rm -rf "$XDG_DATA_HOME"
	mkdir -p "$B5_PRIVATE_HOME"
	printf 1.2.0 > "$B5_PRIVATE_HOME/.initialized"
	b5_start
	b5_waitForState GS_Menu
	b5_click Menu.LevelEditor
	b5_waitForState GS_LevelEditor
}
# The pen on an empty cell: a change, which undo.sh counts as one.
b5_change() { b5_clickAt 168 168; }
b5_playFromMenu()
{
	b5_click LevelEditor.ShowMenu
	b5_click LevelEditor.MenuPane.Menu.Play
	b5_waitForState GS_Game
	sleep 2
	b5_key Escape
	b5_expectShown Game.MenuPane.Menu
}
b5_asks()   # $1 the question's window, $2 what was pressed
{
	b5_dump
	if [ "$(b5_json "el('$1')['shown']")" = True ] && b5_alive; then b5_ok "$2 asks first"
	else b5_note "$2 did not ask"; fi
}
b5_quits()   # $1 what was pressed
{
	local i
	for i in $(seq 1 25); do b5_alive || break; sleep 1; done
	if b5_alive; then b5_note "$1 did not quit the game"
	elif [ -f "$B5_PRIVATE_HOME/config.xml" ]; then b5_ok "$1 quits the game"
	else b5_note "$1 ended the game without Engine::exit() writing config.xml"; fi
}

# --- 1. Changes not saved: asked, answered No, and Yes in the level ----------
b5_freshEditor
b5_change
b5_click LevelEditor.ShowMenu
b5_click LevelEditor.MenuPane.Quit
b5_asks LevelEditor.MessageBoxPane.MessageBox "with the level changed, the editor's Quit"
b5_click LevelEditor.MessageBoxPane.MessageBox.No
b5_expectShown LevelEditor.MessageBoxPane.MessageBox false
b5_expectState GS_LevelEditor
b5_click LevelEditor.MenuPane.Menu.OK

b5_playFromMenu
b5_dump
[ "$(b5_json "el('Game.MenuPane.Quit')['shown']")" = True ] \
	&& b5_ok "a level played from the editor shows Quit" \
	|| b5_note "a level played from the editor shows no Quit"
b5_click Game.MenuPane.Quit
b5_asks Game.MessageBoxPane.MessageBox "with the editor's level changed, the played level's Quit"
b5_key Escape
b5_expectShown Game.MessageBoxPane.MessageBox false
b5_expectShown Game.MenuPane.Menu
b5_expectState GS_Game
b5_click Game.MenuPane.Quit
b5_asks Game.MessageBoxPane.MessageBox "asked again, it"
b5_click Game.MessageBoxPane.MessageBox.No
b5_expectShown Game.MessageBoxPane.MessageBox false
b5_expectShown Game.MenuPane.Menu
b5_expectState GS_Game
b5_click Game.MenuPane.Quit
b5_click Game.MessageBoxPane.MessageBox.Yes
b5_quits "Yes under the played level's question"

# --- 2. Nothing to lose in the level played: quits at once ------------------
b5_freshEditor
b5_playFromMenu
b5_click Game.MenuPane.Quit
b5_quits "with the editor's level unchanged, the played level's Quit"

# --- 3. Changes not saved, the editor's Quit, Yes ----------------------------
b5_freshEditor
b5_change
b5_click LevelEditor.ShowMenu
b5_click LevelEditor.MenuPane.Quit
b5_asks LevelEditor.MessageBoxPane.MessageBox "with the level changed, the editor's Quit"
b5_click LevelEditor.MessageBoxPane.MessageBox.Yes
b5_quits "Yes under the editor's question"

# --- 4. Nothing to lose in the editor: quits at once -------------------------
b5_freshEditor
b5_click LevelEditor.ShowMenu
b5_click LevelEditor.MenuPane.Quit
b5_quits "with the level unchanged, the editor's Quit"

b5_finish
