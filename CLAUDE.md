# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Blocks 5 — "Bob's Amazing Adventures", a 2D tile-based puzzle/action game. C++ on SDL 1.2 +
OpenGL + OpenAL Soft. **Three builds, all from the same sources**: Windows/Win32
(`Build.bat`, needs Visual Studio), a native Linux build (`LinuxBuild/build.sh`), and an
Emscripten port in `WebBuild/`. The last two build and run here, so a change can be compiled,
run and driven without Windows — see `LinuxBuild/README.md` and `WebBuild/README.md`.

**The Windows build compiles and links on v143 and on v145** (Windows 11, VS 2022
Community). Three compile errors had to be fixed to get there, all in vendored libraries and
each written up in the relevant `libs/*/PROVENANCE.txt`: shine's `__attribute__((unused))`,
which MSVC rejects; `misc.c` in the libvorbis file lists, which is a pthreads debug allocator
upstream never compiles; and `windows.h` included inside SDL's `#pragma pack(push,4)`, which
makes every `C_ASSERT` in a modern `winnt.h` fail.

**The fourth fix is a rule, not a one-off: this project must be built as MultiByte, never as
Unicode.** SDL 1.2 is an ANSI codebase — `char*` throughout, calling `RegisterClass`,
`LoadLibrary`, `GetLocaleInfo` and the rest unsuffixed. It was a DLL before, built ANSI by
SDL's own project, so `CharacterSet` here never mattered; now that its 67 sources compile
*inside* `Blocks5.vcxproj`, `Unicode` resolves all of those to the `...W` variants and MSVC
merely warns (C4133). The first such build died in `SDL_RegisterApp` with an access
violation: `GetCodePage` passes `char buff[8]` and `sizeof(buff)` to `GetLocaleInfo`, whose
last parameter counts *characters*, so `GetLocaleInfoW` wrote 16 bytes into 8 and smashed the
stack frame — and forty other C4133 warnings across SDL were the same bug waiting to happen.
The game's own code never depended on Unicode: it calls `MessageBoxA`, `ShellExecuteA` and
friends explicitly and contains no `TCHAR`, `TEXT()` or `wchar_t` outside vendored
`stackwalker.cpp`. `SDL_win32_main.c` keeps its `#undef UNICODE` prologue as a guard.

## Build & run

`Build.bat` at the repo root does the whole thing from a fresh clone — it finds MSBuild,
checks the toolset, builds `Blocks5.sln` for `Win32`, and then packs `data.zip` and
`levels/skins/*.zip`, which are gitignored build products the game cannot start without.
`Build.bat /?` lists its options.

**Toolset: whichever one the installed Visual Studio calls its newest.** The three
`.vcxproj` files set `<PlatformToolset>$(DefaultPlatformToolset)</PlatformToolset>`, so a
Visual Studio newer than anything in this tree needs no change to build with it, and
`Build.bat` passes no `/p:PlatformToolset` unless `/toolset:vNNN` asks for one — a global
property could not be overridden from inside the project, so passing one always would be
hardcoding a version again. `WindowsTargetPlatformVersion` follows the same rule: the
projects set it to `10.0` (newest installed 10.x) for anything past v140, which is what
`Build.bat` used to have to supply.

**Tested with v143 and v145, and with nothing else.** That v120 and v140 still build was
reasoning about the code, never a compiler run — the plumbing for them is still there
(`/toolset:v120` skips the SDK property), but it is untried.

**SDL itself is compiled from source**, all 67 files of the Win32 subset out of
`libs/SDL-1.2.15/src` — the same set SDL's own `VisualC/SDL/SDL.vcproj` builds. It needs one
include directory, `winmm.lib` and `dxguid.lib` from the Windows SDK, and `DECLSPEC=` among
the defines (`begin_code.h` guards it with `#ifndef` and would otherwise mark every entry
point `__declspec(dllexport)`, which is wrong for a static build). Two files carry local
changes; see the patch table below.

To build by hand instead: open `Blocks5.sln` in Visual Studio (only `Debug|Win32` and
`Release|Win32` exist) and build all three projects. Then, from the `Blocks5` directory:

```bat
zip_data.bat     :: pack data\ into the encrypted data.zip the game reads at runtime
zip_skins.bat    :: pack levels\skins\<name>\ into levels\skins\<name>.zip
stage.bat        :: build a redistributable tree in Blocks5\stage (needs ..\Release\*.exe)
```

`zip_*.bat` run `tools\optipng` first, which is slow; `zip_data_no_optipng.bat` and
`zip_skins_no_optipng.bat` skip that step. Both require `tools\7za.exe`.

**`Blocks5/pack.sh` is all four of those in one, without Windows** — `zip -9 -P` in place of
`7za a -tzip -mx=9 -p` (both write traditional ZipCrypto, which is what minizip reads) and the
distribution's `optipng` in place of `tools\optipng`. `./pack.sh` does everything, `data` or
`skins` narrows it, `--no-optipng` skips the slow step. It is what a Linux-only checkout needs:
`data.zip` and the skin archives are build products that are not in Git, and the game will not
start without them.

