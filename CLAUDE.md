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

**The Windows build compiles and links on v143 and on v145** (Windows 11, VS 2022
Community, SDK 10.0.26100 for the first). Four things had to be fixed to get there, all in the vendored libraries or the
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

**Toolset: whichever one the installed Visual Studio calls its newest.** The three
`.vcxproj` files set `<PlatformToolset>$(DefaultPlatformToolset)</PlatformToolset>`, so a
Visual Studio newer than anything in this tree needs no change to build with it, and
`Build.bat` passes no `/p:PlatformToolset` unless `/toolset:vNNN` asks for one — a global
property could not be overridden from inside the project, so passing one always would be
hardcoding a version again. `WindowsTargetPlatformVersion` follows the same rule: the
projects set it to `10.0` (newest installed 10.x) for anything past v140, which is what
`Build.bat` used to have to supply.

**Tested with v143 and v145, and with nothing else.** The claim that v120 and v140 still
build was reasoning about the code, never a compiler run — the plumbing for them is still
there (`/toolset:v120` skips the SDK property, and matches MSBuild 12.0/14.0), but it is
untried. The tree was pinned to v120 for a decade by `libs/bin/tinyxml_STL.lib`, which
carried `/FAILIFMISMATCH:"_MSC_VER=1800"`; TinyXML 2.6.2 is now compiled from vendored source
in `libs/tinyxml-2.6.2` and that library is gone. Two things got the tree off v120:

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
puts that on the screen as one letterboxed quad. Every hardcoded coordinate in the tree —
the single `glViewport`, the `glScissor` calls, the GUI layouts — therefore stays valid
whatever size the window is; only `computePresentRect` changes, and the cursor mapping in
`getCursorPosition`/`setCursorPosition` is its exact inverse. Video capture and screenshots
read `GL_COLOR_ATTACHMENT0` at 640x480 and never see the window size at all.
`glextensions.cpp` loads what the FBO needs: ten FBO entry points and twenty-five GL 2.0
ones, `glGenFramebuffersEXT` first and the core spelling as a fallback; in the browser they
are core and the header just `#define`s them through. Four upscale filters
(`Engine::UpscaleFilter`), a normal game option like the language and saved as `<Upscaler>`;
`sharp-fit` is the default where the machine can run it:

- `nearest` and `bilinear` are just `GL_TEXTURE_MAG_FILTER`. `nearest` additionally snaps the
  blit to an integer scale, which is the whole point of choosing it.
- `sharp-fit` (`src/sharpfit_shader.h`) is nearest at a fractional scale: conceptually the
  frame is nearest-upscaled by the smallest integer that covers the destination and then
  resampled down. That is one texture fetch, not two passes — bilinear over a
  nearest-upscaled image is piecewise linear, so remapping the texture coordinate through
  the same piecewise function and letting the hardware interpolate gives the identical
  result. Verified against a real two-pass: pixel-identical at an integer scale, max channel
  difference 1 (8-bit rounding in the intermediate) at fractional ones. It **must** sample
  with `GL_LINEAR` — the hardware interpolation *is* the filter.
- `crt` (`src/crt_shader.h`) is a CRT monitor: beam profile, scan lines, phosphor mask,
  halation, barrel distortion, rounded corners, vignette. See **The CRT filter** below.

Both shaders share `p_presentVertexShader`, the vertex buffer and four uniforms
(`decal`, `TextureSize`, `FrameSize`, `Prescale`); the CRT one adds `Scanline` and
`Curvature`. `Engine::PresentProgram` holds one set of them, `createPresentProgram` builds
either, and a CRT that fails to link leaves sharp-fit alone.

**The CRT filter.** Everything that gives it its character is a `const` at the top of
`src/crt_shader.h`, meant to be edited. Two of them are runtime sliders instead
(Options → Scaling → *CRT settings …*, saved as
`<Crt scanline= curvature= bloom= flicker= scanflicker=>`), because they are matters of taste
rather than tuning.

