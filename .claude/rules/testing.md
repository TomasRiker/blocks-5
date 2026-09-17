---
paths:
  - "LinuxBuild/test/**"
  - "WebBuild/test/**"
  - "Blocks5/src/testhooks.*"
  - "WebBuild/test_hooks.cpp"
  - "Tools/testlevels/**"
---

# Driving the game: natively, in a browser and on a phone

## Natively

`LinuxBuild/test/smoke.sh` runs the built game under Xvfb with openbox, clicks through menu, options and
manager, toggles fullscreen, screenshots with F11, quits with Escape. It clicks by element name, not
coordinate: `Blocks5/src/testhooks.cpp` — the same hook the browser uses — reports the GUI tree, and
since there is no JavaScript here the request goes through a file (`$B5_TEST_DIR/request`, answered once
per logic tick). Every request carries a serial and the answer repeats it on its first line, so a late
answer to an ask that had given up is never taken for the current one — which used to read as a click
finding a whole dump "on top" of its button. That catches what a screenshot cannot: on a first start `Menu.CrtPane` covers
everything, so a click on the middle of `Menu.Options` lands on the pane.

**The two input layers want opposite treatment — the trap that costs the most time.** A key the GUI reads
is an SDL *event* and must be tapped, not held: `SDL_EnableKeyRepeat(140, 60)` turns an Escape held for
400 ms into six, the first closing the dialog and the second quitting. A key bound to a named *action*
must be held, not tapped, because `Engine::updateVKs` reads `SDL_GetKeyState`, a snapshot taken once per
20 ms tick, so a press and release in the same millisecond is never seen. The same sampling rule governs
mouse and touchscreen, which is why `page.mouse.click()` and `page.touchscreen.tap()` are equally
useless: move, settle, hold, release. Alt+Enter misleads, hanging off `SDL_KEYDOWN`, and events queue.

**A tapped key is not an instantaneous one.** `b5_key` holds 60 ms rather than calling `xdotool key`,
which presses and releases in about twelve. `SDL_PollEvent` runs once per rendered frame, and under
llvmpipe a frame is a fifth of a second — so a run landing inside those twelve milliseconds sees the
press and not the release, and at the *next* run SDL's repeat, 140 ms overdue, posts a second key-down
before the release is read. Measured, every fifth press arrived twice; at 60 ms none did, eight times out
of eight. A real machine renders at 60 Hz and a real finger holds far longer, so this is the harness
lying, not the game.

`xdotool windowclose` calls `XDestroyWindow` and SDL trips over a window it still believes is its own.
Quit the way a player does — Escape in the menu — or `Engine::exit()` never runs and `config.xml` is
never written.

**Two harnesses on one display take each other down**, and the wreckage reads as a rendering fault rather
than a collision: `b5_clearDisplay` clears the Xvfb an aborted run left behind, and on the shared default
`:99` it cannot tell that server from a live one. It refuses where a `blocks5` is still attached and
names `B5_DISPLAY`, which is what separates two runs. A frame taken while the server was going down is not
evidence of anything.

**The select screen's campaign list keeps the keyboard focus after a click**, deliberately —
`GUI_ListBox` handles the same four keys — and is a trap for a test that picks a campaign then presses End
for the last level: the press goes to the list, which selects its own last entry and therefore *another
campaign* at level 0, with nothing saying so. There is no list of levels to click instead, so take the
focus off it by clicking any of the six buttons, or drive by element name.

**Two windows open themselves over the menu, and `b5_start` writes both markers away.** The CRT offer
appears on a first start and the donation window once enough time has been played — which a machine that
has run the tests often enough reaches on its own. Both are one-shot, so no test touching the menu is
repeatable while they may appear; `.crt_offered` and `.donation_asked` are written exactly as the game
writes them.

**The harness drives `build-test/`; `LinuxBuild/build.sh` without `hooks` writes `build/`.** Building one
and testing the other is an afternoon's worth of a change that appears to do nothing, so `b5_start`
compares the binary against `Blocks5/src` and `data.zip` against `Blocks5/data` and refuses to run on
either out of date. `Tools/selftest.py` puts each file's mtime back along with its bytes, or every run
would trip that check.

## The frame oracle

`LinuxBuild/test/frames.sh` renders nineteen named scenes as 640x480 PNGs meant to be byte-identical
between two runs of one binary, and between two binaries when nothing should have moved. It is what
every stage of `RENDERER-REDESIGN.md` is checked against, so what makes a frame reproducible is worth
knowing before adding one. Three things do it, and every scene needs all three: `B5_SEED` seeds the
generator per rendered frame and per tick, keyed on the scene's clock (`Engine::render`, `Engine::update`,
`seedForLoad`); `freeze <tick>` stops the logic at a named tick of that clock, checked before the tick
runs; `shot <path>` writes what the game read out of its own framebuffer. **The scene's clock is
`Engine::sceneTick`**, which a level, the credits and the logo screen each set from their own count — the
engine's `getTime()` counts from program start and stands wherever the harness's timing put it, which is
exactly what a named tick must not depend on.

