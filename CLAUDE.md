# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Blocks 5 — "Bob's Amazing Adventures", a 2D tile-based puzzle/action game. C++ on SDL 1.2 +
OpenGL + OpenAL Soft, **Windows/Win32 only**. Non-Windows code paths are literal
`#error NOT IMPLEMENTED` (`filesystem.cpp`, `main.cpp`, `file_real.cpp`, `util.cpp`), so the
tree cannot be compiled or run on Linux/macOS — code changes here are edit-and-review only
unless you are on Windows with Visual Studio. There is also an Emscripten port in
`WebBuild/`, which does build and run on Linux and is the only way to test a change here
without Windows; see `WebBuild/README.md`.

**The Windows build compiles and links on v143** (Windows 11, VS 2022 Community, SDK
10.0.26100). Four things had to be fixed to get there, all in the vendored libraries or the
project settings, none in the game's own code; each is written up in the relevant
`libs/*/PROVENANCE.txt`. Three were compile errors: shine's `__attribute__((unused))`, which
MSVC rejects; `misc.c` in the libvorbis file lists, which is a pthreads debug allocator
upstream never compiles; and `windows.h` being included inside SDL's `#pragma pack(push,4)`,
which makes every `C_ASSERT` in a modern `winnt.h` fail.

**The fourth is a rule, not a one-off: this project must be built as MultiByte, never as
Unicode.** SDL 1.2 is an ANSI codebase — `char*` throughout, calling `RegisterClass`,
`LoadLibrary`, `GetLocaleInfo` and the rest unsuffixed. It was a DLL before, built ANSI by
SDL's own project, so `CharacterSet` here never mattered; now that its 67 sources are
compiled *inside* `Blocks5.vcxproj`, `Unicode` resolves all of those to the `...W` variants
and MSVC merely warns (C4133). The first run built fine and then died in `SDL_RegisterApp`
with an access violation: `GetCodePage` in `SDL_sysevents.c` passes `char buff[8]` and
`sizeof(buff)` to `GetLocaleInfo`, whose last parameter is a count of *characters* — so
`GetLocaleInfoW` wrote 16 bytes into 8 and smashed the stack frame. Roughly forty other
C4133 warnings across the SDL sources were the same bug waiting to happen. The game's own
code never depended on Unicode: it calls `MessageBoxA`, `ShellExecuteA`, `SHGetFolderPathA`
and friends explicitly, uses `WinExec` (which has no wide variant), and contains no `TCHAR`,
`TEXT()` or `wchar_t` outside vendored `stackwalker.cpp`, which is TCHAR-correct either way.
`SDL_win32_main.c` keeps its `#undef UNICODE` prologue as a guard against this being flipped
back.

## Build & run

`Build.bat` at the repo root does the whole thing from a fresh clone — it finds MSBuild,
checks the toolset, builds `Blocks5.sln` for `Win32`, and then packs `data.zip` and
`levels/skins/*.zip`, which are gitignored build products the game cannot start without.
`Build.bat /?` lists its options.

**Toolset: v143 by default; v140 and v120 also work** (`/toolset:v120`). The tree was pinned
to v120 for a decade by `libs/bin/tinyxml_STL.lib`, which carried `/FAILIFMISMATCH:"_MSC_VER=1800"`;
TinyXML 2.6.2 is now compiled from vendored source in `libs/tinyxml-2.6.2` and that library is
gone. Two things make anything newer than v120 work:

- `<hash_map>` is gone. `pch.h` used to include it for `stdext::hash_map` and
  `hash_multimap`, which from VS2015 on is a hard `static_assert` error and only compiled
  because `_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS` was set. Those 41 uses across 12 files
  are `std::unordered_map` / `std::unordered_multimap` now — every toolset from v120 on has
  them, and the define is gone with the header.

- **SDL itself is compiled from source**, all 67 files of the Win32 subset, out of
  `libs/SDL-1.2.15/src` — the same set SDL's own `VisualC/SDL/SDL.vcproj` builds. There is no
  `sdl.dll` and no `libs/bin/sdl.lib`; that DLL was pre-UCRT and the last thing in the tree
  that needed MSVCR120. It needs one include directory, `winmm.lib` and `dxguid.lib` from the
  Windows SDK, and `DECLSPEC=` among the defines (`begin_code.h` guards it with `#ifndef` and
  would otherwise mark every entry point `__declspec(dllexport)`, which is wrong for a static
  build). Two files carry local changes; see `libs/SDL-1.2.15/PROVENANCE.txt` and the
  patch table below.

