Blocks 5 - Roadmap
==================

Planned work, roughly in the order it was proposed. Each entry records what is
actually in the way, with file references where they are known, so the next
person does not have to rediscover it. Nothing here is scheduled.

Several of these unblock each other — see [How these connect](#how-these-connect)
at the end.


1. Auto-detect the user's language on first start  — **DONE**
------------------------------------------------------------
`Engine::detectSystemLanguage` asks `GetUserDefaultUILanguage` on Windows,
`navigator.languages` in the browser and `LC_ALL`/`LC_MESSAGES`/`LANG` elsewhere.
It answers only `de` or `en`, deliberately: of the 349 IDs in
`data/languages.txt` exactly one has a `§fr:` body and one a `§es:`, so those are
stubs and detecting `fr` would produce an English game with a French label. It is
consulted only when `config.xml` has no `<Language>`; an explicit choice always
wins, and the options dialog still offers exactly the two.

The detection itself was the small half. The reason it could never have worked is
worth recording: a `config.xml` containing nothing but `<Language>en</Language>`
was **tracked in the repository**, copied into the webroot by `WebBuild/build.sh`
and installed into the user directory by `main.cpp` on first run. So English was
pinned before any detection could run, and deleting your own `config.xml` simply
got an English one written back on the next start. The installer ran the same
trick from the other end: `[Run]` called `makeconfig.bat`, which copied
`_config_en.xml` or `_config_de.xml` according to the installer's own language.

All of that is gone - the four files, the `[Run]` entry, the two `ConfigID`
messages in the `.iss`, the copy in `main.cpp` and the copy in `build.sh`. The
game writes `config.xml` itself when it exits, so nothing has to ship a template.
`setLanguage` still refuses anything but `de`/`en`, which is now the only place
that whitelist lives.

Verified in Chromium at three locales: `de-DE` starts the game in German,
`en-GB` in English, and `fr-FR` in English.


2. Replace HQ2X with something that ships as source  — **DONE**
----------------------------------------------------------------
hq2x is gone from the tree: `src/hq2x.cpp`/`.h`, `libs/bin/hq2x32.obj`, the
`AdditionalDependencies` entry that linked it, `useHQ2X`, `Engine::upscaleFrame`,
the `-hq2x` switch, `hq2x.bat`, the installer's HQ2X start-menu entry, and the
`WebBuild` glob exclusion with its stubs. `sharp-fit`, a twenty-line shader of our
own, runs in its place, in both builds. The rest of this entry is why, kept
because the measurements are the argument.

**xBR-lv2 was the answer for a while and is also gone; see below.** Its removal is
the more interesting result of this item, because it says something about the
game's art rather than about hq2x's licence.

Three separate problems, all of them now moot:

- **No source.** The object was MaxSt's hq2x, LGPL 2.1, statically linked. That is
  the arrangement we deliberately avoided for OpenAL Soft — static LGPL linking
  carries relinking obligations that a dynamically shipped DLL does not. Nothing
  in the tree is linked that way any more.
- **`Blocks5/src/hq2x.cpp` had inline `__asm`.** The glue that built the two
  lookup tables ended with an x86 `__asm { cpuid }` block probing for MMX. That is
  MSVC-x86-only: it blocked x64, it blocked Clang and GCC, and it is why
  `WebBuild/build.sh` had to filter `hq2x.cpp` out of the source glob. The MMX
  probe was also pointless on any CPU made this century.
- **The browser had no upscaler at all.** `-hq2x` was simply unavailable there,
  so a 640x480 canvas was scaled by whatever the browser does. Both builds now
  compile the same shader.

### What hq2x actually buys, measured

The object still runs. `objcopy -O elf32-i386` converts it from COFF, its only
undefined symbols are `_LUT16to32` and `_RGBtoYUV` (both in `src/hq2x.cpp`), and
it links into a 32-bit Linux binary. Fed a real captured game frame through the
exact RGBA -> RGB565 conversion `Engine::upscaleFrame` does, against a plain
nearest-2x of the same frame:

| region | output pixels visibly changed (>8/255) |
| --- | --- |
| whole frame | **4.85%** |
| level-title text | 11.39% |
| HUD bar (text + GUI) | 8.23% |
| play area (tiles + rain) | 4.26% |
| rainy sky | 1.93% |

**95% of the frame is plain nearest-neighbour.** hq2x classifies a 3x3
neighbourhood by thresholded YUV distance, which only yields a coherent edge on
flat-coloured, hard-edged art. This game is mostly not that: the tiles are
photographic, the sprites are airbrushed with anti-aliased edges, and every
neighbour reads as "different", so no pattern matches and the filter passes the
pixel through. It earns its keep on the font and the GUI, which *are* two-tone
hard-edged art — and there it looks better than any interpolating filter.

What that 5% costs, timed on the real object at 640x480:

    RGB565 conversion : 0.343 ms/frame
    hq2x_32 (MMX)     : 7.258 ms/frame
    total CPU         : 7.600 ms/frame     (16.7 ms budget at 60 fps)

45% of a frame at 60 fps on a 2.8 GHz Xeon, and that excludes both bus
transfers: the `glReadPixels` (1.2 MB, a full pipeline flush immediately after
rendering) and the `glDrawPixels` upload (4.9 MB of 1280x960 RGBA, every frame).

### The prerequisite is an FBO, not a shader

There is currently **zero** shader and **zero** framebuffer-object infrastructure
in the tree — one `SDL_GL_GetProcAddress` call exists in the whole codebase
(`engine.cpp:352`, for `glBlendFuncSeparate`). The work, in order:

1. **An extension loader** for ~25 entry points (FBO + GLSL). glad generates a
   single dependency-free `.c`, which fits both the compile-from-source rule and
   the one-DLL rule.
2. **The FBO.** Colour texture at 640x480 plus a **packed depth-stencil
   renderbuffer** — `cf_star.cpp` and `level.cpp:625` both use the stencil
   buffer, so a colour-only FBO breaks the star wipe and the light masking. This
   is the one real gotcha.
3. **Frame bracketing.** `glViewport` is set once at init to 640x480 and never
   changes — even with hq2x on, the game rendered into the bottom-left corner of
   a 1280x960 window. So the FBO pass needs no viewport change at all; only the
   present pass does.
4. **Replace `upscaleFrame()`** with bind-texture, `glUseProgram`, one quad.
5. **`glReadBuffer(GL_BACK)` is an error with an FBO bound** — three sites need
   `GL_COLOR_ATTACHMENT0` instead. Easy to miss, and it fails quietly.

The six `glCopyTexSubImage2D` sites need no change: they read the bound
framebuffer, which becomes the FBO. Worth knowing which they are, because they
are often mistaken for CPU readbacks and are not — `glCopyTexSubImage2D` is a
copy inside the GPU:

| site | what it grabs |
| --- | --- |
| `gui.cpp:106` | the GUI layer, every frame any element drew |
| `engine.cpp:802`, `:809`, `:1888` | the before/after images for a crossfade |
| `level.cpp:2261` | the screen, to redraw it through a 65x41 warped grid (toxic haze) |
| `cf_mosaic.cpp:45`, `gs_credits.cpp:78` | mosaic wipe, credits scroller |

Only three calls crossed the bus, all `glReadPixels`: video capture (per
recorded frame), screenshots (on demand), and hq2x (**every frame it was on**) —
the only per-frame CPU round trip in the renderer. With hq2x gone, nothing in a
normal frame reads back at all.

**The FBO half is done.** `src/glextensions.cpp` loads the ten entry points
(`glGenFramebuffersEXT` first, the core spelling as a fallback; core in WebGL, so
nothing to load there), and `Engine` renders every frame into a 640x480 region of
a 1024x512 texture with a packed depth-stencil renderbuffer, then presents it as
one letterboxed quad. Verified in the browser build: the menu renders
pixel-identically to the pre-FBO build within the noise floor of its own animated
background - two runs of the same build differ *more* from each other than the
before/after pair does - and gameplay, the stencil light mask, crossfades, the
scissor-clipped level list and mouse hit-testing all still work.

### Why the answer is not an edge-directed filter

xBR-lv2 was vendored in `libs/xbr` for a while — MIT, ported to compile as either
desktop GLSL 1.10 or GLSL ES 1.00 — and it has been removed again, both variants.
The reasoning is the same measurement that opens this item, taken one step
further.

Every decision inside xBR is a `step()` against a threshold: is this texel the
same colour as that one, is this edge steeper than that one. On flat-shaded pixel
art those decisions are stable, because neighbouring texels are either identical
or plainly different. This game's tiles are airbrushed and photographic, so
neighbouring texels sit *near* the thresholds — and a change too small to see
flips them. Nudge a frame by 0–3 of 255, about what the animated level does where
it shows through a semi-transparent dialog, and:

    filter        mean change   pixels moving >8/255   worst pixel
    nearest          1.5              0.00%                 3
    bilinear         1.7              0.00%                 3
    sharp-fit        1.6              0.00%                 3
    xBR              2.0              1.15%               154
    xBR detailed     1.9              1.05%               154

The first three move by exactly what the input moved. xBR turns an invisible
change into 1% of pixels jumping by up to 154, and those pixels are the glyph
outlines — visible as text flickering over a dialog. The same mechanism smooths
the dithered grass into strokes, which on photographic tiles reads as smearing
rather than as reconstruction.

Two bugs in the libretro GLSL had masked this for a while by suppressing the edge
detection almost entirely (a `f4` that is read without ever being assigned, and
BT.601 and BT.709 luma mixed in the same comparisons; the Cg original has
neither). Fixing them made xBR behave correctly and look worse, which is the
clearest possible statement that the filter does not fit the content.

So both hq2x and xBR — the two edge-directed filters tried here — turned out to be
answers to a question this game does not ask. What is left is honest scaling:
`sharp-fit`, `nearest`, `bilinear`.

Three filters are selectable, and the player picks one in Options -> Scaling,
exactly like the language — there is no command-line switch for it. The choice is
saved as `<Upscaler>` in `config.xml`: `sharp-fit`, `nearest`, `bilinear`.
**`sharp-fit` is the default** wherever the machine can compile the shader, which
means GL 2.0, so in practice everywhere; where it cannot, the effective filter is
`nearest` and the entry is not offered.

`sharp-fit` is `nearest` without the integer-scale restriction, and it is the one
filter here that is ours rather than vendored — `src/sharpfit_shader.h`, about
twenty lines. The idea is to nearest-upscale the frame by the smallest integer
factor N that covers the destination rectangle and then resample that down to the
real size, so the fractional remainder shows up as a roughly one-pixel soft edge
instead of as unevenly doubled source pixels. It does not need two passes:
bilinear over a nearest-upscaled image is piecewise linear — constant inside a
source texel, a ramp of width 1/N across each texel boundary — and sampling the
*original* texture bilinearly at a coordinate remapped through that same function
gives exactly the same values, in one fetch. Checked against a genuine two-pass
implementation in WebGL at five window sizes: pixel-identical at an integer scale,
and elsewhere a maximum channel difference of 1 on 2–10% of pixels, which is the
8-bit rounding of the intermediate buffer the two-pass version has and the
one-pass version does not. The same arithmetic is known as "sharp bilinear" in the
emulator world (Themaister, libretro); it is derived here rather than copied.

Where the machine has no shader or no framebuffer object, `getEffectiveUpscaleFilter()`
returns `nearest` and the `sharp-fit` entry is hidden from the dialog rather than
offered and ignored. The *wish* is kept as the player set it, so the same
`config.xml` does the right thing again on a machine that can run it —
`getUpscaleFilter()` is what was chosen, `getEffectiveUpscaleFilter()` is what is
drawn.

Since item 10 the window can be any size, so the setting has something to do:
`computePresentRect` returns the largest centred 4:3 rectangle that fits, and the
filter decides how the 640x480 frame gets there. `nearest` additionally snaps to
an integer scale — at 1280x900 that means 640x480 dead centre rather than
1200x900, which is exactly the trade it exists to make, and `sharp-fit` exists for
people who do not want to make it.

Cost of one present, measured on a software rasterizer at 1280x960, so the numbers
are only meaningful against each other: nearest 7.3 ms, bilinear 9.7 ms, sharp-fit
12.8 ms. (xBR, while it was here, cost 79.1 ms.) On any real GPU all three are
noise.

### What was considered and rejected

The reasoning that led to xBR is worth keeping, because it was right about the
problem and wrong about the answer. The measurement said the frame has two
regimes: hard-edged text and GUI, where hq2x wins, and anti-aliased photographic
tiles, where it degenerates to nearest. xBR looked like it served both — same
edge-detection premise, but blending along detected edges instead of snapping.
What that missed is that "anti-aliased photographic" is not a regime an
edge-directed filter degrades *gracefully* on; it is one where its decisions
become unstable. See above.

**Scale2x/Scale3x** shares hq2x's flat-art premise and would do even less here.
**Porting hq2x itself to GLSL** — a 256-entry pattern table — would reproduce the
filter this item set out to replace.

Both are moot now. `hq2x.cpp`'s `__asm` block went with it, and so did the
`SDL_ListModes` search that hunted for a fullscreen mode of at least 1280x960 —
it existed *only* because the filter was locked to exactly 2x. The window is a
plain 640x480 again, whatever the filter; see item 10 for making it resizable.

### The browser side is easier than it looks

Verified against the built `WebBuild/build/blocks5.js` rather than assumed.
Emscripten's `LEGACY_GL_EMULATION` installs its generated fixed-function program
only when the app has not bound one of its own:

    if(!GL.currProgram){ if(GLImmediate.fixedFunctionProgram!=this.program){ GLctx.useProgram(this.program); ... } }

and its wrapped `glUseProgram` sets `GL.currProgram`. So `glUseProgram(mine)` ->
draw quad -> `glUseProgram(0)` composes cleanly with the emulation; the two do
not fight. Framebuffer objects are core in WebGL 1, no extension needed. Use a
real VBO for the quad rather than `glBegin`, and `gl_immediate.cpp` is bypassed
entirely. `gl_compat.cpp` only includes `GL/gl.h`, so the GLES2/glext
declarations have to be added there.

This is also where the change is most visible, since the browser build has no
upscaler today — and it is the only half that can be tested without Windows.


3. Compile every dependency from source  — **DONE**
---------------------------------------------------
What is left as a Windows binary:

| Binary | What it is | Notes |
| --- | --- | --- |
| `libs/bin/OpenAL32.lib` + `OpenAL32.dll` | OpenAL Soft 1.25.2 | LGPL, *must* stay a DLL |

That is the whole list, and it is an import library plus the DLL it imports —
there is no compiled code without source anywhere in the tree. Everything else is
built from vendored source: TinyXML, zlib + minizip, libogg, libvorbis,
stb_image, SDL 1.2.15, minih264, shine and minimp4. `hq2x32.obj` was the
last holdout and went with item 2.

**SDL_image is done** — `Blocks5/src/img_load.cpp` supplies `IMG_Load_RW` over
stb_image for both builds. It also retired a latent bug: SDL_image 1.2 loads its
codecs with `LoadLibrary` at runtime and asked for `libjpeg-8.dll`,
`libtiff-5.dll` and `libwebp-2.dll`, none of which were ever in the tree.

**SDL is done** — all 67 files of SDL 1.2.15's Win32 subset are compiled from
`libs/SDL-1.2.15/src`. What that does not change is that SDL 1.2 has been
end-of-life since 2012; the honest version of that task is "move to SDL2", which
is a different and much larger project — the input layer, the window/GL setup
and the event loop all touch it. Worth splitting off as its own item.

**ffmpeg is done, and it fixed a bug at the same time.** It had been used for one
thing: writing an AVI through `avcodec_encode_video` / `avcodec_encode_audio`,
APIs removed from ffmpeg years ago, which is why it was pinned at 0.8 from 2011.
Four DLLs and seven import libraries are gone, and `libs/msinttypes-r26` with
them — it existed only to satisfy ffmpeg's headers, so `__STDC_CONSTANT_MACROS`
and `__STDC_LIMIT_MACROS` left the project defines too.

The replacement is **H.264 Baseline video plus MP3 audio in a non-fragmented
MP4**, written by three vendored source libraries:

| | | |
| --- | --- | --- |
| `libs/minih264` | H.264 encoder, one header | CC0 |
| `libs/shine` | MP3 encoder, ~2,800 lines | LGPL v2 |
| `libs/minimp4` | MP4 muxer, one header | CC0 |

That combination was chosen over the alternatives for one reason: it is the only
one that is native on Windows *and* on Linux, which is what an eventual Linux
build needs.

- **Windows**: documented, not inferred. Microsoft's *Supported Media Formats in
  Media Foundation* lists the MPEG-4 container and the H.264 decoder as Windows
  7, and the Windows Media MP3 Decoder as Windows Vista; the *MPEG-4 File
  Source* page lists the `'mp4a'` sample entry as meaning "AAC **or** MP3", and
  the H.264 decoder page covers "Baseline, Main, and High profiles".
- **Linux**: H.264 decode is normal now — Ubuntu ships `gstreamer1.0-libav`, and
  Fedora enabled it in `libavcodec-free` once the base patents expired.
- ~~Ogg Theora~~ was the tempting option, because libogg and libvorbis were
  already vendored. It is the wrong target: Theora has never shipped in any
  version of Windows and has never been a Store codec extension, and Chromium
  removed Theora decoding in Chrome 123, so Chrome and Edge no longer play it
  either.
- AAC would be the conventional MP4 audio codec, but there is no small AAC
  encoder that can be used here — fdk-aac's Fraunhofer licence is
  GPL-incompatible and faac is old and poor.

Two things found while building it, both recorded in the libraries'
`PROVENANCE.txt`:

- **minimp4 could not actually mux MP3.** It hardcoded the `objectTypeIndication`
  byte to AAC and only wrote the `esds` descriptor at all when a decoder-specific
  info blob had been set, which MP3 does not have. Since that byte is the only
  thing distinguishing AAC from MP3 in an `'mp4a'` track, the audio came back out
  declared as AAC and undecodable. It is fixed **without touching the library**:
  the constant is a macro, and minimp4's declarations and implementation sit on
  opposite sides of its include guard in one file, so `minimp4_impl.c` includes
  the header twice and redefines the macro in between. Two `#error` guards and a
  runtime counter catch an upgrade that breaks the hook — all three tested by
  deliberately breaking a copy of the library. What the macro cannot reach —
  a clear reserved bit, a missing `SLConfigDescriptor`, a `DecoderSpecificInfo`
  that MP3 must not have — `videorecorder.cpp` fixes in the finished file. It
  passes a one-byte DSI rather than a zero-byte one precisely so that upstream's
  descriptor comes out the same length as a conformant one, which makes the
  replacement a pure in-place substitution with no box size changing anywhere.
- **minih264 and minimp4 collide** if both are instantiated in one translation
  unit — each defines a `bs_t` in its implementation half. Hence the two
  one-line `*_impl.c` files.


4. Build with the newest MSVC  — **DONE**
------------------------------------------
Built and run on **v143** (VS 2022) and on **v145**. There is no hardcoded
default any more: the three `.vcxproj` files ask for
`$(DefaultPlatformToolset)`, which is whatever the Visual Studio doing the build
calls its own newest, and `Build.bat` passes no `/p:PlatformToolset` unless
`/toolset:vNNN` names one — a global property cannot be overridden from inside
a project, so passing one always would have been a hardcoded version wearing a
different hat. A Visual Studio newer than this tree therefore needs no change
here at all. `WindowsTargetPlatformVersion` moved into the projects under the
same rule, so the IDE and `Build.bat` now agree without anyone passing anything.

What the older toolsets are worth is now stated honestly: **v120 and v140 have
never been built since the prebuilt libraries went away.** That they still work
was reasoning about the code, not a compiler run. The `/toolset:` plumbing for
them stays — it costs nothing, and it is the starting point for whoever tries.

Getting there: the `<hash_map>` problem is gone (the 41 `stdext::hash_map` /
`hash_multimap` uses across 12 files are `std::unordered_map` /
`std::unordered_multimap` now, the header is out of `pch.h`, and
`_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS` is out of the project defines), the
missing `<algorithm>` includes are in, `register` is gone from
`MersenneTwister.h`, the string literals assigned to `char*` in `e_flipflop.cpp`
/ `e_gate.cpp` are `const char*`, and `cannon.cpp`'s `float*`/`double*` `sscanf`
mismatch is fixed.

That also made the tree far easier to check without MSVC: **110 of the 114
sources in `Blocks5/src` pass `i686-w64-mingw32-g++ -fsyntax-only`** against the
real `pch.h`. The four that do not are `main.cpp` (SEH `__except`),
`filesystem.cpp` (includes `Shlobj.h`, lowercase on case-sensitive systems),
`stackwalker.cpp` (DbgHelp internals) and `videorecorder.cpp` (WASAPI).


5. Enable a Linux build  — **DONE**
-----------------------------------
`LinuxBuild/build.sh` builds a native binary that runs, plays and passes
`LinuxBuild/test/smoke.sh`. What follows is the plan; what it turned into is at
the end of the entry.

Someone did this once and it worked, but the result was never published.

The blockers are enumerable — seven `#error NOT IMPLEMENTED` sites in four
files:

    filesystem.cpp:62     getAppHomeDirectory
    filesystem.cpp:177    createDirectory
    filesystem.cpp:188    deleteDirectory
    file_real.cpp:78      directory listing
    util.cpp:386          high-resolution timer
    main.cpp:113          update check over HTTP
    main.cpp:318          the "update available" prompt

Every one of them is the `#else` of an
`#ifdef _WIN32 / #elif defined(__EMSCRIPTEN__)` chain — and the Emscripten
branches sitting right above them are already plain POSIX: `::mkdir`, `::rmdir`,
`opendir`/`readdir`/`stat`, `emscripten_get_now`. So most of this is widening
`#elif defined(__EMSCRIPTEN__)` into `#else` and supplying a Linux answer for the
three that genuinely differ (home directory, the HTTP update check, the message
box). `audiocapture.cpp` is already `#ifdef _WIN32` with a working `#else` stub,
and `stackwalker.cpp` is already excluded from non-Windows builds.

`WebBuild/platform_stubs.cpp` is worth reading first for a different reason: it
is the list of SDL 1.2 entry points a non-Windows build turned out to need
shimmed.

A mingw sweep named the first four things a GCC-based build would reject, and
all four are gone since: `<algorithm>` is in `pch.h` now, which every source
includes first, so `panel.cpp`, `e_pulsepanel.cpp` and `teleporter.cpp` calling
`std::find` are fine — libstdc++ is the one standard library that does not pull
it in through the container headers. The fourth, `filesystem.cpp` including
`Shlobj.h` where the file is `shlobj.h`, only ever mattered to the mingw sweep:
the include sits inside `#ifdef _WIN32` and a Linux build never reaches it
(`tools/syntax.sh` generates a capitalised forwarder so the sweep can still
compile the Windows path). Everything in `Blocks5/src` except `main.cpp`'s SEH
block parses under GCC today — `sh tools/syntax.sh` is the standing check.

What still needs deciding:

- `getAppHomeDirectory()` — `My Documents\Blocks 5\` becomes
  `$XDG_DATA_HOME/blocks5` or `~/.local/share/blocks5`.
- **Case sensitivity.** The game resolves assets by name. Inside `data.zip` that
  is fine, but the loose-file development mode (`fs.pushCurrentDir("data")` in
  `main.cpp`) and user levels/skins on disk will expose every filename whose case
  does not match. Expect to find some.
- SDL 1.2 from the distro, or the SDL2 move from item 3.

**WSL2 is a usable test bench for this, with one condition.** WSLg supplies
everything the game asks of a host: Weston plus XWayland for SDL 1.2's X11
backend, a real PulseAudio server for OpenAL Soft, and hardware-accelerated
OpenGL through Mesa's d3d12 Gallium driver — 4.1 in *both* core and
compatibility profiles, where the game needs GL 2.1-class compatibility at most
(fixed-function `glBegin`/matrix stack, `gluPerspective`/`gluLookAt`/
`gluOrtho2D`, FBO with packed depth-stencil, GLSL 110).

The condition is **where the tree lives**. Case sensitivity is the open question
above, and it is decided by the filesystem: WSL's own ext4 under `~` is
case-sensitive and exposes exactly those bugs, while `/mnt/c` is case-insensitive
by default and hides every one of them. Building on the Windows drive switches
off the most valuable thing the exercise buys.

Two things it will not settle. Window behaviour — item 10's whole area — is not
representative: WSLg puts each app in a rootless window under Weston-over-RDP,
which is neither the Win32 `WS_POPUP` fullscreen nor a real Linux WM. And the
audio capture above wants `@DEFAULT_MONITOR@`; PulseAudio gives every sink a
`.monitor` source automatically and WSLg's `RDPSink` is an ordinary sink, so it
should be there, but WSLg has known underruns on that sink, so recorded audio
*quality* should not be judged there.

Two things a Linux build will *not* have to decide, because item 3 already
settled them with Linux in mind:

- **Video recording.** minih264, shine and minimp4 are plain C with no platform
  code, and H.264-in-MP4 plays on a current Linux desktop as readily as on
  Windows. `videorecorder.cpp` itself contains nothing Windows-specific any more.
- **Audio capture.** PulseAudio and PipeWire both give every output sink a
  monitor source, which is the exact equivalent of the WASAPI loopback the
  Windows build uses. `@DEFAULT_MONITOR@` resolves to the default sink's monitor,
  so nothing needs enumerating, and `pa_simple_new` takes the sample spec you
  want — asking for S16LE/48000/2 makes the server resample, so none of
  `audiocapture.cpp`'s format conversion or its linear resampler is needed. About
  60 lines, `dlopen`'d so the game still runs where PulseAudio is absent.
  libpulse rather than the native PipeWire API, because `pipewire-pulse` means
  one client API covers both. Verified against a live server: `@DEFAULT_MONITOR@`
  on a 44.1 kHz sink delivered exactly 96000 frames in 2.00 s at 48 kHz.

  `audiocapture.cpp` already splits along the right line — the ring buffer,
  `push`/`pushSilence`/`getSamples` and the public API are platform-neutral, and
  only `threadProc` and the format conversion are Windows-specific. One thing
  should move into the shared half when that happens: the clock-based silence
  padding. `module-suspend-on-idle` is loaded by default, so an idle sink stops
  delivering exactly as WASAPI does, and `getExactTimeMS()` is already
  cross-platform.

**How it went.** Eight sites, not seven — `transfer.cpp` grew one when the
Manager arrived. Five were the widening this entry predicted: `createDirectory`
(now walking the path, since `~/.local/share` may not exist where
`My Documents` always does), `deleteDirectory`, the directory listing, the
timer (`clock_gettime(CLOCK_MONOTONIC)`), and `getAppHomeDirectory()` →
`$XDG_DATA_HOME/blocks5/`. Three needed a Linux answer of their own, and one
thing the entry did not foresee turned out to be the only real design decision:

- **The fullscreen switch.** Item 10 built it as a Win32 style flip behind SDL's
  back, and the reason it must stay behind SDL's back holds under X11 too:
  `X11_SetVideoMode` rebuilds the window for a mode change and takes the GL
  context with it. But the X11 analogue is not "set the style yourself" — under
  X11 a program does not put its own window into fullscreen, it asks the window
  manager with a `_NET_WM_STATE` message (EWMH) and the window manager decides
  size and position. That arrives back as an ordinary `SDL_VIDEORESIZE`, which
  `handleResize()` already owns, so the Windows architecture carried over
  exactly. It also means `applyWindowStyle()` must *not* call `handleResize()`
  itself on this path: the size is not known yet, and forcing it would set
  `SDL_SetVideoMode` against the window manager.

  This is the one piece that needs Xlib, and it is a translation unit of its
  own, `LinuxBuild/linux_window.cpp`, for a reason worth remembering:
  `<X11/Xlib.h>` makes `Font`, `Window`, `Screen` and `Cursor` its own type
  names, and the game has classes called exactly that. Included in
  `engine.cpp`, the next line holding a `Font*` stops compiling.

- **The file dialog.** `zenity` or `kdialog`, whichever is installed, driven
  through `popen()`. Neither GTK nor Qt becomes a dependency, and the import
  runs *asynchronously* — a non-blocking read of the pipe once per tick — so the
  window keeps drawing while the dialog is open. `pollImport()` was built for
  the browser's asynchronous dialog and took this without a change. The export
  cannot: `doExport()` returns its result immediately, as `transfer.h` says, so
  it blocks like the modal Windows dialog does.

- **The update check.** `curl` or `wget` through `popen()`, rather than linking
  an HTTPS client for sixteen bytes. Same user agent, same two-second limit, same
  16-byte guard as the Windows version. Its prompt only reaches the log: at that
  point in `main()` the engine is not up, so there is no toast bar and no window,
  and opening a browser nobody asked for at startup would be worse than a line
  of text. The check is off in the shipped state anyway.

- **`_stricmp` is MSVC's.** Eight call sites. Rather than a
  `-D_stricmp=strcasecmp` shim, `equalsNoCase()` moved into `util.h` — it was
  already hand-rolled inside `transfer.cpp` as `sameFilename`, and for a reason
  that still applies: `strcasecmp` and `tolower` follow the locale, and in
  Turkish 'I' is not the capital of 'i'. Filenames and command-line switches are
  the same in every locale.

Two smaller things fell out. The `update_checker_*.bat` files were being copied
into the user directory on three code paths; on Linux they are two Windows batch
files nobody can run, so the three copies became one helper that skips them off
Windows. And `videorecorder.cpp` compiles and links here — the three encoders are
plain C, as this entry predicted.

**The audio capture is done too**, and along the line this entry drew: `AudioRing`
at the top of `audiocapture.cpp` now holds the ring buffer, the reader side and
the clock-based silence padding, and both platforms derive from it, so only the
two `threadProc`s differ. The Linux one is a third of the size of the Windows one
because `pa_simple_new` is *told* the format to deliver and the server resamples —
none of the format conversion or the linear resampler is needed. libpulse is
`dlopen`'d and its handful of declarations written out by hand, so the build
needs no libpulse-dev and the game still starts where PulseAudio is absent.

Checked against a real server (a null sink at 44.1 kHz, so the resample was
exercised): recording produced an MP4 with H.264 640x480 and MP3 48 kHz stereo,
audio and video the same length to 3 ms. Against a simultaneous `parec` of the
same monitor, the captured track matched to within 0.7 dB RMS and cross-correlated
at 0.706 with a 1 ms offset — the same audio, at the same level, at the same time;
0.706 rather than 1.0 because one side had been through MP3.

**What was actually checked**, since the point of a Linux build is that it can
be: the game starts, reaches the menu, plays level 1 of the shipped campaign
(movement, collision, gravity, rain, the minimap, the HUD), switches to
fullscreen and back, renders all four upscale filters including the CRT shader
with its barrel distortion, writes a 640x480 screenshot with F11, imports a level
through the file dialog and sees the Manager list refresh and select it, exports
it back out byte-identically, and writes `config.xml` on exit. Under llvmpipe,
which is the slowest case there is.


6. Skins in the browser  - **DONE**, and skins that travel with campaigns
-------------------------------------------------------------------------
Two related gaps; the first is closed.

**Skins in the browser - done.** Two halves. Getting a skin *in*: the level
editor's Settings window has an **Import skin ...** button, on its own
`WebTransfer` channel, validated by requiring `tileset.xml` and `sprites.png`
in the archive. Unlike every other import it **overwrites** rather than renaming
on a clash, because a skin's filename is its identity - a level says
`skin0="space"` and `Level::getSkinFilename` looks for `levels/skins/space.zip`
- and the four shipped names are refused outright.

Getting it to *draw* was the predicted blocker, and the guess in this item was
right: WebGL 1 treats a non-power-of-two texture as incomplete unless it is
sampled with `CLAMP_TO_EDGE` and no mipmaps, and every sample then returns black
- silently, with no GL error. The default is `GL_REPEAT`. It really was a
two-line fix in the sampler state, but *not* an unconditional one: `level.cpp`
scrolls the texture matrix without bound for rain, snow and clouds, so those
genuinely need `GL_REPEAT`. `Texture::applyWrapMode` therefore switches only the
textures that are NPOT, which is exactly the set on which `GL_REPEAT` could not
have worked anyway. It runs under Windows too, so a skin does not tile for its
author and clamp for everybody else.

Reproduced before the fix by padding `space.zip`'s `tileset.png` to 130x130 and
`sprites.png` to 258x1026 and importing that: black level, black palette.
Afterwards it renders identically to the power-of-two original, and a rainy
level still tiles its rain.


**Skins in campaigns.** A campaign archive carries its levels and music but not
the skins they reference, so a campaign built on a custom skin still needs the
skin sent separately. That is much less painful than it was - the main menu's
Import takes a skin as readily as a campaign, and tells the player the name to
type - but self-contained campaigns would still be better. `Campaign::save` was
rewritten around a `LevelRef` that knows whether its source is loose or inside
an archive, and skins would follow the same shape. The music half of this
problem was solved differently: a level says `musicFilename="blocks:music2.ogg"`
and borrows a track from the shipped campaign instead of carrying a copy.


7. Translate all source comments to English
-------------------------------------------
Comments across `Blocks5/src` are in German. The translation is mechanical but
enormous, and it wants to be one sweep rather than a drip, because
half-translated files are worse than either end state.

**The encoding half of this is already done, and it was the dangerous half.**
Every source file in the tree is now pure ASCII: umlauts in comments are written
`ae oe ue ss`, and the two bytes that are not text at all are explicit escapes —
`'\xA7'` (§) in `engine.cpp` and `'\xB6'` (¶) in `font.cpp`, plus a few inline
localized strings shaped `"\xA7" "de:…"`. Those two are a wire format shared with
`data/languages.txt`, which is Latin-1 and shipped that way; as characters they
would have changed meaning the moment anybody re-encoded a source file, silently
and with no compiler error. As escapes they cannot.

So the translation is now only a translation. There is no encoding decision left
to get wrong halfway through, no `/utf-8` switch to remember and no BOM to add,
and a file can be edited by any tool on any machine without the question coming
up. The one rule to keep is the one in CLAUDE.md: do not type an umlaut into a
comment.

`data/languages.txt`, `readme.txt` and `levels/readme.txt` are shipped files with
their own encoding and CRLF endings — they are not part of this.


8. Rendering performance
------------------------
The renderer is fixed-function immediate mode: 120 `glBegin` blocks across 38
source files, one draw call per sprite, per GUI element, per particle. On the
desktop this is old but survivable; in the browser every one of them goes
through Emscripten's `-sLEGACY_GL_EMULATION`, which rebuilds a vertex buffer per
block and prints "do not expect it to work" on every start.
`WebBuild/gl_immediate.cpp` exists purely to make the game's blocks palatable to
that emulator.

The work, in order of payoff:

- **Batch sprites.** Everything drawn through `Engine::renderSprite` shares a
  texture atlas per tileset; accumulating quads into one vertex buffer and
  issuing a single draw per texture would collapse thousands of calls into a
  handful. This is where the big win is, in both builds.
- ~~**Kill the readback in `upscaleFrame`.**~~ Done with item 2. It was
  `glReadPixels` → CPU → upload, stalling the pipeline every frame hq2x was on:
  **7.6 ms of CPU per frame**, 45% of a 60 fps budget, before either bus
  transfer, to visibly change under 5% of the pixels. Nothing in a normal frame
  reads back now.
- Then, if it is still worth it, a programmable pipeline for the rest. Items 2, 8
  and 10 converge here.

Worth measuring before optimising: `BEGIN_PROFILE` / `END_PROFILE` from `util.h`
are already available, and `PROFILE_VIDEO_CONVERSION` and
`PROFILE_VIDEO_ENCODING` are existing switches.


9. Stop needing the Visual C++ redistributable — done
-----------------------------------------------------
The three projects build with `/MT` now, SDL is compiled in rather than loaded
from a DLL, and `vcredist_x86.exe` (6.5 MB), the `InstallVC2013Runtime` task and
its four message strings are out of `Blocks5/setup/Blocks 5.iss`. Nothing the
game ships needs a Visual C++ redistributable any more:

    blocks5.exe     static CRT      /MT
    pwencrypt.exe   static CRT      /MT
    showuserdir.exe static CRT      /MT
    OpenAL32.dll    msvcrt.dll      OS-provided

No foreign object file is linked into the exe any more — `hq2x32.obj` was the
last one and went with item 2, and it had carried no `/DEFAULTLIB` or
`/FAILIFMISMATCH` directive and no CRT references, so it had not stood in the way
of `/MT` either. `OpenAL32.dll` is
now the only DLL beside the executables, and it carries its own CRT across the
boundary as it always did; nothing allocates on one side and frees on the other,
because the game calls only core AL/ALC entry points and never takes ownership of
an OpenAL-side allocation.

The one thing given up is that a statically linked CRT no longer picks up
Windows Update's servicing of the shared one. For a single-player puzzle game
that is the right trade against shipping a 6.5 MB installer stub.


10. A window that behaves like a window  — **DONE**
---------------------------------------------------
All three now work, and the design below is what was built rather than what was
proposed. What is left is the small change of `_config_en.xml` / `_config_de.xml`
if the installer should ship a different default window size than 640x480.

- ~~**Be resizable**, keeping the 4:3 aspect ratio and letterboxing with black
  bars when the window does not match.~~ `SDL_RESIZABLE`, an `SDL_VIDEORESIZE`
  handler, and `computePresentRect`. The window will not go below 640x480:
  `handleResize` clamps and re-sets the mode.
- ~~**Enter and leave fullscreen while running**, not only via the command
  line.~~ Alt+Return, borderless — a window styled `WS_POPUP` and sized to the
  desktop — not an exclusive display-mode change. That choice is what keeps the
  toggle from destroying the GL context; see below.
- ~~**Switch the upscaling filter while running.**~~ Options -> Scaling offers
  all four, the change takes effect on the next frame, and it is saved as
  `<Upscaler>` in `config.xml`.

`SDL_SetVideoMode` used to be called exactly once, with no `SDL_RESIZABLE`, and
the mode was decided at startup from `-windowed` / `-fullscreen`. (Fullscreen
also walked `SDL_ListModes` for the smallest mode of at least 1280x960 — a search
that existed only because hq2x was hardwired to exactly 2x. It went with item 2.)

**The FBO from item 2 is what makes all three cheap.** With the game always
rendering 640x480 into an offscreen target, every hardcoded coordinate in the
tree keeps working no matter what size the window is: the one `glViewport`
(`engine.cpp:433`), `glScissor(280, 480 - 60 - 200, 320, 200)` in
`gs_selectlevel.cpp:51`, the GUI XML layouts, the 0..640 x 0..480 texcoords on
the background quad. Only the destination rectangle of the final blit changes,
and only one place computes it. Without the FBO, every one of those is a bug.

### The blit rectangle depends on the filter

The letterbox is one calculation, but not the same one for every filter.

- **Nearest needs an integer scale.** At a fractional scale, nearest duplicates
  some source pixels and not others, so the sprites come out with uneven
  thicknesses and the text goes ragged — the failure it is chosen to avoid. So
  for nearest the destination size is `floor(min(w / 640, h / 480))` clamped to
  at least 1, times 640x480, centred, with black everywhere else. On a 1920x1080
  window that is 2x, i.e. 1280x960 in the middle with 320-pixel bars either side
  and 60 above and below — deliberately not filling the screen.
- **Bilinear and sharp-fit take the full fractional scale**, `min(w / 640, h / 480)`,
  because both resample properly and an integer scale would only throw away
  screen area.

So the destination rectangle is a function of the window size *and* the selected
filter, and changing the filter at runtime has to recompute it. One function,
used by the blit, by the cursor mapping (see below), and by nothing else.

Below 640x480 nearest has no integer scale left. `handleResize` clamps the window
to a 640x480 minimum by re-calling `SDL_SetVideoMode` with the clamped size, so on
the desktop that case cannot arise. The browser is the exception — there the canvas
size is the browser window's, and pushing back against it looped — so
`computePresentRect` snaps to an integer only while the scale is at least 1.0 and
otherwise scales fractionally; a cropped picture would be worse than a slightly
ragged one.

### Video and screenshots stay at 640x480

**Video recording always captures the game's internal 640x480**, never the window
size. It used to be that way by accident — the capture ran *before* `upscaleFrame()`
and read the un-upscaled render out of the back buffer — and with the FBO it is true
on purpose: capture reads `GL_COLOR_ATTACHMENT0` at `screenSize` and is independent
of the window entirely.

Three reasons it has to stay that way, not just for tidiness:

- minih264 requires the frame size to be a multiple of 16 (`videorecorder.cpp`
  centre-crops to enforce it). 640x480 is; an arbitrary resized window is not.
- The encoder is configured once, at `startRecording`. A window resized
  mid-recording would change the frame size under it.
- The cursor is drawn into the capture buffer by hand (`engine.cpp:855`) in
  640x480 coordinates from `getCursorPosition()`. That keeps working unchanged —
  and becomes correct under letterboxing for the first time, because it goes
  through the same inverse transform as everything else.

**Screenshots go the same way: clean 640x480.** `Engine::screenshot` used to run
*after* `upscaleFrame()` and read `displaySize` from `GL_BACK`, so it saved the
upscaled image — and with letterboxing it would have saved the black bars too. It
now runs before the blit and reads `GL_COLOR_ATTACHMENT0` at `screenSize`, like
the video path. The filter is a display setting; it does not belong in the file.

### Resizing is nearly free, and borderless fullscreen is too

Both facts come out of the vendored SDL, so they are facts about *this* build.

`SDL_dibvideo.c:614-625` has a fast path in `DIB_SetVideoMode`: if the flags and
bpp are unchanged, `SDL_OPENGL` is set and `SDL_FULLSCREEN` is not, it calls
`DIB_ResizeWindow` and returns — **the GL context survives**. windib is the
driver the game gets, because `WINDIB_bootstrap` precedes `DIRECTX_bootstrap` in
SDL's table (`SDL_video.c:82` vs `:85`). So a resizable window needs
`SDL_RESIZABLE` in the flags, an `SDL_VIDEORESIZE` handler that re-calls
`SDL_SetVideoMode` with the new size and recomputes the letterbox rectangle, and
nothing else.

**Real fullscreen is what costs.** Setting `SDL_FULLSCREEN` changes the flags, so
the fast path is skipped and `WIN_GL_ShutDown` runs (`SDL_dibvideo.c:627-630`):
the GL context is destroyed and every GL object with it. `SDL_NOFRAME` is no
better — it is also a flag, and any flag change fails the same test.
`SDL_WM_ToggleFullScreen` is not implemented on Windows in SDL 1.2 at all.

**So do not ask SDL for it.** Keep the SDL flags constant at
`SDL_OPENGL | SDL_RESIZABLE` for the entire life of the process, and change the
Win32 window style directly:

1. `SDL_GetWMInfo` gives the `HWND` (`SDL_syswm.h:147`; `WIN_GetWMInfo` is wired
   up at `SDL_dibvideo.c:206` and `SDL_syswm.c` is in the compiled subset).
2. `SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE)` and
   `SetWindowPos(hwnd, HWND_TOP, 0, 0, screenW, screenH, SWP_FRAMECHANGED)`.
3. SDL's own `WM_WINDOWPOSCHANGED` handler
   (`SDL_sysevents.c:576-611`) then posts `SDL_PrivateResize(w, h)` — it gates
   only on `SDL_RESIZABLE`, not on `SDL_resizing` — which updates the mouse range
   (`SDL_resize.c:52`) and delivers an ordinary `SDL_VIDEORESIZE`.
4. The existing resize handler picks that up, calls `SDL_SetVideoMode` with the
   new size and the same flags, hits the fast path, and the context is never
   touched.

Going back to windowed is the same three lines with the saved style and rectangle.

Five details make that safe rather than merely plausible, all checked in the
vendored source rather than assumed:

- **SDL will not fight the style.** `DIB_ResizeWindow` passes
  `GetWindowLong(SDL_Window, GWL_STYLE)` to `AdjustWindowRectEx`
  (`SDL_dibvideo.c:547`) — it reads the window's *current* style rather than
  deriving one from the flags, so with `WS_POPUP` set it computes a zero border
  and the client area is exactly the size asked for.
- **SDL will not re-impose one either.** The block that turns flags into
  `WS_POPUP` / `WS_THICKFRAME` is at `SDL_dibvideo.c:802-832`, past the early
  return, so it runs only on the slow path. After the first mode set the game
  never takes the slow path again, and the style stays whatever it was last set
  to.
- **The event loop does not run away.** Step 4's `SDL_SetVideoMode` reaches
  `DIB_ResizeWindow`, which issues its own `SetWindowPos`, which fires another
  `WM_WINDOWPOSCHANGED`, which calls `SDL_PrivateResize` again. That terminates
  because `SDL_PrivateResize` keeps a `last_resize` and returns immediately on a
  repeat (`SDL_resize.c:43-46`); it also pulls pending `SDL_VIDEORESIZE` events
  out of the queue before posting, which coalesces a drag.
- **SDL will not move the window out from under you.** `DIB_ResizeWindow` ends in
  `SetWindowPos(SDL_Window, HWND_NOTOPMOST, x, y, ...)`, and with no
  `SDL_FULLSCREEN` flag and no `SDL_VIDEO_WINDOW_POS` / `SDL_VIDEO_CENTERED` in
  the environment, `x` and `y` come from `SDL_windowX`/`SDL_windowY` — which
  `WM_WINDOWPOSCHANGED` has just updated from the window's real position
  (`SDL_sysevents.c:596-601`). At (0,0) both are zero, the `else` branch adds
  `SWP_NOMOVE`, and the window is left alone; on a secondary monitor they hold
  that monitor's origin and the window lands there. Worth verifying on a
  multi-monitor setup rather than trusting, and re-applying the position after
  if it ever disagrees. `HWND_NOTOPMOST` is the right answer either way — a
  borderless fullscreen window should not be topmost, or alt-tab stops working.
- **Telling SDL the new size is not optional.** `SDL_PrivateMouseMotion` clamps
  to `SDL_MouseMaxX`/`SDL_MouseMaxY` (`SDL_mouse.c:136-150`, `:206-221`). Change
  the Win32 window without informing SDL and the cursor stays clipped to the old,
  smaller area — which is why step 4 exists at all. Both `SDL_PrivateResize`
  (`SDL_resize.c:52`) and `SDL_SetVideoMode` (`SDL_video.c:653`) set the range, so
  the path is covered twice over.

**There is therefore only one code path — "the window changed size" — and
fullscreen is just a particular size plus a style flip.** That is the whole
answer to the reload problem: nothing is ever destroyed, so nothing has to be
rebuilt. The table of GL objects that `Manager<T>::reload()` does not cover
(`engine.cpp:362`/`:363`, `gui.cpp:471`, `level.cpp:92`, `cf_mosaic.cpp:6`,
`gs_credits.cpp:258`, the display lists at `level.cpp:411` and
`lightning.cpp:13`, plus the FBO and its stencil renderbuffer) stops being work
that has to be done and becomes a list to check only if a context is ever lost
for a reason outside the program's control — a driver reset, an RDP reconnect, a
driver update mid-session. Those kill a WGL context no matter how the window was
made; they are rare enough to ignore deliberately, and `Manager<T>::reload()`
(`manager.h:107`, already used for skin changes at `level.cpp:2433-2435`) is most
of the recovery if it ever matters.

Three things borderless gains beyond that:

- **No `ChangeDisplaySettings`.** The monitor never switches mode, so alt-tab is
  instant and does not rearrange the desktop or other windows.
- **The desktop resolution is what you get** — which is exactly what the FBO
  wants, since the game renders 640x480 and scales to whatever is there. The
  `SDL_ListModes` search at `engine.cpp:222-252` disappears entirely.
- **Nothing is lost by it.** Exclusive fullscreen's remaining advantage is
  running the monitor at a non-native mode, which this game has no use for; on
  Windows 10 and later a borderless window that covers the screen gets DWM's
  independent-flip path and presents as directly as exclusive mode did.

One caveat to design around: SDL's window flags must genuinely never change, so
the window is created with `SDL_RESIZABLE` even when the game starts
"fullscreen". Under `WS_POPUP` there is no drag border, so the flag has no
user-visible effect — it only keeps `SDL_PrivateResize` firing and the fast path
matching.

The browser needs none of this: the Fullscreen API on the canvas, and the WebGL
context survives.

### Mouse coordinates already have the hook — and a bug

`Engine::getCursorPosition` and `setCursorPosition` both run the inverse of the
letterbox transform now, taking the scale and offset from `computePresentRect` —
the same rectangle `presentFrame` blits into. Nothing more is needed here when
the window becomes resizable; the rectangle simply changes.

Both had defects that went with the hq2x branch they lived in: the y offset
assumed the image was centred vertically while `upscaleFrame` placed it flush to
the top, and `setCursorPosition` clamped with `clamp(temp.x, 0, temp.x - 1)`,
whose upper bound came from the value being clamped, so it always returned
`temp.x - 1`.

### Where the settings live

The filter is done: `useHQ2X` gave way to `Engine::UpscaleFilter`, the four
radio buttons live in `data/options.xml` next to the language flags, their
captions are `$O_UPSCALER*` in `data/languages.txt` with `§en:` and `§de:`
bodies, and `<Upscaler>` round-trips through `loadConfig`/`saveConfig`.

`<Fullscreen>`, `<WindowSize>` and `<WindowPosition>` went in the same way, so the
game comes back the way it was left. `config.xml` now holds `<Language>`,
`<Upscaler>`, `<Fullscreen>`, `<WindowSize>`, `<WindowPosition>`, `<SoundVolume>`,
`<MusicVolume>`, `<Details>` and `<Controls>`.

Persisting them needed a second thing that was easy to miss: `saveConfig` had
exactly one caller, the options dialog's OK button. Resize the window, quit, and
the size was gone — the file was never written. `Engine::exit` now calls
`rememberWindowPlacement()` and `saveConfig()` before it tears anything down, so
the last thing the player did is what comes back. `rememberWindowPlacement` reads
the placement off the HWND, except in fullscreen, where the window is at (0,0) and
desktop-sized and the rect `applyWindowStyle` saved before the style flip is the
right answer instead. A restored position that no longer lands on a screen — a
laptop unplugged from a second monitor — is dropped rather than used.

The first-run size used to be 640x480 exactly, which on a modern desktop is a
postage stamp. `getDefaultWindowSize` picks the largest integer multiple of the
640x480 frame that leaves a 120px margin in both directions for the title bar and
the taskbar: 2x on 1920x1080 and on 1440p, 3x on 1600p, 4x on 2160p, and 1x on
anything below Full HD.

120 is derived, not guessed. It is the largest margin at which 1920x1080 still
reaches 2x: 2*480 is 960, 1080-120 is 960, and it fits with nothing to spare -
one pixel more and the most common desktop there is would open at 640x480. The
horizontal margin is the same number, which at every common resolution is slack
the width never needed; it is there because a taskbar is not always at the bottom
of the screen. Integer
multiples on purpose — the letterbox is then empty and "sharp" needs no
resampling at all. The same function catches a stored size that no longer fits. Since fullscreen is just a window size plus a style flip, that is
one boolean and one `Vec2i`, with no mode list behind either. `-fullscreen` and
`-windowed` remain startup overrides and beat the file; the compiled-in default
(windowed in Debug, fullscreen in Release) applies only when neither the file nor
the command line says anything. The templates `_config_en.xml` / `_config_de.xml`
the installer copies still set only `<Language>`, which is right — the rest should
come from the defaults on a fresh install.

One thing SDL could not give and the window procedure had to: while the user
holds the border or the title bar, `DefWindowProc` runs its own modal message
loop and the game's main loop sits in `SDL_PollEvent` until the mouse comes up -
so the picture froze for the length of every drag. SDL 1.2 does not handle
`WM_ENTERSIZEMOVE` or `WM_EXITSIZEMOVE` at all, it passes both to
`DefWindowProc`, so `Engine::hookWindowProc` puts a procedure in front of SDL's
with `SetWindowLongPtr(GWLP_WNDPROC)`. That is safe here for a reason worth
writing down: the HWND is created exactly once, in `DIB_CreateWindow` from
`DIB_VideoInit`, and no later `SDL_SetVideoMode` replaces it - and it is the same
subclassing SDL itself performs when `SDL_WINDOWID` is set.

`WM_ENTERSIZEMOVE` starts a 15 ms timer, and `WM_SIZE` and `WM_TIMER` both
re-present the framebuffer at the current client size. Two signals rather than
one because each covers the other's gap: `WM_SIZE` arrives on every drag step but
never while the user holds still, and `WM_TIMER` is low priority and gets starved
by mouse messages exactly while dragging. No logic runs in there - one
`presentFrame` of the frame already in the FBO, nothing more.

Two traps, both hit while writing it:

- `SDL_SetVideoMode` must not be called during the drag. It calls `SetWindowPos`,
  which fights the user's own drag.
- The repaint therefore updates `displaySize` itself - but it has to put the old
  value back before returning. `handleResize` early-returns when the size has not
  changed, so leaving the new size in place means the `SDL_VIDEORESIZE` that
  arrives after the drag looks like a no-op, `SDL_SetVideoMode` never runs, and
  SDL's own surface stays at the pre-drag size for the rest of the session.

The same procedure answers `WM_GETMINMAXINFO`, chaining to `DefWindowProc` first
because it fills four fields besides the one being overridden, with 640x480 of
client area grown by `AdjustWindowRectEx`. `handleResize` already refused to go
below the internal resolution; now the frame stops there during the drag instead
of springing back afterwards.

Verified as far as this machine allows: a standalone Win32 program built with
mingw and run under Wine confirms the subclass captures the previous procedure,
that `WM_ENTERSIZEMOVE` reaches it, that a timer started there delivers `WM_TIMER`
*inside* the modal loop and the repaint runs, that `WM_EXITSIZEMOVE` and the
unhook are clean, and that the `WM_GETMINMAXINFO` arithmetic clamps a window
asked to be 200x150 to exactly 648x514. What it cannot show is a long drag - Wine
will not hold the modal loop open for synthetic input - nor anything about the GL
present, which needs the real game on Windows.

The browser needs none of SDL for this: the canvas fills the page and follows the
browser window (`WebBuild/pre.js`), the Fullscreen API does the rest, the WebGL
context survives both, and the letterbox arithmetic is the same code. Two things
differ there and are handled explicitly: the game never *starts* fullscreen,
because the Fullscreen API requires a user gesture that does not exist at startup;
and Alt+Return is registered as a DOM `keydown` callback rather than read from the
SDL queue, because the main loop runs inside `requestAnimationFrame` and what it
delivers no longer counts as a gesture. That half is testable without Windows, and
was: resize, letterbox, cursor mapping, the integer snap, the fullscreen toggle and
the config round-trip all verified in Chromium.


11. A CRT effect  — **DONE**
-----------------------------
The idea that replaced xBR, and a much better fit for what a nostalgic filter is
actually for here. xBR tried to *reconstruct* detail the art never had; a CRT
effect adds a period-correct presentation on top of the art as drawn, which is
honest about what it is doing and does not care whether the source is flat-shaded
or airbrushed. It is also stable where xBR was not: no thresholds, no edge
detection, only smooth functions of the source colour and the output position, so
a one-in-255 nudge moves the output by about one.

Shipped as `src/crt_shader.h`, a fourth entry in Options → Scaling. What went in,
and what was learned building it:

- **Which CRT is one number.** Scan-line gaps are an artifact of 240p — a console
  drawing 240 lines into a 480-line raster. A VGA monitor showing 640x480 drew all
  480 with overlapping beam profiles and had no gaps, and that is this game's
  honest reference. So `SCANLINE_PERIOD = 1.0` is the authentic setting and, at a
  2x window, produces no visible stripes whatever, because both output rows are
  equidistant from the row centre. Physically right, useless as an effect. The
  shipped default is 2.0 — pretend 240 lines arrive — and the slider fades that
  in from nothing. The constant chooses the look; the slider chooses how much.

- **The mask belongs to the glass, not the signal.** It is indexed by
  `gl_FragCoord`, in output pixels, so it stays the same fineness at any window
  size. That matters because `getDefaultWindowSize` gives 2x on both 1080p and
  1440p, so almost every player has exactly two output pixels per source pixel and
  three source-locked subpixels are not available.

- **Brightness is derived, not dialled in.** The first version multiplied by a
  hand-picked 1.45 and came out visibly washed. `MASK_AVG` and `scanAvg` are now
  computed from the constants, so both are light-neutral by construction: moving
  the scan-line slider end to end changes mean frame brightness by 0.5%, and
  editing any constant needs no compensating edit elsewhere.

- **Cost.** On the same software rasterizer as the other measurements: nearest
  1.0, bilinear 1.3, sharp-fit 1.35, CRT 7.8. The first draft was 11.0, the same
  as xBR, because `toLinear` ran per tap across ten taps — thirty `pow()` per
  pixel. Averaging the halation ring in gamma space and linearising the sum once,
  and dropping the sharp-fit ramp for taps that get blurred anyway, took it to
  7.8. Halation is about half of what remains and `BLOOM_STRENGTH = 0` compiles it
  out entirely (4.2). The five `exp()` behind `scanAvg` are free — a hard-coded
  literal measured identically.

- **The distortion goes through the mouse.** This was the part worth being careful
  about. A fragment shader maps output pixel to source pixel, which is exactly the
  direction `getCursorPosition` needs, so it calls the identical formula.
  `setCursorPosition` needs the inverse; the pair is coupled (`x` on `y²`, `y` on
  `x²`) and has no closed form, so it iterates `x <- u/(1+a·y²)`, `y <- v/(1+b·x²)`.
  Measured over the whole image: eight rounds are within 2.3e-4 pixels at a
  curvature far past anything reachable, and the forward/inverse pair agrees to
  1.2e-10 pixels. The two curvature constants are `#define`d once and stringified
  into the GLSL as well as read as C++ doubles, so they cannot drift.

  Both cursor functions had a second, older bug: they mapped left edges with
  truncation instead of pixel centres with `floor`, which lost a pixel at
  fractional window sizes. Fixed, and the round trip `get(set(g)) == g` is now
  exact at every scale and curvature — except at exactly 1x, where 640 window
  pixels cannot hold a non-identity warp over 640 game pixels and 0.5% of
  positions land on a neighbouring 16px tile. That is the minimum window size,
  where the effect has nowhere to show anyway.

