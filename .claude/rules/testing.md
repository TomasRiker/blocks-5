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
per logic tick). That catches what a screenshot cannot: on a first start `Menu.CrtPane` covers
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