The one that decides what it *is* is `SCANLINE_PERIOD`. Visible gaps between scan lines are
an artifact of 240p: a console drew 240 lines into a 480-line raster. A VGA monitor showing
640x480 drew all 480 with the beam profiles overlapping, and had no gaps. This game's honest
reference is the VGA monitor, which is `SCANLINE_PERIOD = 1.0` — and at a 2x window that
produces *no visible stripes at all*, because both output rows sit equally far from the row
centre. That is physically right and useless as an effect, which is why the shipped default
is `2.0`: pretend 240 lines arrive, and get the look people mean by "CRT". The slider fades
that in; the constant decides which look it fades into.

The mask sits in **output** pixels (`gl_FragCoord`, `MASK_PITCH`), not source pixels — a real
shadow mask belongs to the glass and does not change when you switch resolution. That matters
because almost everyone runs at exactly 2x (`getDefaultWindowSize` gives 2x on 1080p *and*
1440p), where three source-locked subpixels are impossible.

Brightness is **derived, not tuned**: `MASK_AVG` and `scanAvg` are computed from the
constants, so mask and scan lines are light-neutral by construction and `BRIGHTNESS` is pure
taste at 1.0. Measured: moving the scan-line slider from 0 to 1 changes mean frame brightness
by 0.5%. The five `exp()` calls behind `scanAvg` are constant-folded — a literal measured
identically (7.9 vs 8.0).

The flicker is the one part that reads the clock, and it has three terms — all zero-mean, so
none of them costs brightness, and all functions of `Time` alone, never of the previous frame,
which is why none can turn into the xBR problem. A fast brightness shimmer at roughly 12, 19
and 29 Hz (that is the part people mean by *flimmern*), a much weaker mains-hum bar rolling
slowly down the picture, and the scan lines themselves crawling downward. The first two are
one slider (`Flicker`) and the third is its own (`ScanFlicker`), because wanting an unsteady
brightness and wanting the line structure to drift are separate tastes. Their frequencies
are whole cycles per `FLICKER_CYCLE` (8 s) and `presentFrame` feeds `SDL_GetTicks()` modulo
that, so the clock wraps seamlessly. It is the wall clock and not `Engine::getTime()`, which
counts logic ticks and stops when the game pauses — a screen flickers anyway. Measured at the
maximum setting: 2.55% peak-to-peak between frames.

The crawl is the one term computed on the **CPU**, as the `ScanPhase` uniform. It is a ramp,
not an oscillation, and its slope depends on the slider, so feeding it the already-wrapped
`Time` would jump the scan lines by `fract(flicker · speed)` of a period at every wrap.
`fmod(seconds · CRT_CRAWL_SPEED · crtFlicker, 1.0)` off the unwrapped clock is continuous
instead. `CRT_CRAWL_SPEED` is shared with the GLSL through the same stringify macro as the
curvature.

**Halation averages in linear light, per tap.** The first version averaged the taps in gamma
space and linearised the result before applying the threshold, and produced almost no visible
halo at all: the ring around a bright spot is a mixture of bright and dark, and `pow()` on
that mixture pushes it far below the threshold. Measured, the slider moved 0.6% of pixels from
end to end. Linearising each tap (with `x*x` — gamma 2.0, which for a soft halo is
indistinguishable from 2.4 and costs a multiply instead of a `pow`) and thresholding the
linear average gives a real glow: +23 grey levels at the centre falling smoothly to +4 at 70
output pixels. The taps sit on **two** rings, four axial and four diagonal — eight on one
radius makes a hard-edged ring rather than a glow.

Relative present cost, again on a software rasterizer: nearest 1.0, bilinear 1.3, sharp-fit
1.35, **crt 7.8**. Halation is about half of that; `BLOOM_STRENGTH = 0` compiles the whole
block away and brings it to 4.2. On real hardware this is ~12M texel fetches for a 1280x960
frame, which is nothing, but the browser build can land on a software path.