- **Flicker is the one part that reads a clock**, and that is allowed for the
  same reason the rest is stable: it depends on `Time`, never on the previous
  frame. Two zero-mean terms, so it costs no brightness — a fast shimmer at
  roughly 12, 19 and 29 Hz, which is what "flimmern" actually looks like, plus a
  much weaker mains-hum bar rolling slowly down the picture. Every frequency is a
  whole number of cycles per `FLICKER_CYCLE` (8 s) and the clock is fed in modulo
  that, so the wrap is seamless and float never gets coarse. It reads
  `SDL_GetTicks` rather than `Engine::getTime`, which counts logic ticks and
  stops when the game pauses; a screen flickers anyway. Measured at maximum:
  2.55% peak-to-peak between frames.

- **Halation has to average in linear light, and per tap.** The first version
  averaged the eight taps in gamma space, linearised the result, and then applied
  the threshold — and produced essentially no visible halo. The ring around a
  bright spot is a mixture of bright and dark, and `pow()` on that mixture pushes
  it far below the threshold; measured, the slider moved 0.6% of pixels from end
  to end. Linearising each tap and thresholding the linear average gives a real
  glow: +23 grey levels at the centre falling smoothly to +4 at 70 output pixels.
  The linearisation is `x*x` rather than `pow(x, 2.4)` — for a soft halo the
  difference is invisible and it costs a multiply instead of a `pow`, which
  eight times per pixel would have been the most expensive thing in the shader.
  The taps also sit on **two** rings now, four axial and four diagonal: eight on a
  single radius produces a hard-edged ring a few pixels wide, not a glow.

