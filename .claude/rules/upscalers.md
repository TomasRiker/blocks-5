---
paths:
  - "Blocks5/src/u_*.{cpp,h}"
  - "Blocks5/src/upscaler.{cpp,h}"
  - "Blocks5/src/cf_rewind.{cpp,h}"
  - "Blocks5/src/options.{cpp,h}"
  - "Blocks5/data/options.xml"
---

# The upscale filters

**Four upscale filters, each a class.** `upscaler.h` holds the base — a name, a texture filter, `present()`,
whether it wants a whole-number scale, whether it distorts the cursor, and its own `loadConfig`/`saveConfig`
— plus `PresentContext` (what the Engine owns and lends out: rect, frame texture, shared vertex buffer) and
`PresentProgram` (a linked program and the four uniforms *every* present shader has). They live in
`u_sharp.*`, `u_smooth.*`, `u_sharpfit.*`, `u_crt.*`; `u_all.h` pulls them in and `Engine` owns one of each
in display order. It is a normal game option, saved as `<Upscaler>` with the filter's own `getName()` — the
one name each filter has, shared by the config value, the radio button in `options.xml`, the startup log and
the test hook. `SharpFit` is the default.

- `Sharp` and `Smooth` are just `GL_TEXTURE_MAG_FILTER`, drawn through the base class's pass-through program.
  `Sharp` additionally snaps the blit to an integer scale (`wantsIntegerScale()`), the whole point of it.
- `SharpFit` (`src/u_sharpfit.cpp`) is nearest at a fractional scale, in one texture fetch rather than two
  passes; that file derives why, and why it **must** sample with `GL_LINEAR`.
- `Crt` is a CRT monitor: beam profile, scan lines, phosphor mask, halation, barrel distortion, rounded
  corners, vignette.

All four are a `PresentProgram`: one vertex shader (`upscaler.cpp`, the only place it is read), the filter's
fragment shader — the base class's pass-through for `Sharp` and `Smooth`, their own for the other two — the
shared vertex buffer and `PresentProgram`'s four uniforms; `U_Crt` holds its own nine on top. **No filter carries a uniform it
does not have** — a shared slot table for every filter's uniforms is how `convergence` was once left unset
in two hand-written lists. A failure to link is fatal for all alike: all four are offered unconditionally, so a
driver that will not build one of these shaders cannot be left quietly showing three. The options dialog
ticks the filter in use and leaves the radio buttons where `options.xml` puts them — no show/hide/reflow
loop, no entry that can be missing.

**The CRT filter** (`src/u_crt.cpp`). Everything that gives it its character is a `const` at the top of
that file, meant to be edited, and that file carries the reasoning and the measurements for all of it:
which monitor it imitates (`SCANLINE_PERIOD` — a VGA set at 1.0, the 240p console look at the shipped
2.0), why the mask sits in output and not source pixels, what `getOverscan()` is the sum of and why it
is exactly zero at curvature 0, the convergence model and its per-channel `rasterMask`, the halation
rings, the flicker terms, and what each of them measured.

Three things about it are not that file's business and live here:

- Six of the constants are runtime sliders instead, because they are matters of taste rather than
  tuning: Options → Scaling → *CRT settings …*, saved as `<CrtUpscaler scanline= curvature= bloom=
  flicker= scanFlicker= convergence= rewind=>` in `config.xml`. The seventh, `rewind`, reaches no
  shader: it is how long the restart transition below takes.
- **Nobody finds a filter buried in an options dialog**, so `Menu.CrtPane` offers it once on a first
  start with a button that switches it on there and then. The marker is `.crt_offered` in the user
  directory, the same idiom as `.donation_asked` — absent on a clean install *and* after an upgrade,
  exactly the set of people who have not seen the filter. Skipped where the CRT filter is already in
  use, and it suppresses the donation window for that one start so the two never stack.
- **The barrel distortion goes through the mouse**, so it reaches `Engine`: `Engine::warpToSource` and
  `warpToOutput` forward to the filter, and the base class returns what it was given, so no caller asks
  what kind of filter is on. `CRT_CURVE_X`/`CRT_CURVE_Y` are `#define`d once and stringified into the
  GLSL *and* read as C++ floats, so the shader and the cursor cannot drift apart.

