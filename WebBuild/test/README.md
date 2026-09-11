# Tests for the browser build

The web build can be driven from Playwright. The game is one canvas: only a
screen coordinate can be clicked, and reading that coordinate off a screenshot
goes wrong regularly - a button is eighteen pixels high, the window is scaled,
and whether it or the element beneath it was really hit is not something the
picture shows.

`../test_hooks.cpp` answers that instead. It puts the GUI tree into
`Module["b5_test"]` as JSON: every element with its rectangle in window
coordinates, and whether it is visible and enabled. The test then clicks on a
name instead of on a number, while the click itself stays an ordinary mouse
click and goes the same way through SDL, Engine and GUI as in the game. Which
element a click on a point would reach is a second export,
`blocks5_testHitAt(x, y)`, and deliberately not a field per element: for a
toggle `containsPoint()` measures the width of its caption, and doing that per
element for every element would be forty thousand measurements at two hundred
elements.

The hooks only read; they change nothing and sit behind
`-DBLOCKS5_TEST_HOOKS`. The shipped build does not hold them.

## Building and running

    cd WebBuild
    ./build.sh hooks              # builds into build-test/ instead of build/
    cd test
    NODE_PATH=/opt/node22/lib/node_modules node smoke.js

If node does not find the module ("Cannot find module 'playwright'"),

    cd WebBuild/test
    PLAYWRIGHT_SKIP_BROWSER_DOWNLOAD=1 npm install --no-save playwright

fetches it into `test/node_modules`, and then `node smoke.js` is enough without
NODE_PATH. The browser itself is not downloaded: it already lies where
`PLAYWRIGHT_BROWSERS_PATH` points.

`harness.js` starts the web server on port 8099 itself and clears it away
again. Screenshots land in `$B5_SHOTS` (default `/tmp`).

If a run hangs, the browser really should be gone afterwards: three swiftshader
instances side by side share the cores, and then a test looks as if it were
stuck although it is only crawling.

## On the phone

`mobile.js` runs the same machinery in Chromium's mobile emulation and loads
`index.html` rather than `blocks5.html` - that is the file that ships, and the
only one that registers the service worker.

    node mobile.js

What is checked is the page around the game: the layout viewport, that the
browser keeps its swipe and zoom gestures to itself, the canvas over the whole
area, the manifest with its maskable icon, the service worker's cache, that a
changed `index.html` still arrives on an ordinary reload, and a reload with the
network switched off. Plus a real tap through `Input.dispatchTouchEvent` that
has to reach `Menu.Options` - `page.touchscreen.tap()` is as useless as
`page.mouse.click()`, because a press and a release in the same millisecond
fall between two logic ticks.

The same tap takes the page fullscreen, and that is checked here too: that
afterwards the root element is the fullscreen one, and that landscape was asked
for. The root and not the canvas, because a browser paints only the fullscreen
element and what is inside it - with the canvas promoted the on-screen pad
beside it is simply not drawn. Whether landscape is *granted* cannot be checked
here: a headless emulation has no orientation that could be turned, and refuses
the lock in any case. The test therefore records the call (`addInitScript` puts
itself in front of `screen.orientation.lock`) and passes it on, so that the
refusal stands - that is what the page has to swallow.

## A test

```js
const h = require('./harness');

(async () => {
  const { browser, page } = await h.launch();
  await h.start(page);                       // click-to-start, dismiss the CRT offer
  await h.clickPath(page, 'Menu.Options');
  await h.expectShown(page, 'OptionsPane.Options');
  await h.clickPath(page, 'OptionsPane.Options.Cancel');
  await h.expectShown(page, 'OptionsPane.Options', false);
  process.exit(await h.finish(browser));
})();
```

`clickPath` gives up of its own accord when the element is invisible or
disabled or something lies on top of it - then the fault is not a slipped
coordinate but in the game.

## What the report holds

    state        name of the game state ("GS_Menu")
    language     "de" or "en"
    filter       the effective upscale filter
    crt          whether anything is warping the picture (then the window
                 coordinates are no longer exact)
    focus        full name of the focused element
    appActive    whether the game believes it has the focus
    paused       whether the game is paused; false outside GS_Game
    actionsDown  the named actions that are down right now
    mouseDown    full name of the element the mouse is pressed on
    cursor       where the game sees the cursor
    screen       the internal picture, always 640x480
    display      the canvas
    present      where in the window the picture is drawn
    elements     per element: path, type, rect (game coordinates),
                 win (window coordinates), visible, shown, active

## What a frame costs

    WebBuild/build.sh hooks && node test/perf.js
    B5_WINDOW=30 B5_REPEATS=5 node test/perf.js
    node test/perf.js "?texunits=0" "?texunits=1" "?texunits=8"

