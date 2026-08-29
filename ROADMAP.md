Blocks 5 - Roadmap
==================

Planned work, roughly in the order it was proposed. Each entry records what is
actually in the way, with file references where they are known, so the next
person does not have to rediscover it. Nothing here is scheduled.

Several of these unblock each other — see [How these connect](#how-these-connect)
at the end.


1. Auto-detect the user's language on first start
-------------------------------------------------
Today the language is chosen by the *installer*: `[Run]` in
`Blocks5/setup/Blocks 5.iss` calls `makeconfig.bat` with `en` or `de`, which
copies `_config_en.xml` or `_config_de.xml` to `config.xml` if none exists.
Anyone who does not run the installer — a zip copy, the browser build — gets
`<Language>en</Language>` regardless of where they are.

The detection itself is small. `Engine::loadConfig` (`engine.cpp:1893`) already
defaults to `"en"` and reads `<Language>` if present, so the hook is: when the
config has no `<Language>` element at all, ask the platform instead of falling
back to English.

- Windows: `GetUserDefaultUILanguage()` / `GetLocaleInfoEx` with
  `LOCALE_SISO639LANGNAME`, which gives the two-letter code directly.
- Browser: `navigator.language` / `navigator.languages`, reachable with a
  one-line `EM_ASM_INT` the way `WebBuild/web_audio.cpp` reads the
  `AudioContext`.

Two things make this bigger than it looks:

- `Engine::setLanguage` (`engine.cpp:2025`) hard-rejects anything that is not
  `"de"` or `"en"`, and the options dialog (`data/options.xml`) offers exactly
  those two. Detection is pointless until that whitelist is data-driven.
- `data/languages.txt` looks like it has four languages, but of its 349 string
  IDs only **1** has a `§fr:` body and **1** has a `§es:`. They are stubs, not
  translations. Detecting `fr` today would produce an English game with a French
  label on it. Either fill them in or make detection fall back to English for
  anything but `de`, deliberately and with a comment saying why.

Once the game detects its own language, `makeconfig.bat` and the two
`_config_*.xml` files can go, and the installer loses another moving part.


2. Replace HQ2X with something that ships as source
---------------------------------------------------
`libs/bin/hq2x32.obj` is the last piece of *compiled code* in the tree that is
not an import library. It is linked straight into the exe
(`AdditionalDependencies` in `Blocks5.vcxproj`) and exports one function,
`hq2x_32`. Everything else in `libs/bin` is now an import library for a DLL.

Three separate problems, worth separating:

- **No source.** The object is MaxSt's hq2x, LGPL 2.1, statically linked. That is
  the arrangement we deliberately avoided for OpenAL Soft — static LGPL linking
  carries relinking obligations that a dynamically shipped DLL does not.
- **`Blocks5/src/hq2x.cpp` has inline `__asm`.** The glue that builds the two
  lookup tables ends with an x86 `__asm { cpuid }` block probing for MMX. That is
  MSVC-x86-only: it blocks x64, it blocks Clang and GCC, and it is why
  `WebBuild/build.sh` filters `hq2x.cpp` out of the source glob. The MMX probe is
  also pointless on any CPU made this century.
- **The browser has no upscaler at all.** `-hq2x` is simply unavailable there
  (`WebBuild/platform_stubs.cpp` stubs it out), so a 640x480 canvas is scaled by
  whatever the browser does.

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
3. **Frame bracketing.** `glViewport` is set once at init to 640x480
   (`engine.cpp:433`) and never changes — even with hq2x on, the game renders
   into the bottom-left corner of a 1280x960 window. So the FBO pass needs no
   viewport change at all; only the present pass does.
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

Only three calls cross the bus, all `glReadPixels`: video capture
(`engine.cpp:850`, per recorded frame), screenshots (`:1177`, on demand), and
hq2x (`:1116`, **every frame it is on**). hq2x is the only per-frame CPU round
trip in the renderer.

Staging note: **do the FBO first with no shader at all** and draw the texture with
`GL_LINEAR`. That alone deletes the 7.6 ms and both transfers. The shader is then
a small increment on the same plumbing. Two decisions, not one.

### Which filter

Not an hq2x successor chosen for being newer. The measurement says the frame has
two regimes and a replacement has to serve both: hard-edged text and GUI, where
hq2x wins, and anti-aliased photographic tiles, where it degenerates to nearest —
the wrong answer for downsampled photos.

**xBR (the "lv2" formulation), as a single fragment shader at an arbitrary scale
factor**, is the recommendation. Same edge-detection premise as hq2x, so the text
and GUI keep the look they have, but it *blends* along detected edges instead of
snapping, so on the noisy 95% it degrades toward interpolation rather than toward
nearest. Roughly 150-250 lines of GLSL, no lookup tables, nothing beyond GL 2.0.

Two things to check before committing to it: the licence on whichever port is
used (Hyllian's shaders vary, some permissive and some GPL — the point of the
exercise is to improve on statically linked LGPL, not to trade it sideways), and
whether it turns the rain and the parallax sky to mush, which is where an
edge-directed filter on photographic content would show it first.

Alternatives that stay on the table: **Scale2x/Scale3x**, much simpler and
permissive, but it shares hq2x's flat-art premise and would do even less here.
**Porting hq2x itself to GLSL** — it is a 256-entry pattern table, so a drop-in
visual match is possible; more code than xBR and locked to exactly 2x.

Whatever replaces it, `hq2x.cpp`'s `__asm` block goes with it. And the
`SDL_ListModes` search in `engine.cpp:222-252` that hunts for a fullscreen mode
of at least 1280x960 exists *only* because the filter is locked to exactly 2x —
see item 10.

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


3. Compile every dependency from source
---------------------------------------
What is left as a Windows binary:

| Binary | What it is | Notes |
| --- | --- | --- |
| `libs/bin/hq2x32.obj` | see item 2 | the last piece of compiled code with no source |
| `libs/bin/OpenAL32.lib` + `OpenAL32.dll` | OpenAL Soft 1.25.2 | LGPL, *must* stay a DLL |

Everything else is compiled from vendored source: TinyXML, zlib + minizip, libogg,
libvorbis, stb_image, SDL 1.2.15, minih264, shine, minimp4. `hq2x32.obj` is the
only thing left to do here, and it is item 2.

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


4. Build with the newest MSVC
-----------------------------
`Build.bat` defaults to v143 (VS 2022) and takes `/toolset:vNNN`; its discovery
already probes `Platforms\Win32\PlatformToolsets\<ts>` and falls back through
vswhere, so a newer toolset mostly needs testing and adding to the known list.

The `<hash_map>` problem is dealt with: the 41 `stdext::hash_map` /
`hash_multimap` uses across 12 files are `std::unordered_map` /
`std::unordered_multimap` now, the header is out of `pch.h`, and
`_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS` is out of the project defines.

That also made the tree far easier to check without MSVC: **106 of the 111
sources in `Blocks5/src` now pass `i686-w64-mingw32-g++ -fsyntax-only`** against
the real `pch.h`. The five that do not are `main.cpp` (SEH `__except`),
`filesystem.cpp` (includes `Shlobj.h`, lowercase on case-sensitive systems) and
`panel.cpp` / `e_pulsepanel.cpp` / `teleporter.cpp`, which call `std::find`
without including `<algorithm>` — MSVC and libc++ pull it in transitively,
libstdc++ does not. Those last four are also exactly what a Linux build will trip
over first, so they belong to item 5.

Smaller conformance items already visible in the Emscripten build's warnings:
`register` in `libs/mtrand-1.1/MersenneTwister.h` (removed in C++17), string
literals assigned to `char*` in `e_flipflop.cpp` / `e_gate.cpp`, and a
`float*`/`double*` format mismatch in `cannon.cpp:141`.


5. Enable a Linux build
-----------------------
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
shimmed, and its `hq2x_32` no-op is item 2 in miniature.

A mingw sweep already names the first four things a GCC-based build will reject:
`filesystem.cpp` includes `Shlobj.h` where the file is `shlobj.h`, and
`panel.cpp`, `e_pulsepanel.cpp` and `teleporter.cpp` call `std::find` without
including `<algorithm>`. Everything else in `Blocks5/src` except `main.cpp`'s SEH
block already parses.

What still needs deciding:

- `getAppHomeDirectory()` — `My Documents\Blocks 5\` becomes
  `$XDG_DATA_HOME/blocks5` or `~/.local/share/blocks5`.
- **Case sensitivity.** The game resolves assets by name. Inside `data.zip` that
  is fine, but the loose-file development mode (`fs.pushCurrentDir("data")` in
  `main.cpp`) and user levels/skins on disk will expose every filename whose case
  does not match. Expect to find some.
- SDL 1.2 from the distro, or the SDL2 move from item 3.

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


6. Skins in the browser, and skins that travel with campaigns
-------------------------------------------------------------
Two related gaps.

**Skins in the browser.** `build.sh` already packs `levels/skins/*.zip` into the
staged tree and `main.cpp:193` copies them into the user directory on first
start, so the plumbing is there. The known blocker is that non-power-of-two
textures render black under WebGL; `texture.cpp:291` already warns when it
creates one. The game's own assets are all power-of-two, which is why this only
bites on user skins. Fix is either padding NPOT images up to the next power of
two and adjusting the texture coordinates, or requiring `OES_texture_npot`
behaviour (WebGL1 allows NPOT only with `CLAMP_TO_EDGE` and no mipmaps — which
is exactly how sprites are sampled, so this may be a two-line fix in the
sampler state).

**Skins in campaigns.** A campaign archive carries its levels but not the skins
they reference (`WebBuild/README.md` documents this), so a campaign built on a
custom skin renders with the missing-skin fallback unless the author distributes
the skin separately. `Level` records the skin names it wants and
`level.cpp:2345` already collects a `skinsMissing` set, so the game knows exactly
what is absent. The campaign format would need to carry skin members alongside
levels; `Campaign::save` was recently rewritten around a `LevelRef` that knows
whether its source is loose or inside an archive, and skins would follow the
same shape.


7. Translate all source comments to English
-------------------------------------------
Comments across `Blocks5/src` are in German and the files are ISO-8859-1.

The translation is mechanical but enormous, and it wants to be one sweep rather
than a drip, because half-translated files are worse than either end state. It
pairs naturally with converting the tree to UTF-8: once the comments are
English, almost nothing needs high bytes any more.

The catch is that "almost" is not "nothing". Some *string literals* genuinely
carry Latin-1 bytes and are load-bearing — `engine.cpp:2160` compares
`line[0] == '\xA7'` to parse `data/languages.txt`, `engine.cpp:2208` builds the
same section marker, and files like `activatorblock.cpp:61` hold German UI text.
Converting the sources to UTF-8 changes those literals' bytes. Doing this safely
means either `/utf-8` plus a UTF-8 BOM for MSVC and matching handling of
`languages.txt` (which is itself Latin-1 and shipped), or replacing the byte
literals with explicit escapes first and keeping the data file as it is. Decide
that before starting, not halfway through.

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
- **Kill the readback in `upscaleFrame`.** `glReadPixels` → CPU → upload
  stalls the pipeline every frame that hq2x is on; the browser console reports
  it directly. Measured on the real object at 640x480 it is **7.6 ms of CPU per
  frame** — 45% of a 60 fps budget — before either bus transfer, and item 2 shows
  it visibly changes under 5% of the pixels. See item 2: the FBO removes all of
  it, with or without a shader.
- Then, if it is still worth it, a programmable pipeline for the rest. That is
  also what a shader upscaler needs, so items 2, 8 and 10 converge here.

Worth measuring before optimising: `BEGIN_PROFILE` / `END_PROFILE` from `util.h`
are already available, and `PROFILE_HQ2X`, `PROFILE_VIDEO_CONVERSION` and
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

`hq2x32.obj`, the one foreign object file linked directly into the exe, carries
no `/DEFAULTLIB` or `/FAILIFMISMATCH` directive and no CRT references at all —
its only undefined symbols are `_LUT16to32` and `_RGBtoYUV`, both defined in
`src/hq2x.cpp` — so it did not stand in the way of `/MT`. `OpenAL32.dll` is
now the only DLL beside the executables, and it carries its own CRT across the
boundary as it always did; nothing allocates on one side and frees on the other,
because the game calls only core AL/ALC entry points and never takes ownership of
an OpenAL-side allocation.

The one thing given up is that a statically linked CRT no longer picks up
Windows Update's servicing of the shared one. For a single-player puzzle game
that is the right trade against shipping a 6.5 MB installer stub.


10. A window that behaves like a window
---------------------------------------
Three things the game should do and currently cannot:

- **Be resizable**, keeping the 4:3 aspect ratio and letterboxing with black
  bars when the window does not match.
- **Enter and leave fullscreen while running**, not only via the command line.
  Borderless — a window styled `WS_POPUP` and sized to the desktop, the way most
  games do it now — not an exclusive display-mode change. See below: that choice
  is what keeps the toggle from destroying the GL context.
- **Switch the upscaling filter while running** — nearest, bilinear, xBR, and
  whatever else item 2 adds.

Today `SDL_SetVideoMode` is called exactly once (`engine.cpp:302`), with no
`SDL_RESIZABLE`, and the mode is decided at startup from `-windowed` /
`-fullscreen` / `-hq2x`. Fullscreen with hq2x additionally walks `SDL_ListModes`
looking for the smallest mode of at least 1280x960 (`engine.cpp:222-252`), a
search that exists only because the filter is hardwired to exactly 2x.

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
- **Bilinear and xBR take the full fractional scale**, `min(w / 640, h / 480)`,
  because both resample properly and an integer scale would only throw away
  screen area.

So the destination rectangle is a function of the window size *and* the selected
filter, and changing the filter at runtime has to recompute it. One function,
used by the blit, by the cursor mapping (see below), and by nothing else.

Below 640x480 nearest has no integer scale left. Either clamp the window to a
640x480 minimum in the `SDL_VIDEORESIZE` handler by re-calling `SDL_SetVideoMode`
with the clamped size, or let it fall back to bilinear and say so in the options
dialog. The former is less surprising.

### Video and screenshots stay at 640x480

**Video recording always captures the game's internal 640x480**, never the window
size. That is what the code does today by accident — the capture at
`engine.cpp:850` runs *before* `upscaleFrame()`, so it reads the un-upscaled
render out of the back buffer — and with an FBO it becomes true on purpose:
capture reads `GL_COLOR_ATTACHMENT0` and is independent of the window entirely.

Three reasons it has to stay that way, not just for tidiness:

- minih264 requires the frame size to be a multiple of 16 (`videorecorder.cpp`
  centre-crops to enforce it). 640x480 is; an arbitrary resized window is not.
- The encoder is configured once, at `startRecording`. A window resized
  mid-recording would change the frame size under it.
- The cursor is drawn into the capture buffer by hand (`engine.cpp:855`) in
  640x480 coordinates from `getCursorPosition()`. That keeps working unchanged —
  and becomes correct under letterboxing for the first time, because it goes
  through the same inverse transform as everything else.

**Screenshots go the same way: clean 640x480.** `Engine::screenshot`
(`engine.cpp:1164`) runs *after* `upscaleFrame()` and reads `displaySize` from
`GL_BACK`, so today it saves the upscaled image — and once letterboxing exists it
would save the black bars with it. It moves before the blit and reads
`GL_COLOR_ATTACHMENT0` instead, like the video path. The filter is a display
setting; it does not belong in the file.

That also simplifies the function: `displaySize.x/y` become `screenSize.x/y`
throughout, the buffer is a fixed 921,600 bytes, and the row-flip loop stops
depending on the window.

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

`Engine::getCursorPosition` (`engine.cpp:1795`) and `setCursorPosition` already
undo the upscale, with a hardcoded `/2` and a y-offset for the hq2x case. That is
exactly the right place for a general inverse of the letterbox transform; it just
needs the scale and offset to come from the same rectangle the blit uses.

Two existing defects in that code, both invisible today because the hq2x path is
only ever exercised at exactly 1280x960 where all the offsets are zero:

- `getCursorPosition` offsets y by `(displaySize.y - screenSize.y * 2) / 2`, i.e.
  it assumes the image is centred vertically, but `upscaleFrame` places it with
  `glRasterPos2i(0, displaySize.y - screenSize.y * 2)`, i.e. flush to the top.
  The two disagree on any fullscreen mode taller than 960.
- `setCursorPosition` clamps with `clamp(temp.x, 0, temp.x - 1)`, whose upper
  bound is derived from the value being clamped, so it always returns
  `temp.x - 1`. Same for y.

### Where the settings live

The filter becomes a mode, not a boolean: `useHQ2X` in `Engine` gives way to an
enum, the `-hq2x` switch and `hq2x.bat` are replaced by something general, and a
dropdown joins `data/options.xml` next to the existing video settings. Every new
caption is a `$ID` in `data/languages.txt` with at least `§en:` and `§de:` bodies.

Note that **none of this is currently persisted at all.** `config.xml` holds
exactly `<Language>`, `<SoundVolume>`, `<MusicVolume>`, `<Details>` and
`<Controls>` — `Engine::loadConfig` at `engine.cpp:1893` and `saveConfig` right
below it read and write nothing else. Windowed vs. fullscreen and hq2x are
command-line only (`main.cpp:340-343` and `:350-352`), so they reset on every
launch. So this is not renaming a key: `<Upscaler>`, `<Fullscreen>` and
`<WindowSize>` all have to be added to both functions, along with the template
`_config_en.xml` / `_config_de.xml` the installer copies.

Fullscreen and window size want to be persisted in `config.xml` too, so the game
comes back the way it was left — and since fullscreen is now just a window size
plus a style flip, that is one boolean and one `Vec2i`, with no mode list behind
either. `-fullscreen` and `-windowed` stay as startup overrides.

In the browser none of this needs SDL: the canvas is resized by CSS and the
Fullscreen API, the WebGL context survives both, and the letterbox arithmetic is
the same code. That half is testable without Windows.


How these connect
-----------------
    2 (HQ2X from source) ─┬─> 8 (shader upscaler, no readback)
                          ├─> 5 (Linux: the __asm block blocks GCC/Clang)
                          ├─> 3 (last non-import binary in libs/bin)
                          └─> 10 (the FBO is the shared prerequisite)

   10 (window behaviour) ──> no longer needs SDL2: a borderless window styled
                             behind SDL's back keeps the GL context alive

    3 (all from source) ────> 5 (Linux needs an ffmpeg answer anyway)

    5 (Linux) <────────────── WebBuild/platform_stubs.cpp already does most of it

    7 (English comments) ───> pairs with the UTF-8 conversion; do them together

The one change under both 2 and 10 is the same 80 lines: render into a
framebuffer object instead of the back buffer. Everything else in either item is
an increment on it.

*Done since this list was written:* stb_image in place of SDL_image, the standard
unordered containers in place of `stdext::hash_map`, SDL 1.2.15 compiled from
source, `/MT`, and minih264 + shine + minimp4 in place of ffmpeg — which together
closed item 9, most of item 3, and the bug that made recorded videos unplayable.
Out of the tree: `sdl.dll`, `sdl_image.dll`, `libpng15-15.dll`, `zlib1.dll`, the
four ffmpeg DLLs, `oalinst.exe`, `vcredist_x86.exe`, ten import libraries and the
`msinttypes` shim. What ships now is three executables, **one** DLL that needs
nothing but Windows, and the data.
