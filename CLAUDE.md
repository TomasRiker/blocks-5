# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Blocks 5 — "Bob's Amazing Adventures", a 2D tile-based puzzle/action game. C++ on SDL 1.2 + OpenGL +
OpenAL Soft. **Three builds from the same sources**: Windows/Win32 (`Build.bat`, needs Visual
Studio), native Linux (`LinuxBuild/build.sh`), and an Emscripten port in `WebBuild/`. The last two
build and run here, so a change can be compiled, run and driven without Windows — see
`LinuxBuild/README.md` and `WebBuild/README.md`.

**Windows builds on v143 and v145** (Windows 11, VS 2022 Community). Three compile errors had to be
fixed, all in vendored libraries, each written up in the relevant `libs/*/PROVENANCE.txt`: shine's
`__attribute__((unused))`, which MSVC rejects; `misc.c` in the libvorbis file lists, a pthreads debug
allocator upstream never compiles; and `windows.h` inside SDL's `#pragma pack(push,4)`, which makes
every `C_ASSERT` in a modern `winnt.h` fail.

**The fourth fix is a rule: build MultiByte, never Unicode.** SDL 1.2 is an ANSI codebase — `char*`
throughout, calling `RegisterClass`, `LoadLibrary`, `GetLocaleInfo` unsuffixed. It was a DLL before,
built ANSI by SDL's own project, so `CharacterSet` never mattered; now that its 67 sources compile
*inside* `Blocks5.vcxproj`, `Unicode` resolves those to the `...W` variants and MSVC merely warns
(C4133). The first such build died in `SDL_RegisterApp`: `GetCodePage` passes `char buff[8]` and
`sizeof(buff)` to `GetLocaleInfo`, whose last parameter counts *characters*, so `GetLocaleInfoW` wrote
16 bytes into 8 — and forty other C4133 warnings were the same bug waiting to happen. The game's own
code never depended on Unicode: `MessageBoxA`, `ShellExecuteA` explicitly, no `TCHAR`, `TEXT()` or
`wchar_t` outside vendored `stackwalker.cpp`. `SDL_win32_main.c` keeps `#undef UNICODE` as a guard.

## Build & run

`Build.bat` does everything from a fresh clone — finds MSBuild, checks the toolset, builds
`Blocks5.sln` for `Win32`, packs `data.zip` and `levels/skins/*.zip` (gitignored build products the
game cannot start without). `Build.bat /?` lists options.

**Toolset: whatever the installed Visual Studio calls newest.** The `.vcxproj` files set
`<PlatformToolset>$(DefaultPlatformToolset)</PlatformToolset>`, and `Build.bat` passes no
`/p:PlatformToolset` unless `/toolset:vNNN` asks — a global property could not be overridden from
inside the project, so passing one always would hardcode a version again.
`WindowsTargetPlatformVersion` follows the same rule: `10.0` (newest installed 10.x) past v140.
**Tested with v143 and v145 only**; that v120/v140 still build was reasoning, never a compiler run
(`/toolset:v120` skips the SDK property).

**SDL is compiled from source**, all 67 files of the Win32 subset from `libs/sdl-1.2.15/src` — the
set SDL's own `VisualC/SDL/SDL.vcproj` builds. Needs one include directory, `winmm.lib` and
`dxguid.lib`, and `DECLSPEC=` among the defines (`begin_code.h` guards it with `#ifndef` and would
otherwise mark every entry point `__declspec(dllexport)`, wrong for a static build).

By hand: open `Blocks5.sln` (only `Debug|Win32` and `Release|Win32` exist), build all three projects,
then from `Blocks5`:

```bat
zip_data.bat     :: pack data\ into the encrypted data.zip the game reads at runtime
zip_skins.bat    :: pack levels\skins\<name>\ into levels\skins\<name>.zip
stage.bat        :: build a redistributable tree in Blocks5\stage (needs ..\Release\*.exe)
```

`zip_*.bat` run `Tools\optipng` first, which is slow; `zip_data_no_optipng.bat` and
`zip_skins_no_optipng.bat` skip it. Both
need `Tools\7za.exe`. Those binaries are reached through `%~dp0..\Tools\` rather than relatively,
because the scripts `PUSHD` into the folder they pack — and, for the XML half of `data.zip`, into
`%TEMP%`.

**`Blocks5/pack.sh` is all four in one, without Windows**, using the distribution's `7za` and
`optipng` and refusing to start without either. `7za` and not Info-ZIP's `zip -P`, although both
write traditional ZipCrypto: Info-ZIP sets bit 3 of the general purpose flags, writes sizes in a
trailing data descriptor, and takes its check byte from the time of day rather than the CRC.
`./pack.sh` does everything; `data`, `skins`, `campaign` narrow it; `--no-optipng` skips the slow
step.

**`levels/campaigns/blocks.zip` is a build product**, and why it must be rebuilt matters before
editing a level: `Campaign::load` serves a campaign's levels loose wherever all of them lie in
`levels/` — true in a working tree, never on an installed game, since `stage.bat` ships the archive
and the two examples and not the 42 sources. So a level edited and not packed changes what a
developer sees and nothing a player sees, with no error anywhere. `pack.sh campaign` and
`zip_campaign.bat` (which `Build.bat` calls) rebuild it from `levels/level_NN.xml`, numbering members
the way `makeMemberName` reads them back — entry *i* is `level_{i+1}.xml`, so the padded names in
`campaign.xml` are display text only.

**All 53 members have a source in the tree**, which is what lets the archive be untracked: 42 levels
and ten music tracks loose in `levels/`, `campaign.xml` in `levels/campaigns/blocks/` — a source
folder beside the archive, the same idiom as `levels/skins/<name>/`. Neither script reaches into the
archive it replaces, which cannot work once the file is a build product. Verified against the last
committed archive: same 53 members, every one byte-identical.

**XML and `languages.txt` reach `data.zip` without their comments.** `Tools/strip_comments.py` writes
stripped copies into a staging directory the packing scripts pack from, which is why `pack.sh` and
`zip_data.bat` call `7za` twice: images, sounds and the demo out of `data/`, XML and text out of
staging. Python does the stripping and is the one thing Windows packing has no bundled tool for —
where it is missing both scripts say so and pack those files as they stand. A note, not a stopped
build.

`languages.txt` is the easy half: `Engine::loadStringDB` reads a line at a time and its comment branch
does nothing, so a `//` line can go whole. The blank lines around one stay, because that parser counts
them and the next text line flushes the count into the string. Named explicitly rather than `*.txt`,
for the same reason the packing scripts name `password.txt` rather than globbing: a pattern sweeping
up every text file would one day take a line out of a password.

A comment is removed only if it has its lines to itself — a guard, not tidiness: a level stores one
tile id per character inside `<Row>`, so `<!--` is simply tiles 60, 33, 45, 45, and to a parser that
starts a comment, swallowing everything to the next `-->`. Asking whether the file is well-formed does
not help, because it *is*: ElementTree reads `<Row>aaa<!--bbb</Row><Row>ccc-->ddd</Row>` without
complaint. A row of tiles always has data before it on its line; a comment in a dialog never does. One
written after something else on the same line is reported and kept.

