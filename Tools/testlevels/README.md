Testlevel
=========

Level, die nicht mitgeliefert werden. Sie gehoeren zu einer Sache, die gerade
untersucht wird, und stehen deshalb hier und nicht in `Blocks5/levels` - was
dort liegt, wird beim ersten Start in den Ordner des Spielers kopiert und waere
damit auf jeder Installation.

Zum Benutzen die Datei nach `My Documents\Blocks 5\levels` kopieren (unter Linux
`~/.local/share/blocks5/levels`); sie steht dann in der Levelauswahl unter
"Einzelne Levels".

`diamondmachine.xml`
--------------------
Fuenf mal sechs Diamantenmaschinen, auf jeder ein Block. Der Strom ist aus, Bob
steht neben dem Schalter - ein Schritt nach links, und alle Maschinen laufen
gleichzeitig los. Jede *Spalte* ist ein anderer Blocktyp, damit zu sehen ist,
wie die Farben verschiedener Bloecke sich machen; die fuenf Zeilen zeigen
dasselbe fuenfmal und sind dazu da, den Effekt nebeneinander zu sehen statt
einmal in der Mitte des Bildschirms.

Unten im Boden steht noch eine sechste Maschine, bei x = 6, und der Block darauf
liegt auf Bobs eigener Hoehe. Der ist zum Abbrechen da: mitten in der Umwandlung
wegschieben, oder den Strom wieder ausschalten. Der Block muss dann
augenblicklich wieder voll deckend sein - ein halbdurchsichtiger Block, der
davonrutscht, waere der Fehler, auf den es hier ankommt -, und die Funken, die
noch unterwegs sind, muessen umkehren statt zu verschwinden.

Bob erreicht diese Maschine in gut einer Sekunde. Weiter weg duerfte sie nicht
stehen: die Umwandlung dauert zwei, und was durch ist, laesst sich nicht mehr
abbrechen.

Ein Hinweis zum Schalter: `Player::move` setzt nach einer Beruehrung eine Sperre
von 20 Takten. Wer die Taste laenger als 0.4 s haelt, schaltet den Strom also
wieder aus.