**Restarting a level rewinds the tape**, but only with the CRT filter on: `CF_Rewind` (`cf_rewind.cpp`)
instead of `CF_Slices`, chosen by `crossfadeRestart` in `gs_game.cpp`. On sharp or sharp-fit the game does
not claim to be a tube and a tape effect would be a costume.

**How long it takes is the CRT settings' rewind slider**, and 0 switches it off: the restart then gets the
slices, as without the filter. Above 0 the rewind takes half a second at the lowest step, 1, 1.25 at the
default of 50 and 1.65 at 100, in a straight line between each two (`CF_Rewind::durationFor`): the lower
half rises 15 ms a step and the upper 8. The value is kept in the slider's hundredths, a hand-edited
`config.xml` included (`U_Crt::setRewind`): between 0 and 0.005 the slider shows 0, which says off, and the
rewind must be off too. Nothing else had to learn the length, because everything `render()` draws already
runs on the crossfade's own `t` from 0 to 1: a shorter rewind is the same one played faster — the same ten
screens of roll, the seam over the same path, the arrows in the same three phases. The sound is the one
thing on its own clock, so the constructor fits it by its pitch, 1.5 seconds over the duration: faster and
higher together the way a faster tape sounds, three times the recording's speed at the lowest step, and a
little slower and lower in the full 1.65 seconds.

It exists because a restart is a cut — the game jumps from the current state to the level's first tick with
nothing in between — and a tape in search is the one machine that cuts like that and is forgiven for it.
**Every strip of a searched picture is read from a different place on the tape, which is to say a different
moment**, so a screen made of strips taken alternately from old and new image is not a trick standing in for
frames the game never had: it is what a recorder puts out. The rest follows — no signal between the tracks,
so bands of snow roll through; no lock for the vertical hold, so the picture rolls; the head meets each track
at an angle, so every line starts early or late; and VHS carries colour on a separate low-frequency signal
that does not survive the speed, hence the grey wash.

Two things are load-bearing. The on-screen display must **not** move or fade with any of it — it comes from
the recorder's own character generator, mixed in behind the tape path, and that one steady thing makes the
mess read as a machine. It is the 218x64 strip at (0,112) of `data/misc.png`, word in the left 162 pixels and
two triangles in the 56 beside it, so the blink is a source rectangle rather than a colour: the word every
frame, the arrows on, off and on in thirds of the effect — 0.55 seconds each at full length — hard on and
off, counted from its start so they begin visible. And `ROLL_SCREENS` is a whole number, so the roll offset
lands back on a multiple of the picture height — zero — exactly as the crossfade ends; at 6.5 the picture
would sit half a screen out and jump straight when the effect stops. The last sixth eases tearing, snow and
wash to nothing: the transport braking and the servo locking.

**The sound is a granular resynthesis of a recording of a real transport.** Two attempts at synthesising one
failed, and the lesson is that **a matched spectrum is not a matched timbre**: the second matched the
recording's third-octave curve to a mean error of 1.3 dB and still sounded nothing like a machine, because
the recording carries narrow resonances standing up to 24 dB above its own noise floor and a third-octave
average is precisely the measurement that cannot see them. What ships is overlap-add: 30 ms Hann grains at a
7.5 ms hop, so four deep, each resampled 7.5% short and given ±1.5 dB of its own. Where a grain is *read
from* is what the gesture decides — the recording's own spin-up for the first 0.42 s, a random point in its
long steady stretch for the middle, its brake for the last 0.47 s. The shape is the machine's while no
stretch of the result is a copy of any stretch of the recording. `rewind.wav` is committed beside its `.ogg`
and is the source of record — no script rebuilds it, since that would need the recording. `CF_Rewind`'s
constructor plays it, so picture and sound cannot be had separately. At its own speed it runs 1.75 s, against
the 1.5 of the rewind it was made for, so the run-down is not cut off, and the pitch that fits it to a rewind
of any length keeps that ratio.

**xBR-lv2 was here and is gone**, with hq2x before it: both are edge-directed filters written for
flat-shaded pixel art, and this game's art is airbrushed and photographic. Every decision in xBR is a
`step()` against a threshold, stable when neighbouring texels are identical or plainly different and not when
they sit near it. Nudging a frame by 0–3 of 255 — about what the animated level does behind a semi-transparent
dialog — moved 1% of xBR's output pixels by up to 154, all on glyph outlines, while nearest, bilinear and
sharp-fit moved by exactly what the input moved. That was visible as text flickering.