Every number is milliseconds of wall clock on the main thread, out of
`FrameStats` in the game and through the test hook's `frames`. `render` and
`present` are what *issuing* the draw calls costs, not what drawing them does.

**In the browser nothing here sees the GPU at all.** `SDL_GL_SwapBuffers` is
`Browser.doSwapBuffers?.()`, undefined off a worker, so `swap` measures 0.00;
`glFinish` after render returns in 0.02 ms; and the page composites the canvas
after the callback returns, outside every window this can time. What is left is
exactly main-thread CPU, which is the half that starves the audio and the only
half a setting like `GL_MAX_TEXTURE_IMAGE_UNITS` can move. It is no measure of
the hardware: **`interval` minus `total` is what is left for that**, so a frame
rate that falls while `total` stays flat is time going somewhere this cannot
see.

(Natively it is the opposite - the driver flushes inside `present` or `swap`,
whichever it picks, so those two are one number there and mostly the
rasterizer. `../../CLAUDE.md` has the measurements.)

Two things about the method are the point of it:

- **The arms are query strings, not builds.** `pre.js` reads the knobs off the
  address, so both sides of a comparison are the same binary in the same
  browser and nothing about the link can differ between them.
- **They are interleaved, not run in blocks.** A machine that warms up or
  throttles part-way through then hands that to both arms instead of to one.

The scene is the menu's own title demo: a whole level animating plus the GUI,
with no navigation to go wrong. It is not deterministic - bombs go off when
they go off - so the report gives every repeat and the spread between them, and
says outright when a difference is smaller than the spread within a single arm.

`emscripten_get_now()` is coarsened by the browser to about a tenth of a
millisecond, which is why the numbers land on those boundaries. A difference of
one step is quantisation and not a result; the texunits comparison moved five.

A knob that changes the timing has to be shown not to change the picture. The
level editor is the scene for that - the busiest screen that does not animate -
and two screenshots of it under the two arms should be pixel for pixel the
same.


## A pointer that teleports

    B5_SHOTS=/tmp/blocks5-editor node editorstroke.js

The level editor interpolates between the previous cursor cell and this one, so
that dragging faster than the events arrive still leaves a continuous stroke.
That makes it the one place where a **finger** and a mouse are not the same
input: a mouse cannot lift at one corner and press at the other without moving
across everything in between, and moving is exactly what keeps the previous cell
current.

`page.mouse.down()` presses wherever the last `move` left the pointer, so
Playwright's own API can never produce the case. CDP can:

    cdp.send('Input.dispatchMouseEvent',
             { type: 'mousePressed', x, y, button: 'left', buttons: 1, clickCount: 1 })

with no `mouseMoved` before it. That is the shape a touch delivers, and the game
sees it as one too, because both button events take their position from the
event rather than from the last motion.

The verdict comes off the picture, since no hook reports the level's tiles and
the canvas is WebGL without `preserveDrawingBuffer` - `drawImage` after a frame
hands back an empty image. `count_painted.py` reads the screenshot instead and
counts the 16x16 cells that differ from the median of all of them, which
calibrates itself against whatever the empty field looks like rather than
naming a colour.

Two strokes of three cells are drawn far apart. Both must paint about four
cells; a stroke that paints thirty has drawn a line back to where the last one
ended.

## Measuring an effect in the picture

The hooks see only the GUI tree. Whether an effect in the level is really
drawn is something only the picture says - and for that a demo is already
running in the menu, in which Bob walks over switches and panels.

    B5_SHOTS=/tmp/burst node burst.js panel 45 400

Afterwards measure the tile something is meant to happen on, instead of looking
at the screenshot. A level lies in tiles of 16 pixels, the game always draws
into 640x480, and the screenshot comes out of that:

    game pixel        = tile * 16           (in the menu additionally +65 in y,
                                             the glTranslate in GS_Menu::render)
    screenshot pixel  = game pixel * s + b  (s and b from the canvas)

Do not guess `s` and `b`: look for the black bars in the screenshot. At 800x640
rows 20 to 619 are not black, so s = 1.25 and b = 20.

An effect that lasts two tenths of a second falls between two screenshots -
under swiftshader a screenshot costs about that long. So for the experiment
turn the decay constant up (`FLASH_DECAY` in `object.cpp` from 0.8 to 0.995),
rebuild, measure, put it back: the way through the drawing code is the same, it
only takes ten seconds instead of a fifth of a second. That is how the panels'
flash was seen to arrive - the additive pass runs only on the layer the object
drew its sprite on, and panels draw on layer 0 and not on 1 like the switches,
which is what `flashLayer` in `panel.cpp` is for.