**The frozen frame is rendered once more, with `getTime()` pinned to zero.** The caret's pulse, the
editor's marching ants, the contamination's throb and the "Pause" text read the engine clock, so a frame
that merely stopped would carry the harness's timing in those pixels. `TestHooks::frozenFrameDue()` is the
one-shot that asks for that render; the request is answered while frozen, since the harness still has to
take its picture and quit.

**A crossfade's clock moves once per loop iteration and the screen under it once per tick**, and how
many ticks an iteration bunches is the machine's business — a level load alone is a backlog of several.
So a frame in a transition needs two things: `freeze fade <ms>` stops at the first tick at which the running
crossfade has reached that many milliseconds, and `lockstep 1` makes every iteration exactly one tick
(`Engine::mainLoopIteration` throws the backlog away), which pins the fade to the screen behind it. The
`cube` and `star` scenes are that; measured without lockstep the cube froze at level tick 540 on one run
and 500 on the next. The credits need lockstep for a different reason: they draw their own last frame back
into the next one, so their picture depends on how many frames were rendered, not only on the tick.
`state <name>` switches game state by name, which is how the credits and the logo screen are reached.

**Draw calls are counted at the link, natively.** `LinuxBuild/build.sh hooks` links with
`--wrap=glBegin,--wrap=glDrawArrays,--wrap=glDrawElements`, so every one of those from the game's own
objects passes through the wrappers at the foot of `testhooks.cpp`; no header carries the define and no
other translation unit needs it, which is what the `hooks_layout` check protects. The dump reports them
as `draws.calls` over `draws.frames`, and `frames.sh` prints the ratio per scene beside the sprite batch's
own draws and a histogram of what flushed it (`batch.byReason`: raw geometry, a texture change, a blend
change, a render-target switch, an attribute pop, a texture delete, the batch's own edges, a full batch).
In the browser the same key comes from `WebBuild/test/harness.js`, which counts `drawArrays` and
`drawElements` on the WebGL context's prototype — what is left after the GL emulation, the number a phone
pays — and `resetStats()` starts both counters in one evaluate so no frame falls between them.

**The CRT settings button leaves the filter on, and a fresh home cannot take it back.** The button
switches to the CRT filter there and then, and the dialog's Cancel undoes it through `loadConfig()` —
which returns early where there is no `config.xml`, and a harness run starts from a home that has none.
The filter then stays on with its curvature, every later click is warped off its element, and what that
looks like is a scene three steps later failing on a button that "is not visible". The `crt` scene clicks
the previous filter's own radio button before Cancel, under the name the dump reports as `filter`.

Scenes are driven by element name where there is one and by game coordinate where there is not:
`b5_clickAt` and `b5_mouseAt` take a 640x480 coordinate and map it through the present rectangle, which is
how the editor's palette and the pins of its parts are reached; `b5_drag` presses at one point and releases
at another; `b5_type` and `b5_chord` type into whatever has the focus. Three traps found building the
scenes: **Escape closes an open hint note before it opens the game menu**, so `quitLevel` looks whether
the menu came up and presses again if not; the editor's quit asks a yes/no question once the level is
modified, and the scenes that modify it answer it by name; and a scene without a clock of its own — the
editor's level does not tick — asks for `now`, the next tick whatever it is, rather than a tick it could
miss. The select screen is not such a scene: its preview is a level and the level ticks, with the night
vision's noise and the fire's particles in it, so `select` freezes on a tick and the cube crossfade out
of it starts from a frozen one. The script traps its own exit and stops the game and
the X server it started, because a `FAILED` from inside a function otherwise leaves both attached to
`:88`, which the next run refuses to start over.

**Two clocks that come apart, and the order of the scenes follows from them.** The select screen, the
editors and a played level are *pushed* on top of the menu, and the menu's own clock — the one the title
demo's recording and the clouds run on — carries on across the visit while the restored title level's
starts again from zero; after a pop the two stand apart by whatever the harness's timing made of the
visit, and the clouds at a level tick are a different picture on every run. So the menu and its dialogs
are photographed on the first visit, when both start together, and both crossfades — the `star` into the
editor, the `cube` into a level — are started by the hook's `click <element>` request while the screen
they leave stands frozen at a named tick (`b5_transition`): a click that lands on a tick, which no real
click can. The other clock is the level's own: `Level::clear`
resets `sceneTick`, because `Engine::update` seeds a tick on the clock as it stands, and before that a
new level's first tick was seeded on the previous level's last — a gas cloud's first particles then took
different slots from one run to the next, and alpha-blended particles are drawn in slot order. And one
table was built from the shared generator on the first frame that needed it: the toxic effect's noise,
which therefore depended on how many frames the process had rendered by then — it now comes from a
generator of its own with a fixed seed (`Level::renderToxicEffect`), the one place a render path still
drew from `random()`. The last one hid behind the audio clock: `Sound::createInstance` drops a one-shot
that follows the same sound within ten milliseconds of *wall* time, and `Engine::playSound` drew its
random pitch only when the instance came — so two ticks the machine bunched into one iteration consumed
one draw fewer than two it ran apart, and the gas cloud spread differently from one run to the next.
The pitch is now drawn before the lockout is asked. And the order the objects are walked in was the
render's: `Level::render` sorts the object vector by depth and shown position for painting, so a tick
that followed a rendered frame walked a sorted vector and a tick that followed another tick walked the
spawns in the order they were appended — the same random draws went to different gas cells. The particle
dump of the hook (`particles`) is what showed it: the same particles, sixteen pixels apart.
`Level::update` now sorts before it walks — and the sort's last word, the UID, is unique now: spawned
objects were numbered from the *new* last object's UID, which was still zero, so every spawn reused the
load's numbers and two gas cells of one row had no order at all.

## In a browser

`WebBuild/build.sh hooks` builds to `build-test/` with `-DBLOCKS5_TEST_HOOKS`, turning on
`WebBuild/test_hooks.cpp`. The shipped build has none of it — the whole translation unit is inside the
`#ifdef`, and `blocks5_testDump` does not appear in `build/blocks5.js`.

The hook only reads. It puts the GUI tree into `Module["b5_test"]` as JSON — every element with its window
rectangle, whether visible and enabled, plus game state, language and filter — and `blocks5_testHitAt(x,
y)` says which element a click would reach. `WebBuild/test/harness.js` turns that into `clickPath(page,
'Menu.Options')`, and the click stays an ordinary mouse click travelling through SDL, Engine and GUI.

Do not go back to reading coordinates off a screenshot: the buttons are eighteen pixels high, the window
is scaled, and a pane drawn on top looks like a missed click.

Four things about this environment, each of which cost real time:

- **A wasm trap looks like a hang, not an exception.** `page.evaluate` never settles and there is no error
  anywhere. `computePresentRect` divided by a zero `screenSize` before `main()` had run and the
  NaN-to-int cast trapped; `GUI_Element::getFullName()` walked past a null parent. Guard anything the
  hook calls before the engine is up, and bisect a hang by adding stages.
- **`Module.calledRun` never appears** in this Emscripten. Wait for the page-level `runtimeInitialized`
  *and* a dump with a game state and a non-empty element list; the runtime is up well before `main()` has
  built anything.
- **Under swiftshader the game needs about half a minute to reach the menu**, and a frame takes a fifth of
  a second. Never sleep a guessed interval; wait on the reported state.
- **`GS_Loading` waits for a real gesture**, because the AudioContext is suspended until one arrives — so
  a test must click *before* waiting for `GS_Menu`, or it waits forever.

## On a phone

`WebBuild/test/mobile.js` is the same idea in Chromium's mobile emulation, loading `index.html` rather
than `blocks5.html` — that is the file that ships and the only one registering the service worker. It
checks the page around the game: layout viewport the device width and not the ~980px default, nothing
scrolls or zooms, the canvas covers the viewport, the manifest says what an install needs, the worker's
cache holds the payload, and a reload with the network off still boots. **`isMobile: true` in the context
is what makes any of it mean something** — without it Chromium lays out at the window width and the
viewport meta has nothing to do.

The one check about the game rather than the page is a real touch: `touchStart`, a wait, `touchEnd`,
through `Input.dispatchTouchEvent` over CDP. That found the click-ordering bug in `GUI::update()`; the
dump reports `cursor` and `mouseDown` so a tap that does not arrive can be told from a button that does
not react.

**The dump also lists `actionsDown`**, the only window onto the *action* layer from outside:
`Engine::updateVKs` reads `SDL_GetKeyState` and not `keyData`, so whether a key reached the named actions
cannot be inferred from anything else. It established that an on-screen pad can drive the game with an
ordinary DOM `keydown`/`keyup` on the document — a synthetic `ArrowLeft` with `isTrusted === false` shows
up as `["$A_LEFT"]` and clears on `keyup`, where `Engine::setKeyData` would not have worked at all
(ROADMAP item 19).
