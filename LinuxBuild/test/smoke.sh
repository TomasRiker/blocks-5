#!/bin/bash
# smoke.sh - eine Runde durch die Oberflaeche des Linux-Builds.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/smoke.sh
#
# Geklickt wird auf Elementnamen, nicht auf Koordinaten; wie das geht, steht in
# harness.sh. Gebraucht werden Xvfb, ein Fenstermanager (openbox), xdotool und
# ffmpeg:
#
#   sudo apt install xvfb openbox xdotool ffmpeg
#
# Ohne Fenstermanager laeuft alles ausser dem Vollbildwechsel: darum bittet das
# Spiel nach EWMH, und ohne Fenstermanager hoert das niemand.
set -u
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/harness.sh"

trap b5_stop EXIT
b5_start
b5_waitForState GS_Menu

# Beim allerersten Start liegt die Frage nach dem Roehrenfilter ueber allem.
b5_dump
if [ "$(b5_json "el('Menu.CrtPane.Crt.NoThanks')['shown']")" = "True" ]; then
	b5_ok "die Roehrenfrage steht (erster Start)"
	b5_click Menu.CrtPane.Crt.NoThanks
	b5_expectShown Menu.CrtPane false
fi
b5_shot 1-menu

# --- Optionen: auf, hin, wieder zu ------------------------------------------
b5_click Menu.Options
b5_expectShown OptionsPane.Options
b5_shot 2-options

# Ohne Auswahl in der Liste sind die Knoepfe darunter abgeschaltet. Von aussen
# ist das sonst nur daran zu erkennen, dass sie grau bleiben.
b5_dump
for name in OptionsPane.Options.PrimaryKey OptionsPane.Options.SecondaryKey OptionsPane.Options.ResetSelected; do
	[ "$(b5_json "el('$name')['active']")" = "True" ] \
		&& b5_note "$name ist ohne Auswahl bedienbar" \
		|| b5_ok "$name ist ohne Auswahl abgeschaltet"
done

# Escape gehoert dem Dialog, nicht dem Menue darunter - sonst beendet es das
# Spiel, statt den Dialog zu schliessen.
b5_key Escape
b5_expectShown OptionsPane.Options false
b5_expectState GS_Menu
b5_shot 3-back

# Und dasselbe mit gehaltener Taste. SDL_EnableKeyRepeat(140, 60) macht aus
# 400 ms Escape sechs Ereignisse: das erste schliesst den Dialog, und die
# Wiederholungen dahinter duerfen nicht auch noch das Spiel beenden.
b5_click Menu.Options
b5_expectShown OptionsPane.Options
b5_hold Escape
if kill -0 "$B5_GAME_PID" 2>/dev/null; then
	b5_ok "gehaltenes Escape hat das Spiel nicht beendet"
	b5_expectShown OptionsPane.Options false
	b5_expectState GS_Menu
else
	b5_note "gehaltenes Escape im Optionsdialog hat das Spiel beendet"
fi

# --- Manager: die vier Arten durchschalten ----------------------------------
b5_click Menu.Manager
b5_expectShown Menu.ManagerPane.Manager
for kind in KindLevel KindCampaign KindMusic KindSkin; do
	b5_click "Menu.ManagerPane.Manager.$kind"
done
b5_shot 4-manager

# Auf einem frischen Profil ist alles in diesen Listen mitgeliefert - und
# mitgeliefert heisst: ausgeben ja, loeschen nein. Das ist die Regel aus
# Transfer::isBuiltIn(), von aussen sonst nur an einem grauen Knopf zu erkennen.
for kind in KindLevel KindCampaign KindSkin; do
	b5_click "Menu.ManagerPane.Manager.$kind"
	b5_dump
	[ "$(b5_json "el('Menu.ManagerPane.Manager.Delete')['active']")" = "True" ] \
		&& b5_note "$kind: Loeschen ist bedienbar, obwohl nur Mitgeliefertes in der Liste steht" \
		|| b5_ok "$kind: Loeschen bleibt gesperrt"
	[ "$(b5_json "el('Menu.ManagerPane.Manager.Export')['active']")" = "True" ] \
		|| b5_note "$kind: Ausgeben ist abgeschaltet, obwohl etwas ausgewaehlt ist"
done

