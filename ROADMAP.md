Blocks 5 - Roadmap
==================

Planned work, roughly in the order it was proposed. Each entry records what is
actually in the way, with file references where they are known, so the next
person does not have to rediscover it. Nothing here is scheduled.

Finished items are kept as short entries: what was decided, and the parts that
must not be undone by accident. How things work now is in `CLAUDE.md`; this file
is about what to do next and why the done things came out the way they did.


1. Auto-detect the user's language on first start  — **DONE**
------------------------------------------------------------
`Engine::detectSystemLanguage` asks the OS (or `navigator.languages`) and answers
only `de` or `en` — of the 349 IDs in `data/languages.txt` exactly one has a
`§fr:` body and one a `§es:`, so detecting `fr` would give an English game with a
French label. It runs only when `config.xml` has no `<Language>`.

The detection was the small half. A `config.xml` holding nothing but
`<Language>en</Language>` was **tracked in the repository**, copied into the
webroot by `build.sh` and installed by `main.cpp` on first run, and the installer
did the same through `makeconfig.bat` — so English was pinned before any
detection could run, and deleting your own config got an English one back. All of
that is gone; the game writes `config.xml` itself on exit, so nothing ships a
template. Verified in Chromium: `de-DE` German, `en-GB` and `fr-FR` English.


2. Replace HQ2X with something that ships as source  — **DONE**
----------------------------------------------------------------
hq2x is gone — the object file, the `__asm` MMX probe that blocked every non-MSVC
compiler, `useHQ2X`, `upscaleFrame`, the `-hq2x` switch and the `SDL_ListModes`
search that existed only because the filter was locked to exactly 2x. In its
place: an FBO, and `sharp-fit`, a twenty-line shader of our own, in both builds.

**The measurement is the part worth keeping, because both edge-directed filters
tried here failed for the same reason.** Fed a real game frame, hq2x changed more
than 8/255 on **4.85%** of output pixels — 11% on the level title, 2% on the sky.
95% of the frame is plain nearest, because the art is airbrushed and photographic
rather than flat-shaded, so no 3x3 pattern matches. It cost 7.6 ms of CPU per
frame at 640x480 plus two bus transfers, for that 5%.

xBR-lv2 was vendored for a while and removed for the sharper version of the same
problem: every decision in it is a `step()` against a threshold, and this art sits
*near* the thresholds. Nudging a frame by 0–3 of 255 — what the animated level does
behind a semi-transparent dialog — moved 1.15% of xBR's pixels by up to 154, all on
glyph outlines, visible as text flickering. nearest, bilinear and sharp-fit moved
by exactly what the input moved. Two bugs in the libretro GLSL had masked this by
suppressing the edge detection; fixing them made it behave correctly and look
worse, which is the clearest statement there is that the filter does not fit the
content. **Scale2x and a GLSL port of hq2x share the same premise and are equally
moot.** The answer for a nostalgic filter is item 11, which adds a presentation on
top of the art rather than trying to reconstruct detail the art never had.

`sharp-fit` is nearest without the integer-scale restriction, derived rather than
copied (the emulator world calls the same arithmetic "sharp bilinear"): remapping
the texture coordinate through the piecewise-linear function that a nearest-upscale
plus bilinear downscale would produce, so the hardware's own interpolation *is* the
filter and one fetch does it. Checked against a genuine two-pass implementation:
pixel-identical at an integer scale, max channel difference 1 elsewhere.


3. Compile every dependency from source  — **DONE**
---------------------------------------------------
What is left as a Windows binary is `libs/bin/OpenAL32.lib` plus `OpenAL32.dll` —
an import library and the LGPL DLL it imports, which *must* stay a DLL. There is
no compiled code without source anywhere in the tree. TinyXML, zlib + minizip,
libogg, libvorbis, stb_image, SDL 1.2.15, minih264, shine and minimp4 are all
vendored source.

**SDL_image** became `src/img_load.cpp` over stb_image, which also retired a latent
bug: SDL_image 1.2 `LoadLibrary`s its codecs at runtime and asked for
`libjpeg-8.dll`, `libtiff-5.dll` and `libwebp-2.dll`, none of which were in the tree.

**SDL** is all 67 files of the Win32 subset. What that does not change is that SDL
1.2 has been end-of-life since 2012; "move to SDL2" is a different and much larger
project — input, window/GL setup and the event loop all touch it — and worth its own
item if it ever comes up.