`PWEncrypt` no longer calls `gets()`, which the Universal CRT removed.

To build by hand instead: open `Blocks5.sln` in Visual Studio (only `Debug|Win32` and
`Release|Win32` exist) and build all three projects. Then, from the `Blocks5` directory:

```bat
zip_data.bat     :: pack data\ into the encrypted data.zip the game reads at runtime
zip_skins.bat    :: pack levels\skins\<name>\ into levels\skins\<name>.zip
stage.bat        :: build a redistributable tree in Blocks5\stage (needs ..\Release\*.exe)
```

`zip_*.bat` run `tools\optipng` first, which is slow; `zip_data_no_optipng.bat` and
`zip_skins_no_optipng.bat` skip that step. Both require `tools\7za.exe`.

The game must run with `Blocks5\` as its working directory (VS's default `$(ProjectDir)` is
correct) because it opens `data.zip` relative to the cwd. `Build.bat /run` builds and then
does that for you; it has to come last, because every argument after it goes to `blocks5.exe`
untouched (`Build.bat Debug /rebuild /run -windowed`). There are **no tests and no
linter**.

Command line / launcher scripts: `-windowed` (`windowed.bat`) and `-fullscreen` — that is
the whole list. The upscaling filter is *not* a switch; it is an in-game option like the
language, saved as `<Upscaler>` in `config.xml`. Debug builds default to windowed + Console
subsystem and skip the SEH crash handler; Release defaults to fullscreen + Windows subsystem
and dumps a stack trace via `StackWalker` on an exception.

Installer: `setup\Blocks 5.iss` (Inno Setup). Version number lives in **four** places that
must stay in sync — `p_localVersion` in `src/main.cpp`, `AppVersion`/`OutputBaseFilename` in
the `.iss`, the banner and changelog in `readme.txt`, and `FILEVERSION`/`PRODUCTVERSION` plus
the two string values in `src/resources.rc`, which is what Explorer shows and what a crash
log reports. The `.rc` had been missed before and sat at 1.1.1 through the whole of 1.1.2.

The three projects: **Blocks5** (the game), **PWEncrypt** (CLI that encrypts an archive
password into the bracket form used in paths), **ShowUserDir** (opens the user data folder in
Explorer).

## Architecture

Everything for the game lives flat in `Blocks5/src`. The layering is by naming prefix, not by
directory: `gs_*` = game states, `gui_*` = widgets, `cf_*` = crossfade effects, `e_*` =
electronics parts, `as_*`/`audiostream*` = audio decoding, `file*`/`filesystem*` = virtual FS.

**Singletons and resources.** Global services derive from `Singleton<T>` (`singleton.h`) and are
reached as `Engine::inst()`, `GUI::inst()`, `FileSystem::inst()`, `ProgressDB::inst()`.
Shared assets derive from `Resource<T>` (`Texture`, `TileSet`, `Font`, `Sound`, …) and are
obtained through `Manager<T>::inst().request(filename)` / released with `->release()` —
ref-counted, keyed by filename, never `new`/`delete`d directly.

**Engine** (`engine.cpp`, ~56k) owns the main loop, the window, OpenAL, config, localization,
screenshots and video capture. The loop renders as fast as it can but steps logic at a fixed
`logicRate` of 20 ms (`setLogicRate(20)` in `Engine::init`); one `update()` call is one logic
tick, so gameplay code counts ticks rather than measuring dt.

**Presentation.** The game always renders 640x480 into a framebuffer object
(`createFrameBuffer`, a 640x480 region of a 1024x512 texture plus a packed depth-stencil
renderbuffer — `cf_star.cpp` and `level.cpp` both need the stencil), and `presentFrame`
puts that on the screen as one quad. Every hardcoded coordinate in the tree — the single
`glViewport`, the `glScissor` calls, the GUI layouts — therefore stays valid whatever size
the window is. `glextensions.cpp` loads what that needs: ten FBO entry points and
twenty-five GL 2.0 ones, `glGenFramebuffersEXT` first and the core spelling as a fallback;
in the browser they are core and the header just `#define`s them through. Four upscale
filters (`Engine::UpscaleFilter`), chosen by `<Upscaler>` in `config.xml` or `-filter:`:
`nearest` and `bilinear` are just `GL_TEXTURE_MAG_FILTER`, `xbr` and `xbr-details` run the
shader in `libs/xbr` — which **must** sample with `GL_NEAREST`, or its edge detection has
nothing to work with and the output is bilinear at ten times the cost. Without an FBO the
game renders straight to the back buffer as before; without a shader, xBR degrades to
bilinear. Neither is fatal.

