# tools

Zwei Pruefskripte fuer die Arbeit am Baum. Beide brauchen nur Python 3 und laufen
in wenigen Sekunden.

## verify.py

Statische Pruefungen. Sie suchen die Sorte Fehler, die beim Bearbeiten
stillschweigend entsteht und die weder der Uebersetzer noch ein Blick auf den
Diff findet.

    python3 Tools/verify.py            alles
    python3 Tools/verify.py --list     die Namen mit einer Zeile Erklaerung
    python3 Tools/verify.py --only gui_paths
    python3 Tools/verify.py --quiet    nur die Zusammenfassung

Rueckgabewert 1, sobald etwas beanstandet wird.

| Pruefung | was sie findet |
| --- | --- |
| `encoding` | ein Nicht-ASCII-Zeichen oder CRLF im Quellcode; fehlendes CRLF in den ausgelieferten Textdateien |
| `project_files` | eine Quelldatei, die nicht in `Blocks5.vcxproj` *und* `.filters` steht - Visual Studio uebersetzt sie dann nicht |
| `naming` | eine Klasse mit Basisklasse, deren Header nicht `kleingeschriebener_klassenname.h` heisst |
| `version` | die vier Stellen mit der Versionsnummer, wenn sie auseinandergehen |
| `gui_paths` | `gui["..."]` und `getChild("...")` mit einem Namen, den kein Dialog-XML kennt |
| `strings` | eine `$ID` ohne Eintrag in `languages.txt`, und Eintraege ohne deutschen oder englischen Text |
| `xml_attrs` | ein XML-Attribut, das geschrieben und nirgends gelesen wird |
| `config` | ein Element der `config.xml`, das nur geschrieben oder nur gelesen wird |
| `ctor_init` | eine neue Membervariable, die der Konstruktor nicht setzt |
| `assets` | einen Dateinamen im Code, den es auf der Platte nicht gibt - oder nur anders geschrieben, was unter Linux ein Ladefehler ist |
| `style` | Leerzeichen statt Tabulator, `if (` statt `if(`, Leerzeichen am Zeilenende |
| `windows_icon` | ein Programmsymbol, das nicht mehr zu `data/window.png` passt, oder dem eine Groesse fehlt, die Windows anfragt |
| `comments` | einen englischen Kommentar zwischen den deutschen; ausufernde Kommentardichte |

Drei Pruefungen - `style`, `ctor_init` und die Kommentardichte - beurteilen
nur, was seit dem Stand vor der Ueberarbeitung dazugekommen ist. Was 2015
schon so dastand und seither taeglich laeuft, ist keine Beanstandung, und es
jedesmal zu melden hiesse, die Ausgabe unlesbar zu machen. Der Vergleichsstand
steht als `BASELINE` oben in `verify.py`.

## selftest.py

Beweist, dass die Pruefungen etwas finden. Eine Sammlung, die immer "in
Ordnung" sagt, koennte laengst an ihrem Muster vorbeigreifen, ohne dass es
auffiele - so war die Attributpruefung anfangs wirkungslos, weil `Attribute(`
auch auf das Ende von `SetAttribute(` passt und damit jedes geschriebene
Attribut als gelesen galt.

Das Skript baut deshalb je Pruefung genau den Fehler ein, den sie fangen soll,
laesst sie laufen und legt die Datei danach byteweise zurueck.

    python3 Tools/selftest.py

## syntax.sh

Uebersetzt jede Quelldatei des Spiels mit `i686-w64-mingw32-g++ -fsyntax-only`. Das ist die
einzige Gelegenheit, den Windows-Code unter Linux durch einen Uebersetzer zu schicken, und
sie kostet eine halbe Minute.

    sh Tools/syntax.sh              alle 118 Dateien
    sh Tools/syntax.sh engine.cpp   nur diese

Die drei Dateien, die dabei aussen vor bleiben - `main.cpp`, `videorecorder.cpp`,
`stackwalker.cpp` -, fallen aus denselben Gruenden auch aus dem Web-Build heraus.
Eingecheckt werden muss nichts: die Kopfdateien, die der Baum mit grossem Anfangsbuchstaben
einbindet (`<Windows.h>`, `<Shellapi.h>`, `<al.h>`), entstehen als Weiterleitungen in einem
Wegwerfverzeichnis.

## Was daneben noch laeuft

    WebBuild/build.sh           uebersetzt und linkt den Browser-Build
    WebBuild/build.sh hooks     dasselbe mit den Testhaken, nach build-test/
    WebBuild/test/smoke.js      fuehrt den Browser-Build durch die Oberflaeche

Siehe `WebBuild/test/README.md`.


Erzeuger
--------

Daneben liegen zwei Skripte, die ausgelieferte Dateien herstellen, statt sie zu
pruefen. Beide sind reine Standardbibliothek:

    python3 Tools/make_ico.py Blocks5/data/window.png Blocks5/src/icon1.ico
    python3 Tools/make_rewind_sound.py Blocks5/data/rewind.wav

`make_ico.py` baut das Programmsymbol fuer Windows; die `.ico` ist eingecheckt,
weil der Windows-Build kein Python laufen laesst, und die Pruefung
`windows_icon` haelt sie aktuell.

`make_rewind_sound.py` erzeugt den Ruecklaufton fuer `CF_Rewind` aus lauter
Sinus und Rauschen - Motor, Wickelpfeifen, Bandrauschen und die beiden
Schlaege. Es schreibt eine WAV; ins Spiel gehoert sie als Ogg Vorbis wie die
anderen Klaenge, und das macht ein Kodierer:

    ffmpeg -i Blocks5/data/rewind.wav -c:a libvorbis -q:a 4 Blocks5/data/rewind.ogg

Eingecheckt ist nur die `.ogg`. Das Skript steht daneben, damit sich der Ton
aendern laesst, ohne ihn neu aufnehmen zu muessen.