- **The scan lines crawl.** Sitting perfectly still is the one thing a real
  raster never did. `ScanPhase` shifts the whole pattern slowly downward and
  `CRAWL_JITTER` makes it tremble. It is the only part of the flicker computed on
  the CPU, and for a specific reason: it is a ramp rather than an oscillation and
  its slope depends on the slider, so feeding it the already-wrapped clock would
  jump the lines by `fract(flicker · speed)` of a period at every wrap. Taken
  from the unwrapped clock modulo one, it is continuous at any slider position.
  Verified in an isolated shader probe: `ScanPhase` of 0, 0.25 and 0.5 moves the
  scan-line ripple by exactly a quarter period each step.

All four knobs — scan lines, curvature, glow, flicker — are sliders in
Options → Scaling → *CRT settings …*, stored as
`<Crt scanline= curvature= bloom= flicker=>`. `BLOOM_STRENGTH` stays a constant on
top of the glow slider, so setting it to 0 still compiles the whole halation block
out for anyone who wants the cheap version.

Left for later: anisotropic curvature (real tubes are not spherical), a shadow-mask
dot triad as an alternative to the aperture grille, and moving halation to a second
pass if the single-pass ring ever looks too tight.

12. Tell the player about the hardcoded keys  — **DONE**, it already did
------------------------------------------------------------------------
**The premise was wrong.** `$H_HELP_PAGE1` has listed the toggle all along, in
both languages, at the foot of the *Controls (standard)* table:
"Window/full screen — Alt + Return" and "Fenster/Vollbild — Alt + Return"
(`data/languages.txt:1357` and `:1375`). The help was read for the *actions*
it lists and the hardcoded keys below them were missed. Nothing to write.