**Video recording** writes H.264 Baseline video and MP3 audio into an MP4, with no DLL
involved: `libs/minih264` encodes the video, `libs/shine` the audio, `libs/minimp4` writes the
container, and all three are vendored source. Windows has decoded that combination natively
since Windows 7 — the container and H.264 since 7, the MP3 decoder since Vista, and the
MPEG-4 File Source documents its `'mp4a'` sample entry as meaning "AAC or MP3" — so a
recording plays on a clean install, which the old ffmpeg AVI did not. The three libraries are
plain C and were chosen so an eventual Linux build can use the same ones. `videorecorder.cpp`
does its own RGBX→YUV420 conversion (the frame arrives from `glReadPixels` upside down) and
holds each encoded frame back by one, because a frame's duration is only known when the next
one arrives. minih264 needs the frame size to be a multiple of 16; the game's 640×480 is.

**Recorded audio** does not come from OpenAL. `alcCaptureOpenDevice` can only open an *input*
device, so the old code recorded the microphone into every video. `audiocapture.cpp` replaces it
with WASAPI loopback capture of the default *render* endpoint, converting whatever mix format the
device uses (float32 or 16/24/32-bit PCM, any channel count, any rate) to the 16-bit stereo
48 kHz `videorecorder.cpp` wants, and padding real gaps with silence off the QPC clock so the
audio track stays as long as the video. The whole implementation is behind `#ifdef _WIN32` — the
`#else` half is a stub that fails `open()` — because the Emscripten build globs `src/*.cpp`.

**OpenAL is OpenAL Soft**, vendored in `libs/openal-soft-1.25.2` (headers, public domain) with
its import library in `libs/bin` and `Blocks5/OpenAL32.dll` — `soft_oal.dll` renamed, which is
how that distribution is meant to be used without the router. Because the app directory beats
`system32` in the DLL search order, the game always gets this implementation and never whatever
Creative's 2009 installer may have left on the machine; `oalinst.exe` and the installer's
`InstallOpenAL11` task are gone. The game only calls core AL/ALC 1.1 (23 functions, no
extensions, no `alGetProcAddress`), so the switch needed no source change at all. The DLL is
LGPL v2 and must stay dynamically linked.

**Input** is two-layered. Physical keys/joystick axes/hats are mapped to *virtual keys*
(`VirtualKey`), and named *actions* (`"$A_LEFT"`, `"$A_PLANT_BOMB"`, …) bind a primary and
secondary VK. Gameplay queries `wasActionPressed(name)` / `isActionDown(name)`; bindings are
registered in `main.cpp` and remappable via the options dialog.

**Game states** are a stack. Each derives from `GameState` (`gs_*.cpp`: Loading, Menu,
SelectLevel, Game, LevelEditor, CampaignEditor, Credits) and is registered by constructing it —
the base constructor calls `Engine::registerGameState`. Transitions go through
`setGameState`/`pushGameState`/`popGameState` by string name with an optional `ParameterBlock`
context, and are applied at a safe point by `processGameStateChanges()`, not immediately.

**Level and objects.** `Level` (`level.cpp`, ~61k) holds two tile layers plus a vector of
`Object*` and a spatial hash (`hashObject`/`getAllObjectsAt`) for position lookups. `Object`
(`object.h`) is the base for everything dynamic; behavior is driven by an `OF_*` flag bitmask
(`OF_MASSIVE`, `OF_GRAVITY`, `OF_DEADLY`, `OF_ELECTRONICS`, …) plus virtual `onUpdate`,
`onRender`, `onCollision`, `move`, `reflectLaser`, … `StdObject` covers the plain sprite cases
(blocks, diamonds, grass) so most simple types need no new class at all.

`Level::update()` is the tick order: remove/add pending objects → `frameBegin()` on all →
`update()` on all → `Electronics::updateAll()` → particle systems → AI-trace decay → exit check.