The game must run with `Blocks5\` as its working directory (VS's default `$(ProjectDir)` is
correct) because it opens `data.zip` relative to the cwd. `Build.bat /run` builds and then
does that for you; it has to come last, because every argument after it goes to `blocks5.exe`
untouched (`Build.bat Debug /rebuild /run -windowed`). There is no unit-test suite, but
there are checks that run in seconds and a way to drive the real game — see
**Checking a change** below.

Command line / launcher scripts: `-windowed` (`windowed.bat`), `-fullscreen` and
`-nosplash` — that is the whole list, and `readme.txt` documents all three. `-nosplash`
skips the logo and the jingle by *not requesting* `logo.png`, which is the path
`GS_Loading` already takes when the texture will not load; only `soundPlayed` has to start
`true`, because the jingle hangs off the time threshold rather than off the logo. The
upscaling filter is *not* a switch; it is an in-game option like the language, saved as
`<Upscaler>` in `config.xml`. Debug builds default to windowed + Console
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

## Checking a change

Four things run here, none of them needing Windows. Run at least the first two after any
edit; they take about half a minute together.

```
python3 Tools/verify.py      thirteen static checks over the whole tree
sh Tools/syntax.sh           compile every source with mingw (-fsyntax-only)
LinuxBuild/build.sh          the native build compiles and links with GCC
cd WebBuild && ./build.sh    the browser port actually builds and links
```

There are three ways to *run* it, all scripted: `LinuxBuild/test/smoke.sh` natively,
`WebBuild/test/smoke.js` in a desktop browser, and `WebBuild/test/mobile.js` in an emulated
phone — see **Driving the game** below.

**A check that can pass on a previous run's artifact is worse than no check.**
`WebBuild/build.sh` used to pipe `em++` through `tail`, so the status it tested was `tail`'s,
and the check after it only asked whether `blocks5.wasm` existed — which it did, from the run
before. Worse than merely stale: `em++` writes `blocks5.data` *before* `wasm-ld` runs and the
table of byte offsets into it lives in `blocks5.js`, so a failed link leaves a fresh data
bundle beside the previous run's offsets and every preloaded file is sliced in the wrong
place. That is what ROADMAP item 20 spent a day mistaking for a corrupt `data.zip`. It reads
`${PIPESTATUS[0]}` now and exits 1.

The Linux build is the fastest way to *run* a change: `LinuxBuild/test/smoke.sh` starts it
under Xvfb and clicks through the menus, and unlike the browser it is a real GCC compile of
every source, `videorecorder.cpp` included. What it cannot check is anything Windows-only —
the SEH crash handler, the Win32 window procedure, `audiocapture.cpp`'s WASAPI half — and
those are exactly what `Tools/syntax.sh` is for.

**`Tools/verify.py`** looks for the kind of mistake that leaves no trace in a diff and that
no compiler can see: a `gui["…"]` path no dialog XML knows, a `$ID` missing from
`languages.txt`, an XML attribute written and never read, a source file missing from
`Blocks5.vcxproj` or its `.filters`, a class whose header is not named after it, the version
number drifting apart across the four places it lives, a new member the constructor never sets, an asset filename that is not on
disk or spelled with different case (which only Linux minds), a non-ASCII byte or a CRLF in a source file, `if (` where the tree writes `if(`, an
English comment among the German ones. Exit code 1 on any finding; `--list` names them,
`--only NAME` runs one. `Tools/README.md` has the table.

The attribute check exists because renaming the constant `numLayers` to `NUM_LAYERS` once
took the XML attribute string with it, which silently disabled the level size guard for
every editor-saved level. That is the shape of bug this file is for.

**The Windows program icon is checked for the same reason.** `Blocks5/src/icon1.ico` is
committed, not generated — the Windows build runs no Python and should not start — so it sits
there unchanged when the art moves, and it had. The `windows_icon` check compares its 16x16
image against `data/window.png` and insists on the sizes the shell asks for;
`Tools/make_ico.py` rebuilds it.

**Three checks judge only what changed since `95660bb`**, the last commit before the
2025 overhaul, whose id is `BASELINE` at the top of `verify.py`: indentation and
whitespace, uninitialised members, and comment density. Code that has been there for ten
years and works is not a finding, and reporting it on every run is how a check gets
ignored.

**`Tools/selftest.py`** injects each fault in turn and confirms the matching check fires,
then restores the file byte-for-byte. Run it after touching `verify.py`. It is not
ceremony: the attribute check was inert when first written, because `Attribute(` also
matches the tail of `SetAttribute(` and so every written attribute counted as read — the
one check aimed at the bug above would have found nothing.

**`sh Tools/syntax.sh`** compiles all 118 sources with `i686-w64-mingw32-g++
-fsyntax-only`. It is the only way to put a compiler over the Windows code from here. Three
files can never go through it — `main.cpp`, `videorecorder.cpp`, `stackwalker.cpp` — for
the same reasons they are left out of the web build. It needs nothing checked in: the
handful of case-aliasing headers the tree includes (`<Windows.h>`, `<Shellapi.h>`,
`<al.h>`) are generated into a temp directory. It passes `-w`; for a warning sweep, swap
that for `-Wall -Wextra` and compare against the same sweep before your change, because the
tree emits thousands of warnings that were all there in 2015.

### Driving the game natively

`LinuxBuild/test/smoke.sh` runs the built game under Xvfb with openbox, clicks through menu,
options and manager, toggles fullscreen, takes a screenshot with F11 and quits with Escape.
It clicks by element name, not by coordinate: `Blocks5/src/testhooks.cpp` — the same hook the
browser uses — reports the GUI tree, and since there is no JavaScript here to call it, the
request goes through a file (`$B5_TEST_DIR/request`, answered once per logic tick). That is
what catches the case a screenshot cannot: on a first start `Menu.CrtPane` covers everything,
so a click on the middle of `Menu.Options` lands on the pane.

**The two input layers want opposite treatment, and this is the trap that costs the most
time.** A key the GUI reads is an SDL *event* and must be tapped, not held:
`SDL_EnableKeyRepeat(140, 60)` turns an Escape held for 400 ms into six of them, the first
closing the dialog and the second quitting the game. A key bound to a named *action* must be
held, not tapped, because `Engine::updateVKs` reads `SDL_GetKeyState`, a snapshot taken once
per 20 ms tick, so a press and release in the same millisecond is never seen. The same
sampling rule governs the mouse and the touchscreen, which is why `page.mouse.click()` and
`page.touchscreen.tap()` are equally useless: move, settle, hold, release. Alt+Return
misleads, because it hangs off `SDL_KEYDOWN` and events queue.

`xdotool windowclose` calls `XDestroyWindow` and SDL then trips over a window it still
believes is its own. Quit the way a player does — Escape in the menu — or `Engine::exit()`
never runs and `config.xml` is never written.

### Driving the game in a browser

`WebBuild/build.sh hooks` builds to `build-test/` with `-DBLOCKS5_TEST_HOOKS`, which turns
on `WebBuild/test_hooks.cpp`. The shipped build has none of it — the whole translation unit
is inside the `#ifdef`, and `blocks5_testDump` does not appear in `build/blocks5.js`.

The hook only reads. It puts the GUI tree into `Module["b5_test"]` as JSON — every element
with its window rectangle, whether it is visible and enabled, plus the game state, language
and filter — and `blocks5_testHitAt(x, y)` says which element a click on a point would
reach. `WebBuild/test/harness.js` turns that into `clickPath(page, 'Menu.Options')`, and
the click itself stays an ordinary mouse click travelling the whole way through SDL, Engine
and GUI. `WebBuild/test/smoke.js` walks menu, options and manager that way.

Do not go back to reading coordinates off a screenshot. That is what this replaces, and it
is wrong often enough to waste an afternoon: the buttons are eighteen pixels high, the
window is scaled, and a pane drawn on top looks like a missed click.

Four things about this environment, each of which cost real time:

- **A wasm trap looks like a hang, not an exception.** `page.evaluate` never settles and
  there is no error anywhere. `computePresentRect` divided by a zero `screenSize` before
  `main()` had run, and the NaN-to-int cast trapped; `GUI_Element::getFullName()` walked
  past a null parent and read on through memory. Guard anything the hook calls before the
  engine is up, and bisect a hang by adding stages rather than by staring at it.
- **`Module.calledRun` never appears** on the module object in this Emscripten. Wait for
  the page-level `runtimeInitialized` *and* a dump that comes back with a game state and a
  non-empty element list; the runtime is up well before `main()` has built anything.
- **Under swiftshader the game needs about half a minute to reach the menu**, and a frame
  takes a fifth of a second. Never sleep a guessed interval; wait on the reported state.
- **`GS_Loading` waits for a real gesture** before it goes anywhere, because the AudioContext
  is suspended until one arrives — so a test must click *before* waiting for `GS_Menu`, not
  after, or it waits forever.

### Driving the game on a phone

`WebBuild/test/mobile.js` is the same idea in Chromium's mobile emulation, and it loads
`index.html` rather than `blocks5.html` — that is the file that ships and the only one that
registers the service worker. It checks the page around the game: that the layout viewport is
the device width and not the ~980px default, that nothing scrolls or zooms, that the canvas
covers the viewport, that the manifest says what an install needs, that the worker's cache
holds the payload, and that a reload with the network switched off still boots. **`isMobile:
true` in the context is what makes any of it mean something** — without it Chromium lays out
at the window width and the viewport meta has nothing to do.

The one check that is about the game rather than the page is a real touch: `touchStart`, a
wait, `touchEnd`, through `Input.dispatchTouchEvent` over CDP. That is what found the
click-ordering bug in `GUI::update()`; the dump reports `cursor` and `mouseDown` so that a tap
that does not arrive can be told apart from a button that does not react.

**The dump also lists `actionsDown`**, the only window onto the *action* layer from outside:
`Engine::updateVKs` reads `SDL_GetKeyState` and not `keyData`, so whether a key reached the
named actions cannot be inferred from anything else the hook reports. It is what established
that an on-screen pad can drive the game with an ordinary DOM `keydown`/`keyup` on the
document — a synthetic `ArrowLeft` with `isTrusted === false` shows up as `["$A_LEFT"]` and
clears on `keyup`, where `Engine::setKeyData` would not have worked at all (ROADMAP item 19).

## Architecture

Everything for the game lives flat in `Blocks5/src`. The layering is by naming prefix, not by
directory: `gs_*` = game states, `gui_*` = widgets, `cf_*` = crossfade effects, `u_*` =
upscaling filters, `e_*` =
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
puts that on the screen as one letterboxed quad. Every hardcoded coordinate in the tree —
the single `glViewport`, the `glScissor` calls, the GUI layouts — therefore stays valid
whatever size the window is; only `computePresentRect` changes, and the cursor mapping in
`getCursorPosition`/`setCursorPosition` is its exact inverse. Video capture and screenshots
read `GL_COLOR_ATTACHMENT0` at 640x480 and never see the window size at all.
`glextensions.cpp` loads what the FBO needs: ten FBO entry points and twenty-five GL 2.0
ones, `glGenFramebuffersEXT` first and the core spelling as a fallback; in the browser they
are core and the header just `#define`s them through.

**Four upscale filters, and each is a class.** `upscaler.h` holds the base — a name, a
texture filter, `present()`, whether it wants a whole-number scale, whether it distorts the
cursor, and its own `loadConfig`/`saveConfig` — plus `PresentContext` (everything the Engine
owns and lends out: the rect, the frame texture, the shared vertex buffer) and
`PresentProgram` (a linked program and the four uniforms *every* present shader has). The
four live in `u_sharp.*`, `u_smooth.*`, `u_sharpfit.*` and `u_crt.*`, `u_all.h` pulls them
in, and `Engine` owns one of each in display order. It is a normal game option like the
language, saved as `<Upscaler>` with the filter's own `getName()` — the one name each filter
has, shared by the config value, the radio button in `options.xml`, the startup log and the
test hook. `SharpFit` is the default where the machine can run it:

- `Sharp` and `Smooth` are just `GL_TEXTURE_MAG_FILTER`, drawn by the base class's
  fixed-function quad. `Sharp` additionally snaps the blit to an integer scale
  (`wantsIntegerScale()`), which is the whole point of choosing it.
- `SharpFit` (`src/u_sharpfit.cpp`) is nearest at a fractional scale: conceptually the
  frame is nearest-upscaled by the smallest integer that covers the destination and then
  resampled down. That is one texture fetch, not two passes — bilinear over a
  nearest-upscaled image is piecewise linear, so remapping the texture coordinate through
  the same piecewise function and letting the hardware interpolate gives the identical
  result. Verified against a real two-pass: pixel-identical at an integer scale, max channel
  difference 1 (8-bit rounding in the intermediate) at fractional ones. It **must** sample
  with `GL_LINEAR` — the hardware interpolation *is* the filter.
- `Crt` (`src/u_crt.cpp`) is a CRT monitor: beam profile, scan lines, phosphor mask,
  halation, barrel distortion, rounded corners, vignette. See **The CRT filter** below.

The two shader filters share the vertex shader (`upscaler.cpp`, the only place it is read),
the vertex buffer and the four uniforms in `PresentProgram`; `U_Crt` holds its own eight on
top. **There is no longer a place where a filter carries a uniform it does not have** — which
is what the old twelve-slot struct did, and why `convergence` was once left unset in two
hand-written lists. Each filter compiles on its own, so a CRT that fails to link leaves
sharp-fit alone — and, unlike before, a sharp-fit failure no longer takes the CRT with it.

**Anything that reads the rendered frame must bind the FBO itself.** The main loop binds it
only on an iteration that ran a logic tick. Natively there is no other kind, because the
`SDL_Delay` at the foot of the loop stretches every iteration to at least one tick; in the
browser `requestAnimationFrame` sets the pace instead, so at 16.7 ms against a 20 ms tick most
iterations render nothing and the *screen* is bound, left over from the previous present. That
is what made every screen transition start from black — the crossfade's one-shot capture of
the old image is the only `glCopyTexSubImage2D` not already inside a `frameRendered` block, so
it read the default framebuffer, which WebGL clears before every frame. It calls
`bindFrameBuffer()` first now, which is right on either platform: the FBO holds the last frame
that *was* rendered, which is exactly the screen being faded out.

**The CRT filter.** Everything that gives it its character is a `const` at the top of
`src/u_crt.cpp`, meant to be edited. Six of them are runtime sliders instead
(Options → Scaling → *CRT settings …*, saved as
`<CrtUpscaler scanline= curvature= bloom= flicker= scanFlicker= convergence=>`), because they
are matters of taste rather than tuning.

**Nobody finds a filter buried in an options dialog**, so `Menu.CrtPane` offers it once on a
first start, with a button that switches it on there and then. The marker is `.crt_offered` in
the user directory, the same idiom as `.donation_asked` — absent on a clean install *and*
after an upgrade, which is exactly the set of people who have not seen the filter. Skipped
where `canUseCrt()` is false or the filter is already CRT, and it suppresses the donation
window for that one start so the two never stack.

The one that decides what it *is* is `SCANLINE_PERIOD`. Visible gaps between scan lines are
an artifact of 240p: a console drew 240 lines into a 480-line raster. A VGA monitor showing
640x480 drew all 480 with the beam profiles overlapping, and had no gaps. This game's honest
reference is the VGA monitor, which is `SCANLINE_PERIOD = 1.0` — and at a 2x window that
produces *no visible stripes at all*, because both output rows sit equally far from the row
centre. That is physically right and useless as an effect, which is why the shipped default
is `2.0`: pretend 240 lines arrive, and get the look people mean by "CRT". The slider fades
that in; the constant decides which look it fades into.

**Convergence** is the sixth slider, at 0.5 like the rest. A colour tube has three beams,
converged at the centre and drifting apart toward the rim where the deflection is largest, so
a vertical edge carries a red fringe on one side and a blue one on the other — nothing in the
middle, most at the edge. It is **not** chromatic aberration: that happens in a lens because
glass bends wavelengths differently, and a tube has no lens. Green stays put as the reference,
exactly as it was set on the bench. `CONVERGENCE_MAX` (1.6) is the displacement of *each* of
red and blue at the left and right edge, in source pixels, at full slider; they move apart, so
the visible fringe is twice that. Horizontal only — a vertical component would need its own
two rows and its own beam profile per channel, eight fetches instead of four, and the line
structure hides it anyway.

**The raster edge belongs to the beam, not to the picture.** A tube paints three rasters, and
if red's is narrower than green's then the red *image* ends first — which is what you see at
the rim of a misconverged set before you see anything on an edge inside the picture. So
`rasterMask()` is evaluated at each channel's own source point, not at the output point:
without that, the highest-contrast vertical edge in the whole frame — the picture against the
black — is the one place a fringe could never appear. Measured off the shader: at the slider's
default the outermost output column keeps 89% of its red, at full slider the outermost three
keep 49%, 76% and 95%, and green and blue do not move at all.

Those four fetches are part of the shipped cost now: one present measured 25.3 ms with the
slider at 0 and 30.6 ms with it anywhere above (llvmpipe, 1280x960), a fifth more, and the
same at 0.5 as at 1.0 — the shift changes the coordinate, not the work. On a real GPU it is
noise, and `if(Convergence > 0.0)` hands the whole of it back to anyone who turns the slider
down, since the condition is uniform across the draw.

Measured by asking how far the red channel of a finished frame lags the blue one, which needs
no second frame to compare against and so does not care that the title demo keeps moving: at
0 the lag is within a tenth of a pixel everywhere, at full slider it is −5.3 output pixels at
the left edge, −0.3 in the middle and +4.4 at the right, which is the antisymmetric ramp the
shader asks for.

That rectangle is a pixel wider than the picture on every side, which is the second half of
the same change: **at curvature 0 the CRT filter now covers exactly what the other three
filters cover.** The soft edge used to eat into the frame, leaving the outermost column at 64%
and the next at 88% for no reason anybody had asked for — the fade is there for the barrel
distortion, which at curvature 0 does not exist. Measured against sharp-fit on the same scene:
zero shift in either axis, and the outermost three columns within 0.5% of the columns inside.

The mask sits in **output** pixels (`gl_FragCoord`, `MASK_PITCH`), not source pixels — a real
shadow mask belongs to the glass and does not change when you switch resolution. That matters
because almost everyone runs at exactly 2x (`getDefaultWindowSize` gives 2x on 1080p *and*
1440p), where three source-locked subpixels are impossible.

Brightness is **derived, not tuned**: `MASK_AVG` and `scanAvg` are computed from the
constants, so mask and scan lines are light-neutral by construction and editing any constant
needs no compensating edit elsewhere. Measured, the scan-line slider moves mean frame
brightness by 0.5% end to end.

The flicker is the one part that reads the clock, and it has three terms — all zero-mean, so
none costs brightness, and all functions of `Time` alone, never of the previous frame, which
is why none can turn into the xBR problem. A fast brightness shimmer at roughly 12, 19 and
29 Hz (the part people mean by *flimmern*), a much weaker mains-hum bar rolling slowly down
the picture, and the scan lines crawling downward. The first two are one slider (`Flicker`)
and the third its own (`ScanFlicker`), because an unsteady brightness and a drifting line
structure are separate tastes. Frequencies are whole cycles per `FLICKER_CYCLE` (8 s) and
`presentFrame` feeds `SDL_GetTicks()` modulo that, so the clock wraps seamlessly. It is the
wall clock and not `Engine::getTime()`, which counts logic ticks and stops when the game
pauses — a screen flickers anyway. At maximum the depths give 1.7% peak-to-peak between
frames.

The crawl is the one term computed on the **CPU**, as the `ScanPhase` uniform. It is a ramp
rather than an oscillation and its slope depends on the slider, so feeding it the
already-wrapped `Time` would jump the scan lines by `fract(flicker · speed)` of a period at
every wrap; `fmod(seconds · CRT_CRAWL_SPEED · crtFlicker, 1.0)` off the unwrapped clock is
continuous instead.

**Halation averages in linear light, per tap.** Averaging the taps in gamma space and
linearising the result produces almost no visible halo: the ring around a bright spot is a
mixture of bright and dark, and `pow()` on that mixture falls far below the threshold.
Linearising each tap (with `x*x` — for a soft halo indistinguishable from gamma 2.4, and a
multiply instead of a `pow`) and thresholding the linear average gives a real glow, +23 grey
levels at the centre falling to +4 at 70 output pixels. The taps sit on **two** rings, four
axial and four diagonal; eight on one radius makes a hard-edged ring rather than a glow.

Relative present cost on a software rasterizer, so read it as ratios: nearest 1.0, bilinear
1.3, sharp-fit 1.35, **crt 7.8** — halation about half of that, and `BLOOM_STRENGTH = 0`
compiles the whole block away (4.2). On real hardware all of them are noise, but the browser
build can land on a software path.

**The barrel distortion goes through the mouse as well.** The shader maps output pixel to
source pixel, which is the same direction `getCursorPosition` needs, so it uses the identical
formula — `U_Crt::warpToSource`, which `Engine::warpToSource` forwards to (the base class
returns what it was given, so no caller asks what kind of filter is on).
`setCursorPosition` needs the inverse, and the coupled pair
(`x` depends on `y²`, `y` on `x²`) has no closed form, so `warpToOutput` runs a fixed-point
iteration: `x <- u/(1+a·y²)`, `y <- v/(1+b·x²)`. Measured: eight rounds land within 2.3e-4
pixels even at an absurd curvature, and within 1e-5 at anything reachable from the slider.
`CRT_CURVE_X`/`CRT_CURVE_Y` are `#define`d once and stringified into the GLSL *and* read as
C++ doubles, so the two cannot drift apart.

Both cursor functions map **pixel centres** and `floor`, not left edges and truncation, so the
round trip `get(set(g)) == g` is exact at every scale and curvature — except at exactly 1x,
where 640 window pixels and 640 game pixels cannot both hold a non-identity warp and 0.5% of
positions land on a neighbouring tile. That is the minimum window size, where the effect has
no room to work anyway.

Without an FBO the game renders straight to the back buffer as before; without a shader,
`isAvailable()` is false for that filter, `getEffectiveUpscaler` falls back to `Sharp` — a
fixed fallback, not "the first available one", because the display order starts with
`SharpFit` and belongs to the options dialog — and the dialog hides the entry. The wish
itself stays in `config.xml` untouched, for the next machine. Neither is fatal.

**xBR-lv2 was here and is gone**, together with hq2x before it, and the reasoning is worth
keeping: both are edge-directed filters written for flat-shaded pixel art, and this game's art
is airbrushed and photographic. Every decision in xBR is a `step()` against a threshold, which
is stable when neighbouring texels are either identical or plainly different and is not when
they sit near it. Nudging a frame by 0–3 of 255 — about what the animated level does behind a
semi-transparent dialog — moved 1% of xBR's output pixels by up to 154, all on glyph outlines,
while nearest, bilinear and sharp-fit moved by exactly what the input moved. That was visible
as text flickering. A CRT-style effect is the answer for a nostalgic filter instead; see
ROADMAP items 2 and 11.

**The window.** Resizable, aspect kept, black bars. **SDL's video flags are
`SDL_OPENGL | SDL_RESIZABLE` for the whole life of the process and must stay that way** —
`DIB_SetVideoMode` keeps the GL context only on its fast path, which requires the flags and
bpp to be unchanged and `SDL_FULLSCREEN` to be clear. Setting `SDL_FULLSCREEN` or
`SDL_NOFRAME` runs `WIN_GL_ShutDown` instead and takes every texture, display list and the
FBO with it. So fullscreen is *not* an SDL flag here: `applyWindowStyle` sets the Win32
style to `WS_POPUP` and the size to the desktop directly, SDL notices through its own
`WM_WINDOWPOSCHANGED` and posts an ordinary `SDL_VIDEORESIZE`, and `handleResize` — the one
place that owns `displaySize` — picks it up. Dragging the border and Alt+Return therefore
run the same code, and nothing is ever destroyed. Alt+Return is swallowed so the game never
sees a bare Return.

**A window that stops presenting loses control of what it shows.** While the app is inactive
the main loop skips both the logic and the rendering, but it must still put the last frame up
— `showLastFrame()` does that every 50 ms (unbind, `presentFrame`, swap; `renderAndPresent`
is the same plus a render). A bare `SDL_GL_SwapBuffers` without drawing is not enough: it
flips to the other buffer and shows the frame before the last one. And a full-screen popup is
exactly the shape Windows may hand a direct scanout path, after which the compositor's own
copy of the window stops being updated — with the Start menu open over one, the game showed a
frame from seconds earlier. Re-presenting keeps a fresh copy there. Without an FBO there is
nothing to repeat, so that case keeps the bare swap.

**Drawing while the border is dragged** needs one thing SDL cannot give: while the user holds
the border or the title bar, `DefWindowProc` runs *its own* modal message loop and the main
loop sits in `SDL_PollEvent` until the mouse comes up. The only code that still runs is the
window procedure, so `Engine::hookWindowProc` puts one in front of SDL's with
`SetWindowLongPtr(GWLP_WNDPROC)` — the same subclassing SDL itself does for `SDL_WINDOWID`,
and safe because the HWND is created once in `DIB_VideoInit` and no later `SDL_SetVideoMode`
replaces it. `WM_ENTERSIZEMOVE` starts a 15 ms timer; `WM_SIZE` (every drag step) and
`WM_TIMER` (when the user holds still, where no `WM_SIZE` comes) both call
`repaintDuringSizeMove`, which re-presents the framebuffer at the new client size, so the
upscaler, the letterbox and the aspect all track the drag live. No logic tick runs. Two traps
there: it **borrows** `displaySize` and must put it back, because `handleResize` early-returns
on an unchanged size and would then never call `SDL_SetVideoMode`, leaving SDL's own surface
stuck at the old size for the rest of the session; and `SDL_SetVideoMode` must *not* be called
during the drag at all, since it calls `SetWindowPos` and fights the user's mouse. The same
procedure answers `WM_GETMINMAXINFO` (chaining first, since `DefWindowProc` fills four other
fields) with 640x480 of client plus the frame from `AdjustWindowRectEx`, so the floor
`handleResize` enforces applies *during* the drag instead of snapping back after it.

**The window's placement is saved on exit, and the details matter.** One
`<Window positionX= positionY= sizeX= sizeY= maximized= fullscreen=>` is written by
`Engine::exit`; the position is the only part that can be absent, because on a first start
there is none and a 0,0 would be a claim rather than a fact. Before that the only
caller of `saveConfig` was the options dialog's OK, so resizing and quitting lost the size.
`rememberWindowPlacement` uses `GetWindowPlacement`, not `GetWindowRect`: a maximized
window's rect is the maximized frame, with negative corners because the invisible grab
handles count, and restoring *that* puts a screen-sized window half off the desktop.
`rcNormalPosition` is what "restore" goes back to and is what gets saved, with a `maximized`
flag that `restoreWindowPosition` replays. `handleResize` skips updating `windowedSize` while
`IsZoomed`, and in fullscreen the rect `applyWindowStyle` saved is used instead, so the
remembered window is always the windowed one. Whether the stored spot still exists is
`MonitorFromRect`'s job, which gets negative coordinates right — a monitor to the left of the
first one has them.

On first run, or when the stored size no longer fits, `getDefaultWindowSize` picks the largest
integer multiple of 640x480 leaving a 120px margin in *both* directions, so "sharp" starts
with no black bars. 120 is derived, not felt: it is the largest margin under which 1920x1080
still gets 2x (2*480 = 960 = 1080-120, with nothing to spare). The same value goes
horizontally, where it is pure slack, because a taskbar is not always at the bottom.
`-windowed`/`-fullscreen` set the state for that start rather than overriding it for one run,
since `Engine::exit` always saves. In the browser the canvas fills the page
(`WebBuild/pre.js`), Alt+Return goes through the Fullscreen API from a real DOM keydown — the
main loop's own events do not count as a user gesture — and the main loop reads the canvas
size once a frame.

**On a phone the game takes the fullscreen itself.** Mobile Chrome has no button for it, so
without this the page is played under an address bar, and the picture is small enough already.
`Engine::enforceTouchFullScreen` runs from a second DOM callback beside the Alt+Return one,
registered for **both** `touchstart` and `touchend` and returning `EM_FALSE` so the touch still
belongs to SDL. It requests the fullscreen on every touch that finds the document not in it,
which is what makes it survive a swipe back out.

Both ends of the touch, because the API needs a *transient user activation* and a phone does
not necessarily grant one as early as touchstart — `touchend` is the event the HTML spec names
for it. `emscripten_request_fullscreen_strategy` hid that by deferring the request to the next
handler allowed to perform it (its own allowlist for touch is exactly touchstart and touchend),
and a plain `requestFullscreen()` has no such second chance: dropping the strategy took the
fullscreen away on a real phone while headless, which grants activation at touchstart, kept
working. `b5_setFullscreen` therefore also asks `navigator.userActivation.isActive` first and
stays quiet when there is none, so the touchstart attempt costs no rejected promise. There is no extra tap to pay for it: the browser
build already stops on "click to start", because `GS_Loading` waits for the gesture that
unblocks the AudioContext, and that tap is the one that gets used.

**The fullscreen goes on the root element, never on the canvas** (`Module.b5_setFullscreen`,
not `emscripten_request_fullscreen_strategy("#canvas")`). A browser paints only the fullscreen
element and its descendants, so with the canvas promoted the on-screen pad — its sibling —
disappears the moment the game goes fullscreen. It still reports a full-size
`getBoundingClientRect` while invisible, which is why a test that measured it saw nothing
wrong. From `<html>` both are inside, and the canvas is 100%/100% of the page anyway, so
nothing has to resize it.

**That same callback resumes the AudioContext**, and not only `GS_Loading`. Going fullscreen
turns the phone to landscape, and the rotation makes the browser cancel the touch in flight —
SDL never sees the press, so `GS_Loading` does not know a gesture happened and waits for a
second tap that the player should not have to give. In the DOM callback the gesture is
unambiguous.

Two conditions guard it. `Module.b5_isPhone()` in `pre.js` is coarse-pointer **and not**
`(any-pointer: fine)` — a notebook with a touchscreen has a title bar somebody wants, a phone
has none — and it is one function rather than two copies precisely because C++ asks it too.
And the *browser* is asked whether it is fullscreen, not `Engine::fullScreen`: leaving by a
swipe does not tell the engine anything, so the member says `true` while the page is windowed
and `setFullScreen(true)` would return before reaching the API.

**The landscape lock hangs off `fullscreenchange`, not off the request.** `screen.orientation
.lock` is refused unless the document is already fullscreen, so ordering them the other way
round simply rejects; `Module.b5_lockOrientation` therefore waits for the event, and unlocks
again on the way out. It is Android-only — iPhone Safari has neither API — and it rejects on
a desktop, so every path swallows the failure. The manifest asks for landscape as well, but
only an installed app gets that; this is the same answer for the plain page.

`WebBlueScreen::show` unregisters the touch callback. Otherwise the tap meant to reload the
page would first put the canvas back into fullscreen, and the overlay would sit behind it —
which is the very thing that `exitFullscreen()` at the top of that function avoids.

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

**Recorded audio** does not come from OpenAL: `alcCaptureOpenDevice` can only open an *input*
device, which would record the microphone into every video. `audiocapture.cpp` does a loopback
capture of what the machine is *playing* — WASAPI's loopback mode on the default render
endpoint under Windows, the monitor source of the default sink under Linux. Both end at 16-bit
stereo 48 kHz, which is what `videorecorder.cpp` wants, so the ring buffer, the reader side and
the clock-based silence padding (`AudioRing`) are shared and only the two `threadProc`s differ.
Windows converts the format itself, since the device hands over whatever mix format it likes;
Linux needs none of that because `pa_simple_new` is *told* the format and the server resamples,
which is why that half is a third of the size. libpulse is `dlopen`'d and its declarations
written out by hand, so the build needs no libpulse-dev and the game still starts where
PulseAudio is absent. The browser has no loopback at all; there `open()` fails and videos are
silent.

**OpenAL is OpenAL Soft**, vendored in `libs/openal-soft-1.25.2` (headers, public domain) with
its import library in `libs/bin` and `Blocks5/OpenAL32.dll` — `soft_oal.dll` renamed, which is
how that distribution is meant to be used without the router. Because the app directory beats
`system32` in the DLL search order, the game always gets this implementation and never whatever
Creative's 2009 installer may have left on the machine; `oalinst.exe` and the installer's
`InstallOpenAL11` task are gone. The game only calls core AL/ALC 1.1 (23 functions, no
extensions, no `alGetProcAddress`), so the switch needed no source change at all. The DLL is
LGPL v2 and must stay dynamically linked.

**The mix is turned down, and that is not a taste setting.** The game plays music and a dozen
effects at once, each source at full volume, and the sum stood above the ceiling: measured in
the main menu, where the title demo keeps adding bombs and lasers, **-8.8 LUFS at a true peak
of +0.9 dBFS, with 0.73% of samples hard against the limit and therefore clipped by OpenAL
Soft** — audible as distortion, in the game and in a recorded video alike. `MASTER_HEADROOM`
(0.45, at the top of `engine.cpp`) goes in as `alListenerf(AL_GAIN, …)` right after
`alcProcessContext`, which scales the finished mix inside OpenAL Soft's float pipeline
*before* that clamp; the same passage then measures **-15.5 LUFS at -1.1 dBFS**. Two standards
decide the number: a true peak no higher than **-1 dBTP**, because a lossy decoder — MP3 for
the videos here — can overshoot the samples it was handed, and an integrated loudness of
**-14 to -16 LUFS**, which is where YouTube and Spotify normalise to anyway. 0.50 lands
exactly on the ceiling; 0.40 is quieter than it needs to be. It belongs in the source rather
than in the options because it is a property of the mixture, not a preference — the player's
own sliders are untouched and still read 100%.

**Input** is two-layered. Physical keys/joystick axes/hats are mapped to *virtual keys*
(`VirtualKey`), and named *actions* (`"$A_LEFT"`, `"$A_PLANT_BOMB"`, …) bind a primary and
secondary VK. Gameplay queries `wasActionPressed(name)` / `isActionDown(name)`; bindings are
registered in `main.cpp` and remappable via the options dialog. Resetting comes in two
strengths there — *Reset selected* and *Reset all*, two stacked buttons under the action list,
since `Action` carries `defaultPrimary` and `defaultSecondary`. Those two and the two key
buttons all grey out without a selection.

**Waiting for a key is a state, not a loop.** Clicking a key button sets its caption to
`$O_PRESS_KEY` and calls `Engine::beginKeyGrab()`; `Options::onUpdate` asks `pollKeyGrab()` each
tick and applies the answer — the pressed VK, `GRAB_CANCELLED` for Escape (the binding is left
alone), or `GRAB_NO_KEY` on the three-second deadline, which clears it and is the only way to
leave an action unbound.

A blocking loop around `SDL_PumpEvents` and `SDL_Delay` — the obvious shape, and what this was
— **cannot work in the browser**: the event queue is filled by DOM listeners on the JS thread,
and those only run when C returns to the page, which is precisely why
`emscripten_set_main_loop_arg` calls `mainLoopIteration` once per frame. A loop that never
returns never sees a key. Swapping in a second main loop does not help either
(`emscripten_set_main_loop` either unwinds the wasm stack by throwing or returns at once, and
the caller has to resume later regardless), and once it resumes the ordinary loop already
yields and pumps.

**While a grab runs, the keyboard belongs to it.** `Engine::update` skips `updateActions()` and
calls `flushInput()` — otherwise binding F1 would toggle mute on the way past, and the
cancelling Escape would reach the GUI and close the dialog. The tick in which the key is *found*
still counts as part of the grab (hence the remembered flag, not the state after
`updateKeyGrab()`), or the new binding would fire its own action immediately. Skipping
`updateActions()` leaves nothing stale behind: the main loop clears every action's
pressed/released bits each tick regardless, so `wasActionPressed` is simply false throughout.

One quirk in `flushInput()`: Emscripten's `SDL_PeepEvents` takes the SDL 2 argument shape
*and* asserts `requestedEventCount == 1`, so that branch fetches one event per call.

**A binding is stored in `config.xml` by name, not by number.** A VK is an index into
`virtualKeys`, and that index moves: the keyboard block is `SDLK_LAST` long, which is 323
under SDL 1.2 and 1536 with Emscripten's headers, so every joystick entry after it sits
somewhere else — and the joystick entries themselves depend on what was plugged in at
startup. `VirtualKey::id` is the stable spelling written instead: `key:LEFT`, `key:KP_ENTER`
for the keyboard, from a table of the 136 SDL 1.2 key names that resolve to whatever
constant the current build means, and the already-structural `Joystick1 B3` / `Joystick1 A2+`
/ `Joystick1 H1NE` for the rest. Reading tries the number first, so a pre-1.2.0 config still
loads and is rewritten by name on the next save. An id that resolves to nothing — a joystick
that is not connected — becomes "unassigned" rather than a wrong key.

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

**The hint note is one texture, and it unrolls.** Standing on a note (`hint.cpp`) flies a
300x400 sheet of paper to the middle of the screen. Paper and text are drawn *together* into
one 512x512 texture (`Engine::getOffscreenTexture` + `beginRenderToTexture`), so the writing
belongs to the sheet: it flies with it, turns with it and rolls up with it, instead of
appearing on top once the sheet has landed. Two things about that texture are worth knowing.
It is drawn with (0,0) at the top left like everything else in the game, so it ends up
upside down in texture space — exactly as the game's own frame does in the framebuffer object,
and the mesh samples it with `1 - py/512`. And it is composed with
`glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)`,
which leaves the colour premultiplied by its own alpha and the alpha itself correct; it is
therefore drawn again with `(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)`.

The texture belongs to the **Engine** and not to the note, and that is not tidiness: it falls
with the framebuffer object, which `Engine::exit` destroys while the GL context still stands,
whereas an `Object` is destroyed only after `main()` has returned. There is exactly one,
because only one note can be on screen; a second note baking into it takes it over, which is
what `p_textureOwner` records — without it the first note would find its own text unchanged on
the next visit and show the other one's sheet.

**The roll is geometry, not a shader.** Each end of the sheet is wound onto a cylinder of
`ROLL_TURNS` (0.45) of a turn, tessellated into `ROLL_BANDS` (48) bands, with the perspective
divide done by hand (`f = PERSPECTIVE / (PERSPECTIVE - depth)`) and a `cos` term per vertex for
the shading. **Half a turn is the hard limit**, and it is a painter's-order limit rather than a
matter of taste: this pass has no depth buffer, so the only order that composes correctly is
back to front, and only up to π does every further step of paper come *closer* to the viewer.
Beyond that the far end would come back round and still be painted on top. The strip is
`GL_TRIANGLE_STRIP` and not `GL_QUAD_STRIP` — WebGL has no such primitive, and
`WebBuild/gl_immediate.cpp` hands the mode straight to it.

**The unrolling counts ticks, not alpha.** `shownAlpha` is an exponential ease towards 0.85
that never arrives, so a sheet driven by it would stay a little rolled up for ever.
`activeTicks` counts up while the player stands on the field and down again three at a time
when they leave; the sheet opens between tick 20, where it is at 96% of its size, and tick 40.
Three at a time on the way out because `shownAlpha` falls by 15% per tick: at one per tick
there would be nothing left to roll up by the time it had.

Without a framebuffer object none of this can happen, and `renderNoteFlat` then draws sheet and
text one after the other under the same matrix — no roll, but the writing still flies with the
paper.

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

**One Manager button in the main menu** opens a dialog that imports, exports and deletes, on
all three platforms — `src/transfer.cpp` over `WebBuild/web_transfer.cpp` in the browser,
`GetOpenFileNameA`/`GetSaveFileNameA` under Windows and `zenity`/`kdialog` under Linux, behind
one interface: `beginImport` starts it and `pollImport` is asked each tick, so an asynchronous
dialog and a modal one look the same to the caller.

`Menu.ManagerPane` holds the four kind radios, the list, *Refresh*, and a bottom row of
*Import*, *Export*, *Delete* and *Close* in the same four 92px columns as the radios above.
Import needs no selection and comes first; Export and Delete work on the selection and grey
themselves out without one. `Menu.ConfirmPane`, which must stay the **last** child in
`menu.xml` so it draws last and takes the clicks, asks before a delete — the one thing here
that cannot be undone.

**`Transfer::isBuiltIn` is the one place that knows what ships with the game**: `example01.xml`,
`example02.xml`, `blocks.zip` and the four skins. Two callers, for the same reason — the Manager
must not delete one, and an import must not take its name. They stay listed and exportable;
only *Delete* greys out. The comparison is case-insensitive and hand-rolled, because `tolower`
is locale-dependent and `pch.h` does not pull in `<cctype>`.

**A finished import updates the open list.** `pollImport` runs every tick from `onUpdate`,
because the browser's file dialog cannot be modal — so when it completes with the Manager still
open, it switches the kind radio to whatever `classify` decided, re-reads the list and selects
the new entry. That is why the pane deliberately stays open across the file dialog, export
included: the Manager is a place you keep working in.

Escape belongs to the topmost pane: the confirmation first, then the Manager, and only with
both closed does it quit the game.

**Import takes one file and works out what it is** — `Transfer::classify`, by content and never
by extension: `OggS` at the front is music, an XML whose root is `<Level>` is a level, and an
archive is a campaign if it holds `campaign.xml` or a skin if it holds `tileset.xml` and
`sprites.png`. Anything else is refused. The browser stages the upload outside the home
directory (C hands JS all three possible staging paths and JS picks one by extension, so C still
composes every path), `sanitizeFilenameStem` reduces the name to `[A-Za-z0-9_-]`, and only then
does anything reach IndexedDB.

**An import replaces a file of the same name**, for all four kinds alike, and
`Transfer::install` is the whole rule: sanitized stem, plus the kind's extension, plus a copy.
Not a swerve to `stem_2`, because a skin's filename *is* its identity — a level says
`skin0="space"` and `Level::getSkinFilename` looks for `levels/skins/space.zip`, so
`space_2.zip` would leave every such level exactly as broken, only without a visible cause —
and the weaker form of that holds for the rest: a new version of your level means *your*
level. A re-imported campaign therefore keeps its progress, since `ProgressDB` keys on the
filename and the filename no longer moves.

The one refusal is `isBuiltIn`. `install` reports through `bool* p_replaced` whether it landed
on an existing file, so the toast says **Replaced** rather than **imported** — the only sign
the player would otherwise get that something of theirs is gone. A campaign is checked with
`isImportableArchive` *before* the copy, so a damaged archive cannot destroy a good one of the
same name.

An imported skin also needs `Texture::applyWrapMode`: WebGL 1 samples a non-power-of-two
texture as pure black unless its wrap mode is `GL_CLAMP_TO_EDGE`, silently and with no GL
error, and the default is `GL_REPEAT` — which rain, snow and clouds genuinely need, since
`level.cpp` scrolls the texture matrix without bound to tile them. So the wrap mode is
switched for NPOT textures only, which is precisely the set where `GL_REPEAT` could never have
worked. The game's own art is all power-of-two; this exists for imported skins alone.

**What export writes is a plain copy.** That matters for skins: three of the four shipped ones
are packed with a password, and decrypting them on the way out would be a back door around the
very protection they are packed for. The recipient cannot open such an archive — but can still
*use* it, because the password rides along inside it as `password.txt` and
`Level::getSkinFilename` reads that out of any skin archive whatever its filename. A skin
somebody made themselves has no password anyway, and that is the one people actually share.

**A level can borrow the shipped campaign's music**: `musicFilename="blocks:music2.ogg"`
resolves through `Campaign::resolveMusicPath` to `levels/campaigns/blocks.zip[pw]/music2.ogg`
instead of a file beside the level, and `Campaign::save` deliberately does *not* pack such a
track — it is already on every machine. Without it a browser author has no music at all, since
nothing can put an `.ogg` next to a level.

**Images** are decoded by `img_load.cpp`, not SDL_image. The game needs exactly one function
from it — `IMG_Load_RW`, called from `texture.cpp` and for the window icon — and reads every
texture through its own `SDL_RWops` over the encrypted `data.zip`. stb_image (`libs/stb`, one
header) does that in about eighty lines; PNG and JPEG are enabled, and every image the game
ships is a PNG.

**The page around the browser build is `WebBuild/shell.html`**, not Emscripten's generated
one, and everything in it is there because a phone needs it. `<meta name="viewport"
content="width=device-width, ...">` is the important one: without it a phone lays the page out
at a ~980px virtual viewport and scales the result down, which puts a double-tap zoom in front
of every button and keeps the legacy 300 ms click delay. `touch-action: none` and
`overscroll-behavior: none` stop the browser taking a swipe for scrolling or pull-to-refresh.
`pre.js` keeps the drawing buffer in step with the element, on `orientationchange` and on
`visualViewport` resizes too — that is how a phone reports the address bar sliding away. The
page also handles `webglcontextlost`, a real event when a tab goes to the background, by
saying so instead of freezing: the game cannot rebuild its textures and its framebuffer object
from where it stands.

**The boot screen is pixel art too, and its line is the game's own.** It shows `$LOADING` from
`data/languages.txt` — the same sentence the game puts up a moment later — in the game's own
font, which the page cannot render itself: it stands before `data.zip` and before any GL
context. `WebBuild/make_text.py` draws it at build time straight out of `data/font.xml` and
`font.png` (the same glyph rects, the same advance, the same two-tap shadow `Font::renderText`
uses) and `build.sh` stamps both languages into the page as data URIs, so the line is there
with the first paint and costs no request. The page blows it up by a whole factor, 3 dropping
to 2 or 1 where the line would not fit — replication at an integer factor, never a resize,
the same rule as the icons. The bar fills in whole 12px blocks, and the icon is shown at 5x32
with `image-rendering: pixelated`. Which language is decided the way
`Engine::detectSystemLanguage` decides it, by the same walk over `navigator.languages`.

**The click-to-start goes through the moment the gesture arrives**, and does not wait for the
AudioContext. `resume()` returns a promise, and on a phone — where the fullscreen request
turns the screen — it can take a second or two to settle; waiting for it left the line
pulsing, which reads as "the tap did not register" and gets tapped again. `GS_Loading` starts
the logo intro at once instead, and the jingle waits its turn: it hangs off `time >= 1000`, so
there is a second of slack, and if the context is still suspended by `time >= 2000` the jingle
is given up rather than fired into the menu. Measured with `resume()` stubbed out to never
settle: the menu comes up 3.0 s after the tap, which is the intro and nothing else.

**It installs.** `manifest.json` (fullscreen, landscape) and `sw.js` make it an ordinary
add-to-home-screen web app that launches without the address bar and runs offline; that is
also the answer to iPhone Safari, which has no element-level Fullscreen API.

**Every icon is generated from `data/window.png`** — four for the web by
`WebBuild/make_icon.py`, seven for Windows by `Tools/make_ico.py`; `make_text.py` uses the same
PNG reader for the loading line, and all three are stdlib only. **Pixel replication at an
integer factor, never a resize**: a scaler that smooths turns 16x16 pixel art into a blur, and
that is the whole reason these scripts exist.
The `.ico` is **committed rather than generated**, because the Windows build runs no Python;
`verify.py`'s `windows_icon` check is what stops it going stale.

The web icons come in three kinds because a launcher does two different things with one:

- `purpose: "any"` (192 and 512) is shown **as it is** — the drawing edge to edge, transparency
  intact.
- `purpose: "maskable"` (512) is **cropped to a shape the launcher picks**, and only a centred
  circle of 80% of the width is guaranteed to survive; every transparent pixel becomes a hole
  in that shape. Bob is a full-bleed circle reaching 119% of the width across the diagonal, so
  masked he would lose his rim and his cap. It is therefore drawn at **10x (320px) centred on
  an opaque 512 canvas** — content radius 191px against the 205px allowed, and still an
  integer scale. **Never label one icon `"any maskable"`** unless it satisfies both, which a
  full-bleed drawing cannot.
- `apple-touch-icon.png` exists because iOS reads neither the manifest icons nor any
  transparency: full-bleed like the "any" pair, but opaque.

`Blocks5/src/icon1.ico` carries **seven images** — 16, 20, 32, 40, 48, 64, 256 — because
whenever the shell asks for a size the file does not hold it scales one itself, smoothly. That
is obvious upward and *also* true downward: shrinking 256 to 40 averages six source pixels
into one. One big image is not enough. Where the requested size is not a multiple of 16 — 20
and 40 — the next scale down is centred with a transparent margin rather than rendered at
1.25x, which would double some columns and not others. **24 is deliberately absent**: it is
the one size where the next step down is 1x, and that much margin is visible, so Windows
scales it from the 32 instead. Every entry is a 32bpp DIB (the 256 a PNG) because full 8-bit
alpha has worked since Windows XP and the art needs it; the 1-bit AND mask is still written
alongside, since some legacy paths read only that.

There is **no power-of-two restriction** on either kind: an `.ico` directory entry stores each
edge in a single byte (0 meaning 256), and a manifest's `sizes` is free text — 192 is not a
power of two either.

`Blocks5/setup/setupicon.ico` is the installer's own graphic — a monitor and a disc, not Bob —
and is deliberately left alone.

**The payload filenames carry the build's stamp** — `blocks5-<hash>.js`, `.wasm`, `.data`,
the hash being the md5 of the three — and that is the load-bearing part of the whole caching
story. They must never be mixed: the JS holds absolute byte offsets into the data, and its
`EM_ASM` fragments sit at addresses that fit exactly one wasm. Served in mismatched pairs the
game aborts with *"No EM_ASM constant found at address …"*, which is what a real deployment
did when **mod_pagespeed** kept `blocks5.js` under a rewritten name of its own and later
handed it out beside a newer wasm. With the stamp in the name every such URL is immutable, so
no cache anywhere — the browser's, a proxy's, PageSpeed's, the service worker's — can produce
the mixture. `Module.locateFile` in `shell.html` is the one place that knows the stamp;
`build.sh` writes it in after the link.

**The service worker therefore caches its two halves in opposite directions.** The stamped
payload is cache-first, since asking the network could only confirm what is already there.
Everything else is network-first with the cache as the fallback — above all `index.html`,
which cannot carry a stamp because it is the entry point and the place the current stamp is
written down. Serving *that* from the cache is how a new build becomes invisible: the page
reloads, the worker answers from its own store, and nothing changes until every tab is closed.
`skipWaiting()` and `clients.claim()` are safe now for the same reason the mixture is
impossible — a booted page holds stamped URLs — so an update lands on the next reload instead
of a load later. `install` still fetches the payload with one `addAll`, all-or-nothing.
Registration passes `updateViaCache: 'none'`, or a cached `sw.js` would keep a stale worker
alive indefinitely, and then asks `registration.update()` straight away rather than trusting
how promptly the browser gets round to its own check.

**Nothing stale accumulates there.** `activate` deletes every cache whose name is not the
current one, and the name carries the stamp, so a new build drops the whole previous set. The
cache-first branch also serves **only the stamp that matches its own `BUILD`** and lets
anything else through untouched: while a new worker installs, the old one is still answering,
and without that it would pull the new build's payload into its own doomed cache — both
bundles on disk for the duration.

`WebBuild/htaccess` ships as `.htaccess` beside `index.html` with the matching headers:
a year of `immutable` for the stamped three, `no-cache, must-revalidate` for `index.html`,
`sw.js` and `manifest.json`, `AddType application/wasm`, and `ModPagespeed off` — there is
nothing here for a rewriter to improve, and it has already done damage.

**`-sINITIAL_MEMORY` is 48 MiB, and that number was measured.** Started at 16 MiB the heap
grows exactly once, to 40 MiB, and stays there through the loading screen, the menu, the
editors and a played level. Reserving far more — it was 256 MiB — is on a phone the most
likely reason a tab dies before the menu appears. `ALLOW_MEMORY_GROWTH` stays on, so an
unusually large level still has room.

**Losing focus takes two answers in the browser, not one.** Emscripten's SDL reports focus and
visibility as **`SDL_WINDOWEVENT`** — an SDL 2 shape — and never sends the `SDL_ACTIVEEVENT`
the game switches on, so `mainLoopIteration` has a second case for it under `__EMSCRIPTEN__`.
Both funnel into `handleAppFocus`, which mutes, forgets every held key, stops a running
recording and tells the game state.

That still leaves the audio, because a *hidden* tab gets no `requestAnimationFrame`: no logic
tick runs, so the queued event is never even polled and the mute is applied by a pass that has
stopped. `pre.js` therefore suspends the `AudioContext` on `visibilitychange`, one layer below
the engine, which freezes every source at once. Without it the music dies on its own when its
queue runs dry while every looping effect — a laser above all — keeps sounding in a tab nobody
is looking at.

`appActive` is an `Engine` member rather than a local in `mainLoop` because
`emscripten_set_main_loop` calls one iteration per frame, so nothing may live on the stack
between them — and because the test hook reports it, which is what makes any of this checkable.

**Saves ask to be kept.** They live in IndexedDB through IDBFS, which a browser may evict when
it is short of room; `navigator.storage.persist()` in `pre.js` asks for that not to happen. The
browser grants it silently once the page looks like something the user meant to keep and
otherwise refuses, which costs nothing.

**The browser's Quit button** cannot quit — a page does not close its own tab — so it draws a
Windows blue screen instead (`WebBuild/web_bluescreen.cpp`), hooked into the one `SDL_QUIT`
case in `Engine::mainLoopIteration` so the menu button, Escape and the editors all reach it.
It mutes OpenAL, builds a DOM overlay above the canvas (leaving fullscreen first, or the
overlay would sit behind it) and calls `emscripten_cancel_main_loop`. Any key or click after
a 700 ms arming delay reloads the page, which is the restart the text asks for.

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

**A click goes to the element under the cursor *now*.** `GUI::update()` recomputes
`p_elementAtCursor` at the **top** of the function, before the enter/leave dispatch and before
the button handling — not at the bottom, which would dispatch every click to whatever had been
under the cursor at the end of the *previous* logic tick. With a mouse that is invisible: you
cannot click where the pointer is not, and between arriving over a button and pressing it there
is always at least one 20 ms tick. A finger has no such gap. The other half of the same bug is
in `Engine`: `cursorPosition` used to come only from `SDL_MOUSEMOTION`, and a touch produces no
motion at all, so both button events take the position from the event too. Either fix alone
changes nothing; the pair is what makes a tap land.

Two things about the toggles are worth knowing, because getting either wrong is quiet:

- **`check()` means "the user clicked"; `setChecked()` means "the display caught up".** Only
  the first fires `changed`. Refreshing a checkbox from model state with `check()` makes the
  handler run as if the player had clicked it — which in the level editor meant an Undo that
  toggled the electricity immediately produced a *fresh* undo point and threw the redo list
  away. `GUI_CheckBox::setChecked` must touch **only `checked`, never `newChecked`**:
  `newChecked` is the click in flight, written by `onMouseDown` and read by `onMouseUp`, and a
  per-frame refresh lands between the two. Clobbering it swallows the click, which is exactly
  what made the editor's Electricity box turn itself straight back off.
- **Escape and Return belong to the dialog.** `GUI_EditBox` and `GUI_ListBox` forward both to
  the parent when they have nothing of their own to do, which is what lets a dialog implement
  Escape = Cancel and Return = OK while focus sits in a text field or a list.
- **A checkbox or radio button is hit on its caption too.** The caption is drawn by the
  toggle itself at `size.x + 10`, and `containsPoint` — a new virtual on `GUI_Element`, which
  `getElementAt` calls instead of testing `size` inline — counts that strip as part of the
  control. The width is *measured*, not assumed: a fixed strip would steal clicks from
  whatever sits to the right, and options.xml puts language and detail radios in three tight
  columns. An empty `<Title>` measures zero, so a toggle that delegates its caption to a
  `<For>` label is unaffected.
- **Any element can carry `for="Name"`**, as `<label for>` does in a browser — it lives on
  `GUI_Element`, not on the text class, because a label is not always text: the two language
  flags in `options.xml` are `<StaticImage>` and belong to their radio button exactly as the
  word beside it does. A checkbox or radio target gets the whole set of mouse events forwarded
  (enter/leave included, since a toggle only fires on mouse-up if it believes the cursor is
  over it); anything else — an edit box, a list — just gets the focus, because forwarding a
  position measured against the *label* would drop an edit box's caret in an arbitrary place.
  The attribute is read in `GUI_Element::load`, which is not virtual: `readAttributes` is, and
  no subclass chains up to the base version, so a `for=` parsed there would work on some
  element types and silently vanish on others.
- **A `<StaticText>` label can size its own hit area.** Give it `w="-1" h="-1"` and it matches
  the text it actually draws, re-measured per frame so it follows a language switch; a
  hand-written width would be a guess that is wrong in the other language. `w`/`h` of 0 — the
  default — is still never hit. An image needs none of this: it already has the size of the
  sprite it shows.

**Short messages are the Engine's, not a game state's.** `Engine::showToast(type, text,
duration, suppressSound)` slides a bar in at the top edge, holds it, and slides it out again —
green for `TOAST_OK`, red for `TOAST_ERROR`, 2 s and 4 s by default, with `teleport_failed.ogg`
on an error unless the caller says otherwise. It is drawn at the very end of `Engine::render`,
after `GUI::display`, so it sits over the GUI, over the editors' panes and over everything
else. Everything that has something short to say goes through it — both editors, the menu, and
four failures that used to reach only the log: a skin that is missing or will not load
(`Level::loadSkin`, one message per skin *name* rather than one per missing file), a music
track that cannot be opened (`Engine::playMusic`), a level that will not load
(`Level::loadErrorLevel`, where all three of `Level::load`'s failure paths already met), and a
campaign that will not load (`Campaign::load`, likewise). Each names the bare filename: the
full path leads through an archive and its password and tells nobody anything.

Two callers opt out. `Campaign::load` takes a `quiet` flag for `isImportableArchive`, which
only asks whether an imported file *is* a campaign — a skin that is not one is not a broken
campaign. And `loadErrorLevel` says nothing for the editor's palette levels (`cat<N>.xml`),
which belong to the game and are not a file anybody asked for. In the select-level preview the
skin and level messages come without the sound, because stepping through a broken campaign
would otherwise beep at every keypress.

Several messages stack: the newest takes the top edge and pushes the older ones down a bar
each. One that has run out slides up by exactly one bar height, which puts it off the screen
if it was on top and *behind* its younger neighbour if it was not — hence the draw order,
oldest first. Position and opacity move together, 0.1 s each way and not counted against the
hold time, because the bar is not fully opaque. Asking twice for the same text and type does
not stack two copies; the hold time becomes the longer of the two and the sound plays again,
because the sound answers the click and not the message.

**Language on first start** is the system's, not English. `Engine::detectSystemLanguage`
asks `GetUserDefaultUILanguage` on Windows, `navigator.languages` in the browser and `LANG`
elsewhere, and answers only `de` or `en` — of the 349 IDs in `languages.txt` exactly one has
a French body and one a Spanish, so detecting `fr` would give an English game with a French
label. It runs only when `config.xml` has no `<Language>`.

Nothing ships a `config.xml` template — not the installer, not the web build. The game writes
the file itself on exit, which is what leaves the detection a chance to run at all.

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
  **and** `Blocks5.vcxproj.filters`. `Tools/verify.py` checks this — nothing else will,
  since the Emscripten build globs `src/*.cpp` and so never notices.
- Naming: `p_` prefixes a pointer, `pp_` a pointer-to-pointer; classes are `PascalCase`, methods
  `camelCase`, enum constants `PREFIX_UPPER` (`OF_*`, `SKIN_*`, `FM_*`).
- **A comment says what the code does and why, never what it used to do.** The reader is
  looking at the current code; the previous version is in the history, and an account of it in
  the file is noise they have to read past. No "used to be", no "this was moved from here", no
  retelling of the bug that led to the line. That belongs in the commit message, which is
  where somebody who wants it will look.

  What *is* worth writing is the gotcha: wherever a reader would reasonably stop and ask "why
  like this — why not the obvious thing?", answer that. The platform quirk, the ordering that
  matters, the constraint that rules out the shorter version. One sentence of reason is worth
  more than a paragraph of archaeology, and if the reason is genuinely long, the length is
  earned.

- Comments are in German, and **every source file is pure ASCII** — `Blocks5/src`, `WebBuild`,
  `PWEncrypt` and `ShowUserDir`, all of it. Umlauts are written `ae oe ue ss` (`AE OE UE SS`
  inside an all-caps word), so the encoding of these files no longer matters to anything:
  ASCII is a subset of UTF-8, of Latin-1 and of every codepage, and none of them needs a BOM
  or a `/utf-8` switch. Keep it that way — one umlaut typed into a comment puts the tree back
  to being encoding-dependent.
- **The two bytes that carry meaning are written as escapes.** `data/languages.txt` is
  Latin-1 and shipped that way; the game parses it with `'\xA7'` (the section sign, §) in
  `engine.cpp` and `'\xB6'` (the pilcrow, ¶) in `font.cpp`, and a few inline localized strings
  use the same syntax — `"\xA7" "de:…"`, split because a C++ hex escape is greedy and
  `"\xA7de:"` would parse as `\xA7d`. Those are a wire format shared with a data file, not
  text: they have to stay byte-exact whatever the source encoding is, which is the whole
  reason they are escapes rather than characters.
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

Four libraries are patched, in seven files. Everything else is byte-identical to upstream.
Each is explained where it lives — in the file itself and in that library's `PROVENANCE.txt`.

| library | file | what |
| --- | --- | --- |
| SDL 1.2.15 | `src/main/win32/SDL_win32_main.c` | `#undef UNICODE`/`#undef _UNICODE`; inert under MultiByte, kept as a guard |
| SDL 1.2.15 | `include/SDL_syswm.h` | brackets `#include <windows.h>` out of `#pragma pack(push,4)`, or every `C_ASSERT` in a modern `winnt.h` fails |
| zlib 1.3.1 | `contrib/minizip/unzip.c` | `NOUNCRYPT` commented out — without it nothing in the password-protected `data.zip` can be read |
| zlib 1.3.1 | `contrib/minizip/iowin32.c` | `IOWIN32_USING_WINRT_API` commented out; this is a desktop build |
| shine | `l3mdct.c`, `l3subband.c` | `__attribute__((unused))` guarded for MSVC as well as Borland |
| minimp4 | `minimp4.h` | the `esds` descriptor: real `objectTypeIndication`, optional DSI, reserved bit, `SLConfigDescriptor`, measured bitrate |

`libogg` and `libvorbis` differ from their git tags only in expanded SVN `$Id$` keywords in
five headers, which is what marks them as coming from the release tarballs rather than a
checkout — not a local change. `minih264e_impl.c` and `minimp4_impl.c` are ours by design:
they are the single translation units that instantiate those two headers.

**Re-checking after a library update.** `raw.githubusercontent.com` is reachable from the
build environment, so every vendored file can be fetched at its upstream tag and compared;
doing that across the whole tree finds nothing but the table above. Three libraries cannot be
checked that way and are documented from the tree's own history instead: TinyXML has no
upstream git repository (only the SourceForge tarball, and the GitHub forks that fill the gap
carry patches this tree deliberately does not), and sigslot and MersenneTwister have no
reachable upstream at all.