**The barrel distortion goes through the mouse as well.** The shader maps output pixel to
source pixel, which is the same direction `getCursorPosition` needs, so it uses the identical
formula — `Engine::warpToSource`. `setCursorPosition` needs the inverse, and the coupled pair
(`x` depends on `y²`, `y` on `x²`) has no closed form, so `warpToOutput` runs a fixed-point
iteration: `x <- u/(1+a·y²)`, `y <- v/(1+b·x²)`. Measured: eight rounds land within 2.3e-4
pixels even at an absurd curvature, and within 1e-5 at anything reachable from the slider.
`CRT_CURVE_X`/`CRT_CURVE_Y` are `#define`d once and stringified into the GLSL *and* read as
C++ doubles, so the two cannot drift apart.

Both cursor functions map **pixel centres** and `floor`, not left edges and truncation. The
old code lost a pixel at fractional window sizes; with centres the round trip
`get(set(g)) == g` is exact at every scale and curvature — except at exactly 1x, where 640
window pixels and 640 game pixels cannot both hold a non-identity warp, and 0.5% of positions
land on a neighbouring 16px tile. That is the minimum window size, where the CRT effect has no
room to work anyway.

Relative cost of one present, measured on a software rasterizer so read it as ratios: nearest
1.0, bilinear 1.3, sharp-fit 1.75. Without an FBO the game renders straight to the back buffer
as before; without a shader, sharp-fit degrades to nearest (`getEffectiveUpscaleFilter`) and
the options dialog hides that entry. Neither is fatal.

**xBR-lv2 was here and is gone**, together with hq2x before it, and the reasoning is worth
keeping: both are edge-directed filters written for flat-shaded pixel art, and this game's art
is airbrushed and photographic. Every decision in xBR is a `step()` against a threshold, which
is stable when neighbouring texels are either identical or plainly different and is not stable
when they sit near the threshold. Nudging a frame by 0–3 of 255 — about what the animated level
does where it shows through a semi-transparent dialog — moved 1% of xBR's output pixels by up
to 154, all on glyph outlines, while nearest, bilinear and sharp-fit moved by exactly what the
input moved. That was visible as text flickering. A CRT-style effect is the idea for a
nostalgic filter instead; see ROADMAP item 11.

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

**Drawing while the border is dragged** needs one thing SDL cannot give: while the user holds
the border or the title bar, `DefWindowProc` runs *its own* modal message loop and the main
loop sits in `SDL_PollEvent` until the mouse comes up. The only code that still runs is the
window procedure, so `Engine::hookWindowProc` puts one in front of SDL's with
`SetWindowLongPtr(GWLP_WNDPROC)` — the same subclassing SDL itself does for `SDL_WINDOWID`,
and safe because the HWND is created once in `DIB_VideoInit` and no later `SDL_SetVideoMode`
replaces it. `WM_ENTERSIZEMOVE` starts a 15 ms timer; `WM_SIZE` (every drag step) and
`WM_TIMER` (when the user holds still, where no `WM_SIZE` comes) both call
`repaintDuringSizeMove`, which re-presents the framebuffer at the new client size — the
upscaler, the letterbox and the aspect all track the drag live. No logic tick runs: one
`presentFrame` of the last rendered frame, nothing else. It **borrows** `displaySize` and puts
it back, because `handleResize` early-returns on an unchanged size and would then never call
`SDL_SetVideoMode`, leaving SDL's own surface stuck at the old size forever.
`SDL_SetVideoMode` must *not* be called during the drag — it calls `SetWindowPos` and fights
the user's mouse. The same procedure answers `WM_GETMINMAXINFO` (chaining first, since
`DefWindowProc` fills four other fields) with 640x480 client plus the frame from
`AdjustWindowRectEx`, so the 640x480 floor `handleResize` enforces is applied *during* the
drag instead of snapping back after it. Size, position and fullscreen state persist as `<WindowSize>`,
`<WindowPosition>` and `<Fullscreen>`, written by `Engine::exit` — until then the only
caller of `saveConfig` was the options dialog's OK, so simply resizing and quitting lost
the size. `rememberWindowPlacement` uses `GetWindowPlacement`, not `GetWindowRect`: a
maximized window's rect is the maximized frame, on Windows with negative corners because the
invisible grab handles count, and restoring *that* put a screen-sized window half off the
desktop. `rcNormalPosition` is the rect "restore" goes back to, and that is what gets saved,
together with a `maximized` flag that `restoreWindowPosition` replays with `SW_MAXIMIZE`.
`handleResize` skips updating `windowedSize` while `IsZoomed`, or the remembered size would
become the maximized one. In fullscreen the rect `applyWindowStyle` saved is used instead, so
the remembered window is always the windowed one. Whether the stored spot still exists is
`MonitorFromRect`'s job, which gets negative coordinates right — a monitor to the left of the
first one has them. On first run, or when the stored size no longer fits the desktop,
`getDefaultWindowSize` picks the largest integer multiple of 640x480 that leaves a 120px
margin in *both* directions — which also means "sharp" starts with no black bars. 120 is
not a round number chosen by feel: it is the largest margin under which 1920x1080, by far
the most common desktop, still gets 2x. 2*480 is 960 and 1080-120 is 960, so it fits with
nothing to spare; at 121 it would drop to 1x. The same value goes horizontally, where it
is pure slack at every common resolution, because a taskbar is not always at the bottom.
`-windowed`/`-fullscreen` override the flag for one run. In the browser the
canvas fills the page (`WebBuild/pre.js`), Alt+Return goes through the Fullscreen API from a
real DOM keydown — the main loop's own events do not count as a user gesture — and the main
loop reads the canvas size once a frame, which catches both.

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