The rest of this entry is kept for what it established about the four keys.

The options dialog is not the gap it looks like. It lists *registered actions* —
`$A_LEFT`, `$A_PLANT_BOMB` and the rest from `main.cpp` — which are remappable
by design and already shown. These four bypass the action system entirely and
are read straight from SDL, which is exactly why they never appear:

| key | what it does | where |
| --- | --- | --- |
| Alt+Return | window / full screen | `engine.cpp:769` |
| Shift+C | credits | `gs_menu.cpp:121` |
| Shift+D | turns the donation prompt off for good | `gs_menu.cpp:127` |
| F (held, in game) | frame time overlay | `gs_game.cpp:256` |

Of those, only Alt+Return belongs in the player-facing help. Shift+D is a
one-way switch a player could hit by accident reaching for Shift+C, and the
other two are for us.

**Which key is it, exactly.** *Return* and *Enter* are two different keys here,
not two names for one: **Return** is the big key above the right Shift
(`SDLK_RETURN`), **Enter** is the one on the numeric keypad (`SDLK_KP_ENTER`).
The game already keeps them apart and says so where both work — the hotel binds
`SDLK_RETURN` *and* `SDLK_KP_ENTER` (`main.cpp:442`) and its text reads
"Press the Return/Enter key now" (`$G_HOTEL_WELCOME`).

