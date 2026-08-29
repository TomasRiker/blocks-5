# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Blocks 5 — "Bob's Amazing Adventures", a 2D tile-based puzzle/action game. C++ on SDL 1.2 +
OpenGL + OpenAL Soft, **Windows/Win32 only**. Non-Windows code paths are literal
`#error NOT IMPLEMENTED` (see `filesystem.cpp`, `main.cpp`), so the tree cannot be compiled or
run on Linux/macOS — code changes here are edit-and-review only unless you are on Windows with
Visual Studio. There is also an Emscripten port in `WebBuild/`, which does build and run on
Linux and is the only way to test a change here without Windows; see `WebBuild/README.md`.

The Windows build is verified on **v143** (Visual Studio 2022 Build Tools): it compiles, links,
runs windowed, and hq2x works.

## Build & run

`Build.bat` at the repo root does the whole thing from a fresh clone — it finds MSBuild,
checks the toolset, builds `Blocks5.sln` for `Win32`, and then packs `data.zip` and
`levels/skins/*.zip`, which are gitignored build products the game cannot start without.
`Build.bat /?` lists its options.

**Toolset: v143 by default; v120 still works** (`/toolset:v120`). The tree was pinned to v120
for a decade by `libs/bin/tinyxml_STL.lib`, which carried `/FAILIFMISMATCH:"_MSC_VER=1800"`;
TinyXML 2.6.2 is now compiled from vendored source in `libs/tinyxml-2.6.2` and that library is
gone. Two things make anything newer than v120 work, and both are in the project defines
because they must precede every include:

- `_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS` — `src/pch.h` includes `<hash_map>`, and from
  VS2015 on that is a hard `static_assert` error, not a warning.
- `__STDC_CONSTANT_MACROS` / `__STDC_LIMIT_MACROS` — `libs/msinttypes-r26` shadows the real
  `<stdint.h>`; from VS2015 on the STL pulls that shim in via `<vector>`, long before
  `pch.h` could define the macro, and the shim's include guard then locks `INT64_C` out.

- SDL 1.2.15's own `src/main/win32/SDL_win32_main.c` is compiled from source in
  `libs/SDL-1.2.15/` instead of linking `libs/bin/sdlmain.lib`, which was pre-UCRT and
  imported `__iob_func`. It carries one local change, an `#undef UNICODE`/`#undef _UNICODE`
  prologue: the projects are `CharacterSet=Unicode` but that file uses `char` buffers with
  `TEXT()` literals and `GetCommandLine()`, which the compiler only warns about (C4133) while
  the command line would collapse to one character at runtime.

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
correct) because it opens `data.zip` relative to the cwd. There are **no tests and no linter**.

Command line / launcher scripts: `-windowed` (`windowed.bat`), `-fullscreen`, `-hq2x`
(`hq2x.bat`). Debug builds default to windowed + Console subsystem and skip the SEH crash
handler; Release defaults to fullscreen + Windows subsystem and dumps a stack trace via
`StackWalker` on an exception.

Installer: `setup\Blocks 5.iss` (Inno Setup). Version number lives in three places that must
stay in sync — `p_localVersion` in `src/main.cpp`, `AppVersion`/`OutputBaseFilename` in the
`.iss`, and `readme.txt`.

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

**Engine** (`engine.cpp`, ~52k) owns the main loop, the window, OpenAL, config, localization,
screenshots and video capture. The loop renders as fast as it can but steps logic at a fixed
`logicRate` of 20 ms (`setLogicRate(20)` in `Engine::init`); one `update()` call is one logic
tick, so gameplay code counts ticks rather than measuring dt.

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
  (`pch.cpp` is the Create-PCH translation unit). `pch.h` already pulls in SDL, OpenGL, OpenAL,
  TinyXML, sigslot, MersenneTwister, ffmpeg and the core helpers, so don't re-include those.
- There is no glob-based build: a new source file must be added to `Blocks5/Blocks5.vcxproj`
  **and** `Blocks5.vcxproj.filters`.
- Naming: `p_` prefixes a pointer, `pp_` a pointer-to-pointer; classes are `PascalCase`, methods
  `camelCase`, enum constants `PREFIX_UPPER` (`OF_*`, `SKIN_*`, `FM_*`).
- Comments are in German and files are **ISO-8859-1 / CP1252**, not UTF-8. Preserve the existing
  encoding when editing — don't let a tool rewrite a file as UTF-8, and don't "fix" the mojibake
  umlauts, they are correct in the original encoding.
- Source files use LF; shipped text files (`readme.txt`, `levels/readme.txt`, `data/languages.txt`)
  are deliberately CRLF.
- Log with `printfLog(...)` from `util.h`, not `printf`/`std::cout`. `BEGIN_PROFILE`/`END_PROFILE`
  macros are available for timing a block.
- Third-party libraries are vendored under `Blocks5/libs`. Most are compiled from source by
  both builds (TinyXML, zlib + minizip, libogg, libvorbis, `SDL_win32_main.c`, stb). What is
  left in `Blocks5/libs/bin` is import libraries — SDL, SDL_image, OpenAL, ffmpeg — plus
  `hq2x32.obj`, which is a real object file and the one thing there that still carries v120
  code. Import libraries do not pin the toolset; `hq2x32.obj` and the ffmpeg DLLs do.