Two things about the toggles are worth knowing, because getting either wrong is quiet:

- **`check()` means "the user clicked"; `setChecked()` means "the display caught up".** Only
  the first fires `changed`. Refreshing a checkbox from model state with `check()` makes the
  handler run as if the player had clicked it — which in the level editor meant an Undo that
  toggled the electricity immediately produced a *fresh* undo point and threw the redo list
  away. `GUI_CheckBox::setChecked` must touch **only `checked`, never `newChecked`**:
  `newChecked` is the click in flight, written by `onMouseDown` and read by `onMouseUp`, and a
  per-frame refresh lands between the two. Clobbering it swallows the click, which is exactly
  what made the editor's Electricity box turn itself straight back off.
- **Escape and Return belong to the dialog.** `GUI_EditBox` and `GUI_ListBox` used to swallow
  both — Escape fell into the `default:` branch where `unicode < 32` silently dropped it, and
  Return only ever clicked a `p_submitButton`. Both now forward to the parent when they have
  nothing of their own to do, which is what lets a dialog implement Escape = Cancel and
  Return = OK while focus sits in a text field or a list.
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

**Language on first start** is the system's, not English. `Engine::detectSystemLanguage`
asks `GetUserDefaultUILanguage` on Windows, `navigator.languages` in the browser and `LANG`
elsewhere, and answers only `de` or `en` — of the 349 IDs in `languages.txt` exactly one has
a French body and one a Spanish, so detecting `fr` would give an English game with a French
label. It runs only when `config.xml` has no `<Language>`.

Getting there meant deleting the thing that made it impossible: a `config.xml` holding nothing
but `<Language>en</Language>` was tracked in the repo, copied into the webroot by `build.sh`
and installed into the user directory by `main.cpp` on first run — so the detection could
never fire, and deleting your own config just got an English one written back. The installer's
half of the same mechanism (`makeconfig.bat`, `_config_en.xml`, `_config_de.xml` and the
`[Run]` entry that called them) is gone with it; the game writes `config.xml` itself on exit.

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
build environment, so every vendored file can be fetched at its upstream tag and compared.
Doing that across the whole tree gives 166/169 SDL files identical (plus `SDL_config.h`,
which is upstream's `SDL_config.h.default` verbatim), 35/37 zlib, 68/71 libvorbis, 20/22
shine, and stb, OpenAL Soft's headers and `minih264e.h` clean. TinyXML has no upstream git
repository — only the SourceForge tarball, and the GitHub forks that fill the gap carry
patches this tree deliberately does not — and sigslot and MersenneTwister have no reachable
upstream at all, so those three are documented from the tree's own history instead.