# Ausgeben und Loeschen haengen an der Auswahl: mit einer leeren Liste gibt es
# keine, also bleiben beide grau, und mit einer gefuellten sind beide da.
#
# Ob die Musikliste leer ist, haengt davon ab, woraus das Spiel laeuft. Was
# ausgeliefert wird, sagt stage.bat, und das legt nur die beiden Beispiellevel
# nach levels/. Aus dem Arbeitsverzeichnis heraus - so laeuft dieser Test -
# liegen dort auch die zehn Musikstuecke, aus denen blocks.zip gebaut wird, und
# main.cpp kopiert beim ersten Start alles davon ins Benutzerverzeichnis.
b5_click Menu.ManagerPane.Manager.KindMusic
b5_dump
musicHome="${XDG_DATA_HOME:-$HOME/.local/share}/blocks5/levels"
if [ "$(ls "$musicHome"/*.ogg 2>/dev/null | wc -l)" -eq 0 ]; then wantActive=False; else wantActive=True; fi
for name in Menu.ManagerPane.Manager.Export Menu.ManagerPane.Manager.Delete; do
	have=$(b5_json "el('$name')['active']")
	[ "$have" = "$wantActive" ] \
		&& b5_ok "$name: bedienbar=$have, passend zur Musikliste" \
		|| b5_note "$name: bedienbar=$have, erwartet $wantActive"
done

b5_key Escape
b5_expectShown Menu.ManagerPane.Manager false
b5_expectState GS_Menu
b5_shot 5-back

# --- Vollbild und zurueck ---------------------------------------------------
# Das laeuft ueber den Fenstermanager, nicht ueber SDL - siehe
# LinuxBuild/linux_window.cpp.
if [ -n "$B5_WM_PID" ]; then
	origin_x=$B5_X; origin_y=$B5_Y
	b5_key alt+Return; sleep 3
	b5_geometry
	b5_shot 6-fullscreen
	if [ "$B5_W" -eq "$B5_SCREEN_W" ] && [ "$B5_H" -eq "$B5_SCREEN_H" ] && [ "$B5_X" -eq 0 ] && [ "$B5_Y" -eq 0 ]; then
		b5_ok "Vollbild: $B5_W x $B5_H bei (0, 0)"
	else
		b5_note "Vollbild: $B5_W x $B5_H bei ($B5_X, $B5_Y), erwartet $B5_SCREEN_W x $B5_SCREEN_H bei (0, 0)"
	fi
	b5_key alt+Return; sleep 3
	b5_geometry
	b5_shot 7-windowed
	# Erst die Groesse, dann der Ort. Ein Vollbild, das gar nicht verlassen
	# wurde, sitzt in der Ecke und sah frueher nach einem verlorenen Ort aus -
	# gemeldet wurde die Stelle, kaputt war der Umschalter. Den Ort selbst
	# stellt unter X11 der Fenstermanager wieder her, nicht das Spiel.
	if [ "$B5_W" -ge "$B5_SCREEN_W" ] && [ "$B5_H" -ge "$B5_SCREEN_H" ]; then
		b5_note "nach Alt+Return immer noch $B5_W x $B5_H - das Vollbild wurde nicht verlassen"
	elif [ "$B5_X" -eq "$origin_x" ] && [ "$B5_Y" -eq "$origin_y" ]; then
		b5_ok "zurueck ins Fenster an dieselbe Stelle"
	else
		b5_note "zurueck ins Fenster bei ($B5_X, $B5_Y) statt ($origin_x, $origin_y)"
	fi
fi

# --- Bildschirmfoto ---------------------------------------------------------
# F11 schreibt eines ins Benutzerverzeichnis. Das ist der einzige Weg von hier,
# den Bildpuffer selbst zu sehen - alles andere ist das Fenster. Die Dateien
# heissen .bmp, nicht .png: SDL_SaveBMP schreibt sie.
HOME_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/blocks5"
before=$(ls "$HOME_DIR/screenshots"/*.bmp 2>/dev/null | wc -l)
b5_hold F11
sleep 2
after=$(ls "$HOME_DIR/screenshots"/*.bmp 2>/dev/null | wc -l)
[ "$after" -gt "$before" ] && b5_ok "F11 hat ein Bildschirmfoto geschrieben" \
                           || b5_note "F11 hat kein Bildschirmfoto geschrieben"

# --- Beenden ----------------------------------------------------------------
# Ueber das Spiel und nicht ueber das Fenster: "xdotool windowclose" ruft
# XDestroyWindow, und SDL faellt danach ueber ein Fenster, das es noch fuer
# seines haelt. Escape im Menue ist der Weg, den auch ein Spieler nimmt, und nur
# darueber laeuft Engine::exit() und schreibt die config.xml.
b5_key Escape
for i in $(seq 1 25); do kill -0 "$B5_GAME_PID" 2>/dev/null || break; sleep 1; done
kill -0 "$B5_GAME_PID" 2>/dev/null && b5_note "Escape im Menue hat das Spiel nicht beendet"

[ -f "$HOME_DIR/config.xml" ] && b5_ok "config.xml angelegt" || b5_note "config.xml fehlt"
grep -q "ERROR" "$B5_OUT/run.log" && b5_note "ERROR im Protokoll: $(grep -m3 ERROR "$B5_OUT/run.log" | tr '\n' ' ')" \
                                  || b5_ok "keine Fehlerzeile im Protokoll"

b5_finish
