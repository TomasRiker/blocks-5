---
paths:
  - "WebBuild/**"
---

# The browser build's page, its caching and its lifetime

**The browser build links against Emscripten's plain WebGL library and nothing else** - no
`-sLEGACY_GL_EMULATION`, no GL shim of the tree's own. Every GL call in the game is WebGL 1 core or one of
the `glExt*` names `glextensions.h` declares, the renderer and the present filters draw through programs of
their own, and a fixed-function call that slips into a source fails the browser link as an undefined
symbol, which is a better failure than a picture that is wrong only here. The renderer redesign (ROADMAP
54) is how it got there; `rendering.md` has the rule.

**The page around the browser build is `WebBuild/shell.html`**, not Emscripten's generated one, and
everything in it is there because a phone needs it. `<meta name="viewport" content="width=device-width,
...">` is the important one: without it a phone lays the page out at a ~980px virtual viewport and scales
the result down, which puts a double-tap zoom in front of every button and keeps the legacy 300 ms click
delay. `touch-action: none` and `overscroll-behavior: none` stop the browser taking a swipe for scrolling
or pull-to-refresh. `pre.js` keeps the drawing buffer in step with the element, on `orientationchange` and
on `visualViewport` resizes too — that is how a phone reports the address bar sliding away. The page also
handles `webglcontextlost`, a real event when a tab goes to the background, by saying so instead of
freezing: the game cannot rebuild its textures and its FBO from where it stands.

**Every function key belongs to the game, not to the browser.** `pre.js` cancels the browser's default
for F1 to F24 in the capture phase, ahead of every other listener, and SDL still receives the key: they
are bindable actions like any other key and the
desktop build answers to all of them — a player who knows the game must not find half of them missing, and
taking a named few would be the worst of both. Left alone, F1 opens the browser's help, F5 reloads the page
and loses the level, F10 reaches for the menu bar, F11 goes fullscreen and F12 opens the developer tools.
Nothing is lost: Ctrl+R and the address bar still reload, Ctrl+Shift+I still opens the tools, and
fullscreen is Alt+Enter as on the desktop. Whether a browser hands a page F11 and F12 at all is its own
decision; asking costs nothing where the answer is no.

**Which is why the click prompt names Alt+Enter.** The first gesture takes the fullscreen, but the way back
into it on a keyboard is that chord, which nobody guesses unaided, so `$WEB_FULLSCREEN_HINT` sits under
`$WEB_CLICK_TO_START` in the tooltip font — an aside, not the message. Not where the on-screen pad is up,
which has a button for it and no Alt to press; `Engine::isPadShown()` is the one C++ place that asks, and
`window.md` has the whole fullscreen story.

**The boot screen is pixel art too, and its line is the game's own.** It shows `$LOADING` from
`data/languages.txt` — the same sentence the game puts up a moment later — in the game's own font, which
the page cannot render itself: it stands before `data.zip` and before any GL context.
`WebBuild/make_text.py` draws it at build time straight out of `data/font.xml` and `font.png` (the same
glyph rects, advance and two-tap shadow `Font::renderText` uses) and `build.sh` stamps both languages into
the page as data URIs, so the line is there with the first paint and costs no request. The page blows it up
by a whole factor, 3 dropping to 2 or 1 where the line would not fit — replication at an integer factor,
never a resize, the same rule as the icons. The bar fills in whole 12px blocks, and the icon is shown at
5x32 with `image-rendering: pixelated`. Which language is decided the way `Engine::detectSystemLanguage`
decides it, by the same walk over `navigator.languages`.

**The click-to-start goes through the moment the gesture arrives**, and does not wait for the AudioContext.
`resume()` returns a promise, and on a phone — where the fullscreen request turns the screen — it can take a
second or two to settle; waiting for it left the line pulsing, which reads as "the tap did not register" and
gets tapped again. `GS_Loading` starts the logo intro at once instead, and the jingle waits its turn: it
hangs off `time >= 1000`, so there is a second of slack, and if the context is still suspended by `time >=
2000` the jingle is given up rather than fired into the menu. Measured with `resume()` stubbed out to never
settle: the menu comes up 3.0 s after the tap, which is the intro and nothing else.

