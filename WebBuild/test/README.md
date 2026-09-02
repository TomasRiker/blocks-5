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

`harness.js` startet den Webserver auf Port 8099 selbst und raeumt ihn
wieder ab. Bildschirmfotos landen in `$B5_SHOTS` (Vorgabe `/tmp`).

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