The game must run with `Blocks5\` as working directory (VS's default `$(ProjectDir)`) because it opens
`data.zip` relative to the cwd. `Build.bat /run` does that; it must come last, since every argument
after it goes to `blocks5.exe` untouched (`Build.bat Debug /rebuild /run -windowed`).

Command line / launcher scripts — the whole list, all five documented in `readme.txt`: `-windowed`
(`windowed.bat`), `-fullscreen`, `-nosplash`, `-perf`, `-nobatch`. `-nobatch` makes `renderSprite` draw
every quad on its own instead of collecting a render pass into one call, the arm to measure the sprite
batch against; `?nobatch=1` is the same switch in the browser, as `?perf=1` is for `-perf`. `-nosplash`
skips the logo and jingle by *not requesting* `logo.png`, the path `GS_Loading` already takes when the
texture will not load; only `soundPlayed` has to start `true`, because the jingle hangs off the time
threshold rather than the logo.

**Framebuffer objects, GL 2.0 shaders and vertex buffer objects are requirements; the game says so and
stops where one is missing.** `GLExtensions::init` resolves all three; `createFrameBuffer` and
`createUpscalerGL` add the two failures a resolved entry point can still produce, a framebuffer that
will not complete and a shader that will not link. There is no availability to branch on anywhere: no
`useFrameBuffer`, no `Upscaler::isAvailable`, no fallback to `Sharp`, no unrolled hint note, no 640x480
window pin, no `-nofbo`/`-noshader`. The dates are the argument — buffers core in GL 1.5 (2003),
shaders in GL 2.0 (2004), framebuffer objects an EXT from 2004 — and both software rasterizers tested
against, llvmpipe and SwiftShader, carry all three. In WebGL 1 they are core, so those branches had
been unreachable in the browser from the start.

**The message is written for a new machine in a particular state, not an old one.** Windows with no
graphics driver in play — fresh installation, safe mode, a VM, an RDP session — hands out
`opengl32.dll`'s GDI Generic renderer, which is OpenGL 1.1. So the box names the missing group with
`GL_VERSION`, `GL_RENDERER` and `GL_VENDOR` and says to install the driver: seeing *GDI Generic* there
turns a support mail into a self-fix. English, because `Engine::init` runs before `main()` loads
`languages.txt`.

**`fatalError()` (`fatalerror.h`) is the one way the game gives up**, written once per platform because
that is the whole of what differs: `MessageBoxA` under Windows, zenity or kdialog under Linux (the pair
the file dialogs reach for), a DOM overlay in the browser. The Linux half uses `fork`/`execlp` rather
than `system()`: the message carries strings the driver wrote, and an argument handed straight to the
program needs no quoting and can carry no command. `execlp` returns only where the program is missing,
so the child's `_exit(127)` is how the parent knows to try the other.

**The mouse cursor follows the scale; the framebuffer has nothing to do with it.** The arrow is drawn
once at 16x16 — the size it was designed as, and the size the video recorder and `screenshot()` stamp
into the 640x480 frame whatever the window does. `createCursor(factor)` builds the two the system can
draw, 16 and 32; `updateCursorSize` picks from the width of the rect `presentFrame` fills, not from the
window, because `Sharp` snaps to whole steps and between scale 1.5 and 2 shows the picture unscaled
where other filters nearly double it. `render` asks once a frame rather than hanging off events: the
answer moves on a resize, fullscreen toggle, filter change and the browser's canvas alike, and asking
costs two divisions and a comparison. Two sizes exist, so the choice is which of 16 and 32 lands closer
to 16·s: `|32 − 16s| < |16 − 16s|` from **s = 1.5**. At exactly 1 and 2, where `getDefaultWindowSize`
puts almost everyone, the chosen one is pixel-exact. Measured at 1.40/1.50/1.60: 16, 32, 32.

The upscaling filter is not a switch but an in-game option like the language, saved as `<Upscaler>` in
`config.xml`. Debug builds default to windowed + Console subsystem and skip the SEH crash handler;
Release defaults to fullscreen + Windows subsystem and dumps a stack trace via `StackWalker`.

Installer: `setup\Blocks 5.iss` (Inno Setup). The version number lives in **four** places that must
stay in sync — `p_localVersion` in `src/main.cpp`, `AppVersion`/`OutputBaseFilename` in the `.iss`, the
banner and changelog in `readme.txt`, and `FILEVERSION`/`PRODUCTVERSION` plus the two string values in
`src/resources.rc`, which is what Explorer shows and a crash log reports. The `.rc` had been missed
before and sat at 1.1.1 through the whole of 1.1.2.

Three projects: **Blocks5** (the game), **PWEncrypt** (encrypts an archive password into the bracket
form used in paths), **ShowUserDir** (opens the user data folder in Explorer).

## Checking a change

Four things run here, none needing Windows. Run at least the first two after any edit; about half a
minute together.

```
python3 Tools/verify.py      twenty-four static checks over the whole tree
sh Tools/syntax.sh           compile every source with mingw (-fsyntax-only)
LinuxBuild/build.sh          the native build compiles and links with GCC
cd WebBuild && ./build.sh    the browser port actually builds and links
```

Three ways to *run* it: `LinuxBuild/test/smoke.sh` natively, `WebBuild/test/smoke.js` in a desktop
browser, `WebBuild/test/mobile.js` in an emulated phone.

**A change whose whole question is what it looks like goes to the author to try, unbuilt.** Tuning a
glow, colour, width or timing: the build takes minutes, the screenshot oracle longer, and neither can
answer *is that the look I want*. Make the edit, say what the numbers mean and which way to turn them,
stop. Everything else still applies to anything a compiler or check can judge, and to a visual change
that also moves code around.

**A check that can pass on a previous run's artifact is worse than no check.** `WebBuild/build.sh` used
to pipe `em++` through `tail`, so the status tested was `tail`'s, and the check after it only asked
whether `blocks5.wasm` existed — which it did, from the run before. Worse than stale: `em++` writes
`blocks5.data` *before* `wasm-ld` runs and the table of byte offsets into it lives in `blocks5.js`, so a
failed link leaves a fresh data bundle beside the previous run's offsets and every preloaded file is
sliced in the wrong place. ROADMAP item 20 spent a day mistaking that for a corrupt `data.zip`. It reads
`${PIPESTATUS[0]}` now and exits 1.

The Linux build is the fastest way to *run* a change, and unlike the browser it is a real GCC compile of
every source, `videorecorder.cpp` included. It cannot check anything Windows-only — the SEH crash
handler, the Win32 window procedure, `audiocapture.cpp`'s WASAPI half — and those are what
`Tools/syntax.sh` is for.

**`Tools/verify.py`** looks for the mistake that leaves no trace in a diff and that no compiler sees: a
`gui["…"]` path no dialog XML knows, a `$ID` missing from `languages.txt`, an XML attribute written and
never read, a source file missing from `Blocks5.vcxproj` or its `.filters`, a display list added back, a
class whose header is not named after it, a render layer written as a number, an object that draws raw
geometry without flushing the sprite batch or changes texture state outside `GL::`, the version number
drifting across its four places, a member the constructor never sets, an asset filename not on disk or
spelled with different case (only Linux minds), a sound `playSound()` names that `gs_loading.cpp` does
not preload, a non-ASCII byte or CRLF in a source file, `if (` where the tree writes `if(`, a German
comment among the English. Exit 1 on any finding; `--list` names them, `--only NAME` runs one.
`Tools/README.md` has the table.

The language check reads the two languages against each other rather than searching for one, because
both word lists contain traps: *the particles die* is English although `die` is a German article, and
*so weit kommt das nicht* is German although `so` is English. A line is reported when the German words
outnumber the English. The `style` check learned the same from the other side — it skipped `//` lines
but not the body of a `/* */` block, and where the German never wrote `for (`, an English sentence does.

The attribute check exists because renaming `numLayers` to `NUM_LAYERS` once took the XML attribute
string with it, silently disabling the level size guard for every editor-saved level. That is the shape
of bug this file is for. **The Windows icon is checked for the same reason**: `Blocks5/src/icon1.ico` is
committed, not generated — the Windows build runs no Python — so it sits unchanged when the art moves,
and it had. `windows_icon` compares its 16x16 image against `data/window.png` and insists on the sizes
the shell asks for; `Tools/make_ico.py` rebuilds it.

**Two checks judge only what changed since `95660bb`**, the last commit before the 2025 overhaul, whose
id is `BASELINE` at the top of `verify.py`: indentation/whitespace, and uninitialised members. The
comment-density half of `comments` is an absolute 50% and judges every line. Code that has worked for
ten years is not a finding, and reporting it every run is how a check gets ignored.

**`Tools/selftest.py`** injects each fault in turn, confirms the matching check fires, restores the file
byte-for-byte. Run it after touching `verify.py`. Not ceremony: the attribute check was inert when first
written, because `Attribute(` also matches the tail of `SetAttribute(`.

**`sh Tools/syntax.sh`** compiles all 124 sources with `i686-w64-mingw32-g++ -fsyntax-only`, the only way
to put a compiler over the Windows code from here. Three files never go through it — `main.cpp`,
`videorecorder.cpp`, `stackwalker.cpp`. The last two are left out of the web build for the same reasons;
`main.cpp` is compiled there, and the difference is what mingw cannot parse in it: the `__try`/`__except`
crash handler behind `#if defined(_WIN32) && !defined(_DEBUG)` — true under mingw, false under emcc. It
needs nothing checked in: the headers mingw and OpenAL Soft file differently (`<Windows.h>`,
`<Shlobj.h>`, `<al.h>`) are generated into a temp directory. It passes `-w`; for a warning sweep swap
that for `-Wall -Wextra` and compare against the same sweep before your change, because the tree emits
thousands of warnings that were all there in 2015.

### Driving the game natively

`LinuxBuild/test/smoke.sh` runs the built game under Xvfb with openbox, clicks through menu, options and
manager, toggles fullscreen, screenshots with F11, quits with Escape. It clicks by element name, not
coordinate: `Blocks5/src/testhooks.cpp` — the same hook the browser uses — reports the GUI tree, and
since there is no JavaScript here the request goes through a file (`$B5_TEST_DIR/request`, answered once
per logic tick). That catches what a screenshot cannot: on a first start `Menu.CrtPane` covers
everything, so a click on the middle of `Menu.Options` lands on the pane.

**The two input layers want opposite treatment — the trap that costs the most time.** A key the GUI reads
is an SDL *event* and must be tapped, not held: `SDL_EnableKeyRepeat(140, 60)` turns an Escape held for
400 ms into six, the first closing the dialog and the second quitting. A key bound to a named *action*
must be held, not tapped, because `Engine::updateVKs` reads `SDL_GetKeyState`, a snapshot taken once per
20 ms tick, so a press and release in the same millisecond is never seen. The same sampling rule governs
mouse and touchscreen, which is why `page.mouse.click()` and `page.touchscreen.tap()` are equally
useless: move, settle, hold, release. Alt+Enter misleads, hanging off `SDL_KEYDOWN`, and events queue.

**A tapped key is not an instantaneous one.** `b5_key` holds 60 ms rather than calling `xdotool key`,
which presses and releases in about twelve. `SDL_PollEvent` runs once per rendered frame, and under
llvmpipe a frame is a fifth of a second — so a run landing inside those twelve milliseconds sees the
press and not the release, and at the *next* run SDL's repeat, 140 ms overdue, posts a second key-down
before the release is read. Measured, every fifth press arrived twice; at 60 ms none did, eight times out
of eight. A real machine renders at 60 Hz and a real finger holds far longer, so this is the harness
lying, not the game.

`xdotool windowclose` calls `XDestroyWindow` and SDL trips over a window it still believes is its own.
Quit the way a player does — Escape in the menu — or `Engine::exit()` never runs and `config.xml` is
never written.

**Two harnesses on one display take each other down**, and the wreckage reads as a rendering fault rather
than a collision: `b5_clearDisplay` clears the Xvfb an aborted run left behind, and on the shared default
`:99` it cannot tell that server from a live one. It refuses where a `blocks5` is still attached and
names `B5_DISPLAY`, which is what separates two runs. A frame taken while the server was going down is not
evidence of anything.

**The select screen's campaign list keeps the keyboard focus after a click**, deliberately —
`GUI_ListBox` handles the same four keys — and is a trap for a test that picks a campaign then presses End
for the last level: the press goes to the list, which selects its own last entry and therefore *another
campaign* at level 0, with nothing saying so. There is no list of levels to click instead, so take the
focus off it by clicking any of the six buttons, or drive by element name.

**Two windows open themselves over the menu, and `b5_start` writes both markers away.** The CRT offer
appears on a first start and the donation window once enough time has been played — which a machine that
has run the tests often enough reaches on its own. Both are one-shot, so no test touching the menu is
repeatable while they may appear; `.crt_offered` and `.donation_asked` are written exactly as the game
writes them.

**The harness drives `build-test/`; `LinuxBuild/build.sh` without `hooks` writes `build/`.** Building one
and testing the other is an afternoon's worth of a change that appears to do nothing, so `b5_start`
compares the binary against `Blocks5/src` and `data.zip` against `Blocks5/data` and refuses to run on
either out of date. `Tools/selftest.py` puts each file's mtime back along with its bytes, or every run
would trip that check.

### Driving the game in a browser

`WebBuild/build.sh hooks` builds to `build-test/` with `-DBLOCKS5_TEST_HOOKS`, turning on
`WebBuild/test_hooks.cpp`. The shipped build has none of it — the whole translation unit is inside the
`#ifdef`, and `blocks5_testDump` does not appear in `build/blocks5.js`.

The hook only reads. It puts the GUI tree into `Module["b5_test"]` as JSON — every element with its window
rectangle, whether visible and enabled, plus game state, language and filter — and `blocks5_testHitAt(x,
y)` says which element a click would reach. `WebBuild/test/harness.js` turns that into `clickPath(page,
'Menu.Options')`, and the click stays an ordinary mouse click travelling through SDL, Engine and GUI.

Do not go back to reading coordinates off a screenshot: the buttons are eighteen pixels high, the window
is scaled, and a pane drawn on top looks like a missed click.

Four things about this environment, each of which cost real time:

- **A wasm trap looks like a hang, not an exception.** `page.evaluate` never settles and there is no error
  anywhere. `computePresentRect` divided by a zero `screenSize` before `main()` had run and the
  NaN-to-int cast trapped; `GUI_Element::getFullName()` walked past a null parent. Guard anything the
  hook calls before the engine is up, and bisect a hang by adding stages.
- **`Module.calledRun` never appears** in this Emscripten. Wait for the page-level `runtimeInitialized`
  *and* a dump with a game state and a non-empty element list; the runtime is up well before `main()` has
  built anything.
- **Under swiftshader the game needs about half a minute to reach the menu**, and a frame takes a fifth of
  a second. Never sleep a guessed interval; wait on the reported state.
- **`GS_Loading` waits for a real gesture**, because the AudioContext is suspended until one arrives — so
  a test must click *before* waiting for `GS_Menu`, or it waits forever.

### Driving the game on a phone

`WebBuild/test/mobile.js` is the same idea in Chromium's mobile emulation, loading `index.html` rather
than `blocks5.html` — that is the file that ships and the only one registering the service worker. It
checks the page around the game: layout viewport the device width and not the ~980px default, nothing
scrolls or zooms, the canvas covers the viewport, the manifest says what an install needs, the worker's
cache holds the payload, and a reload with the network off still boots. **`isMobile: true` in the context
is what makes any of it mean something** — without it Chromium lays out at the window width and the
viewport meta has nothing to do.

The one check about the game rather than the page is a real touch: `touchStart`, a wait, `touchEnd`,
through `Input.dispatchTouchEvent` over CDP. That found the click-ordering bug in `GUI::update()`; the
dump reports `cursor` and `mouseDown` so a tap that does not arrive can be told from a button that does
not react.

**The dump also lists `actionsDown`**, the only window onto the *action* layer from outside:
`Engine::updateVKs` reads `SDL_GetKeyState` and not `keyData`, so whether a key reached the named actions
cannot be inferred from anything else. It established that an on-screen pad can drive the game with an
ordinary DOM `keydown`/`keyup` on the document — a synthetic `ArrowLeft` with `isTrusted === false` shows
up as `["$A_LEFT"]` and clears on `keyup`, where `Engine::setKeyData` would not have worked at all
(ROADMAP item 19).

### Measuring a frame

`FrameStats` (`framestats.h`) keeps the last 512 frames' timings — interval start to start, how long the
turn held the main thread, and render, update and present inside it — and answers p50, p95 and maximum.
Percentiles, not a mean, because what tears the audio or drops a beat lives in the tail. Recorded always:
four clock reads a frame.

**`render` and `present` are what *issuing* the draw calls costs, not what drawing them does.** GL is
asynchronous, so the work is paid for wherever the pipeline is next made to catch up — and where that is
differs completely between the platforms, which is why `swap` is a phase of its own.

**Natively it is somewhere in `present` and `swap`, whichever the driver picks — read those two as one
number.** Measured under llvmpipe with a `glFinish` inserted to find out: a single `present` carrying all
of it read 14.9 ms and was really the level rasterizing; with the finish in place render issues in 2.2 ms
and the finish takes another 5.9. Only a `glFinish` isolates the wait; splitting the swap out just stops
one number pretending to be the blit.

**In the browser nothing here sees the GPU at all.** `SDL_GL_SwapBuffers` is `Browser.doSwapBuffers?.()`,
which exists only on the worker path, so off the main thread it does nothing; measured, the swap is 0.00 ms
and a `glFinish` after render returns in 0.02. The page composites the canvas after the callback returns,
outside every window this can time. What is left is exactly main-thread CPU — the right measure for
anything the emulation or the JavaScript does, and no measure of the hardware. **`interval` minus `total`
is what is left for it:** a frame rate that falls while `total` stays flat is time going somewhere this
cannot see.

**The overlay counts two things against the logic rate, with different thresholds.** A frame whose
**interval** went over is one the player did not get; one whose **work** went over is one this game is
responsible for. Under swiftshader the browser ran at 38 ms a frame on 2.7 ms of work: counting the work
alone would have reported nothing wrong at 26 fps.

The interval is counted against **two** ticks and the work against one, and that asymmetry is load-bearing.
The loop aims every iteration at exactly one tick — the `SDL_Delay` at the foot of `mainLoopIteration` — so
an interval threshold of one tick sits on the number the code is targeting and a millisecond of timer
granularity trips it: the menu read **277 of 512 frames late while not one had been dropped**. A frame
actually lost is an interval of two. The work has no such problem, because nothing aims it anywhere; one
tick is simply the budget.

A third count stays at 500 ms: that is what Emscripten's OpenAL has scheduled ahead, so a frame past it is
a hole in the music — browser only, since the native decoder thread fills the queue whatever the main
thread does. It was the *only* count once, which flattered everything: at 50 fps nominal it read `0 of 512`
while a fifth of frames were missing their budget.

One caveat on the work count. `total` includes `swap`, and **nothing in the tree ever asks for vsync** — no
`SDL_GL_SWAP_CONTROL`, no `SDL_GL_SetSwapInterval` — so it is the driver's default. Under Xvfb there is no
vblank and `swap` is real work (6.9 ms by default, 6.8 with `vblank_mode=0`). On a real desktop, where Mesa
syncs by default, that phase would be a *sleep*, and the work count would read a frame that merely waited
as one that overran.

Three ways to read it:

- **`-perf`** / `?perf=1` draws the numbers in the bottom corner — the phone's only way, since the block
  lands in a screenshot. Holding `$A_PLANT_BOMB` while it is on suppresses the game's own drawing and
  clears the stats, giving the upper bound of a frame that draws nothing.
- **The test hook's `frames`** in the JSON, for a desktop harness, without the overlay's own cost. It does
  not clear on read, because the overlay reads the same numbers continuously;
  `blocks5_testResetStats()` (`resetstats` natively) begins a measurement.
- **`WebBuild/test/perf.js`** drives the comparison: arms are query strings rather than builds, so both
  sides are one binary in one browser, and they are **interleaved** rather than run in blocks, so a machine
  that warms up or throttles hands that to both.

**`?texunits=N` is the first knob riding on this**, and it shipped. Emscripten's GL emulation keeps state
for as many texture units as WebGL reports — 8 to 16 — and loops over that count twice per draw call. This
game never leaves unit 0: no `glActiveTexture`, `GL_TEXTURE0` or `glMultiTexCoord` anywhere. `pre.js` sets
`Module.GL_MAX_TEXTURE_IMAGE_UNITS` to 1 by default; `?texunits=0` puts it back to asking WebGL, the arm to
compare against. Measured on the title demo, three interleaved twenty-second runs: median frame **3.20 ms →
2.70**, render half **2.40 → 2.00**, spread within an arm 0.10 ms, picture untouched (0 of 512000 pixels
differ).

Two traps. `Module.<anything>` must be named in **`INCOMING_MODULE_JS_API`** or the start aborts;
`build.sh` passes Emscripten's whole default list plus this key, because naming the setting replaces it,
and `-sFOO+=bar` is not a syntax emcc knows — at link time it is dropped without a word rather than
refused. And the interval barely moved in that measurement, correctly: under swiftshader the frame rate is
capped elsewhere, so the saving shows up as main-thread time. On a phone, where the main thread *is* the
limit, it is the same milliseconds either way.

## Architecture

Everything lives flat in `Blocks5/src`. Layering is by naming prefix, not directory: `gs_*` game states,
`gui_*` widgets, `cf_*` crossfades, `u_*` upscaling filters, `e_*` electronics parts,
`as_*`/`audiostream*` audio decoding, `file*`/`filesystem*` virtual FS.

**Singletons and resources.** Global services derive from `Singleton<T>` (`singleton.h`) and are reached as
`Engine::inst()`, `GUI::inst()`, `FileSystem::inst()`, `ProgressDB::inst()`. Shared assets derive from
`Resource<T>` (`Texture`, `TileSet`, `Font`, `Sound`, …) and come through
`Manager<T>::inst().request(filename)` / `->release()` — ref-counted, keyed by filename, never
`new`/`delete`d directly.

**Engine** (`engine.cpp`, ~56k) owns the main loop, window, OpenAL, config, localization, screenshots and
video capture. The loop renders as fast as it can but steps logic at a fixed `logicRate` of 20 ms
(`setLogicRate(20)` in `Engine::init`); one `update()` call is one logic tick, so gameplay counts ticks
rather than measuring dt.

**Presentation.** The game always renders 640x480 into a framebuffer object (`createFrameBuffer`: a 640x480
region of a 1024x512 texture plus a packed depth-stencil renderbuffer — `cf_star.cpp` and `level.cpp` both
need the stencil), and `presentFrame` puts that on screen as one letterboxed quad. Every hardcoded
coordinate in the tree — the single `glViewport`, the `glScissor` calls, the GUI layouts — therefore stays
valid whatever size the window is; only `computePresentRect` changes, and the cursor mapping in
`getCursorPosition`/`setCursorPosition` is its exact inverse. Video capture and screenshots read
`GL_COLOR_ATTACHMENT0` at 640x480 and never see the window size. `glextensions.cpp` loads ten FBO entry
points and twenty-five GL 2.0 ones, `glGenFramebuffersEXT` first and the core spelling as fallback; in the
browser they are core and the header `#define`s them through.

**The tile grid is a vertex array, built once and drawn three times.** `Level::renderTiles` writes the layer
into a `std::vector<QuadVertex>` whenever `layerDirty` says it changed and hands that to one
`glDrawArrays(GL_QUADS)` out of client memory. The vertices carry a position and a texture coordinate and
**no colour**, which is the point: `Level::render` makes three passes over a layer — two shadow samples and
the picture — differing in nothing but a `glColor` and a translate, so one built array serves all three. Six
places set `layerDirty`, covering every way a tile or the picture it is cut from can move; a tile id alone
would not, since the texture coordinates come from the `TileSet` and a skin change moves every tile without
moving an id. Measured on the title demo: median frame's render half **2.1 → 1.7 ms**, spread 0.1 ms.

**A whole render pass of sprites is one draw call.** `Level::renderObjects` opens a batch around its object
loop (`Engine::beginSpriteBatch`); while one is open `Engine::renderSprite` appends four `ColorQuadVertex` —
position, texture coordinate, and a colour of its own — instead of drawing, and `flushSprites` puts the lot
up with one `glDrawArrays(GL_QUADS)`. The colour must be per vertex and not in `glColor`, because every
object brings its own tint, death countdown and conversion ghost; a shared colour would flush at every
object and leave nothing to batch. Measured in the browser, `?nobatch=1` against the default: **276 draw
calls a frame → 35** in a played level, median frame **3.90 → 2.40 ms**, render half **2.40 → 1.10**, same
vertex count. Those numbers predate the GL state layer and now understate the batch, since the redundant
state calls they were taken with were also breaking it at every `renderSprite(Texture*)` — the *shines* in a
played level each got a draw call of their own. `LinuxBuild/test/frames.sh` reports sprite-batch draws per
frame and quads per draw for five scenes on every run; `WebBuild/test/perf.js` reports real GL draw calls
beside its milliseconds.

**On a real phone the batch is worth six times as much as any desktop number says.** Measured with
`?perf=1` on a level with a full tile map and grass objects over the whole of it, the frame went **57 ms
with `?nobatch=1` and 9 ms without it**. Against a 20 ms tick those are two different games: 57 ms is nearly
three ticks, so every frame was one the player did not get. The gap belongs to the platform, not the scene —
swiftshader is capped by rasterizing, where a phone's main thread *is* the limit and each draw call drags
Emscripten's GL emulation through a stretch of JavaScript. **Read every browser number here as a lower bound
on what a phone gets.** That scene is the best case: `-nobatch` does not touch the tile map, so the whole
48 ms is the *grass* — `StdObject`, therefore `renderSprite`, hundreds of quads off one sprite sheet with
nothing between them to break the batch.

**A sprite is drawn at the size it was given, odd numbers included.** `renderSprite` used to halve the size
and span `size` texels over `size - 1` pixels, so anything odd came out a pixel short and resampled.
`halfSize` and `otherHalf` split it instead, and mirroring swaps the two `u` coordinates rather than
applying `glScaled(-1, 1, 1)`, which on an asymmetric quad would shift it a pixel. The only odd sizes in the
tree are the 39x39 level-status stamp in `gs_selectlevel.cpp` and `Menu.Donate` at 100x43 in `menu.xml`;
neither runs inside a batch, so no `-nobatch` comparison sees the change.

**The transform is baked into the vertices, and that is what the batch costs.** A sprite is drawn under
whatever matrix its caller pushed — `Object::render`'s translate to the cell, the squash of a teleporting
object, the unbalanced `glTranslated` `Enemy` does inside its own `onRender`, the half pixel `Level::render`
puts under the wires — and sprites from different objects cannot share a draw call while that lives in the
matrix stack. So `queueSprite` reads it back with `glGetFloatv(GL_MODELVIEW_MATRIX)` and multiplies the four
corners itself: one GL call in place of the fourteen to sixteen the immediate path made per sprite, and in
the browser that read is a copy of sixteen floats out of a JavaScript array, not a pipeline stall.

**The flush must therefore draw under `glLoadIdentity`**, and getting that wrong is invisible almost
everywhere: the vertices already carry the matrix, so leaving it applied puts it on twice. A level renders
under an identity modelview almost everywhere — the camera shake and the half pixel under the wires are the
exceptions, and either applied twice passes for the effect itself — so the game looked perfect until the
level editor, which draws its object palette under `glTranslated(245, 428, 0)` and lost every sprite off the
right of the screen. The five palettes caught it, as they caught the render-layer conversion before.

**A queued quad is drawn with the state at the flush, not at the call.** No depth buffer in this 2D path, so
painter's order is the only order: anything that draws, or moves state the queued quads will be drawn under,
must flush first. Today that is the texture binding, texture matrix, blend function and framebuffer; nothing
the batch reaches touches the scissor box, colour mask, stencil or alpha test, and neither check would
notice if something started to. `Engine::setBlendFunc`, `beginRenderToTexture`, `endRenderToTexture` and
`acquireOffscreenTexture` flush themselves, and the object sources reach texture state through **`GL::`**
(`glstate.h`, pulled in everywhere by `pch.h`) — `setTexturing`, `bindTexture`, `deleteTexture`,
`pushTexturing`/`popTexturing` — so the rule lives in one file instead of at a dozen call sites.

**Only one of those still flushes, and only when something moves.** `GL::bindTexture` does, since what is
queued was queued against the binding about to be replaced. `setTexturing` does not: `flushSprites` declares
texturing for its own draw (`GL::beginBatchDraw`) and restores the game's wish afterwards, taking the enable
out of the batch's state altogether. The texture matrix is the third piece and the one the first draft of
the check was blind to; it has no entry point of its own, because **every absolute matrix this tree sets is
a function of the binding** — a `Texture`'s own `1/w, 1/h`, or a screen copy's `1/pow2` with flipped y — so
`bindTexture` takes those two numbers as its second argument, there is no way to bind without saying how the
picture is sampled, and the matrix is not independent state at all. The weather wants more than a scale, and
composes its scroll on top of the bound picture's own inside a balanced push and pop.

**So the ordering is said where the drawing is.** The seven `onRender`s that draw raw geometry — the laser's
and the light barrier's beam points, the lava's flow arrow, the censor bar, the projectile's point, the
teleporter's target line and the speech balloon — each call `flushSprites()` themselves, plus the two lava
passes, which draw raw quads under a texture bound from outside and so move no state anything would flush
for. `drawQuadArray`'s two array forms and `LineDrawer::draw` flush at their own definition instead, because
a built array is reached as a member or local through layers of call no static check can follow.
`verify.py`'s `sprite_batch` check counts nothing else as a flush; `gl_state` bans the raw forms, scoped to
what the batch can reach — the sources defining an `Object::onRender`, plus `texture.cpp` and
`linedrawer.cpp` which they all draw through, plus `Level::renderShine` and `Font::drawText` by name. The
crossfades, GUI and credits are deliberately left alone, which keeps the ban checkable by a reader.

**`GLState` skips a call that sets what is already set, and what that is worth is not the calls — it is the
draw calls.** 28 redundant state calls of 4833 GL entry points a frame is noise; what matters is that each
of them *flushed the sprite batch*. A bind and a switch back off sit around every
`Engine::renderSprite(Texture*)`, which is what `Level::renderShine` is, so a level full of shines queued one
quad and drew it, over and over. Measured with `frames.sh`, quads per draw call: a night-vision level **1.1
→ 19.0**, the level select **1.1 → 19.0**. The other three scenes do not move — the title demo stays at
50.2, a plain level at 4.0, the editor at 21.5, none of which has a shine — and 11% to 25% of calls into
`GL::` now do nothing, depending on the scene.

The routing is complete, which is what `gl_doors` says: every raw `glBindTexture`, `GL_TEXTURE_2D` enable,
`glDeleteTextures` and absolute texture matrix comes through `GL::` — a delete included, because GL reverts
the binding to 0 when the bound texture is deleted. `presentFrame` keeps its raw calls inside
`glPushAttrib(GL_ALL_ATTRIB_BITS)` and calls `GL::invalidate()` afterwards, since what the pop restores
differs between desktop and browser and a record nobody can work out is better dropped than guessed. The
failure mode — a wrong picture rather than a slow one — is checked rather than reasoned about: a **native**
test-hooks build reads the real binding, matrix and enable back on **every** call and reports a record that
disagrees, `frames.sh` fails on any such line, and all five scenes come through silent. Not in the browser,
where the same line is a WebGL `getParameter` per call and would swamp `perf.js`; what it looks for is the
tree's own code, the same on both platforms.

The `glPushAttrib(GL_TRANSFORM_BIT)` inside `GL::bindTexture` stays for a different reason: replacing it is
safe as the tree stands, but it would leave the call with a silent precondition, and would save nothing in
the browser, where `glPushAttrib` issues no GL call at all.

**The flush says which matrix stack it means.** It draws under `glLoadIdentity` because the vertices already
carry their modelview — but a flush happens wherever state moves, `Texture::bind` included, and
`Level::render` binds the snow and clouds with `GL_TEXTURE` current. An unqualified `glPushMatrix` there
would push, wipe and pop the *texture* matrix and leave the sprites under whatever modelview stood. The batch
is empty at that call today, the only reason it never showed. The bracket costs three calls a flush on the
desktop and two in the browser, and is the cheap half of a belt and braces: a batch left *open* across the
weather block would still be drawn under the texture matrix the weather scrolls. What keeps that safe is
that `endSpriteBatch` runs long before it.

**`flushSprites` deliberately does not restore the current `glColor`.** A flush happens wherever state moves,
including the middle of somebody else's drawing: `Font::renderText` sets its shadow colour and calls
`drawText`, whose first act is a bind — so a restore repainted every text shadow in the last sprite's
colour. The other direction is worth knowing before the next renderer moves: the spec leaves the current
colour **indeterminate** after a draw with `GL_COLOR_ARRAY` enabled, so a strict reading has `renderText`'s
first shadow pass drawing in whatever the batch left. Measured, both targets keep it — llvmpipe answers
`GL_CURRENT_COLOR` unchanged, Emscripten writes `GLImmediate.clientColor` only from a `glColor*`.

**Browser colour has two quirks, and one of them is not cosmetic.** Emscripten truncates a `glColor*`
issued inside `glBegin`/`glEnd` to a byte, where the same call outside a block becomes a constant
`vertexAttrib4fv` at full float, and a float colour array is not truncated at all. So the shadow pass's
alpha of 0.35 used to arrive as 89/255 and now arrives as 0.35 — measured on the level editor, 3230 of
512000 pixels differ by exactly one, all inside a tile shadow. It follows that **`-nobatch` is not a
byte-exact oracle in the browser**, though it is one on the desktop; and that every colour the remaining
`glBegin` blocks set is still truncated down to the next 1/255.

The second quirk is the **clamp**. The game hands GL colours above 1 deliberately — `Level::renderShine`
takes `deathCountDown * 5.0` from an exploding bomb, the teleport swirl ramps its red to 2.1, and the three
spark bursts add half a level of red a tick until the particle has shrunk away, landing between 5.5 and 25.5
— and relies on the hardware to cut them off. Desktop GL clamps a primitive colour *before* multiplying the
texel; Emscripten does not (its generated vertex shader is `v_color = a_color;`, and the `clamp` it can emit
sits behind `GL_LIGHTING`, never switched on here). So the browser computes `clamp(colour · texel)` where
the desktop computes `clamp(colour) · texel` — at a red of 2.0 every texel above 0.5 saturates and a soft
glow comes out a hard-edged blob. `clampColor()` in `util.h` puts it back in the two places a colour reaches
GL uncut — `Engine::queueSprite` and `ParticleSystem::render`, both colour arrays — and **only in the browser
build**. A colour array is the only unprotected path: every `glColor*` spelling funnels into one `glColor4f`
that clamps on the way in, inside a `glBegin` block and outside alike, and a vertex attribute goes nowhere
near it. ROADMAP item 42 is how to stop paying for it on the CPU.

**Text is cached as laid-out geometry, keyed on what it was laid out with.** `Font::renderText` looks a
string up in a cache of 32 entries — the glyph quads and, in a batch of their own, the keycap frames, which
carry no texture — and draws them three times: twice as a shadow, once as the text. The key is the string
plus every option the layout depends on (`tabSize`, `charSpacing`, `lineSpacing`, `charScaling`, `italic`),
and deliberately not `shadows`, which changes nothing built. **That key is what lets `setOptions` leave the
cache alone.** It used to empty the whole of it whenever any of those five changed, and callers change them
constantly — a speech balloon sets `italic` and puts it back every frame it is on screen, the credits
animate `charScaling` — so one balloon threw away every cached string in the GUI twice a frame. Measured on
the help page: median frame's render half **3.1 → 2.5 ms**; menu 1.7 → 1.5, editor 0.9 → 0.8.

**The cache is budgeted in quads, and the budget is shared.** `QUAD_BUDGET` (8192, half a megabyte of glyph
geometry) replaces a cap of 32 *entries* per font — the wrong unit twice, since an entry is a
`std::vector<QuadVertex>` at 64 bytes a character, so thirty-two keycaps and thirty-two wrapped help pages
were the same number and two orders of magnitude apart, and four fonts holding 32 each was not one budget
but four. Eviction takes the oldest entry of any live font, from a registry `Font` keeps of itself, so a
font barely used stops holding what a busy one needs. The stamp is a counter and not `SDL_GetTicks()`, which
wraps at 49.7 days and then makes every standing entry look newer than every fresh one — a cache that evicts
what it has just built, for ever.

**`renderText` takes a `cache` flag for a string whose layout will not be asked for again**, a parameter and
never part of the key (which would double every entry asked for both ways). One caller uses it: the credits,
whose `charScaling` is `0.75 + 0.25 * alpha` and animated, so both draws build a key no frame will reuse.
Measured over six seconds of credits, **212 evictions → 0**. Everywhere else the cache was already working:
across the five oracle scenes plus the help page every lookup hits, with zero evictions, and the most it
ever holds is 825 quads / 52 KB. The memory was never the problem; the unit and the missing ceiling were.

**Measuring is cached too, in two tiers, and the first costs nothing.** `measureText` used to walk the string
every time, and was asked about twice as often as anything was drawn — `fitText` runs a binary search with
one per probe, `adjustText` one per run and per line. A laid-out string now carries its own dimensions, so
everything both measured and drawn is measured free: every GUI widget, each asking its caption's size in the
`onRender` that draws it. The second tier is for strings nothing draws — the runs `adjustText` wraps, the
candidates `fitText` probes — and holds dimensions and no geometry, which for those would be 64 bytes a
character that never reaches the screen. Measured, every measure in the five oracle scenes was a walk and
none is; the help page goes from 2448 walks to **66 of 2376**, and those are the deactivated `GUI_EditBox`
behind the page asking for character positions every frame.

**That path is uncached on purpose.** The positions depend on the `offset` the caller passes, which is no
part of the key, and there is one per byte of the string; the four callers are the edit boxes, of which one
is on screen at a time.

**The dimensions cache is budgeted in bytes of key** — an entry is two numbers, so what one costs is how
long its key is — and nothing comes near it (the help page holds 22 entries and 1.1 KB). `DIM_BUDGET` is
64 KB, a ceiling for the one shape that could grow without one: stepping through a campaign, where every
level measures a fresh set of `fitText` candidates and a folder of single levels has no length anybody
promised.

**Two orderings inside it are load-bearing.** The dimensions are measured *before* the geometry entry is
inserted, because `measureText` reads that same cache and an entry standing in it but not yet measured would
answer with whatever was in the field. And `lookUpText` holds a *copy* of its key across the build, because
`cacheKey` returns a reference into one buffer per font and the measure builds a key of its own into it.

**A render layer is a pass, and it has a name.** `renderlayer.h` holds the twelve `RL_*` that `Level::render`
walks in order, each a single bit, so an object's set is the OR of the ones it draws on.
`Object::getRenderLayers()` is a plain member behind an inline getter and deliberately **not** virtual:
asking costs a load, and — the real reason — a subclass cannot then answer differently from the `onRender`
it inherits. The mask may name a layer the object is not drawing this frame and may never omit one it is, so
`say()` and `flash()`, which draw from `Object::render` rather than `onRender`, add their bit and never
remove it.

`Level::renderObjects` skips an object whose bit is clear, which is most of them on most passes. **That is
worth almost nothing in milliseconds and was measured before it was built**: adding 27,720 no-op matrix
operations per frame costs 1.0 ms, so removing the 2,772 the old unconditional bracket spent was worth
0.1 ms. It earns its place as names rather than speed — and earned it immediately by making a dead pass
visible: `renderObjects(735, …)` walked all 84 objects with a matrix bracket and virtual call each, and no
`onRender` had ever handled 735.

**The trap it set is what `verify.py`'s `render_layers` check is for.** The values moved, so every surviving
magic number — `layer == 939`, `layer != 18` — became meaningless or the wrong layer; and C++ compares an
enum to an int without a word, so five such lines built on all three platforms and were never true. The
sprite texture stopped being bound for the lava passes, the wires lost their offset, the speech balloons
stopped appearing. The five palette levels caught it: `cat0`..`cat4` hold an instance of 60 of the 65 types
`instancePreset` knows, so walking them draws all but two of the `onRender`s in the tree — `Damage` and
`Projectile`, which the game spawns during play and no palette can place — and four of the five are
byte-identical across such a change. The fifth is `cat1`, whose two ConveyorBelts start their band at
`random(0, 6)` in the constructor, so it differs run to run unless seeded; `frames.sh` seeds it (`B5_SEED`,
and `cat1` is the tab its editor scene opens).

**A sixth place broke that no palette could have caught, and stayed broken for months.** `Electronics` sets
`RL_WIRE` in its own constructor and draws every connection on that pass; all thirteen parts then ran
`renderLayers = RL_MAIN` in theirs, which runs after the base and wipes the bit — so **not one wire was
drawn anywhere in the game** from the commit that named the layers. It compiles, it runs, and the only
symptom is a picture with something missing. The palettes are blind because `cat4` holds the parts and
nothing in it is *connected*, and so were all five oracle scenes. Two things close it: `verify.py`'s
`layer_bits` check, which knows which ancestor put bits in `renderLayers` and reports a subclass that
replaces rather than adds, and a clock wired to a light bulb in the `plain` oracle level.

**There are no display lists anywhere, and `verify.py` keeps it that way.** They were a second way of keeping
geometry beside these arrays, and one WebGL does not have — so every place that used one carried a browser
path under `#ifdef __EMSCRIPTEN__` and a stub in `gl_compat.cpp`. The `display_lists` check reports
`glNewList` and its six relations in either build: added back, one would compile on Windows, link on Linux
and misbehave only in the browser, the build nobody runs first. The three that used them are the tile grid,
the font and `Lightning`, whose two passes are built when the bolt is generated and drawn unchanged for the
forty frames it takes to fade — only colour and alpha move. `quadarray.h` is where the three meet:
`QuadVertex`, and one `drawQuadArray` so the client-state dance is written once rather than three times.

**Four upscale filters, each a class.** `upscaler.h` holds the base — a name, a texture filter, `present()`,
whether it wants a whole-number scale, whether it distorts the cursor, and its own `loadConfig`/`saveConfig`
— plus `PresentContext` (what the Engine owns and lends out: rect, frame texture, shared vertex buffer) and
`PresentProgram` (a linked program and the four uniforms *every* present shader has). They live in
`u_sharp.*`, `u_smooth.*`, `u_sharpfit.*`, `u_crt.*`; `u_all.h` pulls them in and `Engine` owns one of each
in display order. It is a normal game option, saved as `<Upscaler>` with the filter's own `getName()` — the
one name each filter has, shared by the config value, the radio button in `options.xml`, the startup log and
the test hook. `SharpFit` is the default.

- `Sharp` and `Smooth` are just `GL_TEXTURE_MAG_FILTER`, drawn by the base class's fixed-function quad.
  `Sharp` additionally snaps the blit to an integer scale (`wantsIntegerScale()`), the whole point of it.
- `SharpFit` (`src/u_sharpfit.cpp`) is nearest at a fractional scale, in one texture fetch rather than two
  passes; that file derives why, and why it **must** sample with `GL_LINEAR`.
- `Crt` is a CRT monitor: beam profile, scan lines, phosphor mask, halation, barrel distortion, rounded
  corners, vignette.

The two shader filters share the vertex shader (`upscaler.cpp`, the only place it is read), the vertex buffer
and `PresentProgram`'s four uniforms; `U_Crt` holds its own nine on top. **No filter carries a uniform it
does not have** — which is what the old twelve-slot struct did, and why `convergence` was once left unset in
two hand-written lists. A failure to link is fatal for all alike: all four are offered unconditionally, so a
driver that will not build one of these shaders cannot be left quietly showing three. The options dialog
ticks the filter in use and leaves the radio buttons where `options.xml` puts them — no show/hide/reflow
loop, no entry that can be missing.

**Anything that reads the rendered frame must bind the FBO itself**, and `Engine::encodeFrame` is the second
half of that rule: it binds the frame buffer before `glReadPixels` rather than reading
`GL_COLOR_ATTACHMENT0` of whatever stands bound. Its two callers inside the main loop have it bound already
— the screenshot key and the video recorder both sit in the `frameRendered` block, above the
`unbindFrameBuffer()` that precedes the present — but the test hook asks from the *frozen* branch, where no
iteration has rendered and the last present left the window's own viewport standing. What came back then was
the frame rasterized at 2x under a 1280x960 viewport, of which a 640x480 read takes one quarter, so **the
frame oracle compared a doubled quarter-screen for as long as it existed** — silently, because a doubled
quarter is perfectly reproducible. `frames.sh` now fails any capture whose every row *and* column pair is a
copy, the shape of an integer upscale whatever caused it.

The main loop binds it only on an iteration that ran a logic tick. Natively there is no other kind, because
the `SDL_Delay` at the foot stretches every iteration to at least one tick; in the browser
`requestAnimationFrame` sets the pace, so at 16.7 ms against a 20 ms tick most iterations render nothing and
the *screen* is bound, left from the previous present. That is what made every screen transition start from
black — the crossfade's one-shot capture of the old image is the only `glCopyTexSubImage2D` not already
inside a `frameRendered` block, so it read the default framebuffer, which WebGL clears before every frame. It
calls `bindFrameBuffer()` first now, right on either platform: the FBO holds the last frame that *was*
rendered, which is exactly the screen being faded out.

**The CRT filter** (`src/u_crt.cpp`). Everything that gives it its character is a `const` at the top of
that file, meant to be edited, and that file carries the reasoning and the measurements for all of it:
which monitor it imitates (`SCANLINE_PERIOD` — a VGA set at 1.0, the 240p console look at the shipped
2.0), why the mask sits in output and not source pixels, what `getOverscan()` is the sum of and why it
is exactly zero at curvature 0, the convergence model and its per-channel `rasterMask`, the halation
rings, the flicker terms, and what each of them measured.

Three things about it are not that file's business and live here:

- Six of the constants are runtime sliders instead, because they are matters of taste rather than
  tuning: Options → Scaling → *CRT settings …*, saved as `<CrtUpscaler scanline= curvature= bloom=
  flicker= scanFlicker= convergence=>` in `config.xml`.
- **Nobody finds a filter buried in an options dialog**, so `Menu.CrtPane` offers it once on a first
  start with a button that switches it on there and then. The marker is `.crt_offered` in the user
  directory, the same idiom as `.donation_asked` — absent on a clean install *and* after an upgrade,
  exactly the set of people who have not seen the filter. Skipped where the CRT filter is already in
  use, and it suppresses the donation window for that one start so the two never stack.
- **The barrel distortion goes through the mouse**, so it reaches `Engine`: `Engine::warpToSource` and
  `warpToOutput` forward to the filter, and the base class returns what it was given, so no caller asks
  what kind of filter is on. `CRT_CURVE_X`/`CRT_CURVE_Y` are `#define`d once and stringified into the
  GLSL *and* read as C++ doubles, so the shader and the cursor cannot drift apart.
**Restarting a level rewinds the tape**, but only with the CRT filter on: `CF_Rewind` (`cf_rewind.cpp`)
instead of `CF_Slices`, chosen by `crossfadeRestart` in `gs_game.cpp`. On sharp or sharp-fit the game does
not claim to be a tube and a tape effect would be a costume.

It exists because a restart is a cut — the game jumps from the current state to the level's first tick with
nothing in between — and a tape in search is the one machine that cuts like that and is forgiven for it.
**Every strip of a searched picture is read from a different place on the tape, which is to say a different
moment**, so a screen made of strips taken alternately from old and new image is not a trick standing in for
frames the game never had: it is what a recorder puts out. The rest follows — no signal between the tracks,
so bands of snow roll through; no lock for the vertical hold, so the picture rolls; the head meets each track
at an angle, so every line starts early or late; and VHS carries colour on a separate low-frequency signal
that does not survive the speed, hence the grey wash.

Two things are load-bearing. The on-screen display must **not** move or fade with any of it — it comes from
the recorder's own character generator, mixed in behind the tape path, and that one steady thing makes the
mess read as a machine. It is the 218x64 strip at (0,112) of `data/misc.png`, word in the left 162 pixels and
two triangles in the 56 beside it, so the blink is a source rectangle rather than a colour: the word every
frame, the arrows every other half-second, hard on and off, counted from the tick the effect began so they
start visible. And `ROLL_SCREENS` is a whole number, so the roll offset lands back on a multiple of the
picture height — zero — exactly as the crossfade ends; at 6.5 the picture would sit half a screen out and
jump straight when the effect stops. The last sixth eases tearing, snow and wash to nothing: the transport
braking and the servo locking.

**The sound is a granular resynthesis of a recording of a real transport.** Two attempts at synthesising one
failed, and the lesson is that **a matched spectrum is not a matched timbre**: the second matched the
recording's third-octave curve to a mean error of 1.3 dB and still sounded nothing like a machine, because
the recording carries narrow resonances standing up to 24 dB above its own noise floor and a third-octave
average is precisely the measurement that cannot see them. What ships is overlap-add: 30 ms Hann grains at a
7.5 ms hop, so four deep, each resampled 7.5% short and given ±1.5 dB of its own. Where a grain is *read
from* is what the gesture decides — the recording's own spin-up for the first 0.42 s, a random point in its
long steady stretch for the middle, its brake for the last 0.47 s. The shape is the machine's while no
stretch of the result is a copy of any stretch of the recording. `rewind.wav` is committed beside its `.ogg`
and is the source of record — no script rebuilds it, since that would need the recording. `CF_Rewind`'s
constructor plays it, so picture and sound cannot be had separately. It runs 1.75 s against the transition's
1.5 so the run-down is not cut off.

**xBR-lv2 was here and is gone**, with hq2x before it: both are edge-directed filters written for
flat-shaded pixel art, and this game's art is airbrushed and photographic. Every decision in xBR is a
`step()` against a threshold, stable when neighbouring texels are identical or plainly different and not when
they sit near it. Nudging a frame by 0–3 of 255 — about what the animated level does behind a semi-transparent
dialog — moved 1% of xBR's output pixels by up to 154, all on glyph outlines, while nearest, bilinear and
sharp-fit moved by exactly what the input moved. That was visible as text flickering.

**The window.** Resizable, aspect kept, black bars. **SDL's video flags are `SDL_OPENGL |
SDL_RESIZABLE` for the whole life of the process and must stay that way** — `DIB_SetVideoMode` keeps the
GL context only on its fast path, which requires flags and bpp unchanged and `SDL_FULLSCREEN` clear.
Setting `SDL_FULLSCREEN` or `SDL_NOFRAME` runs `WIN_GL_ShutDown` instead and takes every texture and the
FBO with it. So fullscreen is *not* an SDL flag here: `applyWindowStyle` sets the Win32 style to
`WS_POPUP` and the size to the desktop directly, SDL notices through `WM_WINDOWPOSCHANGED` and posts an
ordinary `SDL_VIDEORESIZE`, and `handleResize` — the one place that owns `displaySize` — picks it up.
Dragging the border and Alt+Enter run the same code, and nothing is ever destroyed. Alt+Enter is swallowed
so the game never sees a bare Return.

**A window that stops presenting loses control of what it shows.** While the app is inactive the main loop
skips logic and rendering but must still put the last frame up — `showLastFrame()` does that every 50 ms
(unbind, `presentFrame`, swap). A bare `SDL_GL_SwapBuffers` without drawing is not enough: it flips to the
other buffer and shows the frame before the last. And a full-screen popup is exactly the shape Windows may
hand a direct scanout path, after which the compositor's own copy stops being updated — with the Start menu
open over one, the game showed a frame from seconds earlier.

**Drawing while the border is dragged** needs a window procedure of the game's own in front of SDL's
(`Engine::hookWindowProc`), because `DefWindowProc` runs its own modal message loop and the main loop sits
in `SDL_PollEvent` until the mouse comes up. `engine.cpp` carries the mechanism and its traps — what may not
be called during a drag, and what has to be put back afterwards.

**The window's placement is saved on exit.** One `<Window positionX= positionY= sizeX= sizeY= maximized=
fullscreen=>` is written by `Engine::exit`; the position is the only part that can be absent, because on a
first start there is none and a 0,0 would be a claim rather than a fact.

**`GetWindowPlacement` and `SetWindowPlacement`, and never either paired with `SetWindowPos`.**
`GetWindowRect` on a maximized window gives the maximized frame, whose corners are negative because the
invisible grab handles count, so what is saved is `rcNormalPosition` — the rectangle "restore" goes back to —
with `showCmd` as the `maximized` flag. And `rcNormalPosition` is in **workspace** coordinates: the work
area, with the taskbar and any docked toolbar taken out, where `SetWindowPos` takes **screen** coordinates.
The two agree exactly while the work area begins at the monitor's top left corner, which is what a taskbar
along the bottom or right gives — and that is why putting one back with the other was invisible to almost
everybody. With the taskbar at the top or left, every save and restore shifts the window by its size, in the
same direction each time, until it has walked into the corner. `SetWindowPlacement` closes the round trip,
replays `showCmd` itself, and moves a window that would land on no screen back onto one — three hand-written
things gone, `MonitorFromRect` among them. `rememberWindowPlacement` logs both rectangles, the one place the
two systems meet and the only way to read a machine's work-area offset off a log.

**The same coordinate system has to reach the fullscreen path**, which is why `setFullScreen` asks
`rememberWindowPlacement` *before* it flips the flag rather than letting `Engine::exit` ask afterwards: in
fullscreen the window is the screen-sized popup with no windowed placement left to read. `applyWindowStyle`
therefore keeps only the style.

`handleResize` skips updating `windowedSize` while `IsZoomed`, so the remembered size is always the windowed
one — and the maximized state is replayed on both paths, at startup and on the way out of fullscreen. What
makes the second work is a line in the vendored SDL: `DIB_ResizeWindow` does its entire body inside `if (
!SDL_windowid && !IsZoomed(SDL_Window) )`, so the `SDL_SetVideoMode` that `handleResize` performs afterwards
moves and sizes **nothing** while the window is maximized. The one thing `applyWindowStyle` owes it is the
size the window actually became, read back with `GetClientRect`, instead of the `windowedSize` that
`setFullScreen` passed down: a window that has just come back maximized is the size of the work area, and
passing the windowed size on would resize the maximize away in the same breath as restoring it.

On first run, or when the stored size no longer fits, `getDefaultWindowSize` picks the largest integer
multiple of 640x480 leaving a 120px margin in *both* directions, so "sharp" starts with no black bars. 120 is
derived, not felt: the largest margin under which 1920x1080 still gets 2x (2*480 = 960 = 1080-120, nothing to
spare). The same value goes horizontally, where it is pure slack, because a taskbar is not always at the
bottom. `-windowed`/`-fullscreen` set the state for that start rather than overriding it for one run, since
`Engine::exit` always saves. In the browser the canvas fills the page (`WebBuild/pre.js`), Alt+Enter goes
through the Fullscreen API from a real DOM keydown — the main loop's own events do not count as a user
gesture — and the main loop reads the canvas size once a frame.

**On a phone the game takes the fullscreen itself.** Mobile Chrome has no button for it, so without this the
page is played under an address bar. `Engine::enforceTouchFullScreen` runs from a second DOM callback beside
the Alt+Enter one, registered for **both** `touchstart` and `touchend` and returning `EM_FALSE` so the touch
still belongs to SDL. It requests the fullscreen on every touch that finds the document not in it, which is
what makes it survive a swipe back out.

Both ends of the touch, because the API needs a *transient user activation* and a phone does not necessarily
grant one as early as touchstart — `touchend` is the event the HTML spec names for it.
`emscripten_request_fullscreen_strategy` hid that by deferring the request to the next handler allowed to
perform it, and a plain `requestFullscreen()` has no such second chance: dropping the strategy took the
fullscreen away on a real phone while headless, which grants activation at touchstart, kept working.
`b5_setFullscreen` therefore also asks `navigator.userActivation.isActive` first and stays quiet when there
is none. There is no extra tap to pay for it: the browser build already stops on "click to start".

**The fullscreen goes on the root element, never on the canvas** (`Module.b5_setFullscreen`, not
`emscripten_request_fullscreen_strategy("#canvas")`). A browser paints only the fullscreen element and its
descendants, so with the canvas promoted the on-screen pad — its sibling — disappears the moment the game goes
fullscreen. It still reports a full-size `getBoundingClientRect` while invisible, which is why a test that
measured it saw nothing wrong. From `<html>` both are inside, and the canvas is 100%/100% of the page anyway.

**That same callback resumes the AudioContext**, and not only `GS_Loading`. Going fullscreen turns the phone
to landscape, and the rotation makes the browser cancel the touch in flight — SDL never sees the press, so
`GS_Loading` does not know a gesture happened and waits for a second tap the player should not have to give.

Two conditions guard it. `Module.b5_isPhone()` in `pre.js` is coarse-pointer **and not** `(any-pointer:
fine)` — a notebook with a touchscreen has a title bar somebody wants, a phone has none — and it is one
function rather than two copies precisely because C++ asks it too. And the *browser* is asked whether it is
fullscreen, not `Engine::fullScreen`: leaving by a swipe does not tell the engine anything, so the member says
`true` while the page is windowed and `setFullScreen(true)` would return before reaching the API.

**The landscape lock hangs off `fullscreenchange`, not the request.** `screen.orientation.lock` is refused
unless the document is already fullscreen, so the other order simply rejects; `Module.b5_lockOrientation`
waits for the event and unlocks on the way out. Android-only — iPhone Safari has neither API — and it rejects
on a desktop, so every path swallows the failure. The manifest asks for landscape too, but only an installed
app gets that.

`WebBlueScreen::show` unregisters the touch callback. Otherwise the tap meant to reload the page would first
put the canvas back into fullscreen and the overlay would sit behind it — the very thing `exitFullscreen()`
at the top of that function avoids.

**Video recording** writes H.264 Baseline video and MP3 audio into an MP4, with no DLL involved:
`libs/minih264` encodes video, `libs/shine` audio, `libs/minimp4` writes the container, all vendored source.
Windows has decoded that combination natively since Windows 7 — container and H.264 since 7, the MP3 decoder
since Vista, and the MPEG-4 File Source documents its `'mp4a'` sample entry as meaning "AAC or MP3" — so a
recording plays on a clean install, which the old ffmpeg AVI did not. All three are plain C and were chosen
so an eventual Linux build can use the same ones. `videorecorder.cpp` does its own RGBX→YUV420 conversion
(the frame arrives from `glReadPixels` upside down) and holds each encoded frame back by one, because a
frame's duration is only known when the next arrives. minih264 needs the frame size to be a multiple of 16;
640x480 is.

**Recorded audio is a loopback capture of what the machine is playing**, not OpenAL — `audiocapture.cpp`
says why and how the two platforms differ. One thing about it is a build fact rather than an audio one:
libpulse is `dlopen`'d with its declarations written out by hand, so the build needs no libpulse-dev and the
game still starts where PulseAudio is absent.

**OpenAL is OpenAL Soft**, vendored in `libs/openal-soft-1.25.2` (headers, public domain) with its import
library in `libs/bin` and `Blocks5/OpenAL32.dll` — `soft_oal.dll` renamed, how that distribution is meant to
be used without the router. Because the app directory beats `system32` in the DLL search order, the game
always gets this implementation and never whatever Creative's 2009 installer left. The game calls only core
AL/ALC 1.1 (23 functions, no extensions, no `alGetProcAddress`), so the switch needed no source change. The
DLL is LGPL v2 and must stay dynamically linked.

**The mix is turned down, and that is not a taste setting.** A dozen effects and the music at full volume
summed above the ceiling and were clipped by OpenAL Soft — audible as distortion, in the game and in a
recorded video alike. `MASTER_HEADROOM` at the top of `engine.cpp` scales the finished mix before that
clamp, and the comment there carries the measurement and the two standards that pick the number. It belongs
in the source rather than the options because it is a property of the mixture, not a preference — the
player's own sliders are untouched and still read 100%.

**The sound files are repaired sources, and the mix decisions are not in them.** `Blocks5/data` holds a WAV
beside every shipped OGG, and `Tools/encode_sounds.py` produces one from the other **one to one** — 96 kbit/s
where libvorbis accepts it, stepping down where it does not (11025 Hz mono tops out at 48). Where a sound
should play quieter than its file, that factor lives in `data/sounds.xml` and is applied at playback: `Sound`
looks itself up once at construction and `SoundInstance` multiplies it into the single `alSourcef(…,
AL_GAIN, …)` call, so it covers `slideVolume` and every caller that sets a volume itself.

That split exists because the alternative had already failed silently. Eight OGGs had been exported at a
reduced level while the WAV beside them kept the loud original, so the intent lived only in the compressed
file: re-encoding from the source would have made `ricochet` 6.8 dB louder, `push` 5.1, `thunder` 4.6.
Measuring it back out needs the right comparison — the shipped OGG against a *freshly encoded* one from the
same WAV, since WAV-against-OGG folds in the encoder's own frequency-dependent loss, the same order as the
smallest of these factors (`syringe` at 0.914).

Three things belong in the WAV instead: no DC offset, endpoints on zero, nothing clipped. A 20 Hz high-pass
takes the first — measured, it costs at most 0.8 dB of BS.1770 loudness while removing up to 6.6 dB of RMS,
because what it removes is inaudible. Half-cosine fades of 5 ms take the second, **except on the eight looping
sounds** (`conveyorbelt`, `elevator`, `gas`, `laser`, `mask`, `rain`, `thunderstorm`, `toxic`), where the end
*is* the beginning. Those also need the high-pass convolved **circularly** rather than linearly: a looping
sound is periodic, and the filter's transient otherwise droops both ends and made the seam 10–12 dB worse. The
third cannot be repaired — clipped peaks are gone, and getting back under full scale means lowering the level.

**Input** is two-layered. Physical keys / joystick axes / hats map to *virtual keys* (`VirtualKey`), and
named *actions* (`"$A_LEFT"`, `"$A_PLANT_BOMB"`, …) bind a primary and secondary VK. Gameplay queries
`wasActionPressed(name)` / `isActionDown(name)`; bindings are registered in `main.cpp` and remappable in the
options dialog, where *Reset selected* and *Reset all* work off `Action`'s `defaultPrimary` and
`defaultSecondary` and grey out without a selection.

**Any key and any click leave the pause**, not only the pause key — `wasAnyKeyPressed` and
`wasAnyButtonPressed` read the same per-tick bits. Coming back from another window is what makes it worth
having, since `onAppLoseFocus` pauses and the click that returns is then the one that resumes. The press is
*spent* on resuming, and that ordering is the trick: the resume sits in front of the action chain as its
`if`, so the pause key cannot switch back on in the same tick what it just switched off.

**Waiting for a key is a state, not a loop.** Clicking a key button sets its caption to `$O_PRESS_KEY` and
calls `Engine::beginKeyGrab()`; `Options::onUpdate` asks `pollKeyGrab()` each tick and applies the answer —
the pressed VK, `GRAB_NO_KEY` for Escape (which clears the binding and is the only way to leave an action
unbound), or `GRAB_TIMED_OUT` on the three-second deadline, which leaves it as it was. Waiting costs nothing
on purpose: that is what somebody does who opened the grab by accident, and it must not take the key they had.

A blocking loop around `SDL_PumpEvents` and `SDL_Delay` — the obvious shape, and what this was — **cannot
work in the browser**: the event queue is filled by DOM listeners on the JS thread, which only run when C
returns to the page, which is why `emscripten_set_main_loop_arg` calls `mainLoopIteration` once per frame. A
loop that never returns never sees a key, and a second main loop does not help either
(`emscripten_set_main_loop` either unwinds the wasm stack by throwing or returns at once).

**While a grab runs, the keyboard belongs to it.** `Engine::update` skips `updateActions()` and calls
`flushInput()` — otherwise binding F1 would toggle mute on the way past, and the cancelling Escape would
reach the GUI and close the dialog. The tick in which the key is *found* still counts as part of the grab
(hence the remembered flag, not the state after `updateKeyGrab()`), or the new binding would fire its own
action immediately. Nothing stale is left behind: the main loop clears every action's pressed/released bits
each tick regardless. One quirk in `flushInput()`: Emscripten's `SDL_PeepEvents` takes the SDL 2 argument
shape *and* asserts `requestedEventCount == 1`, so that branch fetches one event per call.

**A binding is stored in `config.xml` by name, not by number.** A VK is an index into `virtualKeys`, and that
index moves: the keyboard block is `SDLK_LAST` long, 323 under SDL 1.2 and 1536 with Emscripten's headers, so
every joystick entry after it sits somewhere else — and the joystick entries depend on what was plugged in at
startup. `VirtualKey::id` is the stable spelling written instead: `key:LEFT`, `key:KP_ENTER` from a table of
the 136 SDL 1.2 key names that resolve to whatever constant the current build means, and the
already-structural `Joystick1 B3` / `Joystick1 A2+` / `Joystick1 H1NE` for the rest. Reading tries the number
first, so a pre-1.2.0 config still loads and is rewritten by name on the next save. An id that resolves to
nothing — a joystick not connected — becomes "unassigned" rather than a wrong key.

**Game states** are a stack. Each derives from `GameState` (`gs_*.cpp`: Loading, Menu, SelectLevel, Game,
LevelEditor, CampaignEditor, Credits) and is registered by constructing it — the base constructor calls
`Engine::registerGameState`. Transitions go through `setGameState`/`pushGameState`/`popGameState` by string
name with an optional `ParameterBlock` context, applied at a safe point by `processGameStateChanges()`, not
immediately.

**Level and objects.** `Level` (`level.cpp`, ~61k) holds two tile layers plus a vector of `Object*` and a
spatial hash (`hashObject`/`getAllObjectsAt`). `Object` (`object.h`) is the base for everything dynamic;
behaviour is driven by an `OF_*` flag bitmask (`OF_MASSIVE`, `OF_GRAVITY`, `OF_DEADLY`, `OF_ELECTRONICS`, …)
plus virtual `onUpdate`, `onRender`, `onCollision`, `move`, `reflectLaser`, …. `StdObject` covers the plain
sprite cases (blocks, diamonds, grass), so most simple types need no new class. `Level::update()` is the tick
order: remove/add pending objects → `frameBegin()` on all → `update()` on all → `Electronics::updateAll()` →
particle systems → AI-trace decay → exit check.

**Nothing in the render path draws a random number**, and the reason is not the one it looks like. **The loop
renders at most once per tick**: `timeProcessed` is zeroed at the top of each iteration and only raised inside
`while(timeToProcess >= logicRate)`, and the render is gated on it — so a machine that cannot keep up renders
*fewer* frames than it runs ticks, and one that flies cannot render more. What such a draw really costs is the
*shared generator*: a shipped build has no per-frame reseed (the one in `render()` is inside
`BLOCKS5_TEST_HOOKS`), so every draw the renderer makes shifts the sequence the logic then reads, by an amount
depending on how many frames the machine dropped and how much was on screen. Thirteen `onRender`s did it, and
`Level::render` twice more for the night vision's noise offsets. `Object::glowJitter` is one value in [-1, 1]
redrawn in `frameBegin()`, and `noiseOffset1`/`noiseOffset2` in `Level::update()` — both once per tick, the
same place and for the same reason the flash decays there.

**`Level::renderBeamShines` was a fourteenth.** Handing it the object's own `glowJitter` instead *looks*
wrong: one value for the whole object makes every point of the beam breathe in unison, which reads as the beam
pulsing rather than light scattering along it. `pointJitter(seed, index)` hashes the per-tick value with the
point's index — `fract(sin(x) * 43758.5453)`, no state — so the jitter differs point to point as it always
did, and the beam draws nothing from the shared generator.

One jitter per object and not one per use: an object's own draws in a frame move together, which nothing can
see, while different objects stay independent, which is what reads as a field of lights. It is drawn for
*every* object rather than only the ones that glow, because a draw from the shared generator has to happen the
same number of times whatever is on screen, or a frame stops being reproducible from a seed. An object never
updated — an editor palette, a level select preview — keeps the 0 it was built with, exactly the brightness
the caller asked for.

**Three seeded streams, not two.** `Engine::render` and `Engine::update` each reseed from `testSeed()` and the
scene's tick — the odd half and the even half — which makes a frame reproducible however many renders fit
inside one 20 ms tick. A **level load** falls between ticks, where neither reaches, and a level's objects draw
from the generator in their constructors: a `Diamond` is `setAnimation(4, 5)` over an `anim` starting at
`random(0, 100000)`. So the phase every object started on continued a sequence whose length depended on how
many frames the machine had managed on the way there, and the same diamond stood on a different animation
frame from run to run — invisible for as long as the oracle was capturing a quarter of the screen.
`Engine::seedForLoad` is the third stream, called from the `Level::load` both overloads come through, offset
`0x40000000` clear of the tick's two. It is declared without a guard because `BLOCKS5_TEST_HOOKS` does not
reach `level.cpp`, and is an empty function in a normal build.

**All of it is night-vision-only**: `Level::render` walks `RL_LIGHT` inside `if(nightVision && !inEditor)`, so
a level without night vision draws no shines at all. The one place a per-frame random is still right is
`CF_Rewind`: a tape's snow, tracking jitter and seam shift belong to an analogue signal synchronised to
nothing — which is also why a rewind transition cannot be captured by a byte-exact oracle.

**Something that reacts lights up.** `Object::flash()` sets `flashAmount` to `FLASH_STRENGTH`; `frameBegin`
decays it by `FLASH_DECAY` per tick and `Object::render` draws the object's own sprites over themselves once
more, additively — about eight ticks, a sixth of a second. It is the acknowledgement a switch gives when
pressed, and the two counters at the bottom left give the same one when a diamond or bomb is collected:
`Player::addInventory` is the single funnel both go through, so it calls `Level::flashHudIcon` there, and
`GS_Game`'s HUD pass draws the preset a second time under the same additive blend. The two constants live in
`object.cpp` and are `extern` so the icons cannot drift away from the objects.

**The diamond machine** (`diamondmachine.cpp`) takes a block apart into sparks in its own colours, sampled
texel by texel through the debris mechanism, and brings more sparks back in to build the diamond; an aborted
conversion runs them backwards. The file carries the derivations — the travel formula the outward cloud and
the inward aim are both solved from, why `n` is the number of *moves* rather than the lifetime, and how the
abort (`DiamondMachine::abortConversion`) mirrors lifetime and damping. The block fades to
`CONVERSION_GHOST` over the conversion, and the outward sparks are dust rather than embers, so `OUT_BRIGHT`
and `OUT_END` are both 1 and only the opacity falls. Two things reach outside that file and break easily:
**`Particle::id`** is the one field that stays 0 everywhere else in the game, since the machine stamps its own
sparks per conversion and finds them again through `ParticleSystem::begin()`/`end()`; and `Particle` has a
constructor that zeroes every member, because the forty-nine callers of `addParticle` build one on the stack
and set only what they need, so a field none of them knows about would arrive as a random number. The sprite
must be the neutral white disc at (32,32) in `particles.png` — a particle is multiplied by its texture region,
and (32,0) is a pre-coloured orange that turned every cyan spark olive.

**The hint note** (`hint.cpp`) flies a 300x400 sheet of paper to the middle of the screen and unrolls it. The
geometry, shading and draw order are derived in that file, around `ROLL_TURNS` (0.50 — half a turn is a hard
painter's-order limit, not taste), `ROLL_BANDS` (48), `ROLL_LENGTH` (0.30), `PERSPECTIVE`, `VIEW_OFFSET_X`
(300), `SHADE_EDGE` (0.75) and `FADE_UNTIL` (0.5). What is worth knowing from outside:

- Paper and text are baked *together* into one texture (`Engine::acquireOffscreenTexture` +
  `beginRenderToTexture`) so the writing belongs to the sheet and flies, turns and rolls with it. It is
  composed with `glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)`,
  which leaves the colour premultiplied, so it is drawn again with `(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)`.
- **The texture belongs to the Engine, not the note**, and that is not tidiness: it falls with the framebuffer
  object, which `Engine::exit` destroys while the GL context still stands, whereas an `Object` is destroyed
  only after `main()` has returned. It is a **pool** because two notes overlap while one fades out, and
  sharing one texture meant a full render-to-texture, an FBO switch and a read-after-write stall per visible
  note per frame.
- **At rest the sheet is drawn at exactly 1:1 on whole pixels**, which is what `SNAP_RESIDUAL` is for: an
  exponential ease never arrives, so scale, angle and position are rounded once the residual is under half a
  pixel over the screen diagonal. Measured off a screenshot, 17% of outline pixels carried the font's own
  colour before, 58% after.
- **Whether it rolls at all belongs to the artwork**: a marker file `hintscroll.txt` beside the `hint.png`
  that is *actually loaded* — contents ignored, existence counts, resolved by `getSkinFilename` so it follows
  `default_hint.png` to wherever the picture really came from, and answered through `Level::isHintScroll()`.
  Beside the image and not an attribute in `tileset.xml`, because `<Level skin0=… skin10=…>` picks each slot
  separately and a flag in the tileset would describe a different file. It must be **named** in the packing
  scripts rather than swept up as `*.txt`, since `password.txt` is deliberately packed unencrypted in a second
  pass; in `pack.sh` the name is tested for first, because `packInto` drops only *patterns* that match nothing
  and a plain filename survives an empty glob, which broke the `space` archive once.
- **Return and Escape put the note away** without walking off the field. `Object::dismiss()` is a virtual
  answering false everywhere except a hint currently showing something, and `Level::dismissDisplay()` asks the
  objects on the player's own field. The key must be caught in `GameGUI::onKeyEvent` and not `Hint::onUpdate`,
  because `GUI::update()` runs before `p_gs->onUpdate()` and the game menu would already be open; Escape
  therefore asks `dismissDisplay()` first and falls through to the menu only when nothing took it.
- The mesh is a `GL_TRIANGLE_STRIP` and **not** a `GL_QUAD_STRIP`: WebGL has no such primitive, and
  `WebBuild/gl_immediate.cpp` hands the mode straight to it. That applies to any strip added anywhere.
- `Hint::onCollect` is deliberately empty, existing solely to stop `Object::onCollect` making the note
  disappear. And a bake that fails — out of texture memory, a lost context — draws nothing that frame rather
  than falling back to a flat sheet; `bakeNote` runs again on the next.

**Presets are the object factory.** `presets.cpp` maps a type-name string to a constructed `Object` in one
long `if/else if` chain (`instancePreset`), plus a `texCoords` table for the editor's sprite. Adding a type
means: write the class (if `StdObject` won't do), add sprite coords and a branch in `presets.cpp`, override
`saveAttributes` to round-trip its XML attributes, and place an instance in the right `data/cat<N>.xml` so it
appears in the editor palette (the palettes are themselves Levels, `p_cat[0..4]`).

**Electronics** (`electronics.cpp`, `pin.cpp`, `e_*.cpp`) is a small wire-level simulation layered on objects:
parts expose input/output `Pin`s, connections are saved separately from ordinary attributes
(`saveConnections`/`loadConnections`), and `Electronics::updateAll` propagates values each tick with an
undefined state for unconnected/unsettled inputs.

**Virtual filesystem.** `FileSystem::openFile` serves either a real file or a member of a zip archive,
selected by path syntax: `archive.zip/file.png` (no password), `archive.zip<plaintextpw>/file.png`, or
`archive.zip[encryptedpw]/file.png` (password encrypted with `PWEncrypt`; see `decryptPassword` in
`util.cpp`).

**`renameFile` renames where the platform can and copies where it cannot** — across a mount, since the browser
stages an upload outside the home directory, and for a member inside an archive, which has no name on disk.
The destination gives way only after the first attempt has failed: POSIX replaces it in one atomic step and
deleting it beforehand would open a window in which neither name exists, while Windows refuses the replacement
and needs the second try. Three callers want exactly that — `retireShadowingCopies`; `Campaign::save`, whose
swap otherwise wrote the whole archive a second time; and the progress database's crash-safety file.

`pushCurrentDir`/`popCurrentDir` maintain a search root, which is how `main.cpp` mounts `data.zip[...]` as the
asset root (the commented-out `fs.pushCurrentDir("data")` beside it switches to loose files for development).
User-writable state — saves, progress, custom levels, screenshots, videos — lives under
`getAppHomeDirectory()` = `My Documents\Blocks 5\`, never next to the executable.

**Levels, campaigns and skins have two roots, and the game folder wins.** What ships stays beside the
executable and is read from there, so it is always as new as the program; the user directory holds only what
the player made or imported. `FileSystem::resolveContentPath("levels/skins/space.zip")` asks the game folder
first and falls back to the user directory, and `isShippedContent` — "it exists in the game folder" — is
simultaneously the definition of undeletable, un-overwritable and un-saveable-over. The paths are absolute on
purpose: a relative one would be resolved inside the mounted `data.zip`.

The order matters and the other one is wrong. User-first would let a stale copy shadow a fresh shipped file:
every installation used to carry a private copy of the campaign, the four skins and the two examples, frozen
at whatever version it first installed, and that is how a skin marker went missing on a machine that had just
built the current sources. Game-first has one cost, and it is why both editors refuse to save under a shipped
name: such a file could never be loaded again.

**Seven files belong to the player even though they ship with the game**, and for them the order is reversed:
`FileSystem::getPlayerFiles` lists the two example levels and the five `readme.txt`, `belongsToPlayer` makes
`isShippedContent` say no for them, and `resolveContentPath` looks in the user directory first, falling back
to the game folder's copy as a template. The two halves belong together — allowing the save while still
answering from the game folder would write a file that could never be read back. The list is hard-written
rather than a directory listing because a working tree also holds the forty-two campaign sources.

Only the five `readme.txt` are *copied* on a first start, and only because nothing in the game reads them:
without the copy they would sit in no folder at all. The examples are not copied — they are listed and
loadable straight out of the game folder, and the player's own version appears the moment they save one, so an
untouched installation keeps getting the newest examples and Delete stays greyed until there is something of
theirs to delete. That is why the Manager asks `Transfer::isRemovable` ("is there a copy in the user
directory") rather than `isBuiltIn`.

**On the first start of 1.2.0 the old copies are set aside.** `retireShadowingCopies` in `main.cpp` renames
every file in the user directory whose name the game folder also has to `<name>.bak` — renamed and not
deleted, because there is no way to tell from outside whether somebody edited one. `.bak` is inert everywhere:
every lister filters on the exact extension, and `convertPath` recognises an archive by `.zip/`, not `.zip`.

**`ProgressDB` keys on the campaign's bare filename, not its path**, which is what made the move survivable:
the key used to be the full path, so shifting `blocks.zip` from the user directory into the game folder would
have silently reset everyone's 42 levels — no error, nothing in the log, just a progress bar back at zero.
`keyFor` strips the directory on the way in and out, which migrates an old `progress.zip` by reading it. Case
is **not** folded there, deliberately: under Linux `Blocks.zip` and `blocks.zip` are two different campaigns,
and joining their solved sets could never be undone.

**It holds nothing.** `query()` reads the file and `markSolved()` reads it, adds and writes it back; there is
no map that lives for the process. That is what makes the Manager able to import, merge and delete a progress
at all — a copy in memory would answer from what was there at startup, the next completed level would write it
straight back over the import, a delete would undo itself within one level, and merging would need a `clear()`
the class never had. Merging then needs no code of its own: read the imported file and mark everything solved.

The reads are per frame — `getLevelStatus` and the progress bar are both inside `GS_SelectLevel::onRender` —
so the screen keeps the answer while it is shown and re-reads in **`onGetFocus`, not `onEnter`**: coming back
from a played level is a *pop*, and `popGameState` gives the state underneath the focus without entering it
again, so a level just solved would still be shown unsolved. The count is clamped to the campaign's length, or
a merged database would draw the bar past its own frame and label it *45/42*.

**A save no longer destroys what it is replacing.** Writing a member into a zip rebuilds the archive, and
`File_Archived` removes the old file before the new one exists (`remove` at `file_archived.cpp:526`), which
for a one-member archive is every save — so a crash or full disk in that window took everything. The database
is renamed to `progress.zip.saving` first and that file deleted only once the new one stands; `query()` puts
it back where the real file is missing or unreadable, and deletes it where the real file reads. The invariant:
**the backup exists exactly while a save is in flight**, so one found lying about is from a run that died, and
leaving it would mean the next unrelated fault restores a database months out of date.

That parse trusts nothing, because the Manager imports this file and it is therefore a stranger's: no root
element, no `campaign` attribute and a level index that is negative or absurd are all skipped rather than
crashing.

**A level somebody sent you is played from the level select screen, not from the editor.**
`Campaign::loadSingleLevels` builds a campaign that exists as no file: every loose `*.xml` in the
user's level folder, listed last in the campaign box under `$LS_SINGLE_LEVELS`. Opening such a level
in the editor was the only way before, and the editor gives the puzzle away by design — `level.cpp`
skips the darkness there (`if(nightVision && !inEditor)`) and `teleporter.cpp` draws a line to every
teleporter's destination.

It carries **no progress**, and that is what `isSingleLevels()` is asked about in five places: every
level is unlocked, finishing one records nothing, the run ends back at the selection instead of at the
next level, and both the *next to do* button and the progress bar — frame, label and all — are hidden.
A bar that can never move reads as a fault, not as an empty one. Levels that have nothing to do with
each other have no order to earn.

The list is sorted by the **localized title**, not by filename: that is the line the player reads.
Getting it means parsing each level's XML for the one `title` attribute (`readLevelTitle`), cheaper
than a `Level::load` with all its objects and skins but still a parse per file at dialog entry. The
caption is `formatSingleLevelCaption` — `Title (filename.xml)` — because three levels called *Unnamed
Level* are otherwise indistinguishable, while inside a campaign the filename would say nothing but
`level_2.xml`.

**The whole select screen is keyboard-operable.** Left and right step through the levels, Home and End
jump to the ends, Return plays, Shift+Right is *next to do*, and up and down change the campaign. The
last four go through `GS_SelectLevel::onUpdate` only while the campaign list does **not** hold the
focus, because those are exactly the four keys `GUI_ListBox::onKeyEvent` handles itself; left, right and
Return are unconditional, since the list ignores the arrows and forwards Return for want of a submit
button. Everything runs through `pressButton`, which asks `isActive()` and `isReallyVisible()` first —
otherwise Return would start a locked level the mouse cannot even click.

**One Manager button in the main menu** opens a dialog that imports, exports and deletes, on all three
platforms — `src/transfer.cpp` over `WebBuild/web_transfer.cpp` in the browser, `GetOpenFileNameA`/
`GetSaveFileNameA` under Windows and `zenity`/`kdialog` under Linux, behind one interface: `beginImport`
starts it and `pollImport` is asked each tick, so an asynchronous dialog and a modal one look the same
to the caller.

`Menu.ManagerPane` holds the five kind radios in 92px columns, the list, *Refresh*, and a bottom row of
*Import*, *Export*, *Delete* and *Close* spread over the window's width. The two rows span the same
x=10..486 without sharing a grid: the captions decide the first width — "Zusammenfuehren" and
"Aktualisieren" are what the 92 is for — and forcing that grid on the second would leave a hole where a
fifth button would be. Import needs no selection and comes first; Export and Delete work on the
selection and grey themselves out without one.

**The progress database is the fifth kind, and it goes round the directory machinery rather than through
it.** It is one file, with one name, in the user directory *itself*, and nothing of the sort ever ships —
so `directoryFor` names it outright. An empty subdirectory would have been the obvious answer and is a
trap: `list()` would then read the game folder's own root, where `data.zip` lies, and `remove()` would
point at whatever it found there. `classify` recognises it by a `progress.xml` inside the archive, which
a zip's table of contents answers without the password, and `install` refuses one that does not parse —
the same guard a campaign has, so a damaged file cannot destroy a good one of the same name.

`Menu.ConfirmPane`, which must stay the **last** child in `menu.xml` so it draws last and takes the
clicks, asks before a delete and before an import replaces anything. Three columns, of which the middle
carries *Merge* and is shown only for a progress database, so *Yes* and *No* keep their places either
way; the text is wrapped, since the code sets it and a filename can be any length. **The two buttons are
renamed for an import** — *Replace* and *Cancel* — because yes and no are no answer to a question that
offers replacing and merging.

Asking before an overwrite needed somewhere to ask *from*: `install()` composed the destination name
inside itself and tested for the overwrite two lines before the copy, so no caller could put the question
first. `Transfer::targetName` and `wouldReplace` are that answer and `install()` is built on the same
two, so the name asked about and the name written cannot drift apart. The whole import waits for the
answer, `finishImport()` included — in the browser that call deletes the staging file the bytes are in.

**`Transfer::isBuiltIn` no longer keeps a list**; it asks whether the file exists in the game folder —
and answers no for the seven files that belong to the player. Three callers, all the same rule: an import
must not take such a name, and neither editor may save under one. Delete goes through `isRemovable`
instead, the stricter question. `Transfer::list` returns the union of both roots, sorted, and needs no
rule for a name in both because no path can create one. Case is the file system's problem rather than a
hand-rolled comparison's, which is right: on Windows `Blocks.zip` *is* `blocks.zip`, and `fileExists`
says so.

**A finished import updates the open list.** `pollImport` runs every tick from `onUpdate`, because the
browser's file dialog cannot be modal — so when it completes with the Manager still open, it switches the
kind radio to whatever `classify` decided, re-reads the list and selects the new entry. That is why the
pane deliberately stays open across the file dialog, export included: the Manager is a place you keep
working in. Escape belongs to the topmost pane: the confirmation first, then the Manager, and only with
both closed does it quit the game.

**Import takes one file and works out what it is** — `Transfer::classify`, by content and never by
extension: `OggS` at the front is music, an XML whose root is `<Level>` is a level, and an archive is a
campaign if it holds `campaign.xml` or a skin if it holds `tileset.xml` and `sprites.png`. Anything else
is refused. The browser stages the upload outside the home directory (C hands JS all three possible
staging paths and JS picks one by extension, so C still composes every path), `sanitizeFilenameStem`
reduces the name to `[A-Za-z0-9_-]`, and only then does anything reach IndexedDB.

**An import replaces a file of the same name**, for all four kinds alike, and `Transfer::install` is the
whole rule: sanitized stem, plus the kind's extension, plus a copy. Not a swerve to `stem_2`, because a
skin's filename *is* its identity — a level says `skin0="space"` and `Level::getSkinFilename` looks for
`levels/skins/space.zip`, so `space_2.zip` would leave every such level exactly as broken, only without a
visible cause — and the weaker form holds for the rest: a new version of your level means *your* level. A
re-imported campaign therefore keeps its progress, since `ProgressDB` keys on the filename.

The one refusal is `isBuiltIn`. `install` reports through `bool* p_replaced` whether it landed on an
existing file, so the toast says **Replaced** rather than **imported** — the only sign the player would
otherwise get that something of theirs is gone. A campaign is checked with `isImportableArchive` *before*
the copy, so a damaged archive cannot destroy a good one of the same name.

**A scrolling texture offset is reduced to one period, and that is a phone bug.** `wrapTextureOffset`
(`util.h`) is called on all five scrollers — the menu's title clouds, the level's rain, snow and clouds,
and the lava — because each scrolls by an offset that had been growing since the level began. A texture
coordinate reaches the fragment shader as a *varying*, and the shader Emscripten's GL emulation builds
opens with `precision mediump float;` with the texcoord varyings under it: ten mantissa bits, which a
desktop GPU implements as fp32 and a phone actually honours. The step it quantizes to is
**offset/2048 texels**, so the clouds — moving one texel a tick — drift smoothly for about forty seconds
and then go visibly steppy, and the rain, at twenty texels a tick, crosses the same line in two seconds.
Nothing is wrong on any desktop, which is what makes it hard to see.

Subtracting whole periods is **exact** under `GL_REPEAT`: it moves the finished coordinate by a whole
number and samples the same texel. Verified against the real matrix order — bind's `1/w,1/h`, the scale,
the translate and the rotate — for the four weather scrollers, deviation 0.000e+00 at offsets up to
900000. Two things to keep right: the wrap goes **after** the `sin` that reads the same offset, whose
phase has to follow the unwrapped value, and the period is the *texture's* own size, since a skin brings
its own art.

**The lava is the one whose period is not the texture**, and getting it wrong is a jump of half a tile.
Its four cousins hand GL a texture matrix; `Lava::onRender` writes the texels into `glTexCoord2d` itself,
on a 16x16 sub-texture cut out of the skin's sprite sheet by `createSubTexture` — a real 16x16 texture of
its own, so `GL_REPEAT` wraps at 16. But the front pass halves the *whole* coordinate (`t /= 2.0`) before
it draws, so a jump of 16 moves that pass by eight texels and only 32 moves it by a period.
`SCROLL_PERIOD` is therefore twice the tile, also exact for the back pass at two periods. The `shift`
beside it is `2·sin(0.1·anim)` and `3·cos(0.05·anim)`, and `anim` stays unwrapped for it: neither period
divides 32, so wrapping what feeds them would jog the wobble every time it came round. Measured over
`anim` 0..900000, both signs and both axes, the sampled fraction agrees to 4e-12 of a texel; the same
wrap at 16 puts the front pass out by exactly 0.5.

**The angle those sines are given needs no such care**, and the arithmetic is worth having once: they are
`double` throughout, so one ULP at argument *A* is `A/2^52`. The snow's argument grows at 0.2 rad/s and
its sine is scaled by 500 pixels, so half a pixel of error needs 1.7e-3 rad and arrives in about **700 000
years**; after 25 days of rain — the fastest — one ULP is 9.6e-9 rad. What runs out first by a wide margin
is the millisecond counter feeding it: `Level::time` is `int` and undefined after **24.9 days** in one
level, `GS_Menu::time` and `Engine::time` are `uint` and wrap at 49.7. All three reset on entering a level
or the menu. In `float` the same rain argument would have a ULP of 4 radians, the same distinction as the
`mediump` one above, two steps further along.

An imported skin also needs `Texture::applyWrapMode`: WebGL 1 samples a non-power-of-two texture as pure
black unless its wrap mode is `GL_CLAMP_TO_EDGE`, silently and with no GL error, and the default is
`GL_REPEAT` — which rain, snow and clouds genuinely need, since `level.cpp` scrolls the texture matrix
without bound to tile them. So the wrap mode is switched for NPOT textures only, precisely the set where
`GL_REPEAT` could never have worked. The game's own art is all power-of-two; this exists for imported
skins alone.

**What export writes is a plain copy.** That matters for skins: three of the four shipped ones are packed
with a password, and decrypting them on the way out would be a back door around the very protection they
are packed for. The recipient cannot open such an archive — but can still *use* it, because the password
rides along inside it as `password.txt` and `Level::getSkinFilename` reads that out of any skin archive
whatever its filename. A skin somebody made themselves has no password anyway, and that is the one people
actually share.

**A level can borrow the shipped campaign's music**: `musicFilename="blocks:music2.ogg"` resolves through
`Campaign::resolveMusicPath` to `levels/campaigns/blocks.zip[pw]/music2.ogg` instead of a file beside the
level, and `Campaign::save` deliberately does *not* pack such a track — it is already on every machine.
Without it a browser author has no music at all, since nothing can put an `.ogg` next to a level.

**Images** are decoded by `img_load.cpp`, not SDL_image. The game needs exactly one function from it —
`IMG_Load_RW`, called from `texture.cpp` and for the window icon — and reads every texture through its own
`SDL_RWops` over the encrypted `data.zip`. stb_image (`libs/stb`, one header) does that in about eighty
lines; PNG and JPEG are enabled, and every image the game ships is a PNG.

**Writing one needs no library at all**, and `img_save.cpp` is the mirror image of that reasoning: decoding
PNG is hard, encoding it is not, because zlib does the work and zlib is already compiled into all three
builds for minizip's sake. `compress2()` returns exactly the zlib datastream an `IDAT` chunk is defined to
hold and `crc32()` is the checksum every chunk carries, so the whole encoder is a signature, three chunks
and a filter loop. Screenshots are PNG because of it — `SDL_SaveBMP` and its 900 KB files are gone.

Two things there are worth knowing. The per-row filter heuristic — pick the filter whose bytes have the
smallest sum of magnitudes read as signed — is twenty lines and worth them: measured on a real 640x480
screenshot, 239 KB against 283 KB for no filtering, out of a 900 KB bitmap. And **the alpha channel is not
free**, the opposite of what it looks like: `glReadPixels` must ask for `GL_RGBA` because that is the only
combination WebGL 1 allows, but a channel of nothing but 255 does not collapse in the deflate — it pushes
every prediction one byte apart. The same frame is 330 KB as RGBA and 247 KB as RGB, so the encoder takes a
source layout and a destination layout separately and drops the channel while building each row, before the
filter ever sees it.

**The browser gets screenshots too**, and this is what unblocked them: the two reasons F11 was refused there
were `GL_BGR` and `SDL_SaveBMP_RW`, and neither is in the path any more. There is nowhere sensible to *put*
the file — the IndexedDB is for saved games, and filling a player's quota with pictures they can never look
at is not a trade worth making — so the bytes go straight into their downloads through
`WebTransfer::downloadBytes`, the same Blob mechanism the Manager's export uses. It copies the heap slice
(`new Uint8Array(HEAPU8.subarray(...))`) rather than handing the Blob a view, because `ALLOW_MEMORY_GROWTH`
can invalidate one at any allocation. `$A_TOGGLE_CAPTURE_VIDEO` is still the one action `main.cpp` withholds
from the web build.

**The page around the browser build is `WebBuild/shell.html`**, not Emscripten's generated one, and
everything in it is there because a phone needs it. `<meta name="viewport" content="width=device-width,
...">` is the important one: without it a phone lays the page out at a ~980px virtual viewport and scales
the result down, which puts a double-tap zoom in front of every button and keeps the legacy 300 ms click
delay. `touch-action: none` and `overscroll-behavior: none` stop the browser taking a swipe for scrolling
or pull-to-refresh. `pre.js` keeps the drawing buffer in step with the element, on `orientationchange` and
on `visualViewport` resizes too — that is how a phone reports the address bar sliding away. The page also
handles `webglcontextlost`, a real event when a tab goes to the background, by saying so instead of
freezing: the game cannot rebuild its textures and its FBO from where it stands.

**Every function key belongs to the game, not to the browser.** `pre.js` swallows F1 to F24 in the capture
phase, before SDL or the browser sees them, because they are bindable actions like any other key and the
desktop build answers to all of them — a player who knows the game must not find half of them missing, and
taking a named few would be the worst of both. Left alone, F1 opens the browser's help, F5 reloads the page
and loses the level, F10 reaches for the menu bar, F11 goes fullscreen and F12 opens the developer tools.
Nothing is lost: Ctrl+R and the address bar still reload, Ctrl+Shift+I still opens the tools, and
fullscreen is Alt+Enter as on the desktop. Whether a browser hands a page F11 and F12 at all is its own
decision; asking costs nothing where the answer is no.

**Which is why the click prompt names Alt+Enter.** A desktop browser offers no way to reach the game's own
fullscreen and nobody guesses that chord unaided, so `$WEB_FULLSCREEN_HINT` sits under
`$WEB_CLICK_TO_START` in the tooltip font — an aside, not the message. Not on a phone, where the game takes
the fullscreen itself on the first touch and there is no Alt to press; `Engine::isPhone()` is the one C++
place that asks, forwarding to the `b5_isPhone` in `pre.js` that the page uses too.

**The boot screen is pixel art too, and its line is the game's own.** It shows `$LOADING` from
`data/languages.txt` — the same sentence the game puts up a moment later — in the game's own font, which
the page cannot render itself: it stands before `data.zip` and before any GL context.
`WebBuild/make_text.py` draws it at build time straight out of `data/font.xml` and `font.png` (the same
glyph rects, advance and two-tap shadow `Font::renderText` uses) and `build.sh` stamps both languages into
the page as data URIs, so the line is there with the first paint and costs no request. The page blows it up
by a whole factor, 3 dropping to 2 or 1 where the line would not fit — replication at an integer factor,
never a resize, the same rule as the icons. The bar fills in whole 12px blocks, and the icon is shown at
5x32 with `image-rendering: pixelated`. Which language is decided the way `Engine::detectSystemLanguage`
decides it, by the same walk over `navigator.languages`.

**The click-to-start goes through the moment the gesture arrives**, and does not wait for the AudioContext.
`resume()` returns a promise, and on a phone — where the fullscreen request turns the screen — it can take a
second or two to settle; waiting for it left the line pulsing, which reads as "the tap did not register" and
gets tapped again. `GS_Loading` starts the logo intro at once instead, and the jingle waits its turn: it
hangs off `time >= 1000`, so there is a second of slack, and if the context is still suspended by `time >=
2000` the jingle is given up rather than fired into the menu. Measured with `resume()` stubbed out to never
settle: the menu comes up 3.0 s after the tap, which is the intro and nothing else.

**It installs.** `manifest.json` (fullscreen, landscape) and `sw.js` make it an ordinary add-to-home-screen
web app that launches without the address bar and runs offline; that is also the answer to iPhone Safari,
which has no element-level Fullscreen API.

**Every icon is generated from `data/window.png`** — four for the web by `WebBuild/make_icon.py`, seven for
Windows by `Tools/make_ico.py`; `make_text.py` uses the same PNG reader, and all three are stdlib only.
**Pixel replication at an integer factor, never a resize**: a scaler that smooths turns 16x16 pixel art into
a blur, and that is the whole reason these scripts exist. The `.ico` is **committed rather than generated**,
because the Windows build runs no Python; `verify.py`'s `windows_icon` check stops it going stale.

The web icons come in three kinds because a launcher does two different things with one:

- `purpose: "any"` (192 and 512) is shown **as it is** — the drawing edge to edge, transparency intact.
- `purpose: "maskable"` (512) is **cropped to a shape the launcher picks**, and only a centred circle of 80%
  of the width is guaranteed to survive; every transparent pixel becomes a hole in that shape. Bob is a
  full-bleed circle reaching 119% of the width across the diagonal, so masked he would lose his rim and cap.
  It is therefore drawn at **10x (320px) centred on an opaque 512 canvas** — content radius 191px against
  the 205px allowed, and still an integer scale. **Never label one icon `"any maskable"`** unless it
  satisfies both, which a full-bleed drawing cannot.
- `apple-touch-icon.png` exists because iOS reads neither the manifest icons nor any transparency:
  full-bleed like the "any" pair, but opaque.

`Blocks5/src/icon1.ico` carries **seven images** — 16, 20, 32, 40, 48, 64, 256 — because the shell smoothly
scales one itself for any size the file does not hold, downward included (256 to 40 averages six source
pixels into one). Where the size is not a multiple of 16 — 20 and 40 — the next scale down is centred with a
transparent margin rather than rendered at 1.25x, which would double some columns and not others. **24 is
deliberately absent**: the one size where the next step down is 1x, and that much margin is visible, so
Windows scales it from the 32. Every entry is a 32bpp DIB (the 256 a PNG), since full 8-bit alpha has worked
since XP; the 1-bit AND mask is still written alongside for legacy paths that read only that. **No
power-of-two restriction** on either kind: an `.ico` entry stores each edge in one byte (0 meaning 256), and
a manifest's `sizes` is free text. `Blocks5/setup/setupicon.ico` is the installer's own graphic — a monitor
and a disc, not Bob — and is left alone.

**The payload filenames carry the build's stamp** — `blocks5-<hash>.js`, `.wasm`, `.data`, the hash being
the md5 of the three — and that is the load-bearing part of the whole caching story. They must never be
mixed: the JS holds absolute byte offsets into the data, and its `EM_ASM` fragments sit at addresses that
fit exactly one wasm. Served in mismatched pairs the game aborts with *"No EM_ASM constant found at address
…"*, which is what a real deployment did when **mod_pagespeed** kept `blocks5.js` under a rewritten name of
its own and later handed it out beside a newer wasm. With the stamp in the name every such URL is immutable,
so no cache anywhere can produce the mixture. `Module.locateFile` in `shell.html` is the one place that
knows the stamp; `build.sh` writes it in after the link.

**The service worker therefore caches its two halves in opposite directions.** The stamped payload is
cache-first, since asking the network could only confirm what is already there. Everything else is
network-first with the cache as fallback — above all `index.html`, which cannot carry a stamp because it is
the entry point and the place the current stamp is written down. Serving *that* from the cache is how a new
build becomes invisible. `skipWaiting()` and `clients.claim()` are safe for the same reason the mixture is
impossible — a booted page holds stamped URLs — so an update lands on the next reload instead of a load
later. `install` fetches the payload with one `addAll`, all-or-nothing. Registration passes `updateViaCache:
'none'`, or a cached `sw.js` would keep a stale worker alive indefinitely, and then asks
`registration.update()` straight away rather than trusting how promptly the browser gets round to its own
check.

**Nothing stale accumulates.** `activate` deletes every cache whose name is not the current one, and the
name carries the stamp, so a new build drops the whole previous set. The cache-first branch also serves
**only the stamp matching its own `BUILD`** and lets anything else through untouched: while a new worker
installs, the old one is still answering, and without that it would pull the new build's payload into its
own doomed cache.

**`touch_controls.js` carries a stamp of its own**, `touch_controls-<hash>.js`, the md5 of that one file
rather than the payload's. Reusing the build stamp would be worse than leaving it unstamped: it hashes the
three payload files, so a pad-only edit would not move it, the URL would not move either — and the file
would then be served `immutable` for a year instead of a few heuristic hours. Measured: editing the pad
moves `8cb19d724ef9` → `3aa14d7f1448` while `blocks5-164c6a033fd7` stays put, so a 17 KB change drags no
part of the 13 MB payload with it. `build.sh` rewrites the name in both pages and substitutes `%%PAD%%` into
`sw.js`, as it already does for `blocks5.js` and `%%BUILD%%`.

**Two files can never carry a stamp**, which is why the header half still exists: `index.html` is where the
stamps are written down, and `sw.js` is registered under a fixed URL — a stamped one would leave the old
worker alive under the old name. `WebBuild/htaccess` ships as `.htaccess` beside `index.html`: a year of
`immutable` for anything stamped, `no-cache, must-revalidate` for everything that is not — those two plus
`blocks5.html`, `manifest.json` and the four icons — `AddType application/wasm`, and `ModPagespeed off`. The
icons and manifest could be stamped and are not, because they change once in a few years and the manifest
would have to be generated rather than copied to name them.

**That list is every unstamped file and not a chosen few, because the gap is silent.** A file with neither a
stamp nor a rule gets a *heuristic* lifetime in the browser, a fraction of its age, and then goes stale with
nothing to say so. The pad was exactly that before it was stamped: a new build's page and payload arrived,
the on-screen pad did not, and only a private window showed the new one. **The service worker cannot fix
that and it is worth knowing why**: a subresource the browser's HTTP cache still thinks fresh never reaches
the worker at all. Measured on a reload, the pad came back with `workerStart` 0, `transferSize` 0 and
`deliveryType` "cache" — so fetching it inside the worker with `cache: 'no-cache'` changes nothing, and with
a header in place the same measurement reads `workerStart` 79.7 and the new bytes arrive. The worker's
network-first branch is what keeps the page working offline; freshness is the URL's job, and the header's
only where there can be no stamp.

**`-sINITIAL_MEMORY` is 48 MiB, and that number was measured.** Started at 16 MiB the heap grows exactly
once, to 40 MiB, and stays there through the loading screen, menu, editors and a played level. Reserving far
more — it was 256 MiB — is on a phone the most likely reason a tab dies before the menu appears.
`ALLOW_MEMORY_GROWTH` stays on, so an unusually large level still has room.

**Losing focus takes two answers in the browser, not one.** Emscripten's SDL reports focus and visibility as
**`SDL_WINDOWEVENT`** — an SDL 2 shape — and never sends the `SDL_ACTIVEEVENT` the game switches on, so
`mainLoopIteration` has a second case for it under `__EMSCRIPTEN__`. Both funnel into `handleAppFocus`,
which mutes, forgets every held key, stops a running recording and tells the game state.

That still leaves the audio, because a *hidden* tab gets no `requestAnimationFrame`: no logic tick runs, so
the queued event is never even polled and the mute is applied by a pass that has stopped. `pre.js` therefore
suspends the `AudioContext` on `visibilitychange`, one layer below the engine, which freezes every source at
once. Without it the music dies on its own when its queue runs dry while every looping effect — a laser above
all — keeps sounding in a tab nobody is looking at.

`appActive` is an `Engine` member rather than a local in `mainLoop` because `emscripten_set_main_loop` calls
one iteration per frame, so nothing may live on the stack between them — and because the test hook reports
it, which is what makes any of this checkable.

**Saves ask to be kept.** They live in IndexedDB through IDBFS, which a browser may evict when short of room;
`navigator.storage.persist()` in `pre.js` asks for that not to happen. The browser grants it silently once
the page looks like something the user meant to keep and otherwise refuses, which costs nothing.

**In the browser the program never ends, so nothing is ever destroyed.**
`emscripten_set_main_loop_arg(…, 1)` asks for the simulated infinite loop, which unwinds the stack with a
JavaScript `throw` — so `Engine::mainLoop` does not return, the `engine.exit()` standing after it in `main()`
never runs, and no destructor runs either: not `~Engine`, not the game states that are locals of `main()`,
not a `Level` and not an `Object`. The unwind is a JS exception and not a C++ one, so it runs no destructors
on its way out. **Anything that has to happen must therefore hang off something that runs *during* play** —
a logic tick, `onLeave`, `onRemove` — and never off teardown. Two consequences visible from outside:
`config.xml` is written only where somebody asks for it (the options dialog's OK, and the CRT pane's *Try
it*) and never on quit, so a browser player who never opens the options has the language detected afresh at
every start; and every GL object the Engine owns is left to die with the page.

**The flag is load-bearing and must not be tidied away.** Emscripten ends the call with `throw "unwind"`, and
`callMain` swallows it without restoring `__stack_pointer` — so the abandoned frames stay above it and every
later `requestAnimationFrame` allocates below them, which is exactly what keeps `main()`'s locals alive. Its
three game states are such locals, and `Engine::registerGameState` keeps raw pointers to them. Passing 0
instead breaks two things at once: `mainLoop()` would return, so the `engine.exit()` after it would tear the
engine down *before the first frame* and the loop would run against the wreckage; and `main()` would return,
destroying the game states the Engine still points at. Getting rid of the flag is a restructure — game states
off the stack, `exit()` moved into the quit path — not a one-word change. To persist settings in the browser,
a `pagehide` handler calling `saveConfig()` is the smaller answer, and it catches a closed tab, which the
Quit button never sees.

**The browser's Quit button** cannot quit — a page does not close its own tab — so it draws a Windows blue
screen instead (`WebBuild/web_bluescreen.cpp`), hooked into the one `SDL_QUIT` case in
`Engine::mainLoopIteration` so the menu button, Escape and the editors all reach it. It mutes OpenAL, builds
a DOM overlay above the canvas (leaving fullscreen first, or the overlay would sit behind it) and calls
`emscripten_cancel_main_loop`. Any key or click after a 700 ms arming delay reloads the page.

**Deployment.** All three projects link the CRT statically (`/MT`, `/MTd` for Debug), so nothing needs a
Visual C++ redistributable — the installer has no runtime task at all any more. Exactly one DLL ships beside
the executables, `OpenAL32.dll`, and the only CRT it imports is `msvcrt.dll`, part of Windows — not a
versioned `MSVCR*`/`VCRUNTIME*`. Its other imports are all core Windows: `KERNEL32`, `USER32`, `SHELL32`,
`ole32`, `WINMM`, `AVRT`. Keep it that way: a new dependency needing a redistributable, or a second DLL,
undoes the whole arrangement.

**GUI** (`gui.cpp`, `gui_*.cpp`) is a retained-mode tree loaded from XML dialogs in `data/`
(`menu.xml`, `leveleditor.xml`, `options.xml`, …). Elements are addressed by dotted path —
`gui["Menu.DonatePane.Donate.Donate"]` — and wired with `sigslot` (`connectClicked(this,
&GS_Menu::handleClick)`); a game state that connects signals must derive from `sigslot::has_slots<>`,
which `GameState` already does.

**A click goes to the element under the cursor *now*.** `GUI::update()` recomputes `p_elementAtCursor`
at the **top** of the function, before the enter/leave dispatch and before the button handling — not at
the bottom, which would dispatch every click to whatever had been under the cursor at the end of the
*previous* logic tick. With a mouse that is invisible: you cannot click where the pointer is not, and
between arriving over a button and pressing it there is always at least one 20 ms tick. A finger has no
such gap. The other half of the same bug is in `Engine`: `cursorPosition` used to come only from
`SDL_MOUSEMOTION`, and a touch produces no motion at all, so both button events take the position from
the event too. Either fix alone changes nothing; the pair is what makes a tap land.

**A mouse-move event has to mean the mouse moved**, which is not the same question as whether
`cursorPos` changed. That position comes from `Engine::getCursorPosition()` and therefore through the
CRT filter's barrel distortion, so anything that changes the warp moves the cursor in game space with
the hand perfectly still — and the one control that changes the warp is the curvature slider, dragged
with the mouse. The synthetic `onMouseMove` set a new slider value, the value set a new curvature, and
the loop closed: measured with the hand held on the handle, the slider took 249 values in six seconds,
flipping between 0 and 1 at the logic rate. The step is what makes it reach that far — `getOverscan()`
is deliberately zero at curvature 0 and its full 1.3% the moment the slider leaves the stop, which a
third of the way out is a pixel and a half, and the bar turns 1.7 pixels into one unit. So
`GUI::update()` asks for both: the window's own cursor position (`Engine::getRawCursorPosition()`) must
have changed **and** the game-space one must have landed on another pixel. `noMoveCounter`, which times
the tooltips, reads the same answer.

Things about the widgets worth knowing, because getting any of them wrong is quiet:

- **`check()` is the user's click and fires `changed`; `setChecked()` is the display catching up and
  does not** — and `setChecked` must touch only `checked`, never the in-flight `newChecked`.
  `gui_checkbox.h` says why both halves matter and what each one broke.
- **Escape and Return belong to the dialog.** `GUI_EditBox` and `GUI_ListBox` forward both to the parent
  when they have nothing of their own to do, which lets a dialog implement Escape = Cancel and Return =
  OK while focus sits in a text field or a list.
- **A checkbox or radio button is hit on its caption too.** The caption is drawn by the toggle itself at
  `size.x + 10`, and `containsPoint` — a virtual on `GUI_Element`, which `getElementAt` calls instead of
  testing `size` inline — counts that strip as part of the control. The width is *measured*, not
  assumed: a fixed strip would steal clicks from whatever sits to the right, and options.xml puts
  language and detail radios in three tight columns. An empty `<Title>` measures zero, so a toggle that
  delegates its caption to a `<For>` label is unaffected.
- **Any element can carry `for="Name"`**, as `<label for>` does in a browser — it lives on `GUI_Element`,
  not on the text class, because a label is not always text: the two language flags in `options.xml` are
  `<StaticImage>` and belong to their radio button exactly as the word beside it does. A checkbox or
  radio target gets the whole set of mouse events forwarded (enter/leave included, since a toggle only
  fires on mouse-up if it believes the cursor is over it); anything else — an edit box, a list — just
  gets the focus, because forwarding a position measured against the *label* would drop an edit box's
  caret in an arbitrary place. The attribute is read in `GUI_Element::load`, which is not virtual:
  `readAttributes` is, and no subclass chains up to the base version, so a `for=` parsed there would
  work on some element types and silently vanish on others.
- **A `<StaticText>` label can size its own hit area.** Give it `w="-1" h="-1"` and it matches the text
  it actually draws, re-measured per frame so it follows a language switch; a hand-written width would
  be a guess that is wrong in the other language. `w`/`h` of 0 — the default — is still never hit. An
  image needs none of this: it already has the size of the sprite it shows.

**Text written to a fixed place has to be measured first.** `Font::renderText` neither wraps nor clips,
so a level whose title is longer than the space kept for it draws over whatever is beside it — in the
select screen across the description column and off the right edge, in the status bar across the Menu
button. `Font::fitText(text, maxWidth)` cuts it down and ends it in three dots; `Font::adjustText` wraps
instead, which is what the multi-line help pages want. Both skip over `<h>…</h>`: it draws nothing, so
it must not count toward a line's width, and a hard break landing inside it turned the markup into
visible text. The two callers keep the level *number* and the *filename* whole and shorten only the
title, since those are what tells two levels called *Unnamed Level* apart: the caption is measured once
with an empty title to learn what the frame costs, and the title gets the rest. A second pass over the
finished caption is the backstop for a filename so long that even the frame does not fit.

The cut may not land inside `<h>…</h>`, so `fitText` drops a half-cut tag entirely and closes whatever it
left open. **`<h>` ends with the string it began in**, and both `measureText` and `buildText` enforce
that with a counter: the option stack they push on belongs to the `Font` and not to the text, so markup
could never have carried across a call anyway. An unclosed `<h>` used to leave `italic` set and an entry
on the stack for the rest of the run; an extra `</h>` used to pop the *caller's* entry, or `top()` an
empty stack — a level titled `</h>Hello` crashed the game, and a level title is a file from a stranger.

**`measureText`'s position array is indexed by byte**, one entry per byte of the string and one behind it
— `text.length() + 1`, always. The bytes of `<h>` and `</h>` get an entry each even though they draw
nothing, all carrying the cursor the tag stands at. That is what the three callers need and already
assumed: the edit boxes look a position up under the same byte index their caret uses, and
`GUI_MultiLineEditBox` reads `[i + 1]` to size a selection. One entry per loop pass instead left the
array short — two per `<h>`, three per `</h>` — so a caret at the end of such a text read past the
vector.

**`<k>…</k>` is the keycap**, and the font draws the frame. It cannot go in the glyph batch — it carries
no texture — so the rectangles are collected while the text is laid out and drawn once the batch is
closed, which also carries them through the two shadow passes with the glyphs; a keycap without the same
shadow would look pasted on.

**The frame is drawn on exactly rows `capTop`..`capBottom` of a glyph cell**, two optional `<Font>`
attributes, and nothing about it is derived from the line. `lineHeight` and `offset` describe the line a
font is *set* at, and the ink is free to sit elsewhere in either direction: the note's font ends its
letters five rows above the foot of its line box, so a frame drawn on the line box sits under the word
instead of around it — while the tooltip font's letters are *taller* than its line, since `Backspace`
reaches a row above the capitals and a row below the baseline and a ten-row line has room for neither.

**That second case is why the frame carries its own height rather than the line's.** A frame fixed at the
line height and merely centred cannot be placed in a font whose ink does not fit inside the line: only
the sum of the two attributes is read, so every pair with the same sum gives the same frame and the next
sum moves it a whole row — always one row too high or one too low, with no third option. Saying the frame
outright is also what makes `verify.py`'s `font_metrics` check a straight comparison against the measured
ink. They default to the line box, which is what `font.xml` and `credits_font.xml` measure out to anyway.
Three files carry them: `tooltip_font.xml`, at rows 1..12, and the two skins that bring a `hintfont.xml`.

**Data, and not the image measured at every start.** Where a font's letters sit is a constant of the art,
and a statistic recomputed at load would move every keycap in the game by a pixel because somebody redrew
one glyph — silently, with nothing in any diff to point at. `verify.py`'s `font_metrics` check is the
other half of writing it down: it reads the first and last inked row of every printable character out of
the font's PNG, takes the **mode** of each — the top of a capital and the line the writing sits on, where
a brace reaches higher and a comma lower than anything a key is ever called — and reports a font whose
frame would cut into its letters or sit off to one side, naming the two numbers to write. Same arrangement
as the committed `.ico`. (`read_png` in `WebBuild/make_icon.py` learned the narrow bit depths for it —
`credits_font.png` is a two-colour palette at one bit.)

**A keycap is an atom to `adjustText`.** A box cannot be broken across two lines, so the whole `<k>…</k>`
run moves down together, the way any typesetter treats an inline box — and the renderer is then never
asked to draw half a frame. That is why the run is measured rather than walked character by character:
the padding either side belongs to its width.

**The right side of the frame carries the slant.** An italic glyph leans right — its top is drawn
`options.italic` pixels further along than its foot, while the cursor advances by the upright width — so
a frame that ends where the cursor does cuts the last letter of the key name. That is every speech
balloon, which sets italic for the whole text. The advance after `</k>` grows by the same amount, or the
following word would move into the frame instead; the left side needs nothing, since the first letter's
foot still stands on the cursor.

**Between keycaps that belong together stands a half space** — `HALF_SPACE` in `font.h`, half of that
font's own space and a space in every other respect: measured like one, and a line breaks at one and
replaces it exactly as a break replaces a space. Each keycap already stands off its own frame, so a full
space either side of the slash leaves it adrift between the two keys instead of the pair reading as one
binding, and the same holds for the plus of a chord: `<k>Alt</k>·+·<k>Enter</k>`. It is a byte rather than
an element like `<k>` because breaking is a matter of characters: `adjustText` searches backwards for the
last one it may cut at, and an element would have to be taught to be a break as well as to be skipped over.

The byte is the **middle dot**, `\xB7` — the character an editor shows a space as, and the third of this
file's meaningful bytes beside `§` and `¶`. It has to be printable because the chords are written out by
hand in `languages.txt` (a `%BINDING{…}` cannot say *Alt*), and a control character there would be
invisible to whoever edits the line. `Engine::getBindingMarkup` writes the same byte around its slash. The
plus that joins a key to a *word* keeps its full space — `%BINDING{$A_PLANT_BOMB} + direction` — so the
help table shows the hierarchy: tight where keys bind to each other, loose where prose follows.

**A keyboard has two Enter keys and the game tells them apart nowhere.** `isReturnKey` (`util.h`) is the
one place that says so, and everything reading the SDL key itself goes through it: confirming a dialog,
playing the selected level, leaving the credits, the level editor's settings, and Alt+Enter for the
fullscreen. The named actions never needed it — a binding has a primary and a secondary, and
`$A_SAVE_IN_HOTEL` has used both since it was written. Both are called `Enter` in both languages — `Enter`
and `Num Enter` — which is the prefix the other seventeen keypad keys already carry. It is what a PC keycap
prints (a German board prints the hooked arrow and no word at all) and the word every neighbouring language
borrowed. `Return` was SDL's own name for the key, and since the fallback capitalises SDL's name, the
English entry for it was replacing nothing.

**Short messages are the Engine's, not a game state's.** `Engine::showToast(type, text, duration,
suppressSound)` slides a bar in at the top edge, holds it, and slides it out again — green for `TOAST_OK`,
red for `TOAST_ERROR`, 2 s and 4 s by default, with `teleport_failed.ogg` on an error unless the caller
says otherwise. It is drawn at the very end of `Engine::render`, after `GUI::display`, so it sits over the
GUI, the editors' panes and everything else. Everything with something short to say goes through it — both
editors, the menu, and four failures that used to reach only the log: a skin missing or unloadable
(`Level::loadSkin`, one message per skin *name* rather than one per missing file), a music track that
cannot be opened (`Engine::playMusic`), a level that will not load (`Level::loadErrorLevel`, where all
three of `Level::load`'s failure paths already met), and a campaign that will not load (`Campaign::load`).
Each names the bare filename: the full path leads through an archive and its password and tells nobody
anything.

Two callers opt out. `Campaign::load` takes a `quiet` flag for `isImportableArchive`, which only asks
whether an imported file *is* a campaign — a skin that is not one is not a broken campaign. And
`loadErrorLevel` says nothing for the editor's palette levels (`cat<N>.xml`), which belong to the game and
are not a file anybody asked for. In the select-level preview the skin and level messages come without the
sound, because stepping through a broken campaign would otherwise beep at every keypress.

Several messages stack: the newest takes the top edge and pushes the older ones down a bar each. One that
has run out slides up by exactly one bar height, which puts it off the screen if it was on top and *behind*
its younger neighbour if it was not — hence the draw order, oldest first. Position and opacity move
together, 0.1 s each way and not counted against the hold time, because the bar is not fully opaque. Asking
twice for the same text and type does not stack two copies; the hold time becomes the longer of the two and
the sound plays again, because the sound answers the click and not the message.

**Language on first start** is the system's, not English. `Engine::detectSystemLanguage` asks
`GetUserDefaultUILanguage` on Windows, `navigator.languages` in the browser and `LANG` elsewhere, and
answers only `de` or `en` — every one of the 440 IDs in `languages.txt` has an English body and a German
one and nothing else, so detecting `fr` would give a wholly English game that merely believed otherwise.
The one `§fr:` and `§es:` in that file are its own header explaining what the tags mean. It runs only when
`config.xml` has no `<Language>`. Nothing ships a `config.xml` template — not the installer, not the web
build — which is what leaves the detection a chance to run at all.

**Localization.** Any user-facing string starting with `$` is an ID resolved against `data/languages.txt`
by `Engine::localizeString` / the free `loadString` helper. In that file a `$ID` line is followed by
per-language bodies tagged `§en:`, `§de:`, `§fr:`, `§es:` — that prefix is the section sign, 0xA7 in
Latin-1, not the pilcrow. A separate character, `¶` (0xB6), inserts a newline inside a body. Missing
translations fall back to English. Level titles, tooltips and menu captions in XML all use these IDs.

**No string names a key.** A message that writes "(F5)" or "Return/Enter" into its text is a lie to everyone
who rebound anything, so `%BINDING{$A_RESTART_LEVEL}` stands there instead and expands to whatever that
action is bound to now: both keys separated by a slash, one on its own, or the word for unassigned.
`%BINDING_OPTIONAL_RIGHT{…}` is the same with a leading space and *nothing at all* when the action is
unbound, which is what a button caption naming its own shortcut wants — "Restart Level" and not "Restart
Level ". `Engine::expandBindings` does it once, on the finished text, which is why `localizeString` is a
shell around `localizeStringRaw`: the raw half recurses through the `$ID` lookup and the English fallback,
and expanding on the way out of each would be the same work several times over. `verify.py`'s `bindings`
check reads every marker against the `registerAction` calls in `main.cpp`, because an action that does not
exist expands exactly like an unbound one and would otherwise be found by a player.

**`VirtualKey::niceName` is what a player reads**, as against `name`, which is SDL's, and `id`, which
config.xml holds and can therefore never be translated. It is a `$ID` and not finished text because the
language can change while the game runs. The table in `engine.cpp` spells each id out rather than composing
it from the key name, so that `verify.py`, which collects `"$…"` literals from the source, catches one that
`languages.txt` does not have; a key with no entry keeps SDL's own name with the first letter raised, which
is all `F5` needs. A joystick keeps a device word in front, localized separately, because `B3` beside a
keyboard key would say nothing about where it is.

**Level file format.** A level is XML: `<Level>` attributes for size, skins, weather, light colour,
diamonds needed and music; one `<Layer>` per tile layer containing `<Row>` strings where each character's
raw code is the tile ID (space = 0); then a flat list of `<Object type="…" x="…" y="…" …/>`. Campaigns
(`campaign.cpp`) are an ordered list of level filenames, shipped zipped in `levels/campaigns/`.

## Conventions

- Every `src/*.cpp` uses the precompiled header: `#include "pch.h"` must be the first line (`pch.cpp`
  is the Create-PCH translation unit). `pch.h` already pulls in SDL, OpenGL, GLU, OpenAL, libvorbis,
  TinyXML, sigslot, MersenneTwister, `img_load.h` and the core helpers (`singleton.h`, `vec.h`,
  `typedefs.h`, `util.h`, `manager.h`, `glstate.h`), so don't re-include those. `glstate.h` is in that
  list rather than per file because every source that draws reaches `GL::`, and a forgotten include is
  then the one way to get a raw `glBindTexture` past the `gl_state` check.
- There is no glob-based build: a new source file must be added to `Blocks5/Blocks5.vcxproj` **and**
  `Blocks5.vcxproj.filters`. `Tools/verify.py` checks this — nothing else will, since the Emscripten
  build globs `src/*.cpp` and so never notices.
- Naming: `p_` prefixes a pointer, `pp_` a pointer-to-pointer; classes are `PascalCase`, methods
  `camelCase`, enum constants `PREFIX_UPPER` (`OF_*`, `SKIN_*`, `FM_*`).
- **A rename goes through a tool that parses the code, never through a text substitution.**
  `clang-rename` and `clang-change-namespace` are installed (LLVM 18, `/usr/lib/llvm-18/bin/`), and
  `sh Tools/compile_db.sh` writes the `compile_commands.json` they need — asking `LinuxBuild/build.sh
  flags` for the real compile flags rather than keeping a copy, because a database with its own idea of
  the include paths has a refactoring tool parsing a different program from the one that ships. The
  file is a build product and is gitignored.

  ```
  sh Tools/compile_db.sh
  clang-rename-18 -i --qualified-name='Texture::bind' --new-name='...' Blocks5/src/*.cpp
  ```

  The reason is not tidiness. A `sed` over `GLState::` → `GL::` also rewrites `GLState::GLState`, the
  constructor of the struct of that name — measured: `clang-rename` asked for the same namespace rename
  leaves that line alone, and `sed` breaks it. The same shape waits wherever a member, a local or a word
  inside a comment or string literal shares a name with the thing being renamed. What saves a blind
  replacement here is that `Tools/syntax.sh` compiles all 124 sources in seconds, so the mistake is a
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
- **Comments are in English, and so is everything the build and test tools print.** The tree was
  commented in German until 1.2.0; the sweep that changed it moved no code at all, which is what made a
  change across 286 files reviewable. German survives in exactly three places, all data rather than
  prose: `data/languages.txt`, the inline `"\xA7" "de:…"` strings, and the two word lists in
  `verify.py`'s `comments` check together with the two faults `selftest.py` injects into it.

  **That check reads further than the other twenty-three**, and the reason is a file it did not catch:
  `WebBuild/htaccess` was wholly German through the whole sweep, because it has no extension and
  `source_files()` walks `.cpp`, `.h` and `.c` under `Blocks5/src`, `WebBuild`, `PWEncrypt` and
  `ShowUserDir` — never `LinuxBuild`, and never a script. `prose_files()` is the second list: the sources
  plus every `.js`, `.sh` and `.py` in `LinuxBuild`, `WebBuild` and `Tools`, plus `htaccess` by name,
  each with the marker its comments begin with. Only the language half uses it; the density guard stays
  on the sources, since a shell script has no ratio worth judging. The shipped `readme.txt` files are
  English and always have been.

  Vocabulary the sweep settled, where the obvious word is wrong: `massiv` is *solid* (`OF_MASSIVE` means
  impassable), `Ebene` is *layer* (*level* would collide with the class), and `Bild` is a *frame*, a
  *picture* or an *image* depending on the sentence — the distinction the browser timing argument rests
  on.
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
  `data/languages.txt`) are deliberately CRLF.
- Log with `printfLog(...)` from `util.h`, not `printf`/`std::cout`. `BEGIN_PROFILE`/`END_PROFILE` macros
  are available for timing a block.
- Third-party libraries are vendored under `Blocks5/libs`, each with a `PROVENANCE.txt` giving its
  upstream, licence, which files are compiled, and what was changed locally. All that is left in
  `Blocks5/libs/bin` is `OpenAL32.lib`, an import library — no compiled code without source anywhere in
  the tree, and nothing that pins the toolset.

### Every local change to a vendored library

Four libraries are patched, in seven files. Everything else is byte-identical to upstream. Each is
explained where it lives — in the file itself and in that library's `PROVENANCE.txt`.

| library | file | what |
| --- | --- | --- |
| SDL 1.2.15 | `src/main/win32/SDL_win32_main.c` | `#undef UNICODE`/`#undef _UNICODE`; inert under MultiByte, kept as a guard |
| SDL 1.2.15 | `include/SDL_syswm.h` | brackets `#include <windows.h>` out of `#pragma pack(push,4)`, or every `C_ASSERT` in a modern `winnt.h` fails |
| zlib 1.3.1 | `contrib/minizip/unzip.c` | `NOUNCRYPT` commented out — without it nothing in the password-protected `data.zip` can be read |
| zlib 1.3.1 | `contrib/minizip/iowin32.c` | `IOWIN32_USING_WINRT_API` commented out; this is a desktop build |
| shine | `l3mdct.c`, `l3subband.c` | `__attribute__((unused))` guarded for MSVC as well as Borland |
| minimp4 | `minimp4.h` | the `esds` descriptor: real `objectTypeIndication`, optional DSI, reserved bit, `SLConfigDescriptor`, measured bitrate |

`libogg` and `libvorbis` differ from their git tags only in expanded SVN `$Id$` keywords in five headers,
which marks them as coming from the release tarballs rather than a checkout — not a local change.
`minih264e_impl.c` and `minimp4_impl.c` are ours by design: the single translation units that instantiate
those two headers.

**Re-checking after a library update.** `raw.githubusercontent.com` is reachable from the build
environment, so every vendored file can be fetched at its upstream tag and compared; doing that across
the whole tree finds nothing but the table above. Three libraries cannot be checked that way and are
documented from the tree's own history instead: TinyXML has no upstream git repository (only the
SourceForge tarball, and the GitHub forks that fill the gap carry patches this tree deliberately does
not), and sigslot and MersenneTwister have no reachable upstream at all.
