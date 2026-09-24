# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.
It is the orientation. The detail lives in `.claude/rules/*.md`, one file per area, each loading itself
when a file it names is read — and since a rule cannot help before its files are opened, **read the rule
for an area before working in it**; the index at the end says which holds what.

## Project

Blocks 5 — "Bob's Amazing Adventures", a 2D tile-based puzzle/action game. C++ on SDL 1.2 + OpenGL +
OpenAL Soft. **Three builds from the same sources**: Windows/Win32 (`Build.bat`, needs Visual
Studio), native Linux (`LinuxBuild/build.sh`), and an Emscripten port in `WebBuild/`. The last two
build and run here, so a change can be compiled, run and driven without Windows — see
`LinuxBuild/README.md` and `WebBuild/README.md`.

**Windows builds on v143 and v145** (Windows 11, VS 2022 Community), and **it is built MultiByte, never
Unicode**: SDL 1.2 is an ANSI codebase and its 67 sources compile inside `Blocks5.vcxproj`, so
`Unicode` turns every unsuffixed Win32 call into its `...W` variant with no more than a warning, and
the first such build smashed a stack frame in `SDL_RegisterApp`. `build-windows.md` has that story
and the three vendored-library fixes the toolset needed.

## Build & run

`Build.bat` at the repo root does the whole thing from a fresh clone — finds MSBuild, checks the toolset,
builds `Blocks5.sln` for `Win32`, and packs `data.zip` and `levels/skins/*.zip`, gitignored build products
the game cannot start without; `Build.bat /?` lists its options. `Blocks5/pack.sh` packs the same archives
without Windows (`./pack.sh`, narrowed by `data`, `skins` or `campaign`; `--optipng` adds the slow
step, off by default because it rewrites tracked PNGs in place), and `levels/campaigns/blocks.zip` is a build product like the rest — so a level edited and not packed
changes what a developer sees and nothing a player sees. `build-windows.md` and `packing.md` have the rest.

