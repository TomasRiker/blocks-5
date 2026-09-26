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

**A request reaches the hook by rename, never by redirection.** The hook polls `request` once a logic tick
and `echo > request` creates the file empty before it writes, so a poll in that gap reads an empty line,
answers with a dump under no serial and deletes the file with the real request in it; `b5_ask` then
waits its five seconds and the scene fails as "could not be written". `b5_ask` writes `request.tmp` and
renames it, as the hook itself publishes `response`, so a poll sees the whole line or no file.

**The harness drives `build-test/`; `LinuxBuild/build.sh` without `hooks` writes `build/`.** Building one
and testing the other is an afternoon's worth of a change that appears to do nothing, so `b5_start`
compares the binary against `Blocks5/src` and `data.zip` against `Blocks5/data` and refuses to run on
either out of date. `Tools/selftest.py` puts each file's mtime back along with its bytes, or every run
would trip that check.

## The frame oracle

`LinuxBuild/test/drag.sh` is the one test that reads the *level* rather than the GUI. The mouse gestures
steer a character, so no widget can be asked whether they worked; the hook's `state` therefore reports
`player`, the cell the active character stands on (`[-1, -1]` with no level running), and `nightVision`,
which is what a light switch does — of the eight switches the one whose effect is a single bit rather
than something to be recognised in a picture. It exists because the feature shipped once without working
at all — the bindings were registered before `Engine::init` had built the virtual-key table — and
because the obvious check does not work: it rains in level 1, so two frames differ by a million pixels
whether or not anybody walked.

It plays **a level of its own**, written by the script for the same reason `frames.sh` writes its scenes:
the geometry *is* the test. A wall five rows tall and open above and below, so that a leg walking west is
stopped by it while one walking north is not — which is what proves a blocked leg hands over to the
other axis rather than leaning on the wall; a switch beside where the character starts and another three
cells off; and a panel under its feet, which must be walked onto and not clicked. In a shipped level all
of that would be whatever happened to lie near the start, and an assertion about it would be a statement
about level 1. The private `XDG_DATA_HOME` is frames.sh's arrangement exactly: it puts the level first in
the single-levels list and keeps the developer's own levels and progress out of it.

`LinuxBuild/test/undo.sh` reads the level editor's `undo` and `redo` depths off the dump, the only
place they show: an undo step that changed nothing looks like any other until Ctrl+Z visibly does
nothing, and by then it has cleared the redo list. The tools, keys and dialogs are worked, most of them
once changing the level and once not; which of the two it was, `GS_LevelEditor::endChange` decides from
the level's XML.

`LinuxBuild/test/frames.sh` renders twenty named scenes as 640x480 PNGs meant to be byte-identical
between two runs of one binary, and between two binaries when nothing should have moved. It is what
every rendering change is checked against, so what makes a frame reproducible is worth
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
(the main loop throws the backlog away), which pins the fade to the screen behind it. The
`cube` and `star` scenes are that; measured without lockstep the cube froze at level tick 540 on one run
and 500 on the next. The credits need lockstep for a different reason: they draw their own last frame back
into the next one, so their picture depends on how many frames were rendered, not only on the tick.
`state <name>` switches game state by name, which is how the credits and the logo screen are reached, and
one word after the name is a boolean parameter set to true in the context the state is entered with.

**The credits are two scenes, because they are two screens.** `GS_Credits::onEnter` runs the ending only
where the shipped campaign has been finished, and a home written fresh per run has no progress in it — so
`credits` says `state GS_Credits full` and gets the ending, and `credits-plain` says nothing and gets
what a player who has not won sees. Seeding a `ProgressDB` with the campaign's 42 levels would be the
other way to reach the first, and the parameter is a line against a fixture.

The two are different pictures and not one with a switch: `credits` is the gradient, the star field and
the motion-blur buffer under `$C_THANKS_FOR_PLAYING` at an animated `charScaling`, which is the one text
in the game nothing caches; `credits-plain` is the *programming* credit on flat black at a scaling held
at 1. Only the first needs `lockstep`, since only it draws its own last frame back into the next one —
which is also why the second costs seconds where the first takes a minute. Running second,
`credits-plain` is a re-entry into a state the run has already left once, which is what proves `onEnter`
starts from nothing.

**Their ticks are not comparable**, 6000 against 4000, and that is the versions and not the scenes: the
plain one's clock starts at 0 where the ending's starts at -2000, since it has no star field to fade up
and nothing to establish, so `sceneTick` (`time + 2000`) begins at 2000 for it and at 0 for the ending.
Both land four fifths of the way through a block's fade-in, clear of the half-way point the fade turns
round on — a frame sitting on that branch would flip between two pictures for a change of a millisecond.

