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
| `sounds` | einen Klang, den `playSound()` beim Namen nennt, ohne dass `gs_loading.cpp` ihn vorlaedt - er bleibt dann stumm |
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

Daneben liegt ein Skript, das eine ausgelieferte Datei herstellt, statt sie zu
pruefen; es ist reine Standardbibliothek:

    python3 Tools/make_ico.py Blocks5/data/window.png Blocks5/src/icon1.ico

`make_ico.py` baut das Programmsymbol fuer Windows; die `.ico` ist eingecheckt,
weil der Windows-Build kein Python laufen laesst, und die Pruefung
`windows_icon` haelt sie aktuell.


Klaenge
-------

Zu jedem Effekt liegt in `Blocks5/data` die WAV als Quelle neben der Ogg, die
ausgeliefert wird - `pack.sh` nimmt nur `*.ogg` mit. Wer eine WAV aendert, muss
die Ogg neu erzeugen:

    ffmpeg -y -i Blocks5/data/<name>.wav -c:a libvorbis -b:a 96k \
           -map_metadata -1 Blocks5/data/<name>.ogg

`-b:a` landet unveraendert als Nennbitrate im Vorbis-Kopf, und 96 kbit/s ist,
was der groesste Teil des Bestandes traegt. Weniger ist fuer kurze Effekte eine
Falle: bei 45 kbit/s ueberschwingt der Dekoder von `cannon_turn.ogg` auf 1,03
und uebersteuert damit beim Abspielen, und das Quantisierungsrauschen des
letzten langen Blocks (2048 Samples, 46 ms) steht am Ende der Datei noch bei
-46 dBFS statt bei -88.

Zwei Dinge gehoeren in die WAV selbst, nicht in den Kodierer. Anfang und Ende
muessen auf null liegen - eine halbe Kosinuswelle ueber 4 ms hinein und 60 ms
hinaus, laenger als ein langer Block. Und ein Gleichanteil muss raus: er kostet
Aussteuerung, er knackt an beiden Enden, und wenn eine Huellkurve darueber
gelegt wurde, wandert er mit ihr und ist deshalb auch kein fester Wert, den man
einfach abziehen koennte. Ein Hochpass bei 20 Hz nimmt ihn und laesst alles
Hoerbare stehen.