The game must run with `Blocks5\` as working directory (VS's default `$(ProjectDir)`) because it opens
`data.zip` relative to the cwd. `Build.bat /run` does that; it must come last, since every argument
after it goes to `blocks5.exe` untouched (`Build.bat Debug /rebuild /run -windowed`).

Command line / launcher scripts — the whole list, all five documented in `readme.txt`: `-windowed`
(`windowed.bat`), `-fullscreen`, `-nosplash`, `-perf`, `-flushall`. `-flushall` makes the renderer put
every quad up on its own instead of batching it, the arm to measure the batching against and the
bisecting tool for an ordering bug; `?flushall=1` is the same switch in the browser, as `?perf=1` is for
`-perf`. `-nosplash`
skips the logo and jingle by *not requesting* `logo.png`, the path `GS_Loading` already takes when the
texture will not load; only `soundPlayed` has to start `true`, because the jingle hangs off the time
threshold rather than the logo.

**Framebuffer objects, GL 2.0 shaders and vertex buffer objects are requirements; the game says so and
stops where one is missing** (`GLExtensions::init`, `createFrameBuffer`, `createUpscalerGL`), so there is
no availability to branch on anywhere — no `useFrameBuffer`, no `Upscaler::isAvailable`, no fallback to
`Sharp`, no 640x480 window pin, no `-nofbo`/`-noshader`. `fatalError()` (`fatalerror.h`) is the one way
the game gives up, written once per platform. `rendering.md` has the argument for the floor, who the
message is written for, and why the Linux half uses `fork`/`execlp`.

The upscaling filter is not a switch but an in-game option like the language, saved as `<Upscaler>` in
`config.xml`. Debug builds default to windowed + Console subsystem and skip the SEH crash handler;
Release defaults to fullscreen + Windows subsystem and dumps a stack trace via `StackWalker`.

Three projects: **Blocks5** (the game), **PWEncrypt** (encrypts an archive password into the bracket
form used in paths), **ShowUserDir** (opens the user data folder in Explorer).

## Checking a change

Four things run here, none needing Windows. Run at least the first two after any edit; about half a
minute together.

```
python3 Tools/verify.py      twenty-two static checks over the whole tree
sh Tools/syntax.sh           compile every source with mingw; an integer or a double to a float fails it
LinuxBuild/build.sh          the native build compiles and links with GCC
cd WebBuild && ./build.sh    the browser port actually builds and links
```

Three ways to *run* it: `LinuxBuild/test/smoke.sh` natively, `WebBuild/test/smoke.js` in a desktop
browser, `WebBuild/test/mobile.js` in an emulated phone. `checks.md` says what `verify.py` looks for,
`testing.md` how the harnesses drive the game and where they lie, `perf.md` what a frame timing means.

**A change whose whole question is what it looks like goes to the author to try, unbuilt.** Tuning a
glow, colour, width or timing: the build takes minutes, the screenshot oracle longer, and neither can
answer *is that the look I want*. Make the edit, say what the numbers mean and which way to turn them,
stop. Everything else still applies to anything a compiler or check can judge, and to a visual change
that also moves code around.

**The same holds for sound: a change whose whole question is how it sounds goes to the author to hear.**
Whether a cue lands with its animation, a fade is quick enough or a level sits right in the mix is a
question the author's ears answer in a few seconds. A spy on the audio calls or a decoded waveform can
prove, at the cost of minutes, that the call was made and when - never that it sounds right. Make the
edit, say what the numbers mean and which way to turn them, stop.

**A check that can pass on a previous run's artifact is worse than no check.** `WebBuild/build.sh` tests
`${PIPESTATUS[0]}` — the `em++` it pipes through `tail`, not `tail` — and exits 1 on it, rather than
asking afterwards whether `blocks5.wasm` exists, which it would from the run before. Worse than stale:
`em++` writes `blocks5.data` *before* `wasm-ld` runs and the table of byte offsets into it lives in
`blocks5.js`, so a failed link leaves a fresh data bundle beside the previous run's offsets and every
preloaded file is sliced in the wrong place — which reads as a corrupt `data.zip`, and cost ROADMAP item
20 a day. The native harness refuses a `build-test/` older than the sources for the same reason.

The Linux build is the fastest way to *run* a change, and unlike the browser it is a real GCC compile of
every source, `videorecorder.cpp` included. It cannot check anything Windows-only — the SEH crash
handler, the Win32 window procedure, `audiocapture.cpp`'s WASAPI half — and those are what
`Tools/syntax.sh` is for.

## Architecture

Everything lives flat in `Blocks5/src`. Layering is by naming prefix, not directory: `gs_*` game states,
`gui_*` widgets, `cf_*` crossfades, `u_*` upscaling filters, `e_*` electronics parts,
`as_*`/`audiostream*` audio decoding, `file*`/`filesystem*` virtual FS.

**Singletons and resources.** Global services derive from `Singleton<T>` (`singleton.h`) and are reached as
`Engine::inst()`, `GUI::inst()`, `FileSystem::inst()`, `ProgressDB::inst()`. Shared assets derive from
`Resource<T>` (`Texture`, `TileSet`, `Font`, `Sound`, …) and come through
`Manager<T>::inst().request(filename)` / `->release()` — ref-counted, keyed by filename, never
`new`/`delete`d directly.

**Engine** (`engine.cpp`, 164k, the biggest file in the tree) owns the main loop, window, OpenAL, config, localization, screenshots and
video capture. The loop renders as fast as it can but steps logic at a fixed `logicRate` of 20 ms
(`setLogicRate(20)` in `Engine::init`); one `update()` call is one logic tick, so gameplay counts ticks
rather than measuring dt.

**Presentation.** The game always renders 640x480 into a framebuffer object (`createFrameBuffer`: a 640x480
region of a 1024x512 texture plus a packed depth-stencil renderbuffer — `cf_star.cpp` and `level.cpp` both
need the stencil), and `presentFrame` puts that on screen as one letterboxed quad. Every hardcoded
coordinate in the tree — the single `glViewport`, the `glScissor` calls, the GUI layouts — therefore stays
valid whatever size the window is; only `computePresentRect` changes, and the cursor mapping in
`getCursorPosition`/`setCursorPosition` is its exact inverse. Video capture and screenshots read
`GL_COLOR_ATTACHMENT0` at 640x480 and never see the window size. `glextensions.cpp` loads ten framebuffer-object
entry points and twenty-six more for the shaders and the buffers, `glGenFramebuffersEXT` first and the core spelling as fallback; in the
browser they are core and the header `#define`s them through.