The fullscreen toggle tests `SDLK_RETURN` only (`engine.cpp:850`, and the
`SDL_KEYUP` arm at `:866` that balances `swallowedReturn`), so Alt + numpad
Enter does nothing. That is consistent rather than broken: the help and
`readme.txt` both name Alt+Return, which is the big key, and only the hotel —
which binds both — writes "Return/Enter". Wiring the keypad up as well is one
`||` in each of those two places if it ever comes up.

**Where the help text lives**, for whenever a page does need editing: six of
them, `$H_HELP_PAGE1` … `$H_HELP_PAGE6` in `data/languages.txt`; `help.cpp:78`
builds the ID from the page number and `help.cpp:61` caps it at 6. Each page has
a `§en:` and a `§de:` body with light markup (`<h>…</h>` for a heading, `¶` for a
newline), so anything added has to be written twice, and a seventh page means
changing that literal `6`.

13. The particle system's container
----------------------------------
`ParticleSystem` keeps its particles in a `std::list`, one 80-byte `Particle`
per heap node, walked by pointer every tick and every frame. The manual
`_mm_prefetch` pair in `update()` and `render()` is there because no hardware
prefetcher follows a pointer chain; it earns 3–9%.

**Nothing here is a bottleneck.** Measured at a heavy 5000 particles — the
biggest single burst in the tree is 500, from a bomb — `update()` costs about
0.05 ms of a 20 ms tick (0.23%) and filling the render vertex buffer about
0.17 ms of a 16.7 ms frame (1.0%). This entry exists so the next person does not
re-derive the four options and, in particular, does not re-discover the two
traps the hard way.

