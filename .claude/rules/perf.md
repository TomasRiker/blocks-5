---
paths:
  - "Blocks5/src/framestats.*"
  - "WebBuild/test/perf.js"
  - "WebBuild/pre.js"
---

# Measuring a frame

`FrameStats` (`framestats.h`) keeps the last **500** frames — ten seconds at the fifty a second the loop
aims for — and answers p50, p95 and maximum of each column: interval start to start, how long the turn
held the main thread, render, update and present inside it, and the draw calls the renderer made in that
turn. Percentiles, not a mean, because what tears the audio or drops a beat lives in the tail. Recorded
always: four clock reads a frame.

**The draw count rides in the same ring as the timings**, as one more column of the same sample row. It is
a count and not a duration, but everything the class does to a column — the ring, the sort, the
percentile, the threshold — is the same work whatever the column means, so a second ring beside it would
be that code written twice. It is the renderer's own counter (`Renderer::stats().draws`), which runs in
every build, and not the link-time wrappers, which are a test-hooks build only; an iteration that rendered
nothing contributes a zero, exactly as it contributes a zero render time.

**`render` and `present` are what *issuing* the draw calls costs, not what drawing them does.** GL is
asynchronous, so the work is paid for wherever the pipeline is next made to catch up — and where that is
differs completely between the platforms, which is why `swap` is a phase of its own.

**Natively it is somewhere in `present` and `swap`, whichever the driver picks — read those two as one
number.** Measured under llvmpipe with a `glFinish` inserted to find out: a single `present` carrying all
of it read 14.9 ms and was really the level rasterizing; with the finish in place render issues in 2.2 ms
and the finish takes another 5.9. Only a `glFinish` isolates the wait; splitting the swap out just stops
one number pretending to be the blit.

**In the browser nothing here sees the GPU at all.** `SDL_GL_SwapBuffers` is `Browser.doSwapBuffers?.()`,
which exists only on the worker path, so off the main thread it does nothing; measured, the swap is 0.00 ms
and a `glFinish` after render returns in 0.02. The page composites the canvas after the callback returns,
outside every window this can time. What is left is exactly main-thread CPU — the right measure for
anything the game or the JavaScript does, and no measure of the hardware. **`interval` minus `total`
is what is left for it:** a frame rate that falls while `total` stays flat is time going somewhere this
cannot see.

**The overlay counts two things against the logic rate, with different thresholds.** A frame whose
**interval** went over is one the player did not get; one whose **work** went over is one this game is
responsible for. Under swiftshader the browser ran at 38 ms a frame on 2.7 ms of work: counting the work
alone would have reported nothing wrong at 26 fps.

The interval is counted against **two** ticks and the work against one, and that asymmetry is load-bearing.
The loop aims every iteration at exactly one tick — the `SDL_Delay` at the foot of `mainLoopIteration` — so
an interval threshold of one tick sits on the number the code is targeting and a millisecond of timer
granularity trips it: the menu read **277 of 512 frames late while not one had been dropped** (measured
when the window was 512 frames). A frame
actually lost is an interval of two. The work has no such problem, because nothing aims it anywhere; one
tick is simply the budget.

A third count stays at 500 ms: that is what Emscripten's OpenAL has scheduled ahead, so a frame past it is
a hole in the music — browser only, since the native decoder thread fills the queue whatever the main
thread does. It was the *only* count once, which flattered everything: at 50 fps nominal it read `0 of 512`
while a fifth of frames were missing their budget.

One caveat on the work count. `total` includes `swap`, and **nothing in the tree ever asks for vsync** — no
`SDL_GL_SWAP_CONTROL`, no `SDL_GL_SetSwapInterval` — so it is the driver's default. Under Xvfb there is no
vblank and `swap` is real work (6.9 ms by default, 6.8 with `vblank_mode=0`). On a real desktop, where Mesa
syncs by default, that phase would be a *sleep*, and the work count would read a frame that merely waited
as one that overran.

Three ways to read it:

- **`-perf`** / `?perf=1` draws the numbers along the bottom — the phone's only way, since the strip
  lands in a screenshot. **One line in the tooltip font**, the small one, and a black strip only as wide as
  the line: this stands over the game while the game is what is being measured. It reads

  ```
  50fps  ms(50/95/max) 16.3/19.0/69.7  r1.4 u0.1 p8.1 s6.0  draws 22/35/36  500f 1 lost 4>20 0>500
  ```

  — frame rate, the whole frame's three figures, the four phases at their median, the draws' three
  figures, and the window with what went over. It is written to fit at its *widest*, not at its usual: 526
  px of 640 as above and 614 with three-figure milliseconds, three-figure draws and hundreds of frames
  lost, because the numbers grow exactly when something is wrong. Holding `$A_PLANT_BOMB` while it is on
  suppresses the game's own drawing and clears the stats, giving the upper bound of a frame that draws
  nothing.
- **The test hook's `frames`** in the JSON, for a desktop harness, without the overlay's own cost. It does
  not clear on read, because the overlay reads the same numbers continuously;
  `blocks5_testResetStats()` (`resetstats` natively) begins a measurement. Beside it, over the same
  window: `draws` (real draw calls and the frames they were made in — `testing.md` says where each
  platform counts them) and `batch` (the renderer's flushes, the draws among them, the quads they put up,
  and `byReason`, which says what ended each batch - `texture`, `blend`, `scope`, `full`, `explicit`,
  `frame` or `direct`, the last being a `Renderer::DirectGL` bracket opening for raw GL: a copy of the
  frame, a render target, the present).
- **`WebBuild/test/perf.js`** drives the comparison: arms are query strings rather than builds, so both
  sides are one binary in one browser, and they are **interleaved** rather than run in blocks, so a machine
  that warms up or throttles hands that to both. It prints draw calls per frame beside the milliseconds,
  because a renderer change moves that number first and it does not wobble with the machine.

**`?texunits=N` was the first knob riding on this** — how many texture units Emscripten's GL emulation
kept state for, a loop over every unit twice per draw — and it went with the emulation when the renderer
redesign (ROADMAP 54) took the whole emulation out from under the draws. Two things it taught stay
true. A knob that sets a `Module.*` property Emscripten reads has to be named in **`INCOMING_MODULE_JS_API`**,
given whole, since naming the setting replaces Emscripten's default list and `-sFOO+=bar` is a syntax emcc
drops at link without a word; the two knobs left, `?perf=1` and `?flushall=1`, are arguments and need none
of it. And under swiftshader the frame rate is capped elsewhere, so a saving in main-thread time barely
moves the interval; on a phone, where the main thread *is* the limit, it is the same milliseconds either way.

**What the renderer redesign bought, in these numbers**, measured with this method on the same scenes and
ticks before and after, so that a later change is read against them. Draw calls per rendered frame on the
desktop, immediate mode against the renderer: `menu` 47 → 29, `options` 209 → 71, `manager` 256 → 58,
`select` 80 → 28, `night` 55 → 24, `lava` 86 → 24; `credits` stays at 403, one `quads3D` draw per star
under its own matrix. The browser's title demo under swiftshader: main-thread work per frame 1.70 → 1.10
ms, WebGL draw calls 48.6 → 30.6, and the page's JavaScript 30% smaller once the emulation went. What ends
a batch now is the texture — the skin's and the fonts' taking turns down a dialog — and the scissor scope a
window, an edit box or a list opens for its children, which is what `byReason` reports and what the atlas
(ROADMAP 51) would remove at the source. The tag `render-baseline` marks the last immediate-mode binary,
the one all of it was measured against; `testing.md` says how to run the oracle on two binaries.
