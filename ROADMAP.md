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


7. Translate all source comments to English
-------------------------------------------
Comments across `Blocks5/src` are in German. The translation is mechanical but
enormous, and it wants to be one sweep rather than a drip, because half-translated
files are worse than either end state.

**The encoding half is already done, and it was the dangerous half.** Every source
file is pure ASCII: umlauts are written `ae oe ue ss`, and the two bytes that are
not text at all are explicit escapes — `'\xA7'` (§) in `engine.cpp` and `'\xB6'`
(¶) in `font.cpp`, plus a few inline localized strings shaped `"\xA7" "de:…"`.
Those are a wire format shared with `data/languages.txt`, which is Latin-1 and
shipped that way; as characters they would have changed meaning the moment anybody
re-encoded a source file, silently and with no compiler error.

So the translation is now only a translation: no `/utf-8` switch to remember, no
BOM, no encoding decision to get wrong halfway through. The one rule to keep is
the one in CLAUDE.md — do not type an umlaut into a comment.

`data/languages.txt`, `readme.txt` and `levels/readme.txt` are shipped files with
their own encoding and CRLF endings; they are not part of this.


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
nudge moves the output by about one. Shipped as `src/crt_shader.h`, a fourth entry
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

Cost of one present on a software rasterizer, as ratios: nearest 1.0, bilinear
1.3, sharp-fit 1.35, CRT 7.8 — halation about half of that, and
`BLOOM_STRENGTH = 0` compiles it out (4.2). On any real GPU all of them are noise.

Left for later: anisotropic curvature (real tubes are not spherical), a shadow-mask
dot triad as an alternative to the aperture grille, and moving halation to a second
pass if the single-pass ring ever looks too tight.


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

The one change under both 2 and 10 was the same 80 lines: render into a
framebuffer object instead of the back buffer. Everything else in either item was
an increment on it.

What left the tree along the way: `sdl.dll`, `sdl_image.dll`, `libpng15-15.dll`,
`zlib1.dll`, the four ffmpeg DLLs, `oalinst.exe`, `vcredist_x86.exe`,
`hq2x32.obj`, ten import libraries and the `msinttypes` shim. What ships now is
three executables, **one** DLL that needs nothing but Windows, and the data.