**It installs.** `manifest.json` (fullscreen, landscape) and `sw.js` make it an ordinary add-to-home-screen
web app that launches without the address bar and runs offline; that is also the answer to iPhone Safari,
which has no element-level Fullscreen API.

**The payload filenames carry the build's stamp** — `blocks5-<hash>.js`, `.wasm`, `.data`, the hash being
the md5 of the three — and that is the load-bearing part of the whole caching story. They must never be
mixed: the JS holds absolute byte offsets into the data, and its `EM_ASM` fragments sit at addresses that
fit exactly one wasm. Served in mismatched pairs the game aborts with *"No EM_ASM constant found at address
…"*, which is what a real deployment did when **mod_pagespeed** kept `blocks5.js` under a rewritten name of
its own and later handed it out beside a newer wasm. With the stamp in the name every such URL is immutable,
so no cache anywhere can produce the mixture. `Module.locateFile` in `shell.html` is the one place that
knows the stamp; `build.sh` writes it in after the link.

**The service worker (`sw.js`) therefore caches its two halves in opposite directions** — the stamped
payload cache-first, everything else network-first with the cache as fallback, above all `index.html`,
which cannot carry a stamp because it is the entry point and the place the current stamp is written down.
The file carries the whole argument: why `skipWaiting()` and `clients.claim()` are safe, the all-or-nothing
`addAll`, `activate` dropping every cache whose name is not the current stamp, and the `MINE` guard that
keeps an old worker from pulling a new build's payload into its own doomed cache. Registration is the page's
half: `updateViaCache: 'none'`, or a cached `sw.js` would keep a stale worker alive indefinitely, and then
`registration.update()` straight away rather than trusting how promptly the browser gets round to its own
check.

**`touch_controls.js` carries a stamp of its own**, `touch_controls-<hash>.js`, the md5 of that one file
rather than the payload's. Reusing the build stamp would be worse than leaving it unstamped: it hashes the
three payload files, so a pad-only edit would not move it, the URL would not move either — and the file
would then be served `immutable` for a year instead of a few heuristic hours. Measured: editing the pad
moves `8cb19d724ef9` → `3aa14d7f1448` while `blocks5-164c6a033fd7` stays put, so a 17 KB change drags no
part of the 13 MB payload with it. `build.sh` rewrites the name in both pages and substitutes `%%PAD%%` into
`sw.js`, as it already does for `blocks5.js` and `%%BUILD%%`.

**Two files can never carry a stamp**, which is why the header half still exists: `index.html` is where the
stamps are written down, and `sw.js` is registered under a fixed URL — a stamped one would leave the old
worker alive under the old name. `WebBuild/htaccess` ships as `.htaccess` beside `index.html`: a year of
`immutable` for anything stamped, `no-cache, must-revalidate` for everything that is not — those two plus
`blocks5.html`, `manifest.json` and the four icons — `AddType application/wasm`, and `ModPagespeed off`. The
icons and manifest could be stamped and are not, because they change once in a few years and the manifest
would have to be generated rather than copied to name them.

**That list is every unstamped file and not a chosen few, because the gap is silent.** A file with neither a
stamp nor a rule gets a *heuristic* lifetime in the browser, a fraction of its age, and then goes stale with
nothing to say so — a new build's page and payload arrive, the on-screen pad does not, and only a private
window shows the new one. **The service worker cannot fix that**: a subresource the browser's HTTP cache
still thinks fresh never reaches the worker at all (measured on a reload: `workerStart` 0, `transferSize`
0, `deliveryType` "cache", so `cache: 'no-cache'` inside the worker changes nothing; with the header in
place the same read gives `workerStart` 79.7 and the new bytes arrive). The
worker's network-first branch is what keeps the page working offline; freshness is the URL's job, and the
header's only where there can be no stamp.

