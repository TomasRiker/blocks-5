# Tests fuer den Browser-Build

Der Web-Build laesst sich mit Playwright fernsteuern. Das Spiel ist dabei
eine einzige Leinwand: anklicken laesst sich nur eine Bildschirmkoordinate,
und die aus einem Bildschirmfoto abzulesen geht regelmaessig daneben - ein
Knopf ist achtzehn Pixel hoch, das Fenster ist skaliert, und ob wirklich er
getroffen wurde oder das Element darunter, sieht man dem Foto nicht an.

`../test_hooks.cpp` beantwortet das statt dessen. Es legt den GUI-Baum als
JSON in `Module["b5_test"]`: zu jedem Element sein Rechteck in
Fensterkoordinaten, ob es sichtbar und bedienbar ist, und ob ein Klick auf
seine Mitte wirklich bei ihm ankaeme. Der Test klickt dann auf einen Namen
statt auf eine Zahl, der Klick selbst bleibt aber ein gewoehnlicher
Mausklick und geht denselben Weg durch SDL, Engine und GUI wie im Spiel.

Die Haken liest nur; sie aendern nichts und stecken hinter
`-DBLOCKS5_TEST_HOOKS`. Der ausgelieferte Build enthaelt sie nicht.

## Bauen und laufen lassen

    cd WebBuild
    ./build.sh hooks              # baut nach build-test/ statt build/
    cd test
    NODE_PATH=/opt/node22/lib/node_modules node smoke.js

Findet node das Modul nicht ("Cannot find module 'playwright'"), holt

    cd WebBuild/test
    PLAYWRIGHT_SKIP_BROWSER_DOWNLOAD=1 npm install --no-save playwright

es nach `test/node_modules`, und dann genuegt `node smoke.js` ohne NODE_PATH.
Der Browser selbst wird dabei nicht geladen: er liegt schon da, wohin
`PLAYWRIGHT_BROWSERS_PATH` zeigt.

`harness.js` startet den Webserver auf Port 8099 selbst und raeumt ihn
wieder ab. Bildschirmfotos landen in `$B5_SHOTS` (Vorgabe `/tmp`).

Bleibt ein Lauf haengen, sollte der Browser danach wirklich weg sein: drei
swiftshader-Instanzen nebeneinander teilen sich die Kerne, und dann sieht ein
Test aus, als bliebe er stehen, obwohl er nur kriecht.

## Ein Test

```js
const h = require('./harness');

(async () => {
  const { browser, page } = await h.launch();
  await h.start(page);                       // Titelbild wegklicken
  await h.clickPath(page, 'Menu.Options');
  await h.expectShown(page, 'Options');
  await h.clickPath(page, 'Options.Cancel');
  await h.expectShown(page, 'Options', false);
  process.exit(await h.finish(browser));
})();
```

`clickPath` bricht von sich aus ab, wenn das Element unsichtbar oder
abgeschaltet ist oder etwas darueber liegt - dann liegt der Fehler nicht an
einer verrutschten Koordinate, sondern im Spiel.

## Was der Bericht enthaelt

    state     Name des Spielzustands ("GS_Menu")
    language  "de" oder "en"
    filter    der wirksame Skalierungsfilter
    crt       ob die Roehrenwoelbung an ist (dann stimmen die
              Fensterkoordinaten nicht mehr genau)
    focus     voller Name des fokussierten Elements
    screen    das interne Bild, immer 640x480
    display   die Leinwand
    present   wohin im Fenster das Bild gezeichnet wird
    elements  je Element: path, type, rect (Spielkoordinaten),
              win (Fensterkoordinaten), visible, shown, active

## Einen Effekt im Bild nachmessen

Die Haken sehen nur den GUI-Baum. Ob ein Effekt im Level wirklich gezeichnet
wird, sagt allein das Bild - und dafuer laeuft im Menue schon eine Demo, in
der Bob ueber Schalter und Panels laeuft.

    B5_SHOTS=/tmp/burst node burst.js panel 45 400

Danach die Kachel ausmessen, auf der etwas passieren soll, statt das Foto
anzusehen. Ein Level liegt in Kacheln zu 16 Pixeln, das Spiel zeichnet immer
in 640x480, und daraus wird das Bildschirmfoto:

    Spiel-Pixel   = Kachel * 16          (im Menue zusaetzlich +65 in y,
                                          das glTranslate in GS_Menu::render)
    Foto-Pixel    = Spiel-Pixel * s + b  (s und b aus der Leinwand)

`s` und `b` nicht raten: die schwarzen Balken im Foto suchen. Bei 800x640
sind die Zeilen 20 bis 619 nicht schwarz, also s = 1,25 und b = 20.

Ein Effekt, der zwei Zehntelsekunden dauert, faellt zwischen zwei Fotos -
unter swiftshader kostet ein Foto etwa so lange. Deshalb fuer den Versuch
die Abklingkonstante hochsetzen (`FLASH_DECAY` in `object.cpp` von 0,8 auf
0,995), neu bauen, messen, zuruecksetzen: der Weg durch den Zeichencode ist
derselbe, nur dauert er zehn Sekunden statt einer Fuenftelsekunde. So war zu
sehen, dass das Aufleuchten der Panels ankommt - sie zeichnen auf Ebene 0,
nicht auf 1 wie die Schalter, und die erste Fassung liess sie deshalb dunkel.