**What neither can see is which key asked for which**, so `smoke.sh` drives the two chords and reads the
answer off the one behaviour that separates the versions: a click or Escape leaves the plain credits,
where the ending takes neither as an exit. Ctrl+Shift+F2 is the plain one and Ctrl+Shift+F3 the ending;
the *modifiers* are held across the key, because `GS_Menu::onUpdate` reads those with `SDL_GetKeyState` —
see the two-input-layers trap above — while the function key itself it reads with `wasKeyPressed()`, the
edge an `SDL_KEYDOWN` sets, so a short press inside the hold is seen however long a frame is taking.

**A transition's own clock is in the dump**, as `crossfade`: milliseconds into a running one, negative
through its lead-in and -1 where there is none — the same number `freeze fade` stops on. It is what makes
the length of a transition measurable rather than something counted under the breath, and `smoke.sh` uses
it for the one bug it was written for: five quick presses of F5 must run **one** transition through, so
the check is not how long that takes — a timing assertion under whatever load the machine is under — but
whether the clock ever runs *backwards* afterwards. It can only do that if a second transition began,
which is the bug exactly. Proved against the code that had it: "the restart transition went back from
679ms to 80ms".

The other two of that family are read the same way, off state the game already reports: `smoke.sh` holds
the pause key for a second and a half and asserts the dump's `paused`, and `drag.sh` — which has a second
character parked in a corner for it — holds Tab and asserts the active cell does not move again, then taps
it three times and asserts it did switch three times. The second half of that one matters as much as the
first: it is what would catch the removal of `GS_Game`'s throttle having made tapping slower. Both were
proved against the actions that repeated. **Neither compares against a cell written down in the file** —
the first character has walked a long way by the time `drag.sh` gets there, so an assertion naming its
starting cell passes whatever Tab does.

**A static text reports how big it is**, and that is what `smoke.sh` asks the help with. The frame around
a help page is a fixed 580x370; what goes in it is written by hand in `languages.txt`, wrapped at display
time, and carries `%BINDING` markers that expand to whatever the player rebound them to — so "it fits" is
not a property anybody can read off the file, and when it stopped being true the only symptom was a row
sliced in half by the frame's bottom edge. The dump carries each `GUI_StaticText`'s laid-out size, measured
through the element's own font by the same `measureDrawnText` its `onRender` and `containsPoint` use, so
the harness compares the game's own answer against the box rather than reimplementing the wrap in shell.
The walk is done twice, the second time with the language switched to German from the options dialog,
since German is the longer of the two everywhere else. It was proved against the geometry that was wrong:
360 px of text in 345 px of box, both languages.

**Draw calls are counted at the link, natively.** `LinuxBuild/build.sh hooks` links with
`--wrap=glDrawArrays,--wrap=glDrawElements`, so every one of those from the game's own
objects passes through the wrappers at the foot of `testhooks.cpp`; no header carries the define and no
other translation unit needs it, which is what the `hooks_layout` check protects. The dump reports them
as `draws.calls` over `draws.frames`, and `frames.sh` prints the ratio per scene beside the renderer's
own draws and a histogram of what ended each batch (`batch.byReason`: a texture change, a blend change, a
scope, a full stream, an explicit flush before a copy, a clear, a 3D draw or a target switch, the end of
the frame, and a `Renderer::DirectGL` bracket opening for raw GL - `perf.md` reads the same keys).
In the browser the same key comes from `WebBuild/test/harness.js`, which counts `drawArrays` and
`drawElements` on the WebGL context's prototype — what reaches WebGL, the number a phone pays: the
renderer's draws and the present's, one to one, with no emulation in between — and `resetStats()` starts
both counters in one evaluate so no frame falls between them.

**The CRT settings button switches the filter on there and then**, and only the dialog's Cancel takes it
back, through `loadConfig()`: every setting starts from its default and then reads `config.xml`, so on a
harness run's fresh home, which has none, Cancel lands on the default filter and not necessarily on the
one that was on. A filter left on with its curvature warps every later click off its element, and what
that looks like is a scene three steps later failing on a button that "is not visible". The `crt` scene
therefore clicks the previous filter's own radio button before Cancel, under the name the dump reports as
`filter`, and does not rest on the default being it.

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

**Two binaries are compared by running the oracle twice, each in a home of its own.** A worktree of the
other commit is built with `Blocks5/pack.sh data && LinuxBuild/build.sh hooks`, and its run
gets `B5_DISPLAY`, `B5_SHOTS` and `B5_FRAMES_XDG` of its own, because two runs cannot share a display, a
shots directory or a home; then `cmp` over the twenty PNGs says which scenes moved, and a pixel diff of
one says where. The tag `render-baseline` marks the last immediate-mode binary, the one the renderer
redesign was measured against, so that comparison can still be made.

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
`#ifdef`, and `blocks5_testDump` does not appear in `build/blocks5.js`. `./build.sh` without `hooks`
writes `build/` and leaves `build-test/` as it was, so `harness.js` refuses a `build-test/` older than
`Blocks5/src` or than what `build.sh` reads from `WebBuild`, as `b5_start` does natively: a smoke or
perf run against the previous build passes for a game that no longer exists.

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