**Rules every source obeys**, each argued in its rule file:

- **Everything draws through `Renderer`, and raw GL lives in the files that own it** - the renderer
  itself, the texture upload, the engine's framebuffer and present, the present filters and the loader.
  A quad handed to the renderer is queued and put up at the next flush, so a raw
  draw anywhere else would land underneath what was queued before it and a raw state change would
  fool the renderer's record; in `texture.cpp` and `engine.cpp` a raw call stands inside a
  `Renderer::DirectGL` bracket, which flushes first and forgets what GL holds after (`rendering.md`).
- **A picture declares at its request whether it tiles** (`Texture::WM_CLAMP`, `WM_WRAP`, `WM_REPEAT`),
  because that decides whether it can share an atlas page with others - `GL_REPEAT` wraps at the
  texture's edge, and in a page the texture is the page. Only the weather needs a texture of its own,
  and `Texture::NEVER_PACK` beside the mode keeps one out for any other reason.
  A quad's uv stays inside its own picture unless it came from `Renderer::tiledQuad`, and a test-hooks
  build fails every quad that does not (`rendering.md`).
- **A class whose ancestor already put bits in `renderLayers` adds with `|=`** — the `Electronics`
  parts, whose base sets `RL_WIRE`; assigning wipes the bit and nothing says so — and a render layer is
  an `RL_*` name, never a number (`rendering.md`).
- **No display lists, no `GL_QUADS`, no wide lines, no alpha test, no fixed function at all**: WebGL has
  none of them, and the browser is the build nobody runs first. The rule above is what keeps them out,
  since nothing outside the renderer can reach GL to ask, and the browser build links against plain WebGL
  with no emulation, so one that slipped in would be an undefined symbol there rather than a black screen
  (`rendering.md`, `web.md`).
- **Nothing in a render path draws a random number**; per-tick jitter comes from `frameBegin()` and
  `Level::update()`, so a frame stays reproducible from a seed (`objects.md`).
- **Anything that reads the rendered frame binds the FBO itself** (`rendering.md`).
- **In the browser nothing hangs off teardown** — no destructor and no `Engine::exit` ever runs there
  (`web.md`).
- **No string names a key** (`%BINDING{$ID}` does), and every user-facing string is a `$ID` in
  `languages.txt` (`gui-text.md`).
- **`check()` is the user's click and fires `changed`; `setChecked()` is the display catching up and does
  not** (`gui-text.md`).

## Conventions

- Every `src/*.cpp` uses the precompiled header: `#include "pch.h"` must be the first line (`pch.cpp`
  is the Create-PCH translation unit). `pch.h` already pulls in SDL, OpenGL, OpenAL, libvorbis,
  TinyXML, sigslot, MersenneTwister, `img_load.h` and the core helpers (`singleton.h`, `vec.h`,
  `typedefs.h`, `util.h`, `manager.h`, `renderer.h`), so don't re-include those. `renderer.h` is in that
  list rather than per file because every source that draws anything reaches `Renderer`, the one way
  to put a pixel on the screen.
- There is no glob-based build: a new source file must be added to `Blocks5/Blocks5.vcxproj` **and**
  `Blocks5.vcxproj.filters`. `Tools/verify.py` checks this — nothing else will, since the Emscripten
  build globs `src/*.cpp` and so never notices.
- Naming: `p_` prefixes a pointer, `pp_` a pointer-to-pointer; classes are `PascalCase`, methods
  `camelCase`, enum constants `PREFIX_UPPER` (`OF_*`, `SKIN_*`, `FM_*`). Every class with a base class
  lives in the header named after it, lower-cased (`CF_Star` in `cf_star.h`) — in a flat directory of
  two hundred files that is the whole navigation, and the `naming` check holds it.
