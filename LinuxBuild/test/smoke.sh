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

# --- Die Regler des Roehrenfilters ------------------------------------------
# Der einzige Dialog, dessen Elemente alle ueber getChild() angesprochen
# werden und den sonst kein Test aufmacht. "CRT settings ..." schaltet den
# Filter dabei absichtlich mit ein, das gehoert zum Knopf.
b5_click Menu.Options
b5_expectShown OptionsPane.Options
b5_click OptionsPane.Options.CrtSettings
b5_expectShown OptionsPane.CrtOptions
for slider in Scan Curve Bloom Flicker ScanFlicker Converge; do
	b5_expectShown "OptionsPane.CrtOptions.$slider"
done
b5_shot 3b-crt

# Ueber OK wieder zu. Der Knopf heisst schlicht Close - Options::handleClick
# vergleicht den Namen, und das ist die eine Zeichenkette hier, die keine
# Pruefung sonst ansieht.
b5_click OptionsPane.CrtOptions.Close
b5_expectShown OptionsPane.CrtOptions false

# Und den Filter ausdruecklich wieder zurueckstellen, nicht ueber Abbrechen:
# das ruft loadConfig(), und beim ersten Lauf gibt es noch keine config.xml,
# die etwas zurueckzunehmen haette. Bliebe die Roehre an, saehe der naechste
# Lauf ein gewoelbtes Bild - und der Testhaken rechnet die Woelbung nicht mit,
# also lande jeder Klick daneben.
b5_click OptionsPane.Options.SharpFit
b5_click OptionsPane.Options.OK
b5_expectShown OptionsPane.Options false

# --- Manager: die vier Arten durchschalten ----------------------------------
b5_click Menu.Manager
b5_expectShown Menu.ManagerPane.Manager
for kind in KindLevel KindCampaign KindMusic KindSkin; do
	b5_click "Menu.ManagerPane.Manager.$kind"
done
b5_shot 4-manager

# Ausgeben und Loeschen folgen verschiedenen Regeln, seit die Liste beide
# Wurzeln vereinigt: ausgeben laesst sich alles, was dasteht, loeschen nur, wovon
# es eine eigene Fassung im Benutzerverzeichnis gibt. Ausgewaehlt ist der erste
# Eintrag der sortierten Vereinigung - liegt er nur im Spielordner, bleibt
# Loeschen grau. Nicht "auf einem frischen Profil" angenommen, sondern jedesmal
# nachgesehen: ein Beispiellevel wandert in dem Augenblick von der einen Seite
# auf die andere, in dem jemand ihn speichert.
b5_managerRoots() { # $1=Art  ->  "<Unterordner> <Endung>"
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
		&& b5_ok "$kind: Loeschen bedienbar=$have, passend zu \"$first\"" \
		|| b5_note "$kind: Loeschen bedienbar=$have, erwartet $wantDelete fuer \"$first\""
	[ "$(b5_json "el('Menu.ManagerPane.Manager.Export')['active']")" = "True" ] \
		|| b5_note "$kind: Ausgeben ist abgeschaltet, obwohl etwas ausgewaehlt ist"
done

# Ausgeben und Loeschen haengen an der Auswahl, und seit es zwei Wurzeln gibt,
# nicht mehr an derselben Bedingung: ausgeben laesst sich alles, was in der
# Liste steht, loeschen nur, was dem Spieler gehoert. Die Liste ist die
# Vereinigung beider Wurzeln und alphabetisch sortiert, ausgewaehlt ist der
# erste Eintrag - liegt der im Spielordner, bleibt Loeschen grau.
#
# Ob ueberhaupt Musik dasteht, haengt davon ab, woraus das Spiel laeuft. Was
# ausgeliefert wird, sagt stage.bat, und das legt nur die beiden Beispiellevel
# nach levels/. Aus dem Arbeitsverzeichnis heraus - so laeuft dieser Test -
# liegen dort auch die zehn Musikstuecke, aus denen blocks.zip gebaut wird.
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
		&& b5_ok "$name: bedienbar=$have, passend zur Musikliste" \
		|| b5_note "$name: bedienbar=$have, erwartet $want"
done

b5_key Escape
b5_expectShown Menu.ManagerPane.Manager false
b5_expectState GS_Menu
b5_shot 5-back

# --- Im Spiel: Escape auf und wieder zu -------------------------------------
# Das Spielmenue ist die einzige Stelle, an der Escape zweierlei bedeutet, und
# das laesst sich nur in einem laufenden Level pruefen. Es ist auch die Stelle,
# an der die Wiederholung wehtut: SDL_EnableKeyRepeat(140, 60) macht aus einer
# gehaltenen Taste ein halbes Dutzend Ereignisse, und ohne die Sperre in
# GS_Game klappte das Menue auf und zu, solange der Finger liegt.
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

# Und gehalten: einmal auf, und dabei bleibt es. Frueher haette das Spiel hier
# gar nichts getan; falsch gemacht flackert es.
b5_hold Escape
b5_expectShown Game.MenuPane.Menu
b5_expectState GS_Game

# Zurueck ueber das Menue des Spiels, damit der Rest wieder im Hauptmenue steht.
b5_click Game.MenuPane.Menu.Quit
b5_waitForState GS_SelectLevel
b5_key Escape
b5_expectState GS_Menu

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
# den Bildpuffer selbst zu sehen - alles andere ist das Fenster.
#
# Geprueft wird nicht nur, dass eine Datei entstanden ist, sondern auch, dass
# sie ein PNG ist: die Signatur vorn und der IEND-Chunk hinten. Den Kodierer
# schreibt das Spiel selbst (src/img_save.cpp), und eine abgeschnittene Datei
# waere an ihrer Groesse allein nicht zu erkennen.
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
		b5_ok "F11 hat ein gueltiges PNG geschrieben ($(wc -c < "$shot") Byte)"
	else
		b5_note "F11 hat eine Datei geschrieben, aber kein gueltiges PNG"
	fi
else
	b5_note "F11 hat kein Bildschirmfoto geschrieben"
fi

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