The struct is already single precision throughout (`Vec4f`, `Vec2f`, `float`);
the `Vec4d`s at the emitter call sites run once per particle *created*, not per
tick. Doubles would cost 16–33% and turn one `addps` into two `addpd`.

Measured, ns per particle-tick, best of two runs:

| container | order | native 5k | native 20k | wasm 5k |
| --- | --- | --- | --- | --- |
| `std::list` + malloc (today) | preserved | 7.9 | 23.0 | 10.3 |
| `std::list` + pool allocator (LIFO) | preserved | 6.2 | 22.0 | 8.9 |
| `std::vector` + swap-and-pop | **unstable** | 4.2 | 4.6 | 6.7 |
| `std::vector` + stable compaction | preserved | 10.3 | 11.4 | 14.7 |
| `std::vector` + tombstones, compact every 16 | preserved | 3.8 | 5.3 | — |
| ring of buckets keyed by death tick | preserved | 4.2 | 4.9 | 5.9 |

**Trap one: swap-and-pop breaks the picture, and it was tried once.** Two of the
three systems are alpha blended and therefore order dependent —
`p_particleSystem` and `p_rainParticleSystem` both draw under
`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` (`level.cpp:666` and `:702`). Only
`p_fireParticleSystem` is additive (`GL_SRC_ALPHA, GL_ONE`, `level.cpp:700`) and
genuinely order-free. What swap-and-pop produces is not a wrong order but an
*unstable* one: a particle jumps from the end of the array into the middle the
instant a neighbour dies, so the layering of overlapping sprites changes between
frames, which reads as flicker. Counted over 800 ticks with 5000 particles, the
relative order of the survivors was unchanged in 800 frames for the list and in
**19** for swap-and-pop.

**Trap two: a pool allocator buys much less than it looks like it should.** It
fixes where the nodes are, not the order they are visited in. Freshly filled,
both malloc and a pool walk perfectly forward — 100% of steps forward and within
128 bytes. After 300 ticks of churn that collapses to 3.0% for malloc and 0.0%
for the pool; the mean jump between list-consecutive nodes is 119 KB against the
pool's 25 KB. Smaller jumps, still jumps. Worth about 1.3x at 5000 and 1.02x at
20000, less in the browser, because Emscripten's dlmalloc over one flat linear
memory already packs same-size nodes densely. It is still the cheapest thing on
this list: about thirty lines, it never touches draw order, and it gives a fixed
memory ceiling.

A **FIFO** free list looks like it beats that — particles die roughly in the
order they were born, so slot reuse tracks list order and the walk goes back to
98.2% forward, worth 1.64x. It is an illusion. That only holds while the pool is
far larger than the live set, so the allocator is marching into memory it has
never touched. Size the pool to the maximum, which is the whole point of having
one, and wrap-around shuffles the free list: 0.0% forward, 1.16x. Let a big pool
lap three times and it is 0.98x. In a linked list, reusing hot slots and walking
in address order are mutually exclusive; a vector gets both for free.

**The one that works: a ring of buckets keyed by the absolute death tick.**
`bucket = deathTick & (WHEEL-1)`, so nothing ever moves. Each tick sweeps buckets
`now` through `now + WHEEL-1` and then `clear()`s bucket `now` — death is O(1)
for the whole cohort, and the inner loop has no branch at all: no `--lifetime`,
no death test, no erase. That is why it beats even swap-and-pop in the browser
(1.74x against 1.53x, and 2.03x against 1.82x with `-msimd128`).

Its order is stable, and the reason is worth writing down because getting it
backwards is easy and silent: sweep `now` **first**, not last. A live particle's
position in the sweep is exactly its remaining lifetime, and every particle's
remaining lifetime falls by one each tick, so no two can ever swap. Sweeping
`now+1 … now+WHEEL` instead puts the about-to-die bucket at the end, and every
particle jumps from the front of the sweep to the back on its final frame.
Measured both ways: 800/800 frames stable with the right sweep, 19/800 with the
wrong one — indistinguishable from swap-and-pop.

What it would cost:

- **Memory.** 13.1x the live count in capacity: 5.0 MB at 5000 particles against
  a vector's 0.8 MB. Each of the 512 buckets keeps its own historical peak and
  those peaks fall at different times. Bounded, and small in absolute terms, but
  it is the price.
- **`WHEEL` must exceed the longest lifetime.** The longest in the tree is
  `random(150, 300)`, so 512 has room — but anything longer would silently alias
  into a bucket that dies early. Wants an assert in `addParticle`.
- **The `p.size <= 0` early death** has to become a death tick computed once at
  insertion, `min(lifetime, ceil(size / -deltaSize))`. Nine emitters have a
  negative `deltaSize`. Float accumulation could put it one tick off today's
  behaviour, on a particle whose size is already about zero.
- **The order becomes remaining-lifetime order rather than creation order.**
  Stable, but different. Where a burst shares one lifetime it is unchanged —
  `enemy.cpp` gives all 150 particles `lifetime = 100`, so they land in one
  bucket in insertion order exactly as today; `object.cpp`'s `random(20, 50)`
  burst spreads over thirty buckets and does reorder.

`getNewParticle()` has no callers anywhere in the tree. It returns
`&particles.back()`, which was the only thing that required stable addresses, so
nothing blocks a vector. It can go either way: delete it, or have it return an
index.

Two smaller notes from the same measurements. In `render()`, `sinf`/`cosf` per
particle is 54–61% of the fill loop (20.2 → 9.2 ns native, 34.8 → 14.7 ns wasm
when hoisted); since `rotation` advances by a constant `deltaRotation` every
tick, the pair could be advanced by a fixed rotation instead of recomputed per
frame. And `-msimd128` is not passed by `WebBuild/build.sh`, so the shipping
wasm contains no vector instructions at all — see item 8.

14. An import overwrites, unless the name is one the game ships  — **DONE**
-----------------------------------------------------------------------------
Two halves of one decision. **An import that names a file the player already
has replaces it** — no more swerving to a numbered name. **An import that names
one of the seven files the game ships is refused.**

The shipped set is exactly `levels/example01.xml`, `levels/example02.xml`,
`levels/campaigns/blocks.zip` and the four skins
`levels/skins/{blocks_01,blocks_02,blocks_03,space}.zip` (plus `readme.txt`,
which no import can be).

**The skins already behave this way**, and the reasoning that got them there is
the reasoning for the rest: a skin's filename *is* its identity — a level says
`skin0="space"` and `Level::getSkinFilename` looks for `levels/skins/space.zip`
— so swerving to `space_2.zip` would leave every level naming `space` just as
broken as before, only without a visible cause. `isShippedSkin` in
`src/transfer.cpp` refuses the four shipped stems; everything else overwrites.

**Everything else swerves today and must stop.** `uniqueName`
(`src/transfer.cpp`, used for levels and music) and the numbering loop in
`Campaign::installArchive` (`src/campaign.cpp`) both walk `stem_2`, `stem_3`, …
until the name is free. Both go. What is left is `sanitizeFilenameStem` plus the
extension, and the copy.

That makes all four kinds one rule instead of two, and it retires the special
case: `installImported`'s skin branch and its comment exist only to say why
skins differ, and they will not differ any more. `Campaign::installArchive`
loses the numbering and its header comment in `campaign.h` — "der Zielname wird
hier gebildet und ist garantiert noch frei" — stops being true.

For the refusal: one predicate answering "is this a name the game ships?" for
all four kinds, called before the copy, plus a message. `$TR_ERROR_SKIN_RESERVED`
already exists and becomes the general one (or gains siblings, if naming the
kind reads better). The campaign name is load-bearing in one more place than the
levels are: `Campaign::resolveMusicPath` resolves a `blocks:` prefix against
`levels/campaigns/blocks.zip` by that literal name, so a campaign that took it
would redirect every borrowed music track in the tree.