- **A rename goes through a tool that parses the code, never through a text substitution.**
  `clang-rename` and `clang-change-namespace` are installed (LLVM 18, `/usr/lib/llvm-18/bin/`), and
  `sh Tools/compile_db.sh` writes the `compile_commands.json` they need — asking `LinuxBuild/build.sh
  flags` for the real compile flags rather than keeping a copy, because a database with its own idea of
  the include paths has a refactoring tool parsing a different program from the one that ships. The
  file is a build product and is gitignored.

  ```
  sh Tools/compile_db.sh
  clang-rename-18 -i --qualified-name='Texture::ref' --new-name='...' Blocks5/src/*.cpp
  ```

  The reason is not tidiness. A `sed` over `GLState::` → `GL::` also rewrites `GLState::GLState`, the
  constructor of the struct of that name — measured: `clang-rename` asked for the same namespace rename
  leaves that line alone, and `sed` breaks it. The same shape waits wherever a member, a local or a word
  inside a comment or string literal shares a name with the thing being renamed. What saves a blind
  replacement here is that `Tools/syntax.sh` compiles all 123 sources in seconds, so the mistake is a
  compile error rather than a silent one — but that is a backstop, not a method, and it catches nothing
  that still compiles.
- **A comment says what the code does and why, never what it used to do.** The reader is looking at the
  current code; the previous version is in the history, and an account of it in the file is noise they
  have to read past. No "used to be", no "this was moved from here", no retelling of the bug that led to
  the line. That belongs in the commit message.

  What *is* worth writing is the gotcha: wherever a reader would reasonably stop and ask "why like this
  — why not the obvious thing?", answer that. The platform quirk, the ordering that matters, the
  constraint that rules out the shorter version. One sentence of reason is worth more than a paragraph
  of archaeology, and if the reason is genuinely long, the length is earned.
- **Documentation that describes one file belongs in that file**, not here and not in a rule file: a
  constant's reasoning next to the constant, a measurement next to what it measured. The rule files
  carry what spans files — an ordering between two of them, a check and what it guards, a platform
  difference — and the orientation.
- **Comments are in English, and so is everything the build and test tools print.** German survives in
  exactly three places, all data rather than prose: `data/languages.txt`, the inline `"\xA7" "de:…"`
  strings, and the two word lists in `verify.py`'s `comments` check together with the two faults
  `selftest.py` injects into it. Vocabulary where the obvious word is wrong: `massiv` is *solid*
  (`OF_MASSIVE` means impassable), `Ebene` is *layer* (*level* would collide with the class), and `Bild`
  is a *frame*, a *picture* or an *image* depending on the sentence.
- **Every source file is pure ASCII** — `Blocks5/src`, `WebBuild`, `PWEncrypt` and `ShowUserDir`, all of
  it. Umlauts are written `ae oe ue ss` (`AE OE UE SS` inside an all-caps word), so the encoding of these
  files no longer matters to anything: ASCII is a subset of UTF-8, of Latin-1 and of every codepage, and
  none needs a BOM or a `/utf-8` switch. Keep it that way — one umlaut typed into a comment puts the tree
  back to being encoding-dependent.
- **The three bytes that carry meaning are written as escapes.** `data/languages.txt` is Latin-1 and
  shipped that way; the game parses it with `'\xA7'` (the section sign, §) in `engine.cpp`, `'\xB6'` (the
  pilcrow, ¶, a line break) in `font.cpp` and `'\xB7'` (the middle dot, ·, a half space) in `font.h`; a
  few inline localized strings use the same syntax — `"\xA7" "de:…"`, split because a C++ hex escape is
  greedy and `"\xA7de:"` would parse as `\xA7d`. Those are a wire format shared with a data file, not
  text: they have to stay byte-exact whatever the source encoding is.
- Source files use LF — except vendored third-party ones, which keep whatever they shipped with
  (`src/stackwalker.*` is CRLF). Shipped text files (`readme.txt`, `levels/readme.txt`,
  `data/languages.txt`) are deliberately CRLF, and so is every `.bat`: cmd can miss a label when it
  jumps in a file with bare LF endings, and three of them ship. `verify.py`'s `encoding` check holds
  both.