**Presets are the object factory.** `presets.cpp` maps a type-name string to a constructed
`Object` in one long `if/else if` chain (`instancePreset`), plus a `texCoords` table for the
editor's sprite. Adding an object type means: write the class (if `StdObject` won't do), add its
sprite coords + a branch in `presets.cpp`, override `saveAttributes` to round-trip its XML
attributes, and place an instance in the right `data/cat<N>.xml` so it appears in the editor
palette (the palettes are themselves Levels, loaded as `p_cat[0..4]`).

**Electronics** (`electronics.cpp`, `pin.cpp`, `e_*.cpp`) is a small wire-level simulation layered
on objects: parts expose input/output `Pin`s, connections are saved separately from ordinary
attributes (`saveConnections`/`loadConnections`), and `Electronics::updateAll` propagates values
each tick with an undefined state for unconnected/unsettled inputs.

**Virtual filesystem.** `FileSystem::openFile` transparently serves either a real file or a member
of a zip archive; the archive is selected by path syntax:

- `archive.zip/file.png` — no password
- `archive.zip<plaintextpw>/file.png` — plaintext password
- `archive.zip[encryptedpw]/file.png` — password encrypted with `PWEncrypt` (see
  `decryptPassword` in `util.cpp`)

`pushCurrentDir`/`popCurrentDir` maintain a search root, which is how `main.cpp` mounts
`data.zip[...]` as the asset root (the commented-out `fs.pushCurrentDir("data")` next to it
switches to loose files for development). User-writable state — saves, progress, custom levels,
screenshots, videos — lives under `getAppHomeDirectory()` = `My Documents\Blocks 5\`, never next
to the executable.

**Images** are decoded by `img_load.cpp`, not SDL_image. The game needs exactly one
function from it — `IMG_Load_RW`, called from `texture.cpp` and for the window icon — and
reads every texture through its own `SDL_RWops` over the encrypted `data.zip`. stb_image
(`libs/stb`, one header) does that in about eighty lines and drops `sdl_image.dll`,
`libpng15-15.dll` and `zlib1.dll` from the shipped tree; both builds compile the same file.
PNG and JPEG are enabled, and every image the game ships is a PNG.

**Deployment.** All three projects link the CRT statically (`/MT`, `/MTd` for Debug), so
nothing needs a Visual C++ redistributable — the installer has no runtime task at all any
more. Exactly one DLL ships beside the executables, `OpenAL32.dll`, and the only CRT it
imports is `msvcrt.dll`, which is part of Windows — not a versioned `MSVCR*`/`VCRUNTIME*`
that would need a redistributable. Its other imports are all core Windows: `KERNEL32`,
`USER32`, `SHELL32`, `ole32`, `WINMM` and `AVRT`. Keep it that way: a new dependency that
needs a redistributable, or a second DLL, undoes the whole arrangement.

**GUI** (`gui.cpp`, `gui_*.cpp`) is a retained-mode tree loaded from XML dialogs in `data/`
(`menu.xml`, `leveleditor.xml`, `options.xml`, …). Elements are addressed by dotted path —
`gui["Menu.DonatePane.Donate.Donate"]` — and wired with `sigslot` (`connectClicked(this,
&GS_Menu::handleClick)`); a game state that connects signals must derive from
`sigslot::has_slots<>`, which `GameState` already does.

**Localization.** Any user-facing string starting with `$` is an ID resolved against
`data/languages.txt` by `Engine::localizeString` / the free `loadString` helper. In that file a
`$ID` line is followed by per-language bodies tagged `§en:`, `§de:`, `§fr:`, `§es:` — that
prefix is the section sign, 0xA7 in Latin-1, not the pilcrow. A separate character, `¶`
(0xB6), inserts a newline inside a body. Missing translations fall back to English. Level titles,
tooltips and menu captions in XML all use these IDs.

**Level file format.** A level is XML: `<Level>` attributes for size, skins, weather, light
color, diamonds needed and music; one `<Layer>` per tile layer containing `<Row>` strings where
each character's raw code is the tile ID (space = 0); then a flat list of
`<Object type="…" x="…" y="…" …/>`. Campaigns (`campaign.cpp`) are an ordered list of level
filenames, shipped zipped in `levels/campaigns/`.

## Conventions

- Every `src/*.cpp` uses the precompiled header: `#include "pch.h"` must be the first line
  (`pch.cpp` is the Create-PCH translation unit). `pch.h` already pulls in SDL, OpenGL, GLU,
  OpenAL, libvorbis, TinyXML, sigslot, MersenneTwister, `img_load.h` and the core helpers
  (`singleton.h`, `vec.h`, `typedefs.h`, `util.h`, `manager.h`), so don't re-include those.
- There is no glob-based build: a new source file must be added to `Blocks5/Blocks5.vcxproj`
  **and** `Blocks5.vcxproj.filters`.
- Naming: `p_` prefixes a pointer, `pp_` a pointer-to-pointer; classes are `PascalCase`, methods
  `camelCase`, enum constants `PREFIX_UPPER` (`OF_*`, `SKIN_*`, `FM_*`).
- Comments are in German and files are **ISO-8859-1 / CP1252**, not UTF-8. Preserve the existing
  encoding when editing — don't let a tool rewrite a file as UTF-8, and don't "fix" the mojibake
  umlauts, they are correct in the original encoding.
- Source files use LF — except vendored third-party ones, which keep whatever they shipped with
  (`src/stackwalker.*` is CRLF). Shipped text files (`readme.txt`, `levels/readme.txt`,
  `data/languages.txt`) are deliberately CRLF.
- Log with `printfLog(...)` from `util.h`, not `printf`/`std::cout`. `BEGIN_PROFILE`/`END_PROFILE`
  macros are available for timing a block.
- Third-party libraries are vendored under `Blocks5/libs`, each with a `PROVENANCE.txt`
  giving its upstream, licence, which files are compiled, and what was changed locally. All
  that is left in `Blocks5/libs/bin` is `OpenAL32.lib`, an import library — no compiled code
  without source anywhere in the tree, and nothing that pins the toolset.

### Every local change to a vendored library

Five libraries are patched, in eight files. Everything else is byte-identical to upstream.
Each is explained where it lives — in the file itself and in that library's `PROVENANCE.txt`.

| library | file | what |
| --- | --- | --- |
| SDL 1.2.15 | `src/main/win32/SDL_win32_main.c` | `#undef UNICODE`/`#undef _UNICODE`; inert under MultiByte, kept as a guard |
| SDL 1.2.15 | `include/SDL_syswm.h` | brackets `#include <windows.h>` out of `#pragma pack(push,4)`, or every `C_ASSERT` in a modern `winnt.h` fails |
| zlib 1.3.1 | `contrib/minizip/unzip.c` | `NOUNCRYPT` commented out — without it nothing in the password-protected `data.zip` can be read |
| zlib 1.3.1 | `contrib/minizip/iowin32.c` | `IOWIN32_USING_WINRT_API` commented out; this is a desktop build |
| shine | `l3mdct.c`, `l3subband.c` | `__attribute__((unused))` guarded for MSVC as well as Borland |
| minimp4 | `minimp4.h` | the `esds` descriptor: real `objectTypeIndication`, optional DSI, reserved bit, `SLConfigDescriptor`, measured bitrate |
| xBR-lv2 | `xbr_lv2.h` | a port, not a patch: same algorithm, rewritten to compile as GLSL 1.10 *and* GLSL ES 1.00 (no `mat4x3`, 7 varyings fewer, const globals) |

`libogg` and `libvorbis` differ from their git tags only in expanded SVN `$Id$` keywords in
five headers, which is what marks them as coming from the release tarballs rather than a
checkout — not a local change. `minih264e_impl.c` and `minimp4_impl.c` are ours by design:
they are the single translation units that instantiate those two headers.

**Re-checking after a library update.** `raw.githubusercontent.com` is reachable from the
build environment, so every vendored file can be fetched at its upstream tag and compared.
Doing that across the whole tree gives 166/169 SDL files identical (plus `SDL_config.h`,
which is upstream's `SDL_config.h.default` verbatim), 35/37 zlib, 68/71 libvorbis, 20/22
shine, and stb, OpenAL Soft's headers and `minih264e.h` clean. TinyXML has no upstream git
repository — only the SourceForge tarball, and the GitHub forks that fill the gap carry
patches this tree deliberately does not — and sigslot and MersenneTwister have no reachable
upstream at all, so those three are documented from the tree's own history instead.