Two things that follow from overwriting. Re-importing an updated campaign now
keeps the player's progress, because `ProgressDB` is keyed by the campaign's
filename and the filename no longer changes — that is the good half. The other:
an import can now replace a file the player made themselves, silently. If that
wants a word, the place for it is the toast `GS_Menu` already shows on a
successful import — "replaced" rather than "imported" costs one language ID and
no new dialog.

**Built as described**, and the toast was worth having: `install` reports through
`bool* p_replaced` and `pollImport` says `Replaced: "name"` instead of the
kind-specific line, which on a replacement had nothing to add anyway — the
player already knew the name, having chosen it. `$TR_ERROR_SKIN_RESERVED` became
`$TR_ERROR_RESERVED`, since the refusal is no longer about skins.

`uniqueName` and `Campaign::installArchive` are both gone. The second is the
better sign that this was the right shape: with the numbering removed it was
letter for letter the generic path, so the campaign stopped being a special case
rather than getting a simpler one.

One ordering detail that needed care and a test of its own. The target name and
the `isBuiltIn` check now come first, so `isImportableArchive` runs *after* the
name is known — but still before the copy. A damaged campaign carrying the name
of a good one therefore leaves the good one alone: verified, 8,280,697 bytes
before and after, with `ERROR: That file is damaged.` on screen.

The predicate wants to live where item 15 can reach it too — see there.


15. Let the Export dialog delete what it lists  — **DONE**, in the Manager
---------------------------------------------------------------------------
**Done as part of item 17**, where the button lives. What is below was the plan;
it came out as written, with the confirmation and with `isBuiltIn` gating the
button. Both open questions there answered themselves: a deleted campaign does
leave its `ProgressDB` rows behind (harmless, and the right thing if it is ever
imported again), and a deleted skin does break levels naming it — visibly,
through `Level::loadSkin`'s toast.

Export lists what is installed of a kind and writes a plain copy of the one you
pick (`GS_Menu`'s export pane over `Transfer::list`/`Transfer::exportTo`). There
is no way to remove anything. Custom levels, imported campaigns, imported skins
and imported music accumulate in `My Documents\Blocks 5\levels\` forever, and
the only way to clear one out is ShowUserDir and Explorer — which the browser
build does not have at all, so there it is genuinely impossible.

The dialog already has the list, the selection and the *Refresh* that re-reads
it, so the work is a *Delete* button beside *Export*, a confirmation (this is
the one irreversible thing either transfer button would do), `FileSystem`'s
delete, and a re-read.

**The shipped seven must not be deletable**, which is the same question item 14
asks: extract the "is this built-in?" test into one helper — something like
`Transfer::isBuiltIn(Kind, const std::string& name)` in `src/transfer.cpp` —
and have both the import refusal and the Delete button consult it. `isShippedSkin`
becomes its skin branch. Do item 14 first and this one inherits the helper;
either order works, but writing the predicate twice would be the mistake.

Two details worth settling before writing it: a deleted campaign leaves its
`ProgressDB` entries behind (harmless, keyed by filename, and worth keeping if
the same campaign is imported again), and a deleted skin can break levels that
name it — which is exactly the case `Level::loadSkin`'s toast now reports, so
it fails visibly rather than silently.


16. Reset one control instead of all of them  — **DONE**
----------------------------------------------------------
The controls pane in `data/options.xml` has an *Actions* list, a *Primary key*
and a *Secondary key* button, and one `ResetControls` button that calls
`Engine::resetActions()` — which walks every action and restores both bindings.
Change one key badly and the only way back is to throw away the whole scheme.

`Action` already carries `defaultPrimary` and `defaultSecondary`
(`src/engine.h:24`), and `resetActions` (`src/engine.cpp:3233`) is a loop over
exactly those two fields, so resetting the selected action alone is a
`resetAction(const std::string& name)` of three lines. The work is the UI.

The proposed shape: replace the single wide button with a small-font label
`Reset:` and two small-font buttons, *selected* and *all*, on the same row —
the pane is tight (the list ends at y=360, OK/Cancel start at y=400), so 170px
of width has to hold all three. *selected* is only meaningful with a selection;
it should follow the same enable/disable idiom the rest of the dialog uses
rather than silently doing nothing. Both need `handleClick(p_actions)` after
them to refresh the two key captions, as `ResetControls` already does.

`$O_RESET_CONTROLS` becomes two new IDs plus a label in `data/languages.txt`;
keep them short, because the English and German bodies both have to fit the
same button.

**Built**, and then rebuilt: the label-plus-two-short-words shape came out
lopsided — "Reset: selected all" reads fine, "Zuruecksetzen: ausgewaehlte alle"
does not, and sizing the buttons for one language wasted space in the other.
What stands is two stacked buttons under the list, as wide as it, each carrying
a whole caption: *Reset selected* / *Ausgewaehlte zuruecksetzen* over *Reset
all* / *Alle zuruecksetzen*. The window grew 20 px for the second row; the 15 px
above OK and the 10 below it are unchanged. The list also lost 5 px so its
bottom edge meets the *Secondary* button's.
`Engine::resetAction(name)` is the three lines predicted, and `resetActions()`
now loops over it rather than repeating it.

**Found while testing: key rebinding did not work in the browser at all** —
`getPressedVK` was a synchronous `while` loop around `SDL_PumpEvents` and
`SDL_Delay(10)`, and under Emscripten nothing can reach the event queue while C
holds the thread, so it always ran its three seconds out and wrote "not
assigned". **Fixed since**, as the state machine the entry guessed at rather
than ASYNCIFY: `Engine::beginKeyGrab()` / `pollKeyGrab()`, advanced one tick at
a time by the ordinary main loop. Same code on both platforms, and the dialog
stays live instead of freezing. See the key-grab paragraph in CLAUDE.md.


17. One Manager button instead of Import and Export  — **DONE**
------------------------------------------------------------------
Import and Export are two small buttons side by side in the main menu
(`data/menu.xml`, `Import` at x=268 and `Export` at x=318, both 50x50 with
`inset="9"`). They are two halves of one subject — what is installed on this
machine — and they will be one large 80x80 button, called *Manager* or whatever
reads best, opening one dialog that imports, exports and deletes.

Why one and not two: Import today has no dialog at all. It opens the file
picker, works out what the file is, copies it and says so in a toast; the player
never sees the list their file joined. Export has that list and nothing else.
Put them together and each fixes what the other lacks — an import lands in a
list you are looking at, and a delete (item 15) has somewhere obvious to live.

**The existing export pane is most of it.** `Menu.ExportPane` is a 400x320
window holding four `ButtonLook` radios for the kind, a list, *Refresh*, *Export*
and *Cancel*, all wired in `gs_menu.cpp` (`connectClicked` around line 245,
`handleClick` around 397, `refreshExportList`/`currentExportKind` near 600). What
changes is the verbs, not the machinery:

- The bottom row is `Do` at x=10 and `Cancel` at x=260, each 130 wide — which
  leaves exactly 130 px free between them. The room for a third button is
  already there; a fourth needs the row re-cut or *Import* moved up beside
  *Refresh*, which may read better anyway since importing does not act on the
  selection the way exporting and deleting do.
- `$TR_EXPORT_TITLE` and `$TR_EXPORT_WHAT` ("what do you want to export?") become
  kind-neutral, and `$MM_IMPORT`/`$MM_EXPORT` collapse into one tooltip ID.
- *Delete* is gated on item 14's `isBuiltIn()` — the seven shipped files are
  exactly the ones an import may not overwrite and the ones a delete may not
  remove, which is the second reason that predicate should exist once.

**The import is asynchronous and that finally pays off.** `Transfer::beginImport`
starts the picker and `GS_Menu::pollImport` is asked every tick from `onUpdate`
(gs_menu.cpp:147), because the browser's dialog cannot be modal. With the Manager
open, the completion has somewhere to go: switch the kind radio to whatever
`Transfer::classify` decided the file was, re-read the list, and select the new
entry. That is a better answer than today's toast, and it costs a few lines
because `pollImport` already knows the kind and the assigned name.

**Art:** `buttons.png` needs one new 80x80 pair (unclicked in column 0, clicked
in column 80, as the other big buttons are); the two 50x50 tiles at u=160/210,
v=250 and v=300 come free. Position is by eye, not arithmetic — the pair spans
x=268..368, so an 80x80 centred on the same spot starts at x=278, but the row
sits on an arc (`CampaignEditor` at y=77, `Options` at y=52) and the y wants
choosing rather than computing.

**Built as described.** The tile is at v=560, straight under *Go!*; the button
is `x="278" y="67"`, which puts its disc exactly where the pair's two discs sat
around 318. The bottom row went into the radios' four columns rather than the
three 130px slots — 92px holds every one of the eight captions, the longest
being "Import ..." at 66. `Transfer::isBuiltIn` and `Transfer::remove` are new;
`isShippedSkin` folded into the first of them. Item 14's other half, the switch
from swerving to overwriting, is still open and untouched here: mixing a
refusal into the current swerve would have been the inconsistent half-step.

One thing found on the way: an Escape with the export pane open quit the game.
Fixed — the confirmation takes the key first, then the Manager, and only with
both closed does it reach the quit.


18. A switch should flash when it is thrown  — **DONE**
---------------------------------------------
Eight objects react to being touched (`onTouchedByPlayer`, or `onCollision` with
an `OF_ACTIVATOR`): `lightswitch`, `electricityswitch`, `barrageswitch`,
`cannonswitch`, `magnet`, `e_pulseswitch`, `e_valueswitch` and the base in
`object.cpp`. Throwing one should light it up for a moment. None of them does
anything of the kind today.

Split them and the gap is sharper than "no feedback anywhere":

- **Four have visible state and change their own tile.** `lightswitch` draws
  `isNightVision() ? 192 : 224`, `electricityswitch` `isElectricityOn() ? 160 :
  128`, and the two electronics switches pick their sprite from `value`. You can
  see *that* something happened, if you are looking at the right 32x32 square.
- **Three have no state at all and never change.** `cannonswitch` fires or
  rotates the cannons, `barrageswitch` toggles the barrages, `magnet` turns the
  arrows — and every one of those effects happens *somewhere else in the level*,
  possibly off the part of the screen the player is watching. The switch itself
  is a still picture before and after. Whatever sound there is belongs to the
  thing being operated, not to the switch.

So the three stateless ones are exactly the ones with nothing to see, which is
also where the feedback is worth the most. A flash on all seven is still right:
it makes the act uniform, and a 32-pixel tile swap is easy to miss even when it
does happen.

**The sprite work from 1.2.0 makes this nearly free.** `Object::onBeforeRender()`
already runs once per frame for every object in the level (`level.cpp:592`) and
its body is `rebuildSprites()` — and *nothing overrides it*. So the flash can sit
entirely in the base class: a tick counter on `Object`, a `flash()` that sets it,
a decrement in `frameBegin()` (which `Level::update()` calls on everything, and
which is per tick, not per frame — the counter must not be tied to the frame
rate), and, in `onBeforeRender` after the rebuild, brightening each `Sprite`'s
colour while the counter runs. No object's `onRender` changes, and any object
that ever wants a flash gets one by calling `flash()`.

The alternative — folding it into the colour each object hands
`Engine::inst().renderSprites(sprites, color)` — touches every call site and buys
nothing, so prefer the first unless brightening the sprite colours turns out to
interact badly with the shadow passes (layer 1 is drawn three times, twice as
shadow; a flash must not brighten the shadow).

Shape of it: additive toward white, strongest on the tick it is thrown, gone in
about 0.15 s — seven or eight ticks at the 20 ms rate. `Level`'s
`flash`/`actualFlash` pair for lightning is the same idea one level up and is the
model for the decay curve. Under night vision the level tints everything green
(`level.cpp:981`), so the flash should be built from the object's own colour
rather than forced to pure white, or it will punch a white hole in the tint.

**Built, but not the way described above — that way cannot work.** The plan was
to brighten each `Sprite`'s colour in `onBeforeRender()`. But `Sprite::color`
defaults to `Vec4d(1, 1, 1, 1)` and `Sprites::add(pos)` resets to that default,
and **five of the seven switches use it unchanged** — only `barrageswitch` and
`cannonswitch` pass a tint, through `getStdColor`. `Engine::renderSprite` ends in
`glColor4dv`, fixed-function, clamped to [0, 1]. There is no brighter than white
in the colour value, so lifting RGB would have done nothing on five of seven and
something on two: worse than no feature at all.

What works is additive. `Object::render()` draws the sprites a second time with
`GL_SRC_ALPHA, GL_ONE` while the counter runs, which accumulates in the
destination and has no ceiling. The strength still passes through
`renderSprites(sprites, colour)`, which multiplies by each sprite's own colour,
so a tinted switch flashes in its own colour exactly as this entry wanted.

The two caveats above both turned out to be unfounded:

- **The shadow cannot be brightened.** `shadowColor` is `Vec4d(0, 0, 0, 0.7 /
  numSamples)` — RGB is zero — so the shadow pass is black whatever the sprite
  colour is. The additive pass is gated on `!shadowPass` anyway, since additive
  black is not black.
- **Night vision is a fullscreen overlay**, not a colour multiplied into the
  objects — the same quad as the lightning, drawn after them. A bright sprite
  shows through it rather than punching a hole in it.

Where things ended up: `flashAmount` on `Object`, `flash()` to set it,
`FLASH_STRENGTH` and `FLASH_DECAY` as tunable constants at the top of
`object.cpp`, decay in `frameBegin()` (per tick, not per frame), the additive
pass in `Object::render()` gated on `layer == flashLayer && !shadowPass`. The
seven call `flash()` at the top of their `onTouchedByPlayer`. It is deliberately
*not* in `Object::onTouchedByPlayer`: `player.cpp:413` calls that for every
object the player bumps into, so a default there would light up every block.

No sound was added. The switches that make one already do, and what you hear
belongs to the thing being operated.

Checked by forcing the flash on in the title demo, which contains a `Magnet` and
a `CannonSwitch`: +112 per channel at the peak, shaped by the sprite rather than
a square, surrounding tiles untouched, and the same scene byte-for-byte
unchanged in brightness when nothing is touched.

**Then the same for everything else that is thrown.** Three additions, each of
which turned up something the first round had not needed to know:

- **The ground panels.** `LightPanel`, `ElectricityPanel`, `Barrage2Panel` and
  `CannonPanel` all reach `Panel::onUpdate`, so one `flash()` there covers the
  four and any fifth somebody writes. `E_PulsePanel` derives from `Electronics`
  and carries its own copy of that loop, so it needs its own call.
- **`layer == 1` was wrong.** The eight objects that lie on the ground —
  the five panels among them — draw their sprites on **layer 0**, which is
  rendered before the layer-1 shadows so that something can stand on top of
  them. A flash gated on layer 1 therefore drew nothing at all for a panel. The
  layer is a property of the object now: `flashLayer`, 1 in `Object`'s
  constructor and 0 in `Panel`'s and `E_PulsePanel`'s.
- **The activator block flashes too.** It is the block, not the switch, that did
  something when it lands on one, so the seven switches call `p_obj->flash()`
  beside their own. Its own `anim` wobble is gone with that: it darkened the
  block for 0.4 s on *any* collision, the block landing on plain ground
  included, so beside a flash that fires only on an activation it read as a
  second, competing effect saying something else. It was purely cosmetic -
  written in the constructor and in `onCollision`, decremented in `onUpdate`,
  and read in one place, `updateSprites()` - and with it `ActivatorBlock` needs
  neither override any more.

The day/night switch is the one place where the flash is invisible, and rightly
so. `LightPanel::onTriggered` and `LightSwitch::onTouchedByPlayer` immediately
start `crossfade(new CF_ColorBlend(..., 0.1), 1.4)`, whose first tenth draws the
captured *old* frame under a quad that is already 89% opaque on the first tick.
The flash is over inside that. Nothing to fix: the switch is doing something far
more visible than a flash.

Checked in the browser rather than by reading, since reading is what got the
layer wrong. `WebBuild/test/burst.js` over the title demo, which walks Bob
across an `ElectricityPanel` and a `Barrage2Panel`, with `FLASH_DECAY` raised to
0.995 for the run so a 0.15 s event cannot fall between two screenshots: both
panel tiles jump and decay on the expected curve, the two `CannonSwitch` tiles
do the same as the layer-1 control, and the `Magnet` Bob never reaches stays
flat. `WebBuild/test/README.md` records the coordinate mapping and the
slowed-decay trick.


19. Playable on a phone
-----------------------
The browser build runs on a phone smoothly — and cannot be played, because
everything except the menus needs a keyboard. What is wanted is a small pad
drawn over the game: the four arrows, and one button each for planting a bomb,
putting one down, switching character, restarting the level and restarting from
the hotel. Text fields should raise the device's own keyboard instead.

**Pointing already works; only keys are missing.** Emscripten's SDL turns
`touchstart`/`touchmove`/`touchend` into `mousedown`/`mousemove`/`mouseup`
(`libsdl.js`, `SDL.receiveEvent`), so a tap already arrives as a click and the
whole GUI is operable today. What no phone can produce is a key, and every one
of the gameplay actions is a key: `Engine::updateVKs` reads `SDL_GetKeyState`,
and the actions in `main.cpp` are bound to virtual keys on top of that.

Four pieces, roughly in the order they are worth doing:

1. **The pad itself.** A DOM overlay above the canvas, like
   `WebBuild/web_bluescreen.cpp` builds one, with `touchstart`/`touchend` on each
   button. Drawn in the page rather than by the game, so it costs no GL work, it
   can sit in the black letterbox bars instead of over the picture, and it never
   goes through `-sLEGACY_GL_EMULATION`.

2. **Getting a press into the engine.** `Engine::setKeyData(SDLKey, int)` is
   public and already used exactly this way: `GS_Menu` drives the title demo by
   writing recorded key states straight into it (`gs_menu.cpp:207`). A pad button
   is the same thing with a finger instead of a recording, so no new mechanism is
   needed. Whether a button presses a *key* (`SDLK_LSHIFT`) or an *action*
   (`$A_PLANT_BOMB`) wants deciding: the key is faithful to how the game reads
   input and survives no rebinding; the action follows a rebind but bypasses the
   layer everything else goes through. The key, plus reading the action's current
   primary binding, is probably both.

3. **Text fields.** `GUI_EditBox` reads `event.keysym.unicode` out of SDL key
   events, and a phone only shows its keyboard for a focused DOM element — the
   canvas is not one. So a real `<input>`, positioned over the field and focused
   when the field is, with what it receives fed back as key events. This is the
   piece with the most edge cases (autocorrect, IME, the keyboard covering the
   field) and the least payoff: it is only needed for naming a level or a
   campaign.

4. **Hitting things.** The buttons are the problem the pad does not solve. A
   finger has no hover, and `Menu.Options` is an 80x80 disc while the Manager's
   bottom row is four 92x20 buttons — comfortable with a mouse, small under a
   fingertip. Options, in rising order of work: leave it (a phone screen scales
   the 640x480 picture up, so the discs are already large), grow the hit areas
   without moving the art (`GUI_Element::containsPoint` is virtual and already
   does exactly this for checkbox captions), or lay the dialogs out differently
   on a touch device.

**Not started.** This entry is the estimate, not a design.


How these connect
-----------------
    2 (scaling, done) ────┬─> 8 (shader upscaler, no readback)  — the readback is gone
                          ├─> 5 (Linux: the __asm block no longer blocks GCC/Clang)
                          ├─> 3 (done: libs/bin is one import library)
                          └─> 10 (the FBO is the shared prerequisite, and it is in)

   10 (window, done) ──────> did not need SDL2 after all: a borderless window
                             styled behind SDL's back keeps the GL context alive

    3 (all from source) ────> 5 (Linux needs an ffmpeg answer anyway)

    5 (Linux) <────────────── WebBuild/platform_stubs.cpp already does most of it

    7 (English comments) ───> pairs with the UTF-8 conversion; do them together

   14 (overwrite, but not a shipped name) ──> 15 (delete): one isBuiltIn() serves both
                                                 — both done, the predicate is shared

   15 (delete) ──> 17 (one Manager dialog): 15 is what the button does,
                   17 is the button — 17 is where the delete lands

The one change under both 2 and 10 was the same 80 lines: render into a
framebuffer object instead of the back buffer. Everything else in either item was
an increment on it.

*Done since this list was written:* stb_image in place of SDL_image, the standard
unordered containers in place of `stdext::hash_map`, SDL 1.2.15 compiled from
source, `/MT`, minih264 + shine + minimp4 in place of ffmpeg, a framebuffer object
with a shader upscaler in place of hq2x, and a window that resizes and goes fullscreen
without ever losing its GL context — which together closed items 2, 3, 9 and 10,
and the bug that made recorded videos unplayable.
Out of the tree: `sdl.dll`, `sdl_image.dll`, `libpng15-15.dll`, `zlib1.dll`, the
four ffmpeg DLLs, `oalinst.exe`, `vcredist_x86.exe`, `hq2x32.obj`, ten import
libraries and the `msinttypes` shim. What ships now is three executables, **one**
DLL that needs nothing but Windows, and the data.


20. A freshly built data.zip breaks the browser  — **DONE**, and it was never the zip
--------------------------------------------------------------------------------------
Pack `data.zip` from the current tree and the Emscripten build stopped finding the
skins and the fonts: 28 console errors, all of the shape `Could not parse tileset
XML file ""`, `Could not load resource ""`, `Skin "" has no usable tileset`. The
empty names said the lookup failed, not the file. The native build read the very
same archive without a murmur, and so did Python's `zipfile`: all 96 entries
decrypted, every CRC checked out, and the extracted bytes were identical to the
archive that worked.

**The archive was innocent. `WebBuild/build.sh` was lying about the link.** It
piped `em++` through `tail`, so the status it tested belonged to `tail`, and the
check after it only asked whether `blocks5.wasm` existed - which it did, from the
run before. From `26903c0` on, the browser link was genuinely broken
(`audiocapture.cpp`'s PulseAudio half sat in the `#else` of `#ifdef _WIN32`, so
Emscripten compiled it and hit SDL semaphores the port does not have), and every
build during the investigation printed `### LINK OK ###` over it.

