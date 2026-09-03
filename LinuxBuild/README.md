# Der Linux-Build

Blocks 5 laeuft nativ unter Linux. Es ist derselbe Quelltext wie unter Windows und
im Browser - der Unterschied steckt in acht `#ifdef`-Zweigen und in dieser einen
Uebersetzungseinheit hier.

## Bauen

    sudo apt install build-essential libsdl1.2-dev libopenal-dev \
                     libglu1-mesa-dev libgl1-mesa-dev

    LinuxBuild/build.sh              inkrementell
    LinuxBuild/build.sh clean        von vorn
    LinuxBuild/build.sh run          bauen und starten

Alles andere - zlib, minizip, libogg, libvorbis, TinyXML, stb, minih264, shine,
minimp4 - kommt aus `Blocks5/libs`, genau wie beim Windows- und beim
Browser-Build. Damit uebersetzen alle drei denselben Code.

Das Spiel muss aus `Blocks5/` heraus laufen, weil es `data.zip` relativ zum
Arbeitsverzeichnis oeffnet. `build.sh run` macht das; von Hand:

    cd Blocks5 && ../LinuxBuild/build/blocks5 -windowed

`data.zip` und `levels/skins/*.zip` sind Bauergebnisse und liegen nicht im Git.
Unter Windows packt `Build.bat` sie; von hier aus braucht es einen Lauf unter
Windows oder eine Kopie davon.

**SDL 1.2 heisst heute sdl12-compat**: Debian, Ubuntu und Fedora liefern unter
`libsdl1.2-dev` die Nachbildung der 1.2-Schnittstelle auf SDL 2. Das ist genau
das, was ein Spieler bekommt, und dagegen ist getestet. Das echte SDL 1.2.15
liegt zwar in `Blocks5/libs`, aber nur mit dem Win32-Teil, den das
Visual-Studio-Projekt uebersetzt.

## Was hier drin liegt

    build.sh            der Build
    linux_window.cpp    der Vollbildwechsel, das Einzige, was Xlib braucht
    linux_window.h      seine Schnittstelle, ohne Xlib darin
    test/smoke.sh       das Spiel starten und durch die Oberflaeche fuehren

`linux_window.cpp` ist aus einem Grund eine eigene Datei: `<X11/Xlib.h>` macht
`Font`, `Window`, `Screen` und `Cursor` zu eigenen Typnamen, und das Spiel hat
Klassen, die genauso heissen. In `engine.cpp` eingebunden, uebersetzt die naechste
Zeile mit einem `Font*` darin nicht mehr.

## Was anders ist als unter Windows

- **Das Benutzerverzeichnis** ist `$XDG_DATA_HOME/blocks5/`, ersatzweise
  `~/.local/share/blocks5/`, statt `My Documents\Blocks 5\`.

- **Vollbild** geht ueber den Fenstermanager. Unter Windows setzt das Spiel den
  Fensterstil auf `WS_POPUP` und die Groesse auf den Bildschirm; unter X11 setzt
  kein Programm sein Fenster selbst auf Vollbild, sondern bittet den
  Fenstermanager mit einer `_NET_WM_STATE`-Nachricht darum (EWMH). Der entscheidet
  ueber Groesse und Ort, schickt ein ConfigureNotify, und daraus wird bei SDL ein
  `SDL_VIDEORESIZE`, das `handleResize()` aufgreift - derselbe Weg wie beim Ziehen
  am Fensterrand. SDLs Flags werden dabei so wenig angefasst wie unter Windows:
  ein `SDL_FULLSCREEN` liesse `X11_SetVideoMode` das Fenster neu aufbauen und
  naehme den GL-Kontext mit.

- **Der Dateidialog** ist `zenity` oder `kdialog`, was davon installiert ist. Das
  spart GTK und Qt als Abhaengigkeit. Ist keines von beiden da, bleibt der
  Import-Knopf wirkungslos und eine Zeile im Protokoll sagt warum.

- **Der Import laeuft nebenher**: `popen()` gibt eine Leitung, die `pollImport()`
  Takt fuer Takt abfragt, so dass das Fenster weiterzeichnet, solange der Dialog
  offen ist. Der Export kann das nicht - `doExport()` liefert sein Ergebnis
  sofort, so steht es in `transfer.h` - und haelt das Spiel deshalb an wie der
  modale Dialog unter Windows.

- **Die Aktualisierungspruefung** ruft `curl` oder `wget` auf, statt einen eigenen
  HTTPS-Klienten mitzubringen. Sie ist im Auslieferungszustand aus
  (`.update_checker` im Benutzerverzeichnis) und meldet sich, wenn sie etwas
  findet, nur im Protokoll: an dieser Stelle laeuft die Engine noch nicht, es gibt
  also weder eine Toast-Leiste noch ein Fenster.

- **Kein Absturzfaenger.** Der unter Windows ist SEH, und das gibt es hier nicht.

- **Die Tonaufnahme fuer Videos** laeuft ueber PulseAudio statt ueber WASAPI:
  `@DEFAULT_MONITOR@` ist die Quelle, die mithoert, was die Standardsenke gerade
  ausgibt. PipeWire taugt mit `pipewire-pulse` genauso. libpulse wird zur
  Laufzeit geladen, nicht dazugebunden - der Build braucht kein libpulse-dev, und
  wo kein PulseAudio laeuft, bleiben die Videos stumm wie bisher.

## Gross- und Kleinschreibung

Das Spiel loest Dateinamen ueber ihren Namen auf. In `data.zip` ist das gleich,
aber die losen Dateien - eigene Levels, Skins, und der Entwicklungsmodus mit
`fs.pushCurrentDir("data")` - liegen auf einem Dateisystem, das unter Linux
zwischen `Sprites.png` und `sprites.png` unterscheidet und unter Windows nicht.
Wer unter WSL baut, sollte das aus dem ext4-Dateisystem heraus tun (`~`) und nicht
von `/mnt/c`: DrvFs ist voreingestellt unterscheidungsblind und versteckt genau
diese Fehler.

## Testen

    LinuxBuild/test/smoke.sh

Startet Xvfb und openbox, laesst das Spiel darin laufen, klickt sich durch Menue,
Optionen und Manager, schaltet ins Vollbild und zurueck, loest ein Bildschirmfoto
aus und beendet ueber Escape. Was es dabei gelernt hat und was daran nicht
offensichtlich ist, steht in den Kommentaren - vor allem, dass Tasten auf zwei
Wegen ankommen und die beiden genau das Gegenteil voneinander wollen.

Anders als im Browser gibt es hier keine Testhaken: der GUI-Baum ist von aussen
nicht zu sehen, und geklickt wird auf Koordinaten.
