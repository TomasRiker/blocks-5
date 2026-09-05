Testlevel
=========

Level, die nicht mitgeliefert werden. Sie gehoeren zu einer Sache, die gerade
untersucht wird, und stehen deshalb hier und nicht in `Blocks5/levels` - was
dort liegt, wird beim ersten Start in den Ordner des Spielers kopiert und waere
damit auf jeder Installation.

Zum Benutzen die Datei nach `My Documents\Blocks 5\levels` kopieren (unter Linux
`~/.local/share/blocks5/levels`); sie steht dann in der Levelauswahl unter
"Einzelne Levels".

`diamondmachine_variants.xml`
----------------------------
Fuenf mal sechs Diamantenmaschinen, auf jeder ein Block. Der Strom ist aus, Bob
steht neben dem Schalter - ein Schritt nach links, und alle Maschinen laufen
gleichzeitig los.

Jede *Zeile* zeigt eine andere Fassung des Funkenflugs: `diamondmachine.cpp`
waehlt sie mit `position.y % 5`, und die Maschinenzeilen liegen auf y = 6, 9,
12, 15 und 18, deren Reste 1, 4, 2, 0 und 3 sind - also alle fuenf, von oben
nach unten in dieser Reihenfolge. Jede *Spalte* ist ein anderer Blocktyp, damit
zu sehen ist, wie die Farben verschiedener Bloecke sich machen.

Unten im Boden steht noch eine sechste Maschine, bei x = 10, und der Block
darauf liegt auf Bobs eigener Hoehe. Der ist zum Abbrechen da: mitten in der
Umwandlung wegschieben, oder den Strom wieder ausschalten. Beides muss den Block
augenblicklich wieder voll deckend machen - ein halbdurchsichtiger Block, der
davonrutscht, waere der Fehler, auf den es hier ankommt.

Ein Hinweis zum Schalter: `Player::move` setzt nach einer Beruehrung eine Sperre
von 20 Takten. Wer die Taste laenger als 0.4 s haelt, schaltet den Strom also
wieder aus.
