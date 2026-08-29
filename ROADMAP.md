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

The detection itself is small. `Engine::loadConfig` (`engine.cpp:1899`) already
defaults to `"en"` and reads `<Language>` if present, so the hook is: when the
config has no `<Language>` element at all, ask the platform instead of falling
back to English.

- Windows: `GetUserDefaultUILanguage()` / `GetLocaleInfoEx` with
  `LOCALE_SISO639LANGNAME`, which gives the two-letter code directly.
- Browser: `navigator.language` / `navigator.languages`, reachable with a
  one-line `EM_ASM_INT` the way `WebBuild/web_audio.cpp` reads the
  `AudioContext`.

Two things make this bigger than it looks:

- `Engine::setLanguage` (`engine.cpp:2031`) hard-rejects anything that is not
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
- **The browser has no upscaler at all.** `-hq2x` is simply unavailable there.

Options, roughly in increasing order of ambition:

- Recover hq2x from source. Maxim Stepin's C sources exist and there are clean
  reimplementations; still LGPL, so it would want to be a DLL, or a permissive
  reimplementation would be needed.
- Scale2x/Scale3x — much simpler, permissively licensed, plain C, and honest
  about what it does.
- **A fragment shader.** This is the interesting one: the upscale happens on the
  already-rendered frame (`Engine::upscaleFrame`, which today does
  `glReadPixels` → CPU filter → upload), so it is a natural post-process. A GLSL
  version runs on the GPU in both builds, deletes the readback stall that shows
  up as `GPU stall due to ReadPixels` in the browser console, and makes xBR-class
  filters affordable. It does mean the renderer needs a programmable path, which
  overlaps with item 8.

Whatever replaces it, `hq2x.cpp`'s `__asm` block goes with it.


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
carry Latin-1 bytes and are load-bearing — `engine.cpp:2164` compares
`line[0] == '\xA7'` to parse `data/languages.txt`, `engine.cpp:2212` builds the
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
  it directly. See item 2 — a shader-based filter removes this entirely.
- Then, if it is still worth it, a programmable pipeline for the rest. That is
  also what a shader upscaler needs, so items 2 and 8 converge here.

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


How these connect
-----------------
    2 (HQ2X from source) ─┬─> 8 (shader upscaler, no readback)
                          ├─> 5 (Linux: the __asm block blocks GCC/Clang)
                          └─> 3 (last non-import binary in libs/bin)

    3 (all from source) ────> 5 (Linux needs an ffmpeg answer anyway)

    5 (Linux) <────────────── WebBuild/platform_stubs.cpp already does most of it

    7 (English comments) ───> pairs with the UTF-8 conversion; do them together

*Done since this list was written:* stb_image in place of SDL_image, the standard
unordered containers in place of `stdext::hash_map`, SDL 1.2.15 compiled from
source, `/MT`, and minih264 + shine + minimp4 in place of ffmpeg — which together
closed item 9, most of item 3, and the bug that made recorded videos unplayable.
Out of the tree: `sdl.dll`, `sdl_image.dll`, `libpng15-15.dll`, `zlib1.dll`, the
four ffmpeg DLLs, `oalinst.exe`, `vcredist_x86.exe`, ten import libraries and the
`msinttypes` shim. What ships now is three executables, **one** DLL that needs
nothing but Windows, and the data.
