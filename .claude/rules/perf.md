---
paths:
  - "Blocks5/src/framestats.*"
  - "WebBuild/test/perf.js"
  - "WebBuild/pre.js"
---

# Measuring a frame

`FrameStats` (`framestats.h`) keeps the last 512 frames' timings — interval start to start, how long the
turn held the main thread, and render, update and present inside it — and answers p50, p95 and maximum.
Percentiles, not a mean, because what tears the audio or drops a beat lives in the tail. Recorded always:
four clock reads a frame.

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
anything the emulation or the JavaScript does, and no measure of the hardware. **`interval` minus `total`
is what is left for it:** a frame rate that falls while `total` stays flat is time going somewhere this
cannot see.

**The overlay counts two things against the logic rate, with different thresholds.** A frame whose
**interval** went over is one the player did not get; one whose **work** went over is one this game is
responsible for. Under swiftshader the browser ran at 38 ms a frame on 2.7 ms of work: counting the work
alone would have reported nothing wrong at 26 fps.

The interval is counted against **two** ticks and the work against one, and that asymmetry is load-bearing.
The loop aims every iteration at exactly one tick — the `SDL_Delay` at the foot of `mainLoopIteration` — so
an interval threshold of one tick sits on the number the code is targeting and a millisecond of timer
granularity trips it: the menu read **277 of 512 frames late while not one had been dropped**. A frame
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

- **`-perf`** / `?perf=1` draws the numbers in the bottom corner — the phone's only way, since the block
  lands in a screenshot. Holding `$A_PLANT_BOMB` while it is on suppresses the game's own drawing and
  clears the stats, giving the upper bound of a frame that draws nothing.
- **The test hook's `frames`** in the JSON, for a desktop harness, without the overlay's own cost. It does
  not clear on read, because the overlay reads the same numbers continuously;
  `blocks5_testResetStats()` (`resetstats` natively) begins a measurement.
- **`WebBuild/test/perf.js`** drives the comparison: arms are query strings rather than builds, so both
  sides are one binary in one browser, and they are **interleaved** rather than run in blocks, so a machine
  that warms up or throttles hands that to both.

**`?texunits=N` is the first knob riding on this**, and it shipped. Emscripten's GL emulation keeps state
for as many texture units as WebGL reports — 8 to 16 — and loops over that count twice per draw call. This
game never leaves unit 0: no `glActiveTexture`, `GL_TEXTURE0` or `glMultiTexCoord` anywhere. `pre.js` sets
`Module.GL_MAX_TEXTURE_IMAGE_UNITS` to 1 by default; `?texunits=0` puts it back to asking WebGL, the arm to
compare against. Measured on the title demo, three interleaved twenty-second runs: median frame **3.20 ms →
2.70**, render half **2.40 → 2.00**, spread within an arm 0.10 ms, picture untouched (0 of 512000 pixels
differ).

Two traps. `Module.<anything>` must be named in **`INCOMING_MODULE_JS_API`** or the start aborts;
`build.sh` passes Emscripten's whole default list plus this key, because naming the setting replaces it,
and `-sFOO+=bar` is not a syntax emcc knows — at link time it is dropped without a word rather than
refused. And the interval barely moved in that measurement, correctly: under swiftshader the frame rate is
capped elsewhere, so the saving shows up as main-thread time. On a phone, where the main thread *is* the
limit, it is the same milliseconds either way.
