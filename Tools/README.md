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

    python3 Tools/strip_xml_comments.py --out VERZEICHNIS Blocks5/data

`strip_xml_comments.py` legt die XML-Dateien ohne ihre Kommentare in einem
Zwischenverzeichnis ab, aus dem dann gepackt wird - die Notizen in den Dialogen
bleiben in den Quelldateien und gehen niemanden etwas an, der `data.zip`
oeffnet. `pack.sh` und `zip_data.bat` rufen es von sich aus auf; ein eigener
Aufruf ist nur zum Nachsehen noetig. Anders als `make_ico.py` laeuft es damit
auch unter Windows - und wo Python fehlt, sagen beide Skripte es und packen die
XML-Dateien, wie sie sind, statt den Build anzuhalten.

Ein Kommentar wird nur entfernt, wenn er seine Zeilen fuer sich hat. Das ist
kein Schoenheitsgrund: in einem Level steht in `<Row>` je Zeichen eine
Kachelnummer, und `<!--` waeren die Kacheln 60, 33, 45, 45 - fuer einen Parser
ein Kommentaranfang wie jeder andere. Eine Kachelzeile hat immer Daten vor sich
auf ihrer Zeile, ein Kommentar in einem Dialog nie. Wer einen doch hinter etwas
anderes setzt, bekommt ihn gemeldet und behaelt ihn im Archiv.


Klaenge
-------

Zu jedem Effekt liegt in `Blocks5/data` die WAV als Quelle neben der Ogg, die
ausgeliefert wird - `pack.sh` nimmt nur `*.ogg` mit. Wer eine WAV aendert, muss
die Ogg neu erzeugen:

    python3 Tools/encode_sounds.py            alle veralteten
    python3 Tools/encode_sounds.py ricochet   nur diese
    python3 Tools/encode_sounds.py --force    alle, ob veraltet oder nicht

Neu kodiert wird nur, was aelter ist als seine WAV, und das ist kein Luxus:
zwei Laeufe ueber dieselbe Quelle liefern nicht dieselbe Datei. Die Ogg-Seiten
tragen eine zufaellige Stromkennung, und mit ihr aendern sich die Pruefsummen
der Seitenkoepfe - vierundzwanzig Byte von neuntausend, bei gleichem Ton. Ohne
die Pruefung schriebe jeder Lauf fuenfundfuenfzig Binaerdateien um.

Das Skript kodiert **eins zu eins**, ohne Pegelaenderung. 96 kbit/s ist, was der
groesste Teil des Bestandes traegt; weniger ist fuer kurze Effekte eine Falle.
Bei 45 kbit/s verschmiert der Kodierer eine Transiente so weit, dass der Dekoder
um Dezibel danebenliegt - `thunder.ogg` lag so 4,9 dB unter seiner Quelle - und
das Quantisierungsrauschen des letzten langen Blocks (2048 Samples, 46 ms) steht
am Ende der Datei noch bei -46 dBFS statt bei -88. libvorbis nimmt 96 aber nicht
bei jeder Abtastrate an: bei 11025 Hz mono ist bei 48 Schluss, deshalb sucht das
Skript nach unten, statt eine Zahl vorzugeben.

**Wo ein Klang leiser sein soll, steht das in `Blocks5/data/sounds.xml`** und
wird beim Abspielen angewandt, nicht beim Kodieren. Die WAV bleibt damit die
unveraenderte Quelle in voller Aufloesung, die Ogg ist reproduzierbar, und der
Kodierer bekommt die volle Aussteuerung fuer dieselbe Rechenlast. Frueher steckte
der Faktor in der Ogg, und weil die WAV daneben lauter blieb, ging die Absicht
beim naechsten Neukodieren verloren - acht Dateien waeren dabei um bis zu 6,8 dB
lauter geworden. Die Pruefung `sound_volumes` haelt die Tabelle mit dem Bestand
im Einklang.

Zwei Dinge gehoeren in die WAV selbst, nicht in den Kodierer und nicht in die
Tabelle. Anfang und Ende muessen auf null liegen - eine halbe Kosinuswelle ueber
5 ms hinein und hinaus, bei sehr kurzen Klaengen entsprechend weniger. **Klaenge,
die in einer Schleife laufen, bekommen keine Blende**, denn dort ist das Ende der
Anfang: conveyorbelt, elevator, gas, laser, mask, rain, thunderstorm, toxic. Und
ein Gleichanteil muss raus: er kostet Aussteuerung, er knackt an beiden Enden,
und wenn eine Huellkurve darueber gelegt wurde, wandert er mit ihr und ist
deshalb auch kein fester Wert, den man einfach abziehen koennte. Ein Hochpass bei
20 Hz nimmt ihn und laesst alles Hoerbare stehen - gemessen kostet er nach
ITU-R BS.1770 hoechstens 0,8 dB Lautheit, waehrend er bis zu 6,6 dB Effektivwert
wegnimmt.

**Bei einem Schleifenklang muss dieser Hochpass zyklisch gefaltet werden.** Ein
solcher Klang *ist* periodisch; linear gefaltet bekommen Anfang und Ende das
Einschwingen des Filters ab, und der Sprung an der Naht war hinterher 10 bis
12 dB groesser als vorher.

Was sich damit nicht reparieren laesst, ist Uebersteuerung: die abgeschnittenen
Spitzen sind weg, und sie wieder unter die Vollaussteuerung zu bringen hiesse,
den Pegel zu senken. Elf Dateien tragen sie noch.