What makes that corrupt the *data* rather than simply run old code is where the
file packager sits. `em++` writes `blocks5.data` **before** `wasm-ld` runs, and
the table that says what is inside it lives in `blocks5.js`:

    loadPackage({files:[{filename:"/.update_checker",start:0,end:4},
                        {filename:"/data.zip",start:4,end:3191909},
                        {filename:"/levels/campaigns/blocks.zip",start:3191909,end:11472606}, ...

Absolute byte offsets. A failed link leaves a **fresh `blocks5.data` beside a
stale `blocks5.js`**, so every preloaded file is sliced at the offsets of the
*previous* archive - and because the entries are consecutive, a `data.zip` even
one byte off shifts everything that follows it: the campaign, the skins, the
fonts. That is the empty resource name, and it is why the failure looked like a
zip problem.

Measured, with a deliberate link failure on the real error: `blocks5.js` and
`blocks5.wasm` unchanged, `blocks5.data` rewritten. Every "ruled out" in the old
version of this entry falls out of that:

- Both packers fail — any new archive moves the offsets.
- An archive byte-identical to the working one passes — same bytes, same length,
  the stale table still fits.
- Padding to within 900 bytes does not help — near is not equal.
- A *larger* archive fails differently, the game not booting at all — the slice
  now runs past the end of the buffer.

Fixed in `fbe8fba`: `build.sh` reads `${PIPESTATUS[0]}` for `em++`'s own status
and exits 1, and the existence check that follows exits 1 too. Verified by
feeding it a bad `-s` flag - `### LINK FAILED ###`, status 1. `audiocapture.cpp`
got the browser its own branch back in the same commit.

Retested afterwards with three freshly packed archives, each different from the
one that had been lying in the tree - with `optipng` (3191905 bytes), without it
(3191979), and the intermediate - building both browser configurations and
running `WebBuild/test/smoke.js` against each: clean, no console errors, no
resource failures.

**The lesson is the build script, not the packer.** A check that reports success
from the artifact of a previous run is worse than no check, and it cost a day of
looking at zip files.