**ffmpeg** was used for one thing, an AVI through APIs removed from ffmpeg years
ago, which is why it was pinned at 0.8 from 2011. Four DLLs, seven import libraries
and the `msinttypes` shim are gone. The replacement is H.264 Baseline plus MP3 in a
non-fragmented MP4, from minih264 (CC0), shine (LGPL v2) and minimp4 (CC0) — chosen
because it is the only combination native on Windows *and* Linux. Ogg Theora was
tempting since libogg was already here, and is the wrong target: never shipped in
any Windows, and Chrome removed Theora decoding in 123. AAC has no usable small
encoder (fdk-aac's licence is GPL-incompatible, faac is old and poor).

Two things found while building it, both in the libraries' `PROVENANCE.txt`:
**minimp4 could not actually mux MP3** (it hardcoded the AAC
`objectTypeIndication` and only wrote `esds` when a DSI was set, which MP3 has
not), fixed without touching the library by including its header twice around a
macro redefinition, with `#error` guards and a runtime counter to catch an upgrade
breaking the hook; and **minih264 and minimp4 collide** in one translation unit —
each defines a `bs_t` — hence the two one-line `*_impl.c` files.


4. Build with the newest MSVC  — **DONE**
------------------------------------------
Built and run on **v143** and **v145**. The three `.vcxproj` files ask for
`$(DefaultPlatformToolset)` and `Build.bat` passes no `/p:PlatformToolset` unless
`/toolset:vNNN` names one — a global property cannot be overridden from inside a
project, so passing one always would be a hardcoded version wearing a different
hat. A newer Visual Studio therefore needs no change here.

**v120 and v140 have never been built since the prebuilt libraries went away.**
That they still work was reasoning about the code, not a compiler run. The
`/toolset:` plumbing stays; it costs nothing and is the starting point for whoever
tries. What got the tree off v120: `<hash_map>` out of `pch.h` (41 uses across 12
files became `std::unordered_map` / `unordered_multimap`), the missing
`<algorithm>` includes, `register` out of `MersenneTwister.h`, `const char*` in
`e_flipflop.cpp` / `e_gate.cpp`, and `cannon.cpp`'s `sscanf` type mismatch.


5. Enable a Linux build  — **DONE**
-----------------------------------
`LinuxBuild/build.sh` builds a native binary that runs, plays and passes
`LinuxBuild/test/smoke.sh`. Eight `#error NOT IMPLEMENTED` sites, not the seven
this entry predicted — `transfer.cpp` grew one when the Manager arrived. Five were
simply widening `#elif defined(__EMSCRIPTEN__)` into `#else`, since those branches
were already POSIX. Four needed a Linux answer of their own:

- **The fullscreen switch.** Item 10 built it as a Win32 style flip behind SDL's
  back, and the reason holds under X11: `X11_SetVideoMode` rebuilds the window for
  a mode change and takes the GL context with it. But the X11 analogue is not
  "set the style yourself" — a program asks the window manager with a
  `_NET_WM_STATE` message and the WM decides size and position. That comes back as
  an ordinary `SDL_VIDEORESIZE`, so the Windows architecture carried over exactly.
  It does mean `applyWindowStyle()` must **not** call `handleResize()` on this
  path: the size is not known yet. This is the one piece needing Xlib, and it is
  its own translation unit (`LinuxBuild/linux_window.cpp`) because `<X11/Xlib.h>`
  claims `Font`, `Window`, `Screen` and `Cursor` as type names and the game has
  classes called exactly that.
- **The file dialog.** `zenity` or `kdialog` through `popen()`, so neither GTK nor
  Qt becomes a dependency. Import reads the pipe non-blockingly once per tick, so
  the window keeps drawing — `pollImport()` was built for the browser and took it
  unchanged. Export blocks, as `transfer.h` says it must.
- **The update check.** `curl` or `wget` through `popen()` rather than linking an
  HTTPS client for sixteen bytes. Its prompt only reaches the log: the engine is
  not up at that point in `main()`, so there is no toast and no window.
- **`_stricmp` is MSVC's.** `equalsNoCase()` moved into `util.h` rather than a
  `-D_stricmp=strcasecmp` shim, and stays hand-rolled for the reason it always
  was: `strcasecmp` and `tolower` follow the locale, and in Turkish 'I' is not the
  capital of 'i'. Filenames and switches are the same in every locale.

**The audio capture came out along the line this entry drew**: `AudioRing` holds
the buffer, the reader side and the clock-based silence padding, and only the two
`threadProc`s differ. The Linux one is a third of the size because `pa_simple_new`
is *told* the format and the server resamples. libpulse is `dlopen`'d, so the build
needs no libpulse-dev and the game still starts where PulseAudio is absent. Checked
against a real server at 44.1 kHz so the resample was exercised: audio and video the
same length to 3 ms, and against a simultaneous `parec` the track matched to within
0.7 dB RMS at a 1 ms offset.

**Case sensitivity** was the open risk and is now a `verify.py` check: every asset
filename in the sources must exist on disk with matching case. If this is ever
tested under WSL, put the tree on ext4 under `~` — `/mnt/c` is case-insensitive and
hides exactly the bugs the exercise is for.


6. Skins in the browser  — **DONE**, and skins that travel with campaigns
-------------------------------------------------------------------------
**Done.** Getting a skin in is the Manager (item 17). Getting it to *draw* was the
predicted blocker: WebGL 1 treats a non-power-of-two texture as incomplete unless
it is sampled with `CLAMP_TO_EDGE` and no mipmaps, and every sample then returns
black, silently and with no GL error. The default is `GL_REPEAT`. The fix is two
lines of sampler state but **not** unconditional: `level.cpp` scrolls the texture
matrix without bound for rain, snow and clouds, so those genuinely need
`GL_REPEAT`. `Texture::applyWrapMode` therefore switches only NPOT textures, which
is exactly the set on which `GL_REPEAT` could never have worked. It runs under
Windows too, so a skin does not tile for its author and clamp for everybody else.

**Still open: skins in campaigns.** A campaign archive carries its levels and music
but not the skins they reference, so a campaign built on a custom skin still needs
the skin sent separately. `Campaign::save` is built around a `LevelRef` that knows
whether its source is loose or inside an archive; skins would follow the same
shape. The music half was solved differently — a level says
`musicFilename="blocks:music2.ogg"` and borrows a track from the shipped campaign
instead of carrying a copy.


7. Translate all source comments to English  — **DONE**
--------------------------------------------------------
One sweep, as the entry asked for: 4493 German comment lines across 286 files,
plus the docstrings and printed output of the build and test tooling, plus the
four READMEs. Half-translated files really would have been worse than either end
state, so nothing was left behind — what German remains is data, not prose:
`data/languages.txt`, the inline `"\xA7" "de:…"` strings, the two word lists in
`verify.py`'s `comments` check and the fault `selftest.py` injects into it. The
shipped `readme.txt` files stay bilingual, because they are for players.

**The encoding half had already been done, and it was the dangerous half.** Every
source file is pure ASCII, and the two bytes that are not text at all are explicit
escapes — `'\xA7'` (§) in `engine.cpp` and `'\xB6'` (¶) in `font.cpp`. So the
translation was only a translation: no `/utf-8` switch, no BOM, no encoding
decision to get wrong halfway through.

Two things made a change this size reviewable. A shared glossary was built first,
by reading the whole corpus rather than translating file by file — about a hundred
traps where the obvious word would have been wrong, of which the worst was `Ebene`,
whose obvious rendering *level* would have collided with the class of that name in
the same files. And every file was reduced to its code tokens with whitespace
normalised, before and after, and the two compared: not one byte of code moved
anywhere. The web build confirmed it independently, stamping the same payload
hashes as before the sweep.

The `comments` check that used to catch an English comment among the German ones
now catches the opposite, counting the two word lists against each other rather
than searching for one of them.


8. Rendering performance
------------------------
The renderer is fixed-function immediate mode: 120 `glBegin` blocks across 38
source files, one draw call per sprite, per GUI element, per particle. On the
desktop this is old but survivable; in the browser every one of them goes through
Emscripten's `-sLEGACY_GL_EMULATION`, which rebuilds a vertex buffer per block and
prints "do not expect it to work" on every start. `WebBuild/gl_immediate.cpp`
exists purely to make the game's blocks palatable to that emulator.

The work, in order of payoff:

- **Batch sprites.** Everything drawn through `Engine::renderSprite` shares a
  texture atlas per tileset; accumulating quads into one vertex buffer and issuing
  a single draw per texture would collapse thousands of calls into a handful. This
  is where the big win is, in both builds.
- Then, if it is still worth it, a programmable pipeline for the rest.
- `-msimd128` is not passed by `WebBuild/build.sh`, so the shipping wasm contains
  no vector instructions at all.

The per-frame readback is already gone with item 2; nothing in a normal frame
crosses the bus. `BEGIN_PROFILE` / `END_PROFILE` from `util.h` are available, and
`PROFILE_VIDEO_CONVERSION` / `PROFILE_VIDEO_ENCODING` are existing switches.


9. Stop needing the Visual C++ redistributable  — **DONE**
-----------------------------------------------------------
All three projects build with `/MT`, SDL is compiled in rather than loaded from a
DLL, and `vcredist_x86.exe` (6.5 MB) plus its installer task is gone.
`OpenAL32.dll` is the only DLL beside the executables and imports only
`msvcrt.dll`, which is part of Windows. Nothing allocates on one side of that
boundary and frees on the other, because the game calls only core AL/ALC entry
points. The one thing given up is Windows Update's servicing of the shared CRT,
which for a single-player puzzle game is the right trade against a 6.5 MB stub.


10. A window that behaves like a window  — **DONE**
---------------------------------------------------
Resizable with a kept aspect ratio and black bars, fullscreen by Alt+Return while
running, and the upscale filter switchable from the options dialog. **The FBO from
item 2 is what makes all three cheap**: with the game always rendering 640x480
into an offscreen target, every hardcoded coordinate in the tree stays valid at
any window size, and only the destination rectangle of the final blit changes.

Two rules came out of it that must not be undone, both explained at length in
CLAUDE.md and both discovered in the vendored SDL rather than assumed:

- **SDL's video flags must stay `SDL_OPENGL | SDL_RESIZABLE` for the life of the
  process.** `DIB_SetVideoMode` keeps the GL context only on a fast path that
  requires the flags and bpp to be unchanged and `SDL_FULLSCREEN` to be clear.
  Setting `SDL_FULLSCREEN` or `SDL_NOFRAME` runs `WIN_GL_ShutDown` instead and
  takes every texture, display list and the FBO with it. So fullscreen is a Win32
  style flip plus a size, SDL notices through its own `WM_WINDOWPOSCHANGED`, and
  there is only one code path — "the window changed size".
- **Video and screenshots read `GL_COLOR_ATTACHMENT0` at 640x480**, never the
  window. minih264 needs a multiple of 16, the encoder is configured once at
  `startRecording`, and the filter is a display setting that does not belong in
  the file.

Drawing while the border is dragged needed the one thing SDL cannot give: during a
drag `DefWindowProc` runs its own modal loop and the main loop sits in
`SDL_PollEvent`. `Engine::hookWindowProc` puts a procedure in front of SDL's —
safe because the HWND is created once in `DIB_VideoInit` and never replaced, and
the same subclassing SDL itself does for `SDL_WINDOWID`.

Verified as far as this machine allows: a standalone Win32 program under Wine
confirmed the subclass, `WM_ENTERSIZEMOVE`, a timer firing *inside* the modal
loop, the clean unhook, and `WM_GETMINMAXINFO` clamping a 200x150 request to
648x514. What it cannot show is a long drag or anything about the GL present.


11. A CRT effect  — **DONE**
-----------------------------
The idea that replaced xBR, and a much better fit for what a nostalgic filter is
for: it adds a period-correct presentation on top of the art as drawn instead of
trying to reconstruct detail the art never had, and it is stable where xBR was
not — no thresholds, no edge detection, only smooth functions, so a one-in-255
nudge moves the output by about one. Shipped as `src/u_crt.cpp`, a fourth entry
in Options → Scaling, with four sliders behind *CRT settings …*.

The decisions worth keeping, all of them detailed in CLAUDE.md:

- **Which CRT is one number.** Scan-line gaps are an artifact of 240p. A VGA
  monitor showing 640x480 drew all 480 lines with overlapping beams and had no
  gaps, which is this game's honest reference — `SCANLINE_PERIOD = 1.0`, and at a
  2x window that produces no visible stripes at all. Physically right, useless as
  an effect. The shipped default is 2.0; the constant chooses the look, the slider
  chooses how much.
- **The mask belongs to the glass**, so it is indexed in output pixels. Almost
  every player runs at exactly 2x, where three source-locked subpixels are
  impossible.
- **Brightness is derived, not dialled in.** `MASK_AVG` and `scanAvg` are computed
  from the constants, so editing any of them needs no compensating edit elsewhere.
- **Halation averages in linear light, per tap.** Averaging in gamma space and
  linearising the result produced almost no visible halo, because the ring around
  a bright spot is a mixture and `pow()` on that mixture falls below the
  threshold. Two rings of four taps, not eight on one radius, which gives a
  hard-edged ring rather than a glow.
- **The distortion goes through the mouse**, using the identical formula in
  `warpToSource`; the inverse has no closed form and iterates. The two curvature
  constants are `#define`d once and stringified into the GLSL as well as read as
  C++ doubles, so they cannot drift.
- **The scan-line crawl is the one term computed on the CPU**, because it is a
  ramp whose slope depends on the slider: feeding it the wrapped clock would jump
  the lines at every wrap.

**Convergence** was filed here as "chromatic aberration", which is the wrong name:
that happens in a lens, because glass bends wavelengths differently, and a tube has
no lens. What a tube has is three beams, converged at the centre and drifting apart
toward the rim where the deflection is largest. Green stays put as the reference,
red and blue are displaced in opposite directions in proportion to the horizontal
distance from the centre, and `CONVERGENCE_MAX` is what that comes to at the edge in
source pixels. Sixth slider, `<CrtUpscaler convergence=>`, at 0.5 like the other five.

The two things this entry said to get right both held. It sits in the *source*
sample and not in the halation ring, where it would have tripled the cost of
something nobody can see in a blur; and it is horizontal only, since a vertical
component would need its own two rows and its own beam profile per channel.
`warpToSource` is untouched: a fringe is not a position, so the cursor mapping had
nothing to learn from it.

What it costs is four extra fetches, and with the slider at its default they are
paid: one present measured 25.3 ms at 0 against 30.6 ms at anything above (llvmpipe,
1280x960, so read it against the ratios below rather than as an absolute), a fifth
more, and identical at 0.5 and 1.0 — the shift moves the coordinate, not the work.
`if(Convergence > 0.0)` is uniform across the draw, so turning the slider down hands
all of it back.

That it works at all is measured as the lag of the red channel against the blue one
*within a single frame*, which needs no second frame to compare against and so is
not fooled by the title demo moving underneath: within a tenth of a pixel of zero
everywhere at 0, and −5.3 / −0.3 / +4.4 output pixels at left, centre and right at
full slider.

Cost of one present on a software rasterizer, as ratios: nearest 1.0, bilinear
1.3, sharp-fit 1.35, CRT 7.8 — halation about half of that, and
`BLOOM_STRENGTH = 0` compiles it out (4.2). Those four were taken with convergence
off; on top of the CRT figure it is a fifth more. On any real GPU all of them are
noise.

Left for later: anisotropic curvature (real tubes are not spherical), a shadow-mask
dot triad as an alternative to the aperture grille, and moving halation to a second
pass if the single-pass ring ever looks too tight.

**A VCR rewind when a level restarts**  - **DONE**, and the ring of captures was
not needed. `CF_Rewind` sits beside the other crossfade classes and both restart
paths in `gs_game.cpp` choose it over `CF_Slices` when the CRT filter is the one
in effect - on `sharp` or `sharp-fit` the game does not claim to be a tube, and a
tape effect there would be a costume rather than a consequence.

The design question above - what it rewinds *through*, given that a crossfade has
only two images and both are the same level - answered itself once the physics was
read up rather than guessed at. A tape in search runs faster than the head can
follow a track, so **every strip of the picture is read from a different place on
the tape, which is to say from a different moment**. Two images cut into strips and
interleaved is therefore not a cheat standing in for frames the game does not have;
it is what the machine actually puts out. Everything else follows from the same
fact: where the head crosses between tracks there is no signal, so bands of snow
roll through the picture; the vertical hold cannot lock to that, so the picture
rolls; the head meets each track at an angle, so every line starts a little early
or late and the image frays; and VHS carries colour as a separate low-frequency
signal that does not survive the speed, so the picture goes nearly grey.

One detail is worth keeping: the `<< REW` in the corner must *not* move with any of
it. It comes from the recorder's own character generator and is mixed in behind the
tape path, so it sits rock steady while everything else tears - and that one stable
thing is what makes the mess read as a machine rather than as a broken renderer.

`ROLL_SCREENS` is a whole number for a reason that is easy to miss: the roll offset
therefore lands back on a multiple of the picture height, which is to say on zero,
at exactly the moment the crossfade ends.


12. Tell the player about the hardcoded keys  — **DONE**, it already did
------------------------------------------------------------------------
**The premise was wrong.** `$H_HELP_PAGE1` has listed Alt+Return all along, in both
languages, at the foot of the *Controls (standard)* table. What is worth keeping is
the list of keys that bypass the action system and are read straight from SDL,
which is why they never appear in the options dialog:

| key | what it does | where |
| --- | --- | --- |
| Alt+Return | window / full screen | `engine.cpp` |
| Shift+C | credits | `gs_menu.cpp` |
| Shift+D | turns the donation prompt off for good | `gs_menu.cpp` |
| F (held, in game) | frame time overlay | `gs_game.cpp` |

Of those only Alt+Return belongs in player-facing help. **Return and Enter are two
different keys here**, not two names for one: Return is the big key
(`SDLK_RETURN`), Enter is the keypad's (`SDLK_KP_ENTER`). The fullscreen toggle
tests `SDLK_RETURN` only, which is consistent with the help and `readme.txt`; the
hotel binds both and its text says "Return/Enter" accordingly.

The help is six pages, `$H_HELP_PAGE1` … `$H_HELP_PAGE6` in `data/languages.txt`,
built by page number in `help.cpp` and capped at 6 there. Each page has a `§en:`
and a `§de:` body with light markup, so anything added has to be written twice,
and a seventh page means changing that literal.


13. The particle system's container
----------------------------------
`ParticleSystem` keeps its particles in a `std::list`, one 80-byte `Particle` per
heap node, walked by pointer every tick and every frame. The manual `_mm_prefetch`
pair is there because no hardware prefetcher follows a pointer chain; it earns
3–9%.

**Nothing here is a bottleneck.** At a heavy 5000 particles — the biggest single
burst in the tree is 500 — `update()` costs about 0.05 ms of a 20 ms tick and
filling the render buffer about 0.17 ms of a 16.7 ms frame. This entry exists so
the next person does not re-derive the options and, above all, does not
re-discover the two traps.

The struct is already single precision throughout; doubles would cost 16–33%.

Measured, ns per particle-tick:

| container | order | native 5k | native 20k | wasm 5k |
| --- | --- | --- | --- | --- |
| `std::list` + malloc (today) | preserved | 7.9 | 23.0 | 10.3 |
| `std::list` + pool allocator (LIFO) | preserved | 6.2 | 22.0 | 8.9 |
| `std::vector` + swap-and-pop | **unstable** | 4.2 | 4.6 | 6.7 |
| `std::vector` + stable compaction | preserved | 10.3 | 11.4 | 14.7 |
| `std::vector` + tombstones, compact every 16 | preserved | 3.8 | 5.3 | — |
| ring of buckets keyed by death tick | preserved | 4.2 | 4.9 | 5.9 |

**Trap one: swap-and-pop breaks the picture.** Two of the three systems are alpha
blended and therefore order dependent; only the fire system is additive. What
swap-and-pop produces is not a wrong order but an *unstable* one — a particle
jumps from the end of the array into the middle the instant a neighbour dies, so
the layering of overlapping sprites changes between frames, which reads as
flicker. Over 800 ticks at 5000 particles the survivors' relative order was
unchanged in 800 frames for the list and in **19** for swap-and-pop.

**Trap two: a pool allocator buys much less than it looks like it should.** It
fixes where the nodes are, not the order they are visited in. After 300 ticks of
churn, forward-and-within-128-bytes steps collapse from 100% to 3.0% for malloc
and 0.0% for the pool. Worth about 1.3x at 5000 and 1.02x at 20000, less in the
browser, where Emscripten's dlmalloc already packs same-size nodes densely. Still
the cheapest thing on this list: about thirty lines, never touches draw order, and
gives a fixed memory ceiling. A **FIFO** free list looks better (98.2% forward,
1.64x) and is an illusion — that only holds while the pool is far larger than the
live set. Size it to the maximum, which is the point of having one, and
wrap-around shuffles the free list back to 0.0% forward and 1.16x.

**The one that works: a ring of buckets keyed by the absolute death tick.**
`bucket = deathTick & (WHEEL-1)`, so nothing ever moves. Each tick sweeps buckets
`now` through `now + WHEEL-1` and then `clear()`s bucket `now` — death is O(1) for
the whole cohort and the inner loop has no branch at all. That is why it beats even
swap-and-pop in the browser.

Its order is stable, and the reason is worth writing down because getting it
backwards is easy and silent: **sweep `now` first, not last.** A live particle's
position in the sweep is exactly its remaining lifetime, and every lifetime falls
by one each tick, so no two can swap. Sweeping `now+1 … now+WHEEL` instead puts
the about-to-die bucket at the end and every particle jumps to the back on its
final frame — measured at 19/800 stable, indistinguishable from swap-and-pop.

What it would cost:

- **Memory**: 13.1x the live count in capacity, 5.0 MB at 5000 particles against a
  vector's 0.8 MB, because each of the 512 buckets keeps its own historical peak.
- **`WHEEL` must exceed the longest lifetime.** The longest in the tree is
  `random(150, 300)`, so 512 has room — but anything longer would silently alias
  into a bucket that dies early. Wants an assert in `addParticle`.
- **The `p.size <= 0` early death** becomes a death tick computed at insertion,
  `min(lifetime, ceil(size / -deltaSize))`. Nine emitters have a negative
  `deltaSize`, and float accumulation could put it one tick off.
- **Order becomes remaining-lifetime order rather than creation order.** Stable,
  but different: a burst sharing one lifetime is unchanged, `random(20, 50)` is not.

`getNewParticle()` has no callers anywhere; it returns `&particles.back()`, which
was the only thing requiring stable addresses, so nothing blocks a vector.

One smaller note from the same measurements: in `render()`, `sinf`/`cosf` per
particle is 54–61% of the fill loop, and since `rotation` advances by a constant
every tick the pair could be advanced by a fixed rotation instead of recomputed.


14. An import overwrites, unless the name is one the game ships  — **DONE**
-----------------------------------------------------------------------------
Two halves of one decision. **An import that names a file the player already has
replaces it** — no more swerving to `stem_2` — and **an import that names one of
the seven shipped files is refused.**

The skins already behaved this way and the reasoning generalised: a skin's
filename *is* its identity — a level says `skin0="space"` and
`Level::getSkinFilename` looks for `levels/skins/space.zip` — so `space_2.zip`
would leave every such level exactly as broken, only without a visible cause. The
weaker form holds for the rest: a new version of your level means *your* level.

Two things follow. Re-importing an updated campaign keeps its progress, because
`ProgressDB` is keyed by the campaign filename and the filename no longer moves.
And an import can now silently replace something the player made, so `install`
reports through `bool* p_replaced` and the toast says **Replaced** rather than
**imported** — the only sign they would otherwise get.

`Campaign::installArchive` is gone, and that is the better sign this was the right
shape: with the numbering removed it was letter for letter the generic path.

One ordering detail needed care and has a test of its own: the target name and the
`isBuiltIn` check come first, so `isImportableArchive` runs *after* the name is
known but still before the copy. A damaged campaign carrying the name of a good
one therefore leaves the good one alone — verified byte-for-byte, with
`ERROR: That file is damaged.` on screen.


15. Let the Export dialog delete what it lists  — **DONE**, in the Manager
---------------------------------------------------------------------------
Done as part of item 17, where the button lives, with a confirmation pane and
`Transfer::isBuiltIn` gating it — the same predicate item 14 needs, which is why
it exists once. Before this there was no way to remove anything: custom levels,
campaigns, skins and music accumulated forever, and in the browser there is no
file manager beside the game to do it with.

Both open questions answered themselves. A deleted campaign leaves its
`ProgressDB` rows behind — harmless, and the right thing if it is ever imported
again. A deleted skin does break levels naming it, visibly, through
`Level::loadSkin`'s toast.


16. Reset one control instead of all of them  — **DONE**
----------------------------------------------------------
`Action` already carried `defaultPrimary` and `defaultSecondary`, so
`resetAction(name)` is three lines and `resetActions()` now loops over it. The
work was the UI, and the first attempt — a `Reset:` label with two short words
beside it — came out lopsided: "Reset: selected all" reads fine, "Zuruecksetzen:
ausgewaehlte alle" does not, and sizing for one language wasted space in the
other. What stands is two stacked buttons as wide as the list, each carrying a
whole caption.

**Found while testing: key rebinding did not work in the browser at all.**
`getPressedVK` was a synchronous `while` loop around `SDL_PumpEvents` and
`SDL_Delay(10)`, and under Emscripten nothing can reach the event queue while C
holds the thread, so it always ran its three seconds out and wrote "not assigned".
It is a state machine now — `beginKeyGrab()` / `pollKeyGrab()`, advanced one tick
at a time by the ordinary main loop — which is the same code on both platforms and
leaves the dialog live instead of freezing. See CLAUDE.md.


17. One Manager button instead of Import and Export  — **DONE**
------------------------------------------------------------------
Import and Export were two 50x50 buttons in the main menu; they are one 80x80
*Manager* button opening one dialog that imports, exports and deletes.

Why one and not two: Import had no dialog at all — it opened the picker, worked
out what the file was, copied it and said so in a toast, and the player never saw
the list their file joined. Export had that list and nothing else. Together each
fixes what the other lacked, and a delete has somewhere obvious to live.

**The asynchronous import finally pays off.** `pollImport` runs every tick because
the browser's dialog cannot be modal; with the Manager open, a completed import
switches the kind radio to whatever `classify` decided, re-reads the list and
selects the new entry.

The bottom row went into the radios' four columns rather than three 130px slots —
92px holds every one of the eight captions, the longest being "Import ..." at 66.
One thing found on the way: an Escape with the export pane open quit the game.
Escape now belongs to the topmost pane — confirmation, then Manager, then quit.


18. A switch should flash when it is thrown  — **DONE**
---------------------------------------------
Seven objects react to being touched, and three of them — `cannonswitch`,
`barrageswitch`, `magnet` — have no visible state at all: their effect happens
somewhere else in the level, possibly off the part of the screen the player is
watching, and the switch itself is a still picture before and after. Those are
exactly the ones where feedback is worth the most.

**The obvious implementation cannot work, and that is the thing to remember.**
Brightening each `Sprite`'s colour in `onBeforeRender()` does nothing: the colour
defaults to white, five of the seven use it unchanged, and `renderSprite` ends in
`glColor4dv`, clamped to [0, 1]. There is no brighter than white in a colour
value. What works is **additive**: `Object::render()` draws the sprites a second
time with `GL_SRC_ALPHA, GL_ONE` while a tick counter runs, which accumulates in
the destination and has no ceiling, and still multiplies by each sprite's own
colour so a tinted switch flashes in its own colour.

Where it ended up: `flashAmount` on `Object`, `flash()` to set it,
`FLASH_STRENGTH` / `FLASH_DECAY` at the top of `object.cpp`, decay in
`frameBegin()` (per tick, not per frame), the additive pass gated on
`layer == flashLayer && !shadowPass`. It is deliberately **not** in
`Object::onTouchedByPlayer`: `player.cpp` calls that for every object the player
bumps into, so a default there would light up every block.

Three things the second round turned up:

- **`layer == 1` was wrong.** The eight objects lying on the ground — the five
  panels among them — draw on **layer 0**, so a flash gated on layer 1 drew
  nothing at all for a panel. Hence `flashLayer`, 1 in `Object` and 0 in `Panel`.
- **The activator block flashes too**, since it is the block that did something;
  the switches call `p_obj->flash()` beside their own. Its old `anim` wobble is
  gone with that — it darkened the block on *any* collision, so beside a flash
  that fires only on an activation it read as a competing effect.
- **The day/night switch is the one place the flash is invisible**, and rightly
  so: `crossfade(new CF_ColorBlend(..., 0.1), 1.4)` is 89% opaque on the first
  tick. The switch is doing something far more visible.

Checked in the browser rather than by reading, since reading is what got the layer
wrong: `WebBuild/test/burst.js` walks Bob over two panels with `FLASH_DECAY` raised
so a 0.15 s event cannot fall between two screenshots.


19. Playable on a phone
-----------------------
The browser build runs on a phone smoothly — and cannot be played, because
everything except the menus needs a keyboard. What is wanted is a small pad drawn
over the game: the four arrows, and one button each for planting a bomb, putting
one down, switching character, restarting the level and restarting from the hotel.
Text fields should raise the device's own keyboard instead.

**Pointing already works; only keys are missing.** Emscripten's SDL turns
`touchstart`/`touchmove`/`touchend` into `mousedown`/`mousemove`/`mouseup`, so a
tap already arrives as a click and the whole GUI is operable. What no phone can
produce is a key, and every gameplay action is a key.

**A prototype exists**: `WebBuild/touch_controls.js`, reachable with `?pad=on`. It
puts a four-way d-pad low in the left letterbox bar with Swap as a round button
above it, Bomb and Put as two round buttons in the right one, and Menu, Retry and
Hotel as a small block high on the right where a mis-hit costs nothing. Swap
belongs with the gameplay buttons rather than with those three: it is a move made
mid-level and often, and a finger that misses Retry by one button throws the level
away. Measured on an emulated Pixel 7 with real touches: every control reaches the
action it should and clears on release. Stepping and running need no code — the
movement actions keep `registerAction`'s defaults (`delay 240`, `interval 80`), so
a tap is one step and a held finger runs. Menu is not optional: `GS_Game` opens
its menu on Escape and there is no other way out of a level without a keyboard.

**The page around it takes care of itself now.** The one tap the browser build
already demands — `GS_Loading` waits for the gesture that unblocks the
AudioContext — also puts the canvas into fullscreen and asks for landscape, and
every later touch that finds the document out of fullscreen puts it back. Mobile
Chrome offers no way to do either by hand, and at this size portrait is
unplayable. See `Engine::enforceTouchFullScreen` and `Module.b5_lockOrientation`.

What is still missing is points 3 and 5 below, and a proper place to switch the
pad on other than a URL parameter.

1. ~~**The pad itself.**~~ A DOM overlay above the canvas, drawn in the page
   rather than by the game, so it costs no GL work, can sit in the black letterbox
   bars, and never goes through `-sLEGACY_GL_EMULATION`.

2. ~~**Getting a press into the engine.**~~ `Engine::setKeyData` looks like the
   obvious route and **cannot work for gameplay**: it writes `keyData`, the raw
   layer that `wasKeyPressed`, the GUI and the title demo read. The named actions
   read `SDL_GetKeyState` in `Engine::updateVKs`, which `setKeyData` never
   touches, so movement and bombs would stay dead while the menu appeared to work.
   The demo works precisely because it replays *raw* keys and not actions.

   The route that does work is to dispatch an ordinary `keydown`/`keyup` on the
   document. Emscripten's SDL updates `SDL.keyboardState` from those, so the whole
   action layer works with no engine change at all — including the player's own
   rebindings, since the pad sends a key and the action layer maps it. Two details
   fall out: the press has to be *held* for as long as the finger is down, because
   `updateVKs` samples once per 20 ms tick; and a synthetic event carries
   `isTrusted === false`, which is exactly the flag the "hide the pad when a real
   keyboard is used" rule needs, for free.

3. **Text fields.** `GUI_EditBox` reads `event.keysym.unicode` out of SDL key
   events, and a phone only shows its keyboard for a focused DOM element — the
   canvas is not one. So a real `<input>`, positioned over the field and focused
   when the field is, with what it receives fed back as key events. This is the
   piece with the most edge cases (autocorrect, IME, the keyboard covering the
   field) and the least payoff: it is only needed for naming a level or a campaign.

4. **Hitting things.** See item 22 — the buttons are the problem the pad does not
   solve.

5. **Haptic feedback on the pad's buttons.** A glass button gives a finger nothing
   back: without a click or an edge to feel, the only confirmation that a press
   landed is what happens on screen a moment later. `navigator.vibrate` is the
   whole mechanism — a few milliseconds on `pointerdown`, guarded with
   `if (navigator.vibrate)` so it is silently nothing where the API is absent.
   Three things decide whether it is any good:

   - **Only on the state change, never on the repeat.** A held direction repeats
     every 80 ms; buzzing on each repeat is a continuous vibration, not feedback.
     The d-pad already has the right hook — `setDirection` fires once when the
     direction changes.
   - **It must be switchable off.** Some people hate it, and it costs battery. The
     pad is a page file that knows nothing of `config.xml`, so the cheap version
     is `localStorage`, the same idiom as the `b5pad` key; putting it in the
     options dialog means a bridge from C++ to JS that does not exist yet.
   - **Android only.** Safari on iOS has no `navigator.vibrate` at all. That is a
     reason to keep it optional and small, not a reason to skip it.


20. A freshly built data.zip breaks the browser  — **DONE**, and it was never the zip
--------------------------------------------------------------------------------------
Packing `data.zip` from the current tree made the Emscripten build stop finding
skins and fonts: 28 console errors of the shape `Could not load resource ""`. The
empty names said the lookup failed, not the file. The native build read the same
archive without a murmur, and Python's `zipfile` checked every CRC.

**The archive was innocent. `WebBuild/build.sh` was lying about the link.** It
piped `em++` through `tail`, so the status it tested belonged to `tail`, and the
check after it only asked whether `blocks5.wasm` existed — which it did, from the
run before. From `26903c0` on the browser link was genuinely broken, and every
build during the investigation printed `### LINK OK ###` over it.

What makes that corrupt the *data* rather than simply run old code is where the
file packager sits: `em++` writes `blocks5.data` **before** `wasm-ld` runs, and the
table of absolute byte offsets into it lives in `blocks5.js`. A failed link leaves
a fresh `.data` beside a stale `.js`, so every preloaded file is sliced at the
previous archive's offsets — and because the entries are consecutive, a `data.zip`
one byte off shifts everything after it. Every "ruled out" in the old version of
this entry falls out of that: both packers fail, a byte-identical archive passes,
padding to within 900 bytes does not help, and a *larger* archive fails differently
because the slice runs past the end of the buffer.

Fixed in `fbe8fba`: `build.sh` reads `${PIPESTATUS[0]}` and exits 1.

**The lesson is the build script, not the packer.** A check that can pass on a
previous run's artifact is worse than no check, and it cost a day of looking at
zip files.


21. The browser build on a phone  — **DONE** for the page, item 19 owes the controls
--------------------------------------------------------------------------------------------
It ran on a real phone before any of this — smooth graphics and sound, the GUI
usable but fiddly. The page around it was still the one Emscripten generates,
which is a desktop page. What was wrong:

- **No `<meta name="viewport">`**, so a phone laid out at a ~980px virtual
  viewport and scaled down: wrong canvas from the first frame, a double-tap zoom
  in front of every button, and the legacy 300 ms click delay. `shell.html`
  replaces the generated page and adds `touch-action: none` and
  `overscroll-behavior: none` with it.
- **`-sINITIAL_MEMORY` was 256 MiB** with `ALLOW_MEMORY_GROWTH` already on.
  Measured, the heap grows once to 40 MiB and stays there through a played level,
  so it is 48 MiB now — on a phone that is the difference between a tab that lives
  and one that does not.
- **Nothing installable and nothing cached**: 14 MB re-downloaded every visit.
  `manifest.json` and `sw.js` make it an add-to-home-screen app that works
  offline, which is also the answer to iPhone Safari having no element-level
  Fullscreen API. The worker can never serve one build's `.js` beside another's
  `.data`; see item 20 and the head of `sw.js`.
- **Saves could be evicted**: `navigator.storage.persist()`.

**Two real bugs in the game came out of the touch test**, and neither had anything
to do with the page. `GUI::update()` recomputed `p_elementAtCursor` at the
*bottom*, so a click went to whatever had been under the cursor at the end of the
previous logic tick — invisible with a mouse, since you cannot click where the
pointer is not and there is always a tick between arriving and pressing, but a
finger has no such gap. And `Engine` only took the cursor position from
`SDL_MOUSEMOTION`, which a touch never produces. Either fix alone changes nothing;
together they are why a tap lands. That was the whole of "the buttons were a bit
difficult" — the hit areas turned out to be innocent.


22. A finger is not a point: hit testing with a tap radius
----------------------------------------------------------
Item 21 got taps to land where the finger is. What it did not do is make the
targets big enough for a finger: `GUI::getElementAt` tests a single point, which
is right for a mouse and wrong for a fingertip whose contact patch is eight to ten
millimetres across. The accessibility guidance everybody uses is a minimum target
of about 44 CSS px.

**The numbers say how far off it is.** The GUI is laid out in the game's 640x480
space — buttons are eighteen pixels high, the Manager's bottom row is four 92x20
buttons — and that space is letterboxed into the canvas. On an emulated Pixel 7 in
landscape the present rect is 549x412, a scale of 0.858, so an eighteen-pixel
button is **15 CSS px** on the glass: about a third of the recommended minimum. On
a narrower phone it is worse, because the scale is `min(w/640, h/480)` and the
height usually binds.

**What unit the radius lives in.** It is a property of a finger, so it belongs in
CSS pixels, and the conversion into game coordinates is the inverse of the present
transform:

    scale      = pw / 640.0        // from Engine::computePresentRect
    radiusGame = radiusCss / scale

That is the "the smaller the game renders, the bigger the circle has to be"
intuition, falling straight out of the transform rather than needing a second
rule. It works because `b5_fitCanvas` sizes the drawing buffer in CSS pixels, so
canvas pixels and CSS pixels are the same thing here — **if that ever becomes a
device-pixel-ratio-sized buffer, this formula changes with it.** With the barrel
distortion on the radius is not constant across the picture; near the edge it
should be divided by the local derivative of `warpToSource`, or simply left alone,
since the CRT filter is a desktop indulgence.

**The sampling idea**: lay a fixed grid over a disc of that radius around the tap,
run the ordinary `getElementAt` at each sample, and count the votes. It inherits
everything the point test knows — z-order, and `containsPoint` being virtual so a
checkbox is hit on its caption too — and a 5x5 or 7x7 grid clipped to the disc is
21 to 37 lookups once per tap, which is nothing.

Four things to get right, three of which "highest count wins" gets wrong:

- **A large element must not outvote a small one it surrounds.** A pane behind a
  button wins on area every time. Two composable fixes: count only elements that
  are active and really visible, and weight each sample by its distance from the
  centre.
- **The exact hit still wins.** If the centre sample lands on an active element,
  take it and do not vote. That makes this a *fallback* for a near miss rather
  than a reinterpretation of every tap, and it cannot make an accurate tap worse.
- **Only for touch.** A mouse is exact and must stay exact. SDL 1.2 has no flag,
  but Emscripten's SDL pushes an `SDL_FINGERDOWN` alongside the synthetic mouse
  event, so the information is there; failing that, `pre.js` can set one.
- **The cursor itself must not move.** Only the element receiving the click is
  chosen by the vote; `cursorPosition` stays where the finger landed, or the level
  editor would place tiles somewhere other than where you touched.

**The cheaper alternative worth measuring against it**: grow each candidate's hit
area by the radius, keep those that then contain the point, and pick the one whose
true distance is smallest. O(elements), no sampling, exactly "the nearest target
within a finger's reach", and no grid resolution to tune; the sampling version is
easier to trust where `containsPoint` is overridden into a non-rectangular shape.
Both hang off the one line in `GUI::update()` that computes `p_elementAtCursor`.

`WebBuild/test/mobile.js` is where this gets its test: tap a few pixels *outside*
a small button and expect it to fire.


23. The hint note should be a sheet of paper  — **DONE**
--------------------------------------------------------
Two things were wrong with the note that flies up when the player steps on one.
The sheet arrived first and the text was faded in on top of it afterwards, in its
own coordinate system, so the writing did not turn or scale with the paper it was
supposed to be written on - it simply appeared. And the sheet was a flat quad,
which is a poster rather than a note somebody left behind.

**Both fall out of the same change: bake the text into the paper.** The sheet and
the text are rendered together into one 512x512 texture, and from then on there is
only one thing on screen. The Engine grew the two calls that needs
(`getOffscreenTexture`, `beginRenderToTexture`/`endRenderToTexture`); the texture
belongs to it and not to the note, because it falls with the framebuffer object
while the GL context still stands, and an `Object` is destroyed long after it is
gone.

With the writing part of the paper, the paper can be bent. The top and bottom are
wound onto a cylinder and unroll once the note has almost reached its final size -
48 bands each, the perspective divide by hand, no shader anywhere. **The one
number that is a constraint rather than a taste is `ROLL_TURNS`**: at most half a
turn, because this pass has no depth buffer and only up to a half turn does every
further step of paper come closer to the viewer, which is what makes back-to-front
painting correct. `CLAUDE.md` has the rest.

The texture is borrowed from a small pool in the Engine, because more than one note can be on
screen: stepping from one note onto its neighbour overlaps them for as long as the outgoing one
takes to fade. One shared texture was not wrong there - each note bakes immediately before it
draws - but it cost a render-to-texture and an FBO round trip per visible note per frame. A
note gives its texture back as soon as it is invisible, so the pool is as large as the most
notes ever seen at once and no larger.

Whether a note rolls is a property of the picture, so it is switched by a marker file,
`hintscroll.txt`, next to the hint.png that is actually loaded - existence only, no contents.
It sits there rather than in tileset.xml because every skin slot is chosen on its own, so the
note can come from a different skin than the tiles; and getSkinFilename() has already followed
default_hint.png by the time it returns, so the marker is looked for where the picture really
came from. The shipped set needs one file: blocks_01 has the paper, blocks_02 and blocks_03
borrow it through default_hint.png, and the space skin's display panel stays flat.

Left undone deliberately: **no mipmaps**. The note is minified while it flies in
and out, and the 3D cube crossfade has the same problem at a steep angle; both
want the same answer, and it is a separate piece of work.


24. The diamond machine's conversion deserves a real effect  — **DONE**
------------------------------------------------------------------------
The block is taken apart and put back together. Sparks in its own colours fly
out, sampled texel by texel through `Sprites::sample()`; from the cloud they
leave behind, more sparks come back, each taking on the colour the diamond will
have where it lands and dies. The block fades to `CONVERSION_GHOST` over the
same hundred ticks and what is left standing is the diamond.

**The spiral never happened, and it was the wrong question.** Three ways round
the integrator were listed below; none was needed, because the shape that reads
as a transformation is out-and-back, not round-and-round. Two straight flights
with damping — `d < 1` outward so it disperses, `d > 1` inward so it snaps
shut — say "apart" and "together" in a way a circle does not.

**Plain dust beat the glowing version.** Five variants were built and looked at
side by side; the winner is the one with no over-brightness at all, many large
slow motes in the block's own colour. Glowing sparks read as welding, and the
machine is handed rock, ice and grass as readily as metal. The over-bright trick
survives only on the inward motes, and for an unrelated reason: a linear ramp
from a blue block to the diamond's warm white passes through green.

**The ownership split held exactly as reasoned.** The block owns
`conversionProgress`, renders itself faint, and clears it in its own
`frameBegin()`; the machine merely pushes the value each tick through the
pointer it just looked up. Nobody reaches into `p_objOnMe`. One correction to
that: `frameBegin()` must *not* clear it once the object is dying or scheduled
to die, or the finished block snaps back to full opacity over the new diamond.

**`Particle` did get a new field after all**, which this item called a last
resort. `uint id`, four bytes at the very end, outside the six the vertex path
keeps in one cache line — and the reason is the abort, which was not foreseen
here: an abandoned conversion runs its sparks *backwards* rather than deleting
them, and the machine has to find its own again. `ParticleSystem` hands out
`begin()`/`end()` and filtering by id is an `if` at the call site.

How the four traps landed:

- **A faint ghost, not full transparency** — `CONVERSION_GHOST` is 0.22, and the
  fade snaps back rather than eases when the block moves. Right as predicted.
- **The effect must not depend on particles** — the fade and the machine's own
  five-frame animation still carry it with `getParticleDensity()` at minimum.
- **A hundred ticks** stayed a hundred; the spark phases are pinned to the
  machine's own image changes at 20, 40, 60 and 80 so the two cannot drift.
- **The sound** is still fire-and-forget; that is item 25, untouched.

The test level is `Tools/testlevels/diamondmachine.xml`.

25. The diamond machine's sound handle is never set
----------------------------------------------------
`DiamondMachine::p_soundInst` is assigned 0 in the constructor and nowhere else.
The block at the foot of `onUpdate()` that stops it when the conversion is
abandoned -

    if(counter == -1 && p_soundInst) { p_soundInst->stop(); p_soundInst = 0; }

- can therefore never run. The sound is played fire-and-forget instead, with
`playSound("diamondmachine.ogg", false, 0.0, 100)`, whose return value (a
`SoundInstance*`) is discarded; it is a one-shot, so a conversion that is
abandoned half way through still plays it out to the end.

Nothing is broken by this today - a two-second one-shot that outlives its cause
by a moment is not something a player notices - so it is written down rather
than patched. It matters for item 24: a longer conversion wants a looping sound
that stops when the block is pushed off, and the member and the stop are already
sitting there waiting for the one line that assigns them. `conveyorbelt.cpp` is
the pattern to copy - `createInstance()`, `play(true)`, `stop()` - rather than
`playSound()`, which is the fire-and-forget path.


26. The shipped content should not be copied into the user's folder  — **DONE**
--------------------------------------------------------------------
`Level::getSkinFilename()` and `Campaign` look for skins, campaigns and levels
in **one** place: `getAppHomeDirectory()`. The game's own `levels/` folder is
never read at runtime - it is only the source for a one-time copy in `main.cpp`,
which runs when `.initialized` says `not_played` or `<= 1.0.7` and never again.
So every player's `My Documents\Blocks 5\levels` holds a private copy of the
shipped campaign, the four skins and the two example levels, frozen at whatever
version they first installed.

That is how the `hintscroll.txt` marker went missing on a machine that had built
the current sources: the archive the game reads was two weeks older than the one
`zip_skins.bat` had just built. `Transfer::refreshBuiltIns()` now patches over
it - on every start, each built-in whose size differs from the shipped one is
copied across - but that is a plaster on the shape of the thing.

**The shape it wants is two roots.** The shipped content stays in the game folder
and is read from there, so it is always exactly as new as the executable; the
user directory holds only what the player made or imported. `getSkinFilename()`
would look in the user directory first and fall back to the game folder, and
`isBuiltIn()` would stop being a hand-written list - "it lives in the game
folder" *is* the definition, which is also what makes it undeletable and
un-overwritable.

What it touches: `Level::getSkinFilename()`, `Campaign::makeLooseRef()` and
`resolveMusicPath()`, and in `Transfer` the `list()`, `remove()` and `install()`
paths, which would have to merge two directories and keep the Manager's Delete
greyed out for anything from the game folder. And an upgrade would want to
*delete* the stale copies it finds in the user directory, or they would go on
shadowing the shipped ones for ever - which is the one part that touches a
player's folder and therefore wants care.

**And one line that will break silently, so fix it in the same change:** the
credits after the last level. `GS_Game::onUpdate` decides whether the game is
over by comparing the campaign's filename against the literal
`FileSystem::inst().getAppHomeDirectory() + "levels/campaigns/blocks.zip"` - a
hardcoded path into exactly the directory this item empties. Move the shipped
campaign to the game folder and the comparison simply stops matching: no error,
no warning, the player finishes all 42 levels and is dropped back into the level
list. Nothing in the tree would notice, and nothing short of playing to the end
would either.

It is also the second place that knows what ships with the game, which is the
thing this item is meant to end: once "it lives in the game folder" is the
definition, that branch should ask `Transfer::isBuiltIn()` - or better, the
campaign should say so itself - rather than spelling a path. See item 27, which
wants the same answer for a Credits button in the menu.

Worth doing before the next release that changes a shipped asset. `blocks.zip`
is 8 MB, and every installation is carrying a second copy of it for no reason.

**Done in 1.2.0, and with the search order the other way round from what stands
above.** User-first is exactly how a stale copy shadows a fresh shipped file —
the bug — so the game folder is asked first and the user directory is the
fallback. `FileSystem::resolveContentPath` is the one place that knows, and
`isShippedContent` ("it exists in the game folder") replaced the hand-written
list in `Transfer::isBuiltIn`. `refreshBuiltIns()` and the one-time copy in
`main.cpp` are both gone.

Seven files belong to the player and reverse the order: the two example levels
and the five `readme.txt` are looked up in the user directory first, with the
game folder's copy as a template, and they never count as shipped.
`FileSystem::getPlayerFiles` is that list. Only the readmes are actually copied
on a first start, because nothing in the game reads them and they would
otherwise sit in no folder; an example needs no copy, since it is listed and
loadable from the game folder and the player's own version appears the moment
they save one.

Game-first costs one thing, and it is worth writing down: a user file carrying a
shipped name could never be loaded again, because the game folder answers first.
So both editors refuse to save under such a name, which is the same rule an
import already followed. `retireShadowingCopies` renames the old copies to
`<name>.bak` on the first start of 1.2.0 — renamed, not deleted, because nothing
can tell from outside whether somebody edited one.

**And a trap that was not in this item at all:** `ProgressDB` keyed on the
campaign's *full path*. Moving `blocks.zip` into the game folder would have
changed the key and silently reset every player's 42 levels, with no error and
nothing in the log. It keys on the bare filename now, which also means an old
`progress.zip` migrates simply by being read.


27. A Credits button in the main menu
--------------------------------------
There is no way to see the credits except by finishing the shipped campaign - or
by knowing that **Shift+C in the main menu** already runs them
(`gs_menu.cpp`, in `onUpdate` beside the Shift+D that opens the user folder).
That shortcut is undocumented and unconditional; the entry it stands for should
be a visible one, in the style of the `Website` link at the top right of
`menu.xml` rather than a ninth big button.

Two presentations, chosen by whether the player has earned the first:

- **Finished the shipped campaign** - the whole sequence, exactly as it runs
  after the last level today.
- **Not finished** - the names only, as scrolling text, without
  `$C_THANKS_FOR_PLAYING` and `$C_STAY_TUNED`. Those two address someone who has
  just won, and they give away that there is an ending to reach.

**Asserted, and it holds: only the built-in campaign triggers the full credits.**
`gs_game.cpp` compares the campaign's filename against the literal
`FileSystem::inst().getAppHomeDirectory() + "levels/campaigns/blocks.zip"`; every
other campaign pops back to the level selection instead. A level run from the
editor and the single levels never even reach that branch, because `ownLevel`
forces `status` to -3 first.

That comparison is the fragile part, and it is worth fixing while touching this.
It is a **second** place that knows what ships with the game - `Transfer::isBuiltIn`
is meant to be the only one - and it hardcodes the campaign into the *user's*
directory, which is exactly what **item 26** proposes to stop doing. Move the
shipped content out of `getAppHomeDirectory()` and this check quietly stops
matching: the game would end with a level list instead of the credits, with
nothing failing anywhere.

Deciding "finished" from the menu needs the same bar the game uses, and it is not
simply "all levels": `GS_Game::loadLevel` treats `getLevels().size() - 1` as the
count when the campaign has a bonus level, since the bonus is the last entry and
only unlocks once the rest is done. So the test is
`ProgressDB::getNumLevelsCompleted(f)` against the number of non-bonus levels -
and the menu holds no campaign, so it would have to `Campaign::load` the shipped
one to learn that number. That is cheap (it reads only `campaign.xml`), but it is
a load the menu does not do today.

What the second presentation costs: `GS_Credits` is not a scroll and has no
notion of a mode. `onRender` builds a local array of eight blocks - position,
title, text, start time, duration - and each one fades and zooms in and out over
a flying starfield with a motion-blur buffer. The clock is hardcoded against that
table: the fade to black starts at 53 s, three `character*.ogg` play at 55, 56
and 57 s, and `setGameState("GS_Menu")` fires at 58 s; Return, Escape and Space
fast-forward at five times speed rather than skipping. A names-only variant is
therefore not "hide two entries" - it is a second layout and a second timeline,
and the table has to leave `onRender` first.

28. Video recording in the browser
----------------------------------
Screenshots work there now (`img_save.cpp` writes the PNG, `WebTransfer::
downloadBytes` delivers it), and `$A_TOGGLE_CAPTURE_VIDEO` is the one action
`main.cpp:509` still withholds from the web build. Four things stand in the way,
and only two of them are real.

- **The action is not registered.** One `#ifndef __EMSCRIPTEN__`.
- **The recorder is stubbed.** `WebBuild/build.sh:39` filters `videorecorder.cpp`
  out and links `videorecorder_stub.cpp`, whose `getError()` answers `true`.
  `minih264e_impl.c`, `minimp4_impl.c` and shine's nine files are not in that
  build's `CSRCS` either.
- **The encoder runs on its own thread.** `videorecorder.cpp:415` calls
  `SDL_CreateThread` and `:428` `SDL_WaitThread`, and the thread proc blocks on
  `SDL_SemWaitTimeout`. `streamedsound.cpp:279` already writes down what that
  does here: `SDL_CreateThread` aborts, `SDL_WaitThread` calls `abort()`, and
  Emscripten's SDL has no semaphores at all. The build passes no `-pthread`.
- **Nothing captures the audio.** `audiocapture.cpp`'s `#else` branch is a stub
  that reports silence.

**The stub's stated reason for the last one was wrong, and the comment has been
corrected.** A page *can* hear its own output: in Emscripten's OpenAL every
source does `connect(AL.currentCtx.gain)` and that gain does
`connect(ac.destination)`, so one extra connection from that summing node to a
`createMediaStreamDestination()` yields exactly the finished mix - the same thing
WASAPI loopback gives under Windows and the monitor source under Linux.
`web_audio.cpp` already reaches `AL.currentCtx.audioCtx` for the suspend gate.

Where the file goes is no longer a question either, and `GL_BGR` never was one
on this path: `engine.cpp` already reads the frame as `GL_RGBA`.

**The route to take: let the browser encode, off an offscreen 2D canvas.**
`glReadPixels` at 640x480 stays exactly as it is - so does the cursor that
`Engine` draws into that buffer by hand - and the frame goes into a 2D canvas
that nothing displays. `canvas.captureStream()` on *that* canvas, plus the audio
track from the summing node above, is a `MediaStream`, and `MediaRecorder`
turns it into a file. No encoder in the wasm, no thread, and the chunks are
Blob parts the browser may spill to disk rather than 22 MB of resident memory
per minute (`engine.cpp` asks for 2.84 Mbit/s video and 160 kbit/s audio at
30 fps).

Capturing the *game's* canvas directly would be simpler still and is the wrong
trade: `captureStream` sees the composited canvas, which is the upscaled,
letterboxed picture. Screenshots and videos are deliberately the clean 640x480,
and an off-screen canvas is what keeps that promise.

Three details that decide the work:

- **`VideoRecorder`'s interface survives unchanged.** `isReadyForNextFrame()` /
  `getInputFrameBuffer()` / `encodeNextFrame(timecode)` map onto "hand JS a heap
  buffer, then push it into the canvas", so `engine.cpp` needs no edit beyond
  the missing action. A `WebBuild/videorecorder_web.cpp` replaces the stub.
- **The frame arrives upside down.** OpenGL's first row is the bottom one, and
  `putImageData` ignores the 2D context's transform - so the flip has to happen
  while filling the `ImageData`, or through
  `createImageBitmap(..., { imageOrientation: "flipY" })` and `drawImage`.
- **The container is the browser's choice.** WebM/VP8 everywhere, MP4/H.264
  where `MediaRecorder.isTypeSupported("video/mp4")` agrees. That is a step down
  from the desktop's MP4, which was picked precisely because Windows plays it
  with nothing installed - but a browser that recorded the file can play it back.

The alternative is to port the existing encoder: drop the thread and run
`convertFrame()` + `H264E_encode()` from the logic tick, the way `StreamedSound`
gave up its decoder thread. It keeps one code path and the same MP4 on every
platform, and it pays for that with a full 640x480 H.264 frame encoded 30 times
a second inside the game's own frame budget, single-threaded
(`createParam.max_threads = 0`), plus `minimp4`'s seek-and-write sink
(`videorecorder.cpp:29`) holding the whole file in memory until it is closed.

A backgrounded tab gets no `requestAnimationFrame` and therefore no frames,
whichever route is taken - the recording simply stops there, which is also what
`handleAppFocus` already does to it.

29. Eight new levels for 1.2.0, and a skin to put them in
---------------------------------------------------------
The shipped campaign has **42** levels, so eight more make it 50.

The list starts from what the campaign does not use. Three presets are placed in
no shipped level and are not spawned by anything either — `TeleporterNoPlayer`,
`ShieldedActivatorBlock` and `E_Multiplexer`. (`ToxicGas` is placed in none, but
that means nothing: `ToxicWaste` makes it when a barrel is destroyed, and nine
levels hold 39 barrels between them. What no level does is start with gas
already there.) The electronics family is thin everywhere:
`E_PulseSwitch` and `E_PulsePanel` live in one level between them, `E_HexDigit`
in two, `LightSwitch` in exactly one level with exactly one piece. `Syringe`
appears in three levels and `Eye`, `Spike` and `ShieldedBlock` in three each.

1. **Stock up before you go in.** Collect enough syringes first, then survive
   long enough inside the toxic gas to reach what is on the other side. The
   syringes are a supply, not a cure: `contamination` is allowed to go negative
   for exactly this, and `gs_game.cpp` only crackles and spreads toxin above
   zero.

   A `Hint` before the gas has to say so, in the shape of *"you will need enough
   protection"* — the mechanic is invisible otherwise, since nothing on screen
   counts the syringes and a player who walks in with two instead of five simply
   dies. Hint text is a `$ID` in a `<Text><![CDATA[...]]></Text>` child, resolved
   through `data/languages.txt`, so it needs an entry there with `§en:` and
   `§de:` bodies, named like the existing `$HINT_BLOCKS_NN_MM`.

   Placing the gas in the level file rather than bursting a barrel for it is the
   first time the campaign does that.
2. **The mask is worth more than the mask.** One mask, two gassed corridors, and
   the mask has to be dropped and fetched again — `inventory[2]` holds only one.
3. **A door that only blocks you.** `TeleporterNoPlayer` sends blocks somewhere
   the player cannot follow, so the way through has to be built remotely.
4. **Counting.** `E_HexDigit` as the visible goal: feed it a number with
   `E_BlockDetector` and `E_Gate`, and the exit opens on the right one.
5. **One switch, four places.** `E_Multiplexer` steering a single pulse train to
   one of several barrages, so the order of the throws is the puzzle.
6. **Light and mirrors.** `LightBarrierSender` and the receiver, with `Mirror`
   redirecting the beam and blocks casting the gaps.
7. **Everything on rails.** `Elevator` and `Rail` carrying blocks past `Spike`
   rows on a timing the player sets with `E_Clock`.
8. **The eye in the dark.** `Eye` plus `nightVision`, where what you cannot see
   is watching, and `LightSwitch` decides which of you is blind.

**A skin for them.** The four that ship are `blocks_01/02/03` — earth, brick and
grass — and `space`. Both themes that would fit these levels are indoors, which
is what neither existing family offers:

- **Laboratory or chemical plant.** Tiled walls, pipework, warning stripes.
  It covers the most of the list above at once — gas, syringe, mask, and the
  diamond machine reads as a centrifuge rather than as magic. The hint would be
  a clipboard on the wall, so no `hintscroll.txt` and no roll.
- **Inside the machine.** Circuit board green, gold traces, solder pads; the
  natural home for the `E_*` family, which is the thinnest part of the campaign.
  The hint would be a small display, again unrolled.

Of the two, the laboratory earns its keep across more levels; the circuit board
is closer to a single level's gimmick. Other themes that were considered and are
weaker for this set: ice cavern, volcano, temple ruins, sewers, greenhouse.

A skin needs `tileset.xml`, `sprites.png` and its own `hint.png`; see
`Level::loadSkin` and the packing rules in `Blocks5/pack.sh`.

30. Let a skin override the sound effects too
---------------------------------------------
A skin replaces everything a level *looks* like and nothing it *sounds* like. The
laboratory of item 29 would want its own door, its own machine, its own alarm,
and a skin somebody else writes has no way to bring them.

The two halves of the game meet nowhere at the moment, and that is the whole of
the work. Pictures go through `Level::getSkinFilename(SKIN_*)`, which walks the
loose folder, then `default_<name>`, then the archive, and answers with the
*final* path — that is what makes `blocks_02` reach `blocks_01`'s paper through
`default_hint.png`. Sounds go through `Engine::playSound(filename)` straight into
`Manager<Sound>::inst().request(filename)`, which resolves against the asset root
mounted in `main.cpp` and therefore always lands inside `data.zip`. Nothing in
that path knows a level is loaded, let alone which skin it wears.

The shape that fits the tree: keep `playSound` taking a bare filename, and give
the resolution a hook — the level, when it has a skin, answers "this name comes
from here instead". `p_skinFilenames` is a fixed table of eleven entries, one per
`SKIN_*` slot, so sounds cannot join it as they are: there are fifty-odd effects
and a skin would override two or three. A per-skin `sounds.xml` listing only what
it replaces is the smaller answer, and it can share the file `data/sounds.xml`
already uses for playback gains rather than inventing a second format.

Four things will need deciding, and each is a trap:

- **Which sounds may be overridden.** A skin taking over `screenshot.ogg` or the
  menu jingle is nobody's idea of a skin. The set that belongs to the *level* —
  blocks, machines, doors, weather — is not currently marked as such anywhere.
- **`gs_loading.cpp` preloads every sound by name**, and `verify.py`'s `sounds`
  check enforces that a `playSound()` name is preloaded, or the first play is
  silent while the file is read. A skin's sounds are known only once a level is
  loaded, so they need loading at `Level::loadSkin` time, not at startup.
- **The cache is keyed by filename.** `Manager<Sound>` hands out one `Sound` per
  name; two skins overriding `push.ogg` differently would collide unless the key
  becomes the resolved path, which is what `getSkinFilename` already returns for
  pictures.
- **`Sound` looks up its playback gain once at construction** out of
  `data/sounds.xml` (see the mix notes in `CLAUDE.md`). A skin's own file needs
  the same treatment, or an imported effect plays at whatever level it was
  exported at while the shipped ones sit 6 dB down.

The export side is free: `Transfer` copies a skin archive as it stands, so an
`.ogg` inside it travels with everything else.

31. Menu music that picks up where it left off, with a slider of its own
-------------------------------------------------------------------------
The menu music plays in the level editor now, and editing means switching to a
level and straight back — so a track that always restarts from zero is heard
from the beginning a dozen times an hour and turns into a nag. The new piece is
meant to be long enough to sit with, which only makes a restart worse.

What was proposed:

- **`Engine::playMusic` remembers where a track was stopped** and takes an
  optional argument to resume from that position instead of from zero. It is the
  one funnel — `gs_menu.cpp:349`, `gs_selectlevel.cpp:353`, `gs_game.cpp:598` and
  `:710`, `gs_credits.cpp:284` and `gs_leveleditor.cpp:1325` all go through it —
  and it already calls `stopMusic()` itself when another track displaces one, so
  that is where the index would be taken.
- **A separate volume slider for the menu and the editors**, in case somebody
  gets tired of the music that follows them around. `options.xml:75-77` is the
  one that exists; this is a second `ScrollBar` beside it and a second key
  in `config.xml`, since `<MusicVolume>` is taken.
- **Its default is copied from the existing music volume**, which is renamed in
  the GUI to *in-game music*. `$O_VOLUME_MUSIC` is the string; the config key and
  `Engine::musicVolume` need not follow the label.


32. A sound when a hint note opens
-----------------------------------
Nothing is heard when a note flies up, which is the one thing on a field that
opens a window over the play area. Two sounds rather than one: a generic one,
and paper for the scroll type - `blocks_01`'s sheet unrolls, and a sheet of
paper being handled is what that motion sounds like.

`Level::isHintScroll()` already answers which of the two, and `hint.cpp:411`
reads it for the same purpose. The tick to fire on is `hint.cpp:467`, `if(open
&& activeTicks == 0)` - the one place that already runs exactly once per
opening, which is why the flight target is decided there.

What the tree will ask for:

- **A committed `.wav` beside the `.ogg`.** `Tools/encode_sounds.py` makes the
  one from the other; `data/sounds.xml` carries the playback gain if either
  should sound quieter than its file.
- **`gs_loading.cpp` must preload both by name**, and `verify.py`'s `sounds`
  check enforces it - the first play is otherwise silent while the file is read.
- **The scroll flag belongs to the skin, not to the level** (`hintscroll.txt`
  beside the `hint.png` that is actually loaded), so a skin bringing its own
  panel would want its own sound with it. That is item 30, and this is the
  first concrete caller for it.


33. Draw the keycap frames behind the text
-------------------------------------------
A frame is drawn over the letters it surrounds, and where the text sits a row
high in its box - which a small font cannot always avoid, see the keycap notes
in `CLAUDE.md` - the top edge crosses the capitals. Behind the glyphs it would
pass under them instead, and the same row of overlap would stop being visible.

The obstacle is the order the two are produced in. `Font::renderTextPure`
(`font.cpp:280`) collects the rectangles in the same loop that emits the glyph
quads, because a `<k>` is only closed when its `</k>` is reached, and draws them
after `glEnd()` and `p_texture->unbind()` - untextured, and therefore
necessarily after the batch. Putting them first means knowing them first: a
layout pass ahead of the draw, over the walk `measureText` already does with
exactly the same advances (`font.cpp:569`).

Two things not to lose:

- **The frames go through both shadow passes with the glyphs** (`font.cpp:173`),
  or a keycap looks pasted on.
- **`renderText` caches a display list per string**, so a second walk costs once
  per new string. Not in WebGL, which has no display lists and redraws the text
  for every shadow sample (`font.cpp:261`) - there it is once per sample.


34. Switch the language inside the hint editor
-----------------------------------------------
A hint's text is usually a `$ID`, and what the player reads is whatever
`languages.txt` has under it in their language. The editor shows only the one
the game is currently running in, so checking that a note fits its paper in
both means leaving the editor, changing the language in the options and coming
back. Two radio buttons in `EditHintPane` would answer it on the spot.

Most of the machinery is already there. The preview is rendered by
`gs_leveleditor.cpp:1246` at layer 43, from the edit box's text set one line
above, and both render paths resolve it through `localizeString()` at draw time
- so nothing has to be re-baked by hand, `bakeNote()` re-bakes on its own when
the resolved text changes. `Engine::setLanguage()` is the switch.

Two things to settle:

- **It is a preview, not a preference.** `Engine::setLanguage` changes the whole
  game, and `Engine::exit` writes `<Language>` to `config.xml` - so leaving the
  editor on the other language would silently change the player's setting. Either
  put it back on leaving the pane, or resolve the preview against a language the
  note is told rather than against the engine's.
- **The editor's own captions would switch too** if the engine's language is
  what moves, which is a lot of visible churn for a preview of one note.


35. Close the hint note with a click, and spend the input that does it
-----------------------------------------------------------------------
Return and Escape put an open note away (`gs_game.cpp:147`); a click does not,
although a click is what a player reaches for after the note has covered the
play area they were looking at. The pause already takes any key *and* any
button - `wasAnyKeyPressed() || wasAnyButtonPressed()`, `gs_game.cpp:378` - and
the note should read the same way.

**One input must do one thing, and today it does two.** The press that leaves
the pause also closes the note, and it moves the player besides:

- `GUI::update()` runs before `p_gs->onUpdate()`, so `GameGUI::onKeyEvent` has
  already called `dismissDisplay()` by the time the resume is decided. One
  Escape therefore resumes *and* closes.
- `Player::onUpdate` reads `wasActionPressed("$A_LEFT")` and the rest -
  an edge, not a held state - and the level is updated in the same tick the
  resume clears `paused` (`gs_game.cpp:533`). So the key that resumes takes a
  step as well. The `else if(!menuVisible)` chain around the resume protects
  only the three actions inside it, and movement is not one of them.

The shape that fits: one notion of "this input has been spent this tick",
consulted by the GUI's key handler, by the dismissal and by the action layer -
not a third guard beside the two that already disagree. `Engine::flushInput()`
is the precedent and possibly the mechanism: the key grab already says "the
keyboard belongs to something else this tick", and `Engine::update` acts on it
by skipping `updateActions()`.

Both gestures want it, and in the same order: resume, then dismiss, then act.
Closing the note must not step either.


36. Let the details setting reach the text shadows
---------------------------------------------------
Every string is drawn three times: `Font::renderText` (`font.cpp:173`) lays down
two offset copies in black before the text itself, and `Engine::getDetails()` is
not asked about it. Level rendering, the weather and the lightning all consult
it (`level.cpp:719`, `:1068`, `lightning.cpp:48`); the font does not, so the one
thing drawn on every screen in the game ignores the setting meant for exactly
this.

`Font::Options::shadows` is where it would go, and its name is the first thing
to fix: it reads as a count and is an offset style. Both non-zero values draw
**two** samples - 2 gives (2,1) and (1,2), 1 gives (1,0) and (0,1) - and the
alpha is divided by the number of them, so dropping one is a matter of changing
`0.7 / numSamples`, not of leaving a hole. 0 already means none, which is what
`gs_credits.cpp:175` uses.

Where it is worth most is still the browser, though less than when this was
written. Each pass there used to be a full walk over the string - the tags
parsed, the glyph quads built and the keycap frames collected again - because
WebGL has no display lists; both builds now lay a string out once and draw the
cached arrays three times. So a dropped sample saves a draw call per string per
frame rather than a whole walk over the text, which on a phone is still worth
having and is no longer the headline.

Two things to decide:

- **Whether the setting picks the sample count or the whole style.** One sample
  at an offset of (1,1) is cheaper than two and still reads as a shadow; two
  exist to soften the corner.
- **Who wins where a caller already asked.** `gui.cpp:39`, `hint.cpp:161` and
  the credits all set `shadows` themselves, so the details setting has to be a
  ceiling over what they ask for rather than a replacement - the credits' 0 must
  stay 0 at any detail level.


37. SDL 1.2 -> SDL 3, planned and deliberately not done
--------------------------------------------------------
Surveyed in depth and written up in `SDL3-MIGRATION.md` at the repository root. **The decision
was not to do it**, and the plan exists so that decision can be revisited from evidence rather
than from memory.

Why it is worth doing eventually: Emscripten's SDL 1.2 is a 134 KB hand-written *JavaScript*
reimplementation, and `WebBuild/platform_stubs.cpp` exists only to patch its gaps; SDL 1.2 has had
no upstream since 2013; 2.4 MB, 170 tracked files, 67 `.vcxproj` entries and two vendored patches
would leave the tree; and `rememberWindowPlacement` and its neighbours, wholly inside `#ifdef
_WIN32` today, become portable and therefore testable.

Why not now: seven of the ten surveyed areas came back **hard** after adversarial review, the
estimate is **36-52 engineer-days plus about nine hours on a Windows machine**, and **not one MSVC
compile happened anywhere in the survey** - the largest cost centre is the least verified part, on
the platform the game actually ships. The failure mode is the expensive kind: `SDL3` does not break
this tree loudly. Three silent landmines were proven, of which the sharpest is that
`LinuxBuild/linux_window.cpp` wraps its whole implementation in `#ifdef SDL_VIDEO_DRIVER_X11`, a
macro SDL3 does not publish - after a header rename both functions compile to `return false` with
no error and no warning, and Linux fullscreen is simply gone.

**Not a reason to do it, contrary to a first reading:** the licence. SDL 1.2 is LGPL 2.1 and is
statically linked, but the game is GPL v3 with its complete source published, which more than
satisfies LGPL 2.1 section 6 - see the LICENCE section of `Blocks5/libs/sdl-1.2.15/PROVENANCE.txt`.
`shine` is in the same position. Nothing has to change.

Two things settled while planning, kept because they are cheap to lose and expensive to rediscover:

- **Main callbacks do not require `SDL_MAIN_USE_CALLBACKS`.** `SDL_EnterAppMainCallbacks` is
  declared unconditionally (`SDL_main.h:581`), so `main.cpp` keeps its own `main()` and its SEH
  crash handler, and MSVC's `/Yu` rule - which silently discards anything written above
  `#include "pch.h"` - never gets a chance to bite.
- **A classic `main()` with `emscripten_set_main_loop_arg` works against SDL3 in the browser**,
  measured here: 45 frames, 7 events, `driver=emscripten`, WebGL 1.0. So the callbacks restructure
  is a preference and SDL3 is the goal, which is what makes the retreat in the plan real.


38. A finger cannot scroll a list
----------------------------------
`GUI_ListBox` scrolls two ways and a phone has neither. `onMouseWheel`
(`gui_listbox.cpp:133`) needs a wheel, and the `GUI_ScrollBar` the constructor
puts down the right-hand edge (`:12`) is **16 pixels wide** - a quarter of the
48-pixel target a finger wants, and at a 2x window still only 32 device pixels.
A touch on the list body goes to `onMouseDown` (`:101`), which selects the item
under it and nothing else. So on a phone the entries past the bottom of the box
are reachable only by hitting a 16-pixel bar.

Three lists carry real content: the campaign list in the select screen, the
action list in the options, and the Manager's file list, which is as long as the
player's folder.

What is missing is a *gesture* layer, and that is the item rather than the list
box alone. The GUI knows down, up, move and wheel; a touch surface wants at
least drag-to-scroll with the press held back until the finger has moved less
than a threshold - otherwise every scroll also selects whatever it started on -
and probably a fling with friction after it, since the whole point is to cross a
long list quickly. Both belong above `GUI_ListBox`: a multi-line edit box and
the level editor's own field want the same distinction between a tap and a drag.

Two things to settle first:

- **Where the threshold lives.** `GUI::update()` already has the answer to "did
  the pointer move" and holds `p_elementAtCursor`; a drag that has passed the
  threshold has to reach the element as something other than a click, which
  means a new event and not a flag on the old one.
- **What a scrollbar is for afterwards.** Once the body scrolls, the bar is a
  position indicator that could stop being a control - which frees its 16
  pixels, and would be the first widget in the tree drawn for a phone rather
  than for a mouse.


39. The level editor paints a line to wherever the finger last was  - **DONE**
-------------------------------------------------------------------------------
`LevelEditorGUI::onMouseMove` interpolates with `bresenham(oldCursor, p)`
(`gs_leveleditor.cpp:385`) so that dragging faster than the events arrive still
leaves a continuous stroke rather than a dotted one. `oldCursor` was never
cleared when the button was released - invisible with a mouse, because the
button-less moves between two strokes keep it under the pointer by themselves,
and with a finger a line from the end of one stroke to the start of the next.
Same shape as the two touch bugs already fixed in `GUI::update()` and
`Engine::cursorPosition`: code that was correct only because a mouse never
teleports.

`onMouseDown` now sets it, and three things about that must not be undone:

- **The `realDown` guard.** `onMouseMove` calls `onMouseDown` for every cell of
  the interpolated line with the flag false. Resetting on those would undo the
  interpolation it is there to perform.
- **The press and not the release.** A touch can be cancelled without an up ever
  arriving; a press starts a stroke whatever came before it.
- **The press position and not `Vec2i(-1, -1)`.** The sentinel makes the first
  move of a stroke start from wherever that move landed, which loses the cells
  between it and the press - the very gap the interpolation exists to fill.

`WebBuild/test/editorstroke.js` is the check, and it has to be a browser: a
mouse cannot reproduce this at all, since moving the pointer somewhere is
exactly what keeps `oldCursor` current. CDP dispatches a `mousePressed` with no
`mouseMoved` before it, which is the shape a touch has. Measured against the
unfixed build, a three-cell drag painted 30 cells; four after.

40. A pad button says which key it sends; it does not say what that does
------------------------------------------------------------------------
The on-screen pad's six buttons are labelled `Shift`, `Ctrl`, `Tab`, `F5`, `F10`
and `Esc` - the keys they dispatch - and not `Bomb`, `Put`, `Swap`, `Retry`,
`Hotel` and `Menu`, which is what they used to say. The reason is that the label
has to be true, and only one of the two always is: the pad sends a **fixed** key
(`KEYS` in `WebBuild/touch_controls.js`) and the game's action layer maps it, so
a rebinding in the options dialog moves the meaning and leaves the key alone. A
button reading `Bomb` after the player has bound something else to Shift is
simply wrong, and nothing would ever correct it. The other half of the argument
is that the game goes on naming keys by name - in the hints, in the help table,
in the options dialog - and a thumb should be able to find the button that
sentence is talking about. The words are the game's own, shortened from
`$VK_KEYBOARD_LCTRL` and its neighbours in `data/languages.txt`, and they follow
the language the game settled on rather than the browser's, through
`Engine::publishLanguage()`.

What is missing is the *other* information, which the old labels did carry: what
the key is for. The answer is a symbol beside the name rather than instead of
it - a fused bomb on the key bound to `$A_PLANT_BOMB`, an inert one on
`$A_PUT_DOWN_BOMB`, three figures with arrows between them on
`$A_SWITCH_CHARACTER`. A picture needs no translating, which is the same reason
the four arrows of the d-pad are drawn and not written (`.b5arrow`, and it keeps
that file plain ASCII).

Two things to settle first, and the second is the real work:

- **Where the art comes from.** The pad is DOM and not GL, so it cannot sample
  the sprite sheet the game draws from - and the sheet is inside `data.zip`
  behind its password anyway, which no page script can open. So these are new
  drawings, inline in the page as SVG or as a data URI, in the idiom
  `WebBuild/make_icon.py` and `make_text.py` already use for the icons and the
  boot line: generated at build time from something committed, never fetched.

- **Which button gets which symbol.** That is a question about the *current*
  bindings, and the pad does not know them - by design, since not knowing is
  exactly what lets a rebinding work for free. Baking the symbol to the default
  binding would reintroduce the lie the key names were chosen to avoid, only in
  a form that cannot be read off the screen. So the pad needs the bindings
  pushed to it, which is the channel `publishLanguage()` has just opened: the
  same idea, carrying `$A_*` names per key instead of a language, republished
  whenever the options dialog changes one. Until that exists the symbols would
  be decoration that is right by luck.

Item 22 (tap radius) is the neighbour: both are about a button a finger has to
find, one by size and this one by what it says.


41. Blurred shadows in one pass, with a shader
-----------------------------------------------
Every shadow in the game is the same geometry drawn again in black at an offset:
the tile grid and the objects two or three times at (2,1), (1,2) and (2,2)
(`level.cpp:717`), every string twice at the first two of those (`font.cpp:191`),
each sample at `0.7 / numSamples` so that the copies sum to one shadow. Two
offset copies of a hard-edged glyph are not a blur, they are a double image; what
softness there is comes from the corner where the two overlap. Draw the shadow
**once** instead, in a fragment shader, with a real falloff.

It is worth doing twice over: one draw in place of two or three on everything the
game puts on the screen, and it retires the sample count that item 36 was going
to hang a detail setting on.

Four things are in the way.

- **There is no shader path for ordinary drawing.** The only programs the game
  builds are the present filters (`upscaler.cpp`, `u_sharpfit.cpp`,
  `u_crt.cpp`), and each runs on one quad at the very end of the frame with
  `PresentContext` handing it the finished frame. Putting the level or the text
  through a shader means a second kind of program with its own uniforms and a
  fixed-function path beside it for `-noshader` and for any machine where
  `createUpscalerGL` gives up - so this is an addition, never a replacement.

- **The atlases have no margin to blur into.** A shadow computed from the same
  texture fetch needs taps around the sample point, and the glyph rectangles in
  `data/font.xml` sit about five pixels apart with six pairs touching outright
  (measured over all 256 entries), so a tap would pick up the neighbouring
  letter. `sprites.png` and the tile sets are packed the same way. Either every
  atlas gains a margin - which moves every rectangle in every `*.xml`, skins
  other people made included - or the shadow comes from a silhouette rendered
  into a texture of its own, which is the second pass this item set out to
  avoid.

- **The shadow is not one silhouette.** `Level::render` draws the tile shadow and
  the object shadow from separate passes at the same offsets, objects opt out of
  it with `OF_NO_SHADOW` (`level.cpp:1239`), and the text shadow has to cover the
  keycap frames, which carry no texture at all. A shader handed "the frame" would
  shadow everything on the screen, the GUI over it included.

- **What it should look like is a taste decision, not an optimisation.** The
  shadow today is two pixels down and to the right and hard-edged; a soft one
  changes the look of every screen in the game. `u_crt.cpp`'s halation is the
  precedent for both halves of that - two rings of four taps thresholded in
  linear light, and a slider rather than an imposition.


42. Clamp the vertex colour where it is free, not on the CPU
------------------------------------------------------------
The game hands GL colours above 1 on purpose - `Level::renderShine` takes
`deathCountDown * 5.0` from an exploding bomb, the teleport swirl ramps its red
from 0.1 to 2.1 over sixty ticks (`object.cpp:385`), the two spark bursts reach
1.5 (`object.cpp:424`, `projectile.cpp:190`) - and relies on GL to cut them off.
Desktop GL does: a primitive colour is clamped to [0,1] before it is multiplied
by the texel. The browser does not. Dumping the shader Emscripten's emulation
generates for this game gives a vertex stage of

    v_color = a_color;

with no clamp anywhere; the emulation emits `v_color = clamp(v_color, 0.0, 1.0)`
only inside its lighting branch, and this tree never enables `GL_LIGHTING`. The
colour therefore reaches the fragment stage raw, is multiplied by the texel, and
is only clamped when `gl_FragColor` is written - so the browser computes
`clamp(colour * texel)` where the desktop computes `clamp(colour) * texel`. At a
red of 2.0 every texel above 0.5 saturates, which eats the particle's falloff and
turns a soft glow into a hard-edged blob.

The clamp is on the CPU today, per vertex, in the two places that write a colour
array - `ParticleSystem::render` and `Engine::queueSprite` - and under
`#ifdef __EMSCRIPTEN__`, because on the desktop it is work that GL has already
done. That is the stopgap, not the answer: it is four `clamp` calls per quad in
C++ for something a GPU does for nothing.

Two ways out, both cheap:

- **Ask the emulation for the clamp.** It is already written; it just sits behind
  `GL_LIGHTING`. Whether a lighting setup exists that emits the clamp and changes
  nothing else - ambient only, `u_lightModelAmbient` at 1 and every material at
  1 - is a question about `libglemu.js`'s generated vertex shader, and the answer
  is one shader dump away. If it works it is two `glEnable`/`glLightModel` calls
  at startup and the CPU clamp goes.

- **Give the browser build its own vertex shader.** Item 41 wants a shader path
  for ordinary drawing anyway, and a `clamp` in the vertex stage is the cheapest
  line in it.

The desktop must not pay for either. Whatever lands, `-nobatch` and the native
build should still reach GL with the raw colour and let the hardware clamp it.

43. One quad budget for all fonts, and a way to say "do not cache this"
-----------------------------------------------------------------------
`Font` keeps 32 laid-out strings (`font.cpp`), and the number is the wrong
measure twice over. It is per font, so the four fonts in play - the GUI's, the
tooltip's, the credits', a skin's hint font - hold 128 entries between them with
no ceiling on what that costs; and an entry is a `std::vector<QuadVertex>`, so
what 32 actually reserves depends entirely on how long the strings are. Thirty-two
keycaps and thirty-two wrapped help pages are the same number and two orders of
magnitude apart in geometry.

Count the quads instead, and share one budget across every font, the way the
particle systems share one vertex buffer. Then the cost is stated in the one unit
that matters and a font that is barely used stops holding a slot a busy one needs.

The other half is a caller that knows better. `renderText` should take a "do not
cache" option for a string that is certainly not coming back:

- the **credits**, which animate `charScaling`, so every frame lays the same text
  out at a size it will never see again - every one of those is a cache miss that
  also evicts something useful;
- **anything drawn while the colour changes per frame** and the layout does not,
  where the entry is a hit but the cache is doing no work - the paused game's
  pulsing text is the case that prompted this.

Mind what the key does and does not carry: it is the string plus the five options
the layout depends on (`tabSize`, `charSpacing`, `lineSpacing`, `charScaling`,
`italic`) and deliberately not `shadows` or the colour, which is what lets one
entry serve the three passes `renderText` makes. A "do not cache" flag must not
leak into the key, or it would double every entry.



44. Cache the GL state, or decide once and for all not to
---------------------------------------------------------
`GLState` (`glstate.h`) owns the flush and nothing else: `setTexturing`,
`bindTexture` and the enable-stack pair put the sprite batch on the screen
before they change what a queued quad reads. What it deliberately does **not**
do is skip a call that sets what is already set, and the reason is a
measurement rather than an oversight.

Instrumenting the whole tree - every state entry point redirected to a counting
wrapper, with the attribute stack modelled properly - gave, per frame in a
played level: 4833 GL entry points in all, of which 2928 were immediate-mode
geometry, 1615 matrix work and **290 state**, and of that state **28 calls were
redundant**. A quarter of the texture binds and a third of the texture-matrix
loads set what was already set, which sounds worth having until it is put beside
the total. The sprite batch has since taken the geometry and most of the matrix
traffic away, so the same 28 calls are now a larger share of a much smaller
number - and still nowhere near the spread between two runs of the same build.

What a cache would cost is the other half of the arithmetic. It is sound only if
*everything* comes through it: there are some forty raw `glBindTexture` calls and
fifty raw `GL_TEXTURE_2D` enables in the crossfades, the GUI, the credits, the
upscalers and the framebuffer code, and one missed call leaves an entry saying
the wrong thing - after which the next skipped call is a wrong picture rather
than a missing optimisation. The `gl_state` check keeps the object sources
honest precisely because that set is small enough to read; a hundred sites
across the whole renderer is a different proposition.

Two things would change the answer:

- **A cache that checks itself.** In a `BLOCKS5_TEST_HOOKS` build, read the real
  state back before skipping and report a mismatch. Then the invariant is tested
  by the smoke run rather than argued about, and the routing can be done a file
  at a time instead of in one sweep.
- **A platform where the calls are not cheap.** These numbers come from
  swiftshader in a desktop browser. A phone, where the main thread *is* the
  limit, may read differently - and `WebBuild/test/perf.js` with an arm that
  turns the cache off is how to find out rather than guess.

The same file holds the second half of this: `Texture::bind` brackets its
texture-matrix load in `glPushAttrib(GL_TRANSFORM_BIT)`/`glPopAttrib`, two of
the heaviest calls in the API to restore one enum. Replacing them with an
explicit `glMatrixMode(GL_MODELVIEW)` would save about forty calls a frame and
is **not** safe as it stands: `Level::render` binds the rain, the snow and the
clouds with `GL_TEXTURE` already current, and only happens to survive because
each of those three sets the mode again straight after. Tracking the matrix mode
in `GLState` would settle it, and that means routing the fifty-odd
`glMatrixMode` calls through it as well - the same sweep, and the same argument
about whether it earns its keep.



How these connect
-----------------
    2 (scaling) ──┬─> 8 (shader upscaler, no readback)  — the readback is gone
                  ├─> 5 (Linux: the __asm block no longer blocks GCC/Clang)
                  ├─> 3 (libs/bin is one import library)
                  └─> 10 (the FBO is the shared prerequisite)

   10 (window) ────> did not need SDL2 after all: a borderless window styled
                     behind SDL's back keeps the GL context alive

    3 ─────────────> 5 (Linux needed an ffmpeg answer anyway)

    7 (English comments) ───> the encoding half is done; only the translation left

   14 (overwrite) ──> 15 (delete) ──> 17 (the Manager): one isBuiltIn() serves
                      all three, and 17 is where the delete lands

   19 (controls) <──> 22 (tap radius): the pad answers the keys, 22 the buttons

   PNG screenshots ──> 28 (browser video): the browser half of the recording is
                      what is left once the picture can leave the page at all

   26 (shipped content) ──> 27 (credits): the "was it the shipped campaign?"
                      test is a hardcoded path into the user's folder, and 26
                      moves that folder out from under it

   30 (skin sounds) <──> 32 (hint sound): the paper one belongs to the skin
                      that brings the paper, so 32 is 30's first real caller

   38 (gestures) <──> 39 (editor strokes, done): both are code that is correct
                      only because a mouse never teleports. 39 is fixed; 38 has
                      to leave the editor's field painting rather than scrolling

   22 (tap radius) <──> 40 (pad labels): a button a finger has to find, by size
                      and by what it says. 40's symbols wait on the bindings
                      being pushed to the page, which publishLanguage() started

   36 (shadow detail) <──> 41 (one-pass shadows): 36 makes the sample count a
                      setting, 41 removes the count altogether - so 41 is the
                      answer 36 is a stopgap for, and 36 is worth doing only
                      while 41 is out of reach

The one change under both 2 and 10 was the same 80 lines: render into a
framebuffer object instead of the back buffer. Everything else in either item was
an increment on it.

What left the tree along the way: `sdl.dll`, `sdl_image.dll`, `libpng15-15.dll`,
`zlib1.dll`, the four ffmpeg DLLs, `oalinst.exe`, `vcredist_x86.exe`,
`hq2x32.obj`, ten import libraries and the `msinttypes` shim. What ships now is
three executables, **one** DLL that needs nothing but Windows, and the data.