**`-sINITIAL_MEMORY` is 48 MiB, and that number was measured.** Started at 16 MiB the heap grows exactly
once, to 40 MiB, and stays there through the loading screen, menu, editors and a played level. Reserving far
more is on a phone the most likely reason a tab dies before the menu appears.
`ALLOW_MEMORY_GROWTH` stays on, so an unusually large level still has room.

**Losing focus takes two answers in the browser, not one.** Emscripten's SDL reports focus and visibility as
**`SDL_WINDOWEVENT`** — an SDL 2 shape — and never sends the `SDL_ACTIVEEVENT` the game switches on, so
`mainLoopIteration` has a second case for it under `__EMSCRIPTEN__`. Both funnel into `handleAppFocus`,
which mutes, forgets every held key, stops a running recording and tells the game state.

That still leaves the audio, because a *hidden* tab gets no `requestAnimationFrame`: no logic tick runs, so
the queued event is never even polled and the mute is applied by a pass that has stopped. `pre.js` therefore
suspends the `AudioContext` on `visibilitychange`, one layer below the engine, which freezes every source at
once. Without it the music dies on its own when its queue runs dry while every looping effect — a laser above
all — keeps sounding in a tab nobody is looking at.

`appActive` is an `Engine` member rather than a local in `mainLoop` because `emscripten_set_main_loop_arg` calls
one iteration per frame, so nothing may live on the stack between them — and because the test hook reports
it, which is what makes any of this checkable.

**Saves ask to be kept.** They live in IndexedDB through IDBFS, which a browser may evict when short of room;
`navigator.storage.persist()` in `pre.js` asks for that not to happen. The browser grants it silently once
the page looks like something the user meant to keep and otherwise refuses, which costs nothing.

**In the browser the program never ends, so nothing is ever destroyed.**
`emscripten_set_main_loop_arg(…, 1)` asks for the simulated infinite loop, which unwinds the stack with a
JavaScript `throw` — so `Engine::mainLoop` does not return, the `engine.exit()` standing after it in `main()`
never runs, and no destructor runs either: not `~Engine`, not the game states that are locals of `main()`,
not a `Level` and not an `Object`. The unwind is a JS exception and not a C++ one, so it runs no destructors
on its way out. **Anything that has to happen must therefore hang off something that runs *during* play** —
a logic tick, `onLeave`, `onRemove` — and never off teardown. Two consequences visible from outside:
`config.xml` is written only where somebody asks for it (the options dialog's OK, and the CRT pane's *Try
it*) and never on quit, so a browser player who never opens the options has the language detected afresh at
every start; and every GL object the Engine owns is left to die with the page.

**The flag is load-bearing and must not be tidied away.** Emscripten ends the call with `throw "unwind"`, and
`callMain` swallows it without restoring `__stack_pointer` — so the abandoned frames stay above it and every
later `requestAnimationFrame` allocates below them, which is exactly what keeps `main()`'s locals alive. Its
three game states are such locals, and `Engine::registerGameState` keeps raw pointers to them. Passing 0
instead breaks two things at once: `mainLoop()` would return, so the `engine.exit()` after it would tear the
engine down *before the first frame* and the loop would run against the wreckage; and `main()` would return,
destroying the game states the Engine still points at. Getting rid of the flag is a restructure — game states
off the stack, `exit()` moved into the quit path — not a one-word change. To persist settings in the browser,
a `pagehide` handler calling `saveConfig()` is the smaller answer, and it catches a closed tab, which the
Quit button never sees.

**The browser's Quit button** cannot quit — a page does not close its own tab — so it draws a Windows blue
screen instead (`WebBuild/web_bluescreen.cpp`), hooked into the one `SDL_QUIT` case in
`Engine::mainLoopIteration` so the menu button, Escape and the editors all reach it. It mutes OpenAL, builds
a DOM overlay above the canvas (leaving fullscreen first, or the overlay would sit behind it) and calls
`emscripten_cancel_main_loop`. Any key, click or touch after a 700 ms arming delay reloads the page.