- Log with `printfLog(...)` from `util.h`, not `printf`/`std::cout`. `BEGIN_PROFILE`/`END_PROFILE` macros
  are available for timing a block.
- Third-party libraries are vendored under `Blocks5/libs`, each with a `PROVENANCE.txt` giving its
  upstream, licence, which files are compiled, and what was changed locally. All that is left in
  `Blocks5/libs/bin` is `OpenAL32.lib`, an import library — no compiled code without source anywhere in
  the tree, and nothing that pins the toolset.

## The rule files

`.claude/rules/`, each with the globs it loads for in its front matter. They assume this file is loaded
and do not repeat it.

| file | loads for | holds |
| --- | --- | --- |
| `build-windows.md` | `Build.bat`, the `.sln`/`.vcxproj`, `setup/`, `resources.rc`, `main.cpp`, `libs/`, `PWEncrypt`, `ShowUserDir` | the toolset, SDL from source, the by-hand build, the version in four places, OpenAL Soft, deployment, every vendored patch |
| `packing.md` | `pack.sh`, `zip_*.bat`, `levels/`, `data/*.xml`, `languages.txt`, `campaign.cpp` | `data.zip`, the skins and `blocks.zip` as build products, and the comment stripping |
| `checks.md` | `verify.py`, `selftest.py`, `syntax.sh`, `compile_db.sh`, `make_ico.py`, `Tools/README.md` | what `verify.py` looks for and why, `selftest.py`, `syntax.sh` |
| `testing.md` | `LinuxBuild/test/`, `WebBuild/test/`, the test hooks, `Tools/testlevels/` | driving the game natively, in a browser and on a phone, and every trap in the harnesses |
| `perf.md` | `framestats.*`, `perf.js`, `pre.js` | what each frame timing means per platform, the overlay's counts, the query knobs |
| `rendering.md` | `renderer`, `renderstate`, `level`, `texture`, `textureatlas`, `tileset`, `sprite`, `engine`, `particlesystem`, `lava`, `lightning`, the crossfades | the renderer, its scopes and its bracket, what it bakes and why it is byte-exact, the files that own raw GL, browser colour, render layers, the FBO bind rule, the atlas and the three wrap modes |
| `upscalers.md` | `u_*`, `upscaler.*`, `cf_rewind.*`, `options.*`, `options.xml` | the four filters, the CRT offer and sliders, the rewind transition |
| `window.md` | `engine.*`, `linux_window.*`, `pre.js`, `shell.html`, `web_bluescreen.*`, SDL's `windib/` | SDL flags, fullscreen, placement, the default size, the cursor size, phone fullscreen |
| `audio-video.md` | `audiocapture`, `videorecorder`, `sound*`, `streamedsound`, `as_*`, `sounds.xml`, `encode_sounds.py` | recording, loopback capture, the mix headroom, the sound sources and `sounds.xml` |
| `input.md` | `engine.*`, `options.*`, `main.cpp`, `gs_game.*`, `touch_controls.js` | virtual keys and actions, the pause, the key grab, bindings by name |
| `objects.md` | `level`, `object`, every object source, `gs_*`, `cf_*`, `e_*`, `cat*.xml`, `levels/*.xml` | game states, the tick order, randoms in the render path, flash, the diamond machine, the hint note, presets, electronics, the level format |
| `filesystem.md` | `file*`, `filesystem*`, `progressdb`, `transfer`, `campaign`, `gs_selectlevel`, `gs_menu`, `main.cpp`, `menu.xml` | archives and passwords, the two content roots, `ProgressDB`, single levels, the Manager |
| `images.md` | `img_*`, `make_ico.py`, `make_icon.py`, `make_text.py`, `manifest.json`, `libs/stb` | decoding, the PNG writer, screenshots, every icon |
| `web.md` | `WebBuild/` | the page, function keys, the boot screen, stamps and the service worker, memory, focus, the lifetime, Quit |
| `gui-text.md` | `gui*`, `font.*`, `help.*`, `data/*.xml`, `languages.txt` | the font caches, the GUI tree and widgets, text fitting and keycaps, toasts, localization |
