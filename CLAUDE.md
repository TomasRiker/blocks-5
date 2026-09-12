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
`libs/sdl-1.2.15/src` — the same set SDL's own `VisualC/SDL/SDL.vcproj` builds. It needs one
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

`zip_*.bat` run `Tools\optipng` first, which is slow; `zip_data_no_optipng.bat` and
`zip_skins_no_optipng.bat` skip that step. Both require `Tools\7za.exe`. Those two binaries
sit in the same `Tools\` as the Python scripts and are reached through `%~dp0..\Tools\`
rather than relative to the current directory, because the scripts `PUSHD` into the folder
they are packing — and, for the XML half of `data.zip`, into `%TEMP%`.

**`Blocks5/pack.sh` is all four of those in one, without Windows** — the distribution's `7za`
and `optipng` in place of `Tools\7za.exe` and `Tools\optipng`, and it refuses to start without
either. `7za` and not Info-ZIP's `zip -P`, although both write traditional ZipCrypto: Info-ZIP
sets bit 3 of the general purpose flags and then writes the sizes in a trailing data descriptor,
and takes its check byte from the time of day rather than from the CRC. What comes out here
should be the archive the Windows build produces. `./pack.sh` does everything, `data`,
`skins` or `campaign` narrows it, `--no-optipng` skips the slow step. It is what a Linux-only
checkout needs: `data.zip` and the skin archives are build products that are not in Git, and
the game will not start without them.

**`levels/campaigns/blocks.zip` is a build product like the rest**, and the reason it has to
be rebuilt is worth knowing before editing a level. `Campaign::load` serves a campaign's levels
loose wherever all of them lie in `levels/` — which is true in a working tree and never on an
installed game, since `stage.bat` ships the archive and the two examples and not the 42
sources. So a level edited and not packed changes what a developer sees and nothing a player
sees, with no error anywhere. `pack.sh campaign` and `zip_campaign.bat` (which `Build.bat`
calls) rebuild it from `levels/level_NN.xml`, numbering the members from the index the way
`makeMemberName` reads them back — entry *i* is `level_{i+1}.xml`, so the padded names in
`campaign.xml` are display text only.

**Every one of its 53 members has a source in the tree**, which is what lets the archive be
untracked at all: the 42 levels and the ten music tracks lie loose in `levels/`, and
`campaign.xml` sits in `levels/campaigns/blocks/`, a source folder beside the archive in the
same idiom as `levels/skins/<name>/`. Neither script reaches into the archive it is replacing,
which is the shape the old ones had and which cannot work once the file is a build product.
Verified against the last committed archive: same 53 members, every one byte-identical.

**The XML files and `languages.txt` reach `data.zip` without their comments.** The dialogs in
`data/` are commented the way the source is, and the string table opens with a page explaining
its own format; those notes are nobody's business who opens the archive — but they belong in
the files. So the sources are left alone and
`Tools/strip_comments.py` writes stripped copies into a staging directory that the
packing scripts then pack from, which is why `pack.sh` and `zip_data.bat` call `7za` twice:
the images, the sounds and the demo out of `data/`, the XML and the text out of the staging
directory. Python is what does the
stripping, and it is the one thing here that Windows packing has no bundled tool for — so
where it is missing, both scripts say so and pack those files as they stand. A note, not a
stopped build: `data.zip` is what the game needs to start, and the comments are a tidiness.

`languages.txt` is the easier half: `Engine::loadStringDB` reads it a line at a time and its
comment branch does nothing at all, so a `//` line can go whole without changing a string.
The blank lines around one stay, because that parser counts them and the next text line
flushes the count into the string. It is that file by name and not `*.txt`, for the same
reason the packing scripts name `password.txt` rather than globbing: a pattern that swept up
every text file would one day take a line out of a password. Both halves are checked the same
way — the XML tree, and the string table the game would build, read out of the bytes before
and after.

A comment is removed only if it has its lines to itself, and that is a guard rather than a
matter of tidiness: a level stores one tile id per character inside `<Row>`, so `<!--` is
simply tiles 60, 33, 45, 45 — and to a parser that is the start of a comment like any other,
which would swallow everything up to the next `-->` and quietly destroy the level. Asking
whether the file is well-formed does not help, because it *is*: ElementTree reads
`<Row>aaa<!--bbb</Row><Row>ccc-->ddd</Row>` without complaint. A row of tiles always has data
before it on its line; a comment in a dialog never does. One written after something else on
the same line is reported and kept.

The game must run with `Blocks5\` as its working directory (VS's default `$(ProjectDir)` is
correct) because it opens `data.zip` relative to the cwd. `Build.bat /run` builds and then
does that for you; it has to come last, because every argument after it goes to `blocks5.exe`
untouched (`Build.bat Debug /rebuild /run -windowed`). There is no unit-test suite, but
there are checks that run in seconds and a way to drive the real game — see
**Checking a change** below.

Command line / launcher scripts: `-windowed` (`windowed.bat`), `-fullscreen`, `-nosplash`,
`-nofbo`, `-noshader`, `-perf` and `-nobatch` — that is the whole list, and `readme.txt`
documents all seven. `-nobatch` makes `renderSprite` draw every quad on its own again instead
of collecting a render pass into one call; it is the arm to measure the sprite batch against,
and `?nobatch=1` is the same switch in the browser.
`-perf` puts what the last few hundred frames cost in the corner; in the browser `?perf=1` on
the address becomes the same switch. See **Measuring a frame** below.
`-nosplash` skips the logo and the jingle by *not requesting* `logo.png`, which is the path
`GS_Loading` already takes when the texture will not load; only `soundPlayed` has to start
`true`, because the jingle hangs off the time threshold rather than off the logo.

**`-nofbo` and `-noshader` force the two fallback paths** that otherwise only appear on
hardware nobody here has, and each is one early return: `createFrameBuffer()` gives up
before asking the extension, `createUpscalerGL()` before allocating the shared vertex
buffer — after which every shader filter reports itself unavailable of its own accord and
`getEffectiveUpscaler` falls back to `Sharp`. Both paths are otherwise unreachable from
this machine, and they carry real code: without a framebuffer object there is no upscaler,
no crossfade and no rolled hint note.

**Without one the window is nailed to 640x480**, and that is not a preference: the game then
draws straight into the back buffer, the viewport is 640x480, and `presentFrame` returns
without doing anything — so a larger window does not get a larger picture, it gets the same
picture somewhere else, while `getCursorPosition` still believes `displaySize` and puts every
click in the wrong place. `handleResize` has always clamped the size; what was missing is that
nothing told the *window*, so maximizing left the frame large and the clamp then early-returned
on an unchanged `displaySize` and did nothing at all. `Engine::fixWindowSize` takes
`WS_THICKFRAME` and `WS_MAXIMIZEBOX` off the window under Windows (and answers
`WM_GETMINMAXINFO` with the same size for the maximum as for the minimum, since Win+Arrow does
not drag a border), and sets equal min and max size hints under X11, which is the ICCCM way of
saying "this window has one size". Fullscreen was already refused on that path.

**The mouse cursor follows the scale, and the framebuffer has nothing to do with it.** The
arrow is drawn once at 16x16 — the size it was designed as, and the size the video recorder and
`screenshot()` stamp into the 640x480 frame whatever the window is doing. `createCursor(factor)`
builds the two the system can draw, 16 and 32, and `updateCursorSize` picks between them from
the width of the rect `presentFrame` fills, not from the window: `Sharp` snaps to whole steps,
so at a window scale between 1.5 and 2 it shows the picture unscaled where the other filters
nearly double it. `render` asks once a frame rather than hanging off events: the answer moves
on a resize, a fullscreen toggle, a filter change and the browser's canvas alike, and asking
costs two divisions and a comparison.

Two sizes are all there are — nothing takes a larger cursor — so the choice is which of 16 and
32 lands closer to the 16·s the scale asks for: `|32 − 16s| < |16 − 16s|` from **s = 1.5**. At
exactly 1 and exactly 2, where `getDefaultWindowSize` puts almost everyone, the chosen one is
also pixel-exact against the picture. Measured at 1.40, 1.50 and 1.60: 16, 32, 32.

The
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
python3 Tools/verify.py      twenty-one static checks over the whole tree
sh Tools/syntax.sh           compile every source with mingw (-fsyntax-only)
LinuxBuild/build.sh          the native build compiles and links with GCC
cd WebBuild && ./build.sh    the browser port actually builds and links
```

There are three ways to *run* it, all scripted: `LinuxBuild/test/smoke.sh` natively,
`WebBuild/test/smoke.js` in a desktop browser, and `WebBuild/test/mobile.js` in an emulated
phone — see **Driving the game** below.

**A change whose whole question is what it looks like goes to the author to try, unbuilt.**
Tuning a glow, a colour, a width, a timing: the build takes minutes, the screenshot oracle
takes longer still, and neither of them can answer *is that the look I want* — only the person
asking can, and they have the game in front of them. So make the edit, say what the numbers
mean and which way to turn them, and stop. Everything above still applies to anything a
compiler or a check can judge, and to a visual change that also moves code around, where the
question is no longer only what it looks like.

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
`Blocks5.vcxproj` or its `.filters`, a display list added back to a tree that has none, a class
whose header is not named after it, a render layer written as a number, an object that
draws raw geometry without flushing the sprite batch first or changes the texture state
without going through GLState, the version
number drifting apart across the four places it lives, a new member the constructor never sets, an asset filename that is not on
disk or spelled with different case (which only Linux minds), a sound `playSound()` names that
`gs_loading.cpp` does not preload, a non-ASCII byte or a CRLF in a source file, `if (` where the tree writes `if(`, a
German comment among the English ones. Exit code 1 on any finding; `--list` names them,
`--only NAME` runs one. `Tools/README.md` has the table.

The last of those reads the two languages against each other rather than searching for one of
them, because both word lists contain traps: *the particles die* is English although `die` is a
German article, and *so weit kommt das nicht* is German although `so` is an English word. A line
is reported when the German words outnumber the English ones. The `style` check had to learn the
same lesson from the other side — it skipped `//` lines but not the body of a `/* */` block, and
where the German never wrote `for (`, an English sentence does.

The attribute check exists because renaming the constant `numLayers` to `NUM_LAYERS` once
took the XML attribute string with it, which silently disabled the level size guard for
every editor-saved level. That is the shape of bug this file is for.

**The Windows program icon is checked for the same reason.** `Blocks5/src/icon1.ico` is
committed, not generated — the Windows build runs no Python and should not start — so it sits
there unchanged when the art moves, and it had. The `windows_icon` check compares its 16x16
image against `data/window.png` and insists on the sizes the shell asks for;
`Tools/make_ico.py` rebuilds it.

**Two checks judge only what changed since `95660bb`**, the last commit before the
2025 overhaul, whose id is `BASELINE` at the top of `verify.py`: indentation and
whitespace, and uninitialised members. The comment-density half of `comments` is an
absolute 50% and judges every line. Code that has been there for ten
years and works is not a finding, and reporting it on every run is how a check gets
ignored.

**`Tools/selftest.py`** injects each fault in turn and confirms the matching check fires,
then restores the file byte-for-byte. Run it after touching `verify.py`. It is not
ceremony: the attribute check was inert when first written, because `Attribute(` also
matches the tail of `SetAttribute(` and so every written attribute counted as read — the
one check aimed at the bug above would have found nothing.

**`sh Tools/syntax.sh`** compiles all 123 sources with `i686-w64-mingw32-g++
-fsyntax-only`. It is the only way to put a compiler over the Windows code from here. Three
files never go through it — `main.cpp`, `videorecorder.cpp`, `stackwalker.cpp`. The last two
are left out of the web build for the same reasons; `main.cpp` is compiled there, and the
difference is the one thing worth knowing about this list: what mingw cannot parse in it is
the `__try`/`__except` crash handler, which sits behind `#if defined(_WIN32) && !defined(_DEBUG)`
— true under mingw, false under emcc. It needs nothing checked in: the handful of headers the
tree includes under names mingw and OpenAL Soft file differently (`<Windows.h>`, `<Shlobj.h>`,
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
`page.touchscreen.tap()` are equally useless: move, settle, hold, release. Alt+Enter
misleads, because it hangs off `SDL_KEYDOWN` and events queue.

**A tapped key is not the same as an instantaneous one, and that costs a whole extra press.**
`b5_key` holds for 60 ms rather than calling `xdotool key`, which presses and releases in
about twelve. `SDL_PollEvent` runs once per rendered frame, and under llvmpipe a frame is a
fifth of a second — so a run that lands inside those twelve milliseconds sees the press and
not the release, and at the *next* run SDL's own repeat, 140 ms overdue by then, posts a
second key-down before the release is read. Measured, every fifth press arrived twice; at
60 ms none did, eight times out of eight. A real machine renders at 60 Hz and a real finger
holds far longer than either number, so this is the harness lying, not the game.

`xdotool windowclose` calls `XDestroyWindow` and SDL then trips over a window it still
believes is its own. Quit the way a player does — Escape in the menu — or `Engine::exit()`
never runs and `config.xml` is never written.

**Two harnesses on one display take each other down**, and the wreckage reads as a rendering
fault rather than as a collision: `b5_clearDisplay` exists to clear the Xvfb an aborted run
left behind, and on the shared default `:99` it cannot tell that server from a live one. It
now refuses where a `blocks5` is still attached to that display and names `B5_DISPLAY`, which
is what separates two runs. Worth knowing before comparing screenshots from a parallel run:
a frame taken while the server was going down is not evidence of anything.

**The select screen's campaign list keeps the keyboard focus after a click**, which is
deliberate — `GUI_ListBox` handles the same four keys — and is a trap for a test that picks a
campaign and then presses End to reach the last level. The press goes to the list, which selects
its own last entry and therefore *another campaign* at level 0, and the screenshot is of a level
nobody asked for with nothing anywhere saying so. There is no list of levels to click instead —
the screen has one list box and six buttons — so take the focus off it by clicking any of them,
or drive the navigation by element name.

**Two windows open themselves over the menu, and `b5_start` writes both markers away.**
The CRT offer appears on a first start and the donation window once enough time has been
played — which a machine that has run the tests often enough reaches on its own. Both are
one-shot, so no test that touches the menu can be repeatable while they may appear;
`.crt_offered` and `.donation_asked` are written exactly as the game writes them.

**The harness drives `build-test/`, and `LinuxBuild/build.sh` without `hooks` writes
`build/`.** Building the one and testing the other is a full afternoon's worth of a change
that appears to have no effect, so `b5_start` compares the binary against `Blocks5/src` and
`data.zip` against `Blocks5/data` and refuses to run on either one that is out of date — the
same rule as `WebBuild/build.sh`'s exit code, for the same reason. `Tools/selftest.py` puts
each file's mtime back along with its bytes, or every run of it would trip that check and
force a full rebuild besides.

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

### Measuring a frame

`FrameStats` (`framestats.h`) keeps the last 512 frames' timings — the interval start to
start, how long the turn held the main thread, and render, update and present inside it —
and answers p50, p95 and the maximum. Percentiles and not a mean, because what tears the
audio or drops a beat lives in the tail. It is recorded always: four clock reads a frame.

**`render` and `present` are what *issuing* the draw calls costs, not what drawing them
does.** GL is asynchronous, so the work queues and is paid for wherever the pipeline is next
made to catch up — and where that is differs completely between the two platforms, which is
why `swap` is a phase of its own.

**Natively it is somewhere in `present` and `swap`, whichever the driver picks — read those
two as one number.** Measured under llvmpipe with a `glFinish` inserted to find out: render
*issues* in 2.2 ms and the finish after it takes another 5.9; the blit issues in 1.3 and takes
3.0; `glXSwapBuffers` costs 3.9 once nothing is outstanding. Take the finish away and that
same 5.9 turns up inside `present`, which then reads 8.5 against 1.3 of actual work. So a
single `present` carrying all of it read as 14.9 ms and was really the level rasterizing: the
wall clock was true and the label was a lie. Splitting the swap out does not isolate the wait —
nothing short of a `glFinish` does — it just stops one number pretending to be the blit.

**In the browser nothing here sees the GPU at all.** `SDL_GL_SwapBuffers` is
`Browser.doSwapBuffers?.()`, and `doSwapBuffers` exists only on the worker path, so off the
main thread it does nothing; measured, the swap is 0.00 ms and a `glFinish` after render
returns in 0.02. The page composites the canvas after the callback returns, outside every
window this can time. What is left is exactly main-thread CPU — the right measure for anything
the emulation or the JavaScript does, and no measure of the hardware. **`interval` minus
`total` is what is left for it:** a frame rate that falls while `total` stays flat is time
going somewhere this cannot see.

**What counts as a late frame is the logic rate**, 20 ms, because that is the frame budget
fifty times a second asks for — and the overlay counts against it twice, because the two
questions come apart. A frame whose **interval** went over is one the player did not get; one
whose **work** went over is one this game is responsible for. Under swiftshader the browser
ran at 38 ms a frame on 2.7 ms of work: counting the work alone would have reported nothing
wrong at 26 fps. Natively in the menu the two read 277 and 169 of 512.

A third count stays at 500 ms, and it is a different question again: that is what Emscripten's
OpenAL has scheduled ahead, so a frame past it is a hole in the music — in the browser only,
since the native decoder thread fills the queue whatever the main thread is doing. It was the
*only* count once, which flattered everything: at 50 fps nominal it read `0 of 512` while a
fifth of the frames were missing their budget.

One caveat on the work count. `total` includes `swap`, and **nothing in the tree ever asks for
vsync** — no `SDL_GL_SWAP_CONTROL`, no `SDL_GL_SetSwapInterval` — so it is the driver's
default. Under Xvfb there is no vblank to wait for and `swap` is real work: measured, 6.9 ms by
default and 6.8 with `vblank_mode=0`, which is the same number. On a real desktop, where Mesa
syncs by default, that same phase would be a *sleep*, and the work count would then read a
frame that merely waited as a frame that overran.

Three ways to read it, and the platform decides which:

- **`-perf`**, or `?perf=1` in a browser, draws the numbers in the bottom corner. That is the
  phone's only way: no console, no command line, no harness — and the block lands in a
  screenshot, which is how the figure gets off the device.
- **The test hook's `frames`** in the JSON, for a desktop harness, without the overlay's own
  cost in the picture. It does not clear on read, because the overlay reads the same numbers
  continuously; `blocks5_testResetStats()` (`resetstats` natively) is where a measurement
  begins.
- **`WebBuild/test/perf.js`** drives the comparison: arms are query strings rather than
  builds, so both sides are one binary in one browser, and they are **interleaved** rather
  than run in blocks, so a machine that warms up or throttles hands that to both.

**`?texunits=N` is the first knob that rides on this**, and it shipped. Emscripten's GL
emulation keeps state for as many texture units as WebGL reports — 8 to 16 — and loops over
that count twice per draw call. This game never leaves unit 0: there is no `glActiveTexture`,
`GL_TEXTURE0` or `glMultiTexCoord` anywhere in the tree. `pre.js` therefore sets
`Module.GL_MAX_TEXTURE_IMAGE_UNITS` to 1 by default, and `?texunits=0` puts it back to asking
WebGL, which is the arm to compare against. Measured on the menu's title demo, three
interleaved runs of twenty seconds: the median frame **3.20 ms → 2.70**, its render half
**2.40 → 2.00**, against a spread within an arm of 0.10 ms. The picture is untouched — 0 of
512000 pixels differ.

Two traps there. `Module.<anything>` has to be named in **`INCOMING_MODULE_JS_API`** or the
start aborts; `build.sh` passes Emscripten's whole default list plus this one key, because
naming the setting replaces it, and `-sFOO+=bar` is not a syntax emcc knows — at link time it
is dropped without a word rather than refused. And the interval barely moved in that
measurement, correctly: under swiftshader the frame rate is capped elsewhere, so the saving
shows up as main-thread time and not as frames per second. On a phone, where the main thread
*is* the limit, it is the same milliseconds either way.


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

**The tile grid is a vertex array, built once and drawn three times.** `Level::renderTiles`
writes the layer into a `std::vector<QuadVertex>` whenever `layerDirty` says it changed and
hands that to one `glDrawArrays(GL_QUADS)` out of client memory. The vertices carry a position
and a texture coordinate and **no colour**, and that is the point: `Level::render` makes three
passes over a layer — two shadow samples and the picture — differing in nothing but a
`glColor` and a translate, so one built array serves all three. Six places set `layerDirty`,
and between them they cover every way a tile or the picture it is cut from can move; a tile id
alone would not, since the texture coordinates come from the `TileSet` and a skin change moves
every tile without moving an id. Measured on the menu title demo, three interleaved runs of twenty
seconds: the median frame's render half **2.1 ms → 1.7 ms**, against a spread within an arm of
0.1 ms. The interval does not move, for the same reason it did not move for `?texunits`.

**A whole render pass of sprites is one draw call.** `Level::renderObjects` opens a batch around
its object loop (`Engine::beginSpriteBatch`), and while one is open `Engine::renderSprite` appends
four `ColorQuadVertex` — a position, a texture coordinate and a colour of its own — instead of
drawing. `flushSprites` puts the lot up with one `glDrawArrays(GL_QUADS)`. The colour has to be
per vertex and not in `glColor`, because every object brings its own tint, death countdown and
conversion ghost; a shared colour would flush at every object and there would be nothing left to
batch. Measured in the browser, `?nobatch=1` against the default: **276 draw calls a frame → 35**
in a played level, 243 → 48 on the menu's title demo, 314 → 72 on the level select. In the played
level the median frame goes **3.90 ms → 2.40** and its render half **2.40 → 1.10**, against a
spread within an arm of 0.90 and 0.30. The vertex count does not move — the same geometry, in a
quarter to an eighth of the calls.

**A sprite is drawn at the size it was given, odd numbers included.** `renderSprite` used to
halve the size and span `size` texels over `size - 1` pixels, so anything odd came out a pixel
short and resampled. `halfSize` and `otherHalf` split it instead, and mirroring swaps the two `u`
coordinates rather than applying `glScaled(-1, 1, 1)`, which on an asymmetric quad would shift it
a pixel. Two shipped screens change by a pixel because of it, and no `-nobatch` comparison can
see either, since neither runs inside a batch: the 39x39 level-status stamp in the select screen
(`gs_selectlevel.cpp`) and `Menu.Donate`, which is 100x43 in `menu.xml`. They are the only odd
sizes in the tree — the rewind OSD, the shine, the note and every tile are even.

**The transform is baked into the vertices, and that is what the batch costs.** A sprite is drawn
under whatever matrix its caller pushed — `Object::render`'s translate to the cell, the squash of
a teleporting object, the unbalanced `glTranslated` `Enemy` does inside its own `onRender`, the
half pixel `Level::render` puts under the wires — and sprites from different objects cannot share
a draw call while that lives in the matrix stack. So `queueSprite` reads it back with
`glGetFloatv(GL_MODELVIEW_MATRIX)` and multiplies the four corners itself: one GL call in place of
the fourteen to sixteen the immediate path made per sprite, and in the browser that read is a copy
of sixteen floats out of a JavaScript array, not a pipeline stall.

**The flush must therefore draw under `glLoadIdentity`**, and getting that wrong is invisible
almost everywhere. The vertices already carry the matrix; leaving it applied puts it on twice. A
level renders under an identity modelview almost everywhere — the camera shake and the half pixel
under the wires are the exceptions, and either applied twice passes for the effect itself — so the
game looked perfect until the level editor, which draws its object palette under
`glTranslated(245, 428, 0)` and lost every sprite in it off the right of the screen. The five
palettes are what caught it, as they caught the render-layer conversion before.

**A queued quad is drawn with the state standing at the flush, not at the call.** There is no
depth buffer in this 2D path, so painter's order is the only order there is: anything that draws,
or that moves any state the queued quads will be drawn under, has to flush first. In the tree as
it stands that is the texture binding, the texture matrix, the blend function and the framebuffer —
nothing the batch can reach touches the scissor box, the colour mask, the stencil or the alpha
test, and neither check would notice if something started to. `Texture::bind`, `Texture::unbind`,
`Engine::setBlendFunc`, `beginRenderToTexture`, `endRenderToTexture` and `acquireOffscreenTexture`
do it themselves, so most of it is automatic. The object sources reach the other three through
**`GLState`** (`glstate.h`) — `setTexturing`, `bindTexture`, `pushEnables`/`popEnables` and the
texture matrix, which is the third piece of state and the one the first draft of the check was
blind to — all of which flush before they change anything, so the rule lives in one file instead
of at a dozen call sites where it can be forgotten. `verify.py`'s `gl_state` check bans the raw
forms there, scoped to what the batch can reach: the sources that define an `Object::onRender`,
plus `texture.cpp` and `linedrawer.cpp`, which every one of them draws through, plus
`Level::renderShine` and `Font::drawText` by name out of two files that are otherwise full of
drawing with no batch open. The crossfades, the GUI and the credits are deliberately left alone,
which is what keeps the ban something a reader can check.

**`GLState` is not a cache, and that was measured rather than assumed.** Skipping a call that sets
what is already set is worth about a quarter of the texture binds in a played level — 28 calls of
the 290 the state layer issues, out of 4833 in all, measured under swiftshader in a browser before
the batch landed. The batch has since taken the geometry away, so those same 28 are a larger share
of a much smaller number and still well under the spread between two runs. Against that: all 37
raw `glBindTexture` and 35 raw `GL_TEXTURE_2D` enables outside `glstate.cpp` and `texture.cpp` —
39 in the
crossfades and the rest across the engine, the credits, the GUI, `level.cpp` and the level editor —
would have to come through it too, the failure mode of a stale entry is a wrong picture rather
than a slow one, and `presentFrame`'s `glPushAttrib(GL_ALL_ATTRIB_BITS)` restores the binding on
the desktop while `gl_compat.cpp` restores only the mode and the enables — so the invalidation
would differ per platform. The `glPushAttrib(GL_TRANSFORM_BIT)` that `Texture::bind` reaches
through `GLState::loadTextureMatrix` stays for a different reason: replacing it is safe as the
tree stands, but it would leave `bind()` with a silent precondition, and it would save nothing in
the browser, where `glPushAttrib` issues no GL call at all. ROADMAP 44 has both.

**The flush says which matrix stack it means**, and that is not pedantry. It draws under
`glLoadIdentity` because the vertices already carry their modelview — but a flush happens wherever
the state moves, `Texture::bind` included, and `Level::render` binds the snow and the clouds with
`GL_TEXTURE` current. An unqualified `glPushMatrix` there would push, wipe and pop the *texture*
matrix and leave the sprites under whatever modelview happened to stand. The batch is empty at
that call today, which is the only reason it never showed. The `glPushAttrib(GL_TRANSFORM_BIT)`
bracket costs three calls a flush on the desktop and two in the browser, where `gl_compat.cpp`'s
push issues none — and it is the cheap half of a belt and braces rather than the whole answer,
since a batch left *open* across the weather block would still be drawn under the texture matrix
the weather scrolls, which no bracket at the flush can help with. What keeps that safe is that
`endSpriteBatch` runs long before it.

**Two explicit flushes survive.** In `lava.cpp` the two lava passes draw raw quads under a texture
bound from outside and change no state on the way in, so nothing else would flush for them. And
`LineDrawer::draw` — the raw `glDrawArrays` behind every laser, wire and shot — flushes at its own
definition rather than at its seven call sites, because each of the four `LineDrawer`s they share
reaches it as a member or a local and no static check can follow that. That is what `sprite_batch`
still checks for, now that `gl_state` has the state half.

**`flushSprites` deliberately does not put the current `glColor` back.** Immediate mode left
the last sprite's colour standing, and restoring it looked like the faithful thing to do. It is
not: a flush happens wherever the state moves, which includes the middle of somebody else's
drawing. `Font::renderText` sets its shadow colour and then calls `drawText`, whose first act is
a bind — so the restore repainted every text shadow in the last sprite's colour.

What the *spec* says about the other direction is worth knowing before the next renderer moves: a
draw with `GL_COLOR_ARRAY` enabled leaves the current colour **indeterminate**, so a strict
reading has `renderText`'s first shadow pass drawing in whatever the batch left. Measured, both
targets keep it — llvmpipe answers `GL_CURRENT_COLOR` unchanged after an array draw, and
Emscripten writes `GLImmediate.clientColor` only from a `glColor*` — so nothing is wrong today.
It is the kind of thing that stops being true on a driver nobody here has.

**In the browser the batched colour arrives unquantised**, the same effect the tile grid has: a
`glColor4dv` inside `glBegin`/`glEnd` is truncated to a byte by Emscripten's emulation, and a
float colour array is not. Object shadows, which are drawn at alpha 0.35, move by one grey level
there. The desktop is byte-identical — the four editor palettes that do not animate hold 29 of the
65 types between them, and all four come out unchanged. It follows that **`-nobatch` is not a
byte-exact oracle in the browser**: it is one on the desktop, where the same frame reads back
identically either way, and in the browser every batched sprite may differ by 1/255 from the
immediate path for this reason alone.

**The same array also loses the clamp, and that one is not cosmetic.** The game hands GL colours
above 1 deliberately — `Level::renderShine` takes `deathCountDown * 5.0` from an exploding bomb,
the teleport swirl ramps its red to 2.1, and the three spark bursts add half a level of red a tick
until the particle has shrunk away, which lands between 5.5 and 25.5 — and relies on the hardware
to cut them off. Desktop GL clamps a primitive colour *before* it multiplies the texel.
Emscripten's emulation does not: dumping the vertex shader it generates for this game gives
`v_color = a_color;`, and the `clamp` it can emit sits behind `GL_LIGHTING`, which this tree never
switches on. So the browser computes `clamp(colour · texel)` where the desktop computes
`clamp(colour) · texel` — at a red of 2.0 every texel above 0.5 saturates, and a soft glow comes
out a hard-edged blob. `clampColor()` in `util.h` puts it back, in the two places a colour reaches
GL without being cut off on the way — `Engine::queueSprite` and `ParticleSystem::render`, both
colour arrays — and **only in the browser build**, because everywhere else the hardware has
already done it. A colour array is the only unprotected path: every `glColor*` spelling the
emulation offers funnels into one `glColor4f` that clamps each channel on the way in, inside a
`glBegin` block and outside one alike, and a vertex attribute goes nowhere near it. ROADMAP item
42 is how to stop paying for it on the CPU at all.

**Text is the same arrangement, keyed on what it was laid out with.**
`Font::renderText` looks a string up in a cache of 32 laid-out entries — the glyph quads and,
in a batch of their own, the keycap frames, which carry no texture — and draws them three
times: twice as a shadow, once as the text. The key is the string together with every option
the layout depends on (`tabSize`, `charSpacing`, `lineSpacing`, `charScaling`, `italic`), and
deliberately not `shadows`, which changes nothing that is built. **That key is what lets
`setOptions` leave the cache alone.** It used to empty the whole of it whenever any of those
five changed, and the callers change them constantly — a speech balloon sets `italic` and puts
it back on every frame it is on screen, the credits animate `charScaling` — so one balloon
threw away every cached string in the GUI twice a frame. Measured in the browser on the help
page, the most text the game puts on one screen: the median frame's render half **3.1 ms →
2.5 ms**; on the menu 1.7 → 1.5, in the level editor 0.9 → 0.8.

**A colour set inside `glBegin`/`glEnd` is quantised to a byte in the browser; the same call
outside one is not.** Emscripten's GL emulation writes a `glColor4f` issued between the two
into its vertex buffer as four unsigned bytes and reads the attribute back normalized, where
outside a block it becomes a constant `vertexAttrib4fv` at full float. The shadow pass's alpha
of 0.35 therefore used to arrive as 89/255, and now arrives as 0.35 — one four-hundredth
stronger, which puts 40 of the 256 possible background values one level lower. Measured on
the level editor: 3230 of 512000 pixels differ by exactly one, every one of them inside a
tile shadow, and building the new path with the alpha quantised the old way reproduces the
old screenshot byte for byte. The desktop never had it, where the colour has always been the
float the code asked for — the same screen read back from the framebuffer at 640x480 is
byte-identical before and after. Worth knowing before the next renderer moves: every colour
the remaining `glBegin` blocks set is truncated down to the next 1/255 — a weaker alpha and
a darker tint than the code asks for.

**A render layer is a pass, and it has a name.** `renderlayer.h` holds the twelve `RL_*` that
`Level::render` walks in order, and each is a single bit, so an object's set of them is the OR
of the ones it draws on. `Object::getRenderLayers()` is a plain member behind an inline getter
and deliberately **not** a virtual: asking costs a load, and — the real reason — a subclass
cannot then answer differently from the `onRender` it inherits. The mask may name a layer the
object is not drawing this frame and may never omit one it is, so `say()` and `flash()`, which
draw from `Object::render` rather than from `onRender`, add their bit and never remove it.

`Level::renderObjects` skips an object whose bit is clear, which is most of them on most
passes. **That is worth almost nothing in milliseconds and was measured before it was built**:
adding 27,720 no-op matrix operations per frame costs 1.0 ms, so removing the 2,772 that the
old unconditional bracket spent was worth 0.1 ms, and the finished change measures 1.7 ms of
render against 1.6. It earns its place as names rather than as speed — and it earned it
immediately, by making a dead pass visible. `renderObjects(735, …)` walked all 84 objects with
a matrix bracket and a virtual call each, and no `onRender` in the tree had ever handled 735.

**The trap it set on the way in is what `verify.py`'s `render_layers` check is for.** The values
moved, so every surviving magic number — `layer == 939`, `layer != 18` — became either meaningless
or the wrong layer; and C++ compares an enum to an int without a word, so five such lines built on
all three platforms and were simply never true. The sprite texture stopped being bound for the
lava passes, the wires lost their offset, the speech balloons stopped appearing. The five palette
levels are what caught it: `cat0`..`cat4` hold an instance of 60 of the 65 types `instancePreset`
knows, so walking them draws all but two of the `onRender`s in the tree — `Damage` and
`Projectile`, which the game spawns during play and no palette can place — and four of the five
are byte-identical across a change like this. The fifth is `cat1`, whose two
ConveyorBelts start their band at `random(0, 6)` in the constructor, so it differs run to run by
a couple of hundred pixels whatever you do.

**There are no display lists anywhere in the tree, and `verify.py` is what keeps it that way.**
They were a second way of keeping geometry beside these arrays, and one WebGL does not have at
all — so every place that used one carried a browser path of its own under `#ifdef
__EMSCRIPTEN__`, and a stub in `gl_compat.cpp` so the browser would link. The `display_lists`
check reports `glNewList` and its six relations in either build: added back, one would compile
on Windows, link on Linux and misbehave only in the browser, which is the build nobody runs
first. The three that used them are the tile grid, the font and `Lightning`, whose two passes
are built when the bolt is generated and then drawn unchanged for the forty frames it takes to
fade — only the colour and the alpha move. `quadarray.h` is where the three meet: `QuadVertex`,
a position and a texture coordinate and no colour, and one `drawQuadArray` so that the
client-state dance is written once rather than three times.

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

That rectangle is a pixel wider than the picture on every side, so the fade lies entirely
outside it and nothing inside the picture is touched: **at curvature 0 the CRT filter covers
exactly what the other three filters cover.** The soft edge used to eat into the frame,
leaving the outermost column at 64% and the next at 88% for no reason anybody had asked for —
the fade is there for the barrel distortion, which at curvature 0 does not exist. Measured
against sharp-fit on the same scene: zero shift in either axis, and the outermost three
columns within 0.5% of the columns inside.

**The whole raster then steps back from the edge of the glass, and that is `getOverscan()`.**
The warp moves the corners outward and leaves the edge midpoints exactly where they are, so at
the middle of each side the picture ran to the last output row and the fade, the convergence
fringe and the halo — everything the shader draws *outside* the picture — had nowhere to go.
Measured before: 0 black rows above the picture at the top centre, 4 at a quarter out, 20 at
nine tenths. Soft and rounded everywhere, guillotined at four places.

The step back is one isotropic factor on `w`, and it is the sum of the two things that need
the room: the fade, twice `EDGE_ROWS` source rows, and the convergence offset, twice
`CONVERGENCE_MAX` source columns. Isotropic because anything else would stop the pixels being
square — horizontally both terms are needed, vertically only the first, and the rest is black
surround. Measured after: 2 black rows at the top centre rising to 26, with the picture fading
in over the next four; and at the right edge with convergence at full, red's raster dies at
output column 1274, green's at 1277 and blue's is still burning at 1279, which is the point of
giving each channel its own `rasterMask` in the first place.

**It is exactly zero at curvature 0**, which is what keeps the promise above: measured against
sharp-fit, zero shift in either axis and the picture still reaching row 0 and column 0. The
price is a step of six output pixels at 2x as the curvature slider leaves its stop — a flat
tube is pixel-exact, a curved one is inset. `warpToSource`/`warpToOutput` carry the same
factor, so the cursor round trip stays exact: 0 of 34240 positions off, at curvature 0, 0.25,
0.5 and 1.0 and at three window sizes.

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

**Restarting a level rewinds the tape**, but only with the CRT filter on: `CF_Rewind`
(`cf_rewind.cpp`) instead of `CF_Slices`, chosen by `crossfadeRestart` in `gs_game.cpp`. On
sharp or sharp-fit the game does not claim to be a tube and a tape effect would be a costume.

It exists because a restart is a cut — the game jumps from the current state to the level's
first tick with nothing in between — and a tape in search is the one machine that cuts like
that and is forgiven for it. **Every strip of a searched picture is read from a different place
on the tape, which is to say from a different moment**, so a screen made of strips taken
alternately from the old image and the new one is not a trick standing in for frames the game
never had: it is what a recorder actually puts out. The rest follows from the same fact — no
signal between the tracks, so bands of snow roll through; no lock for the vertical hold, so the
picture rolls; the head meets each track at an angle, so every line starts early or late; and
VHS carries colour on a separate low-frequency signal that does not survive the speed, hence
the grey wash.

Two things there are load-bearing. The on-screen display must **not** move with any of it, or
fade with it either — it comes from the recorder's own character generator, mixed in behind
the tape path, and that one steady thing is what makes the mess read as a machine. It is
`data/rewind.png`, 256x64 with the word in the left 162 pixels and the two triangles in the
56 next to it, so the blink is a source rectangle rather than a colour: the word is drawn
every frame, the arrows every other half-second, hard on and hard off, counted from the tick
the effect began so that they start visible. And `ROLL_SCREENS` is a whole number, so the
roll offset lands back on a multiple of the picture height — zero — exactly as the crossfade
ends; at 6.5 the picture would sit half a screen out and jump straight when the effect stops.
The last sixth of the transition eases the tearing, the snow and the wash to nothing, which is
the transport braking and the servo locking.

**The sound is a granular resynthesis of a recording of a real transport**, and the two
attempts at synthesising one from scratch are worth keeping as a lesson. The second of them
matched the recording's third-octave curve to a mean error of 1.3 dB — two humps, a narrow one
at 250 Hz and a broad one at 2.6 kHz with 500–1000 Hz sitting 13 dB lower, the envelope
clattering at 46.5 and 12 Hz, a crest factor of 14 dB — and still sounded nothing like a
machine. **A matched spectrum is not a matched timbre.** The recording carries narrow
resonances standing up to 24 dB above its own noise floor, and a third-octave average is
precisely the measurement that cannot see them; filtered noise shaped to that average is a
hiss with a tilt.

What ships instead is overlap-add: 30 ms Hann grains at a 7.5 ms hop, so four deep, each one
resampled 7.5% short and given ±1.5 dB of its own. Where a grain is *read from* is what the
gesture decides — the recording's own spin-up for the first 0.42 s, a random point in its long
steady stretch for the middle, its brake for the last 0.47 s. The shape is therefore the
machine's while no stretch of the result is a copy of any stretch of the recording: the grains
arrive out of order, at another pitch, at another level, four at a time.

`rewind.wav` is committed beside its `.ogg` exactly like every other effect, and it is the
source of record — there is no script that rebuilds it, since rebuilding it would need the
recording. `CF_Rewind`'s constructor plays it, so the picture and the sound cannot be had
separately. It runs 1.75 s against the transition's 1.5 so that the run-down is not cut off
with the picture.

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
`SDL_NOFRAME` runs `WIN_GL_ShutDown` instead and takes every texture and the FBO with it. So fullscreen is *not* an SDL flag here: `applyWindowStyle` sets the Win32
style to `WS_POPUP` and the size to the desktop directly, SDL notices through its own
`WM_WINDOWPOSCHANGED` and posts an ordinary `SDL_VIDEORESIZE`, and `handleResize` — the one
place that owns `displaySize` — picks it up. Dragging the border and Alt+Enter therefore
run the same code, and nothing is ever destroyed. Alt+Enter is swallowed so the game never
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
(`WebBuild/pre.js`), Alt+Enter goes through the Fullscreen API from a real DOM keydown — the
main loop's own events do not count as a user gesture — and the main loop reads the canvas
size once a frame.

**On a phone the game takes the fullscreen itself.** Mobile Chrome has no button for it, so
without this the page is played under an address bar, and the picture is small enough already.
`Engine::enforceTouchFullScreen` runs from a second DOM callback beside the Alt+Enter one,
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

**The sound files are repaired sources, and the mix decisions are not in them.**
`Blocks5/data` holds a WAV beside every shipped OGG, and `Tools/encode_sounds.py` produces
the one from the other **one to one** — 96 kbit/s where libvorbis accepts it, stepping down
where it does not (11025 Hz mono tops out at 48). Where a sound should play quieter than its
file, that factor lives in `data/sounds.xml` and is applied at playback: `Sound` looks itself
up once at construction and `SoundInstance` multiplies it into the single `alSourcef(…,
AL_GAIN, …)` call, so it covers `slideVolume` and every caller that sets a volume itself.

That split exists because the alternative had already failed silently. Eight OGGs had been
exported at a reduced level while the WAV beside them kept the loud original, so the intent
lived only in the compressed file: re-encoding from the source would have made `ricochet`
6.8 dB louder, `push` 5.1, `thunder` 4.6. Measuring it back out needs the right comparison —
the shipped OGG against a *freshly encoded* one from the same WAV, since WAV-against-OGG
folds in the encoder's own frequency-dependent loss, which is the same order as the smallest
of these factors (`syringe` at 0.914).

Three things belong in the WAV instead: no DC offset, endpoints on zero, and nothing clipped.
A 20 Hz high-pass takes the first — measured, it costs at most 0.8 dB of BS.1770 loudness
while removing up to 6.6 dB of RMS, because what it removes is inaudible. Half-cosine fades
of 5 ms take the second, **except on the eight looping sounds** (`conveyorbelt`, `elevator`,
`gas`, `laser`, `mask`, `rain`, `thunderstorm`, `toxic`), where the end *is* the beginning.
Those also need the high-pass convolved **circularly** rather than linearly: a looping sound
is periodic, and the filter's transient otherwise droops both ends and made the seam 10–12 dB
worse. The third cannot be repaired at all — clipped peaks are gone, and getting back under
full scale means lowering the level.

**Input** is two-layered. Physical keys/joystick axes/hats are mapped to *virtual keys*
(`VirtualKey`), and named *actions* (`"$A_LEFT"`, `"$A_PLANT_BOMB"`, …) bind a primary and
secondary VK. Gameplay queries `wasActionPressed(name)` / `isActionDown(name)`; bindings are
registered in `main.cpp` and remappable via the options dialog. Resetting comes in two
strengths there — *Reset selected* and *Reset all*, two stacked buttons under the action list,
since `Action` carries `defaultPrimary` and `defaultSecondary`. Those two and the two key
buttons all grey out without a selection.

**Any key and any click leave the pause**, not only the pause key — `wasAnyKeyPressed` and
`wasAnyButtonPressed` read the same per-tick bits the named queries do. Coming back from
another window is the case that makes it worth having, since `onAppLoseFocus` pauses and the
click that returns is then the one that resumes. The press is *spent* on resuming, and that
ordering is the whole trick: the resume sits in front of the action chain as its `if`, so the
pause key cannot switch back on in the same tick what it just switched off.

**Waiting for a key is a state, not a loop.** Clicking a key button sets its caption to
`$O_PRESS_KEY` and calls `Engine::beginKeyGrab()`; `Options::onUpdate` asks `pollKeyGrab()` each
tick and applies the answer — the pressed VK, `GRAB_NO_KEY` for Escape, which clears the
binding and is the only way to leave an action unbound, or `GRAB_TIMED_OUT` on the three-second
deadline, which leaves it as it was. Waiting costs nothing on purpose: that is what somebody
does who opened the grab by accident or thought better of it, and it must not take the key
they had. The caption says which is which — `$O_PRESS_KEY` reads *Press key or Esc to clear*.

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

**Something that reacts lights up.** `Object::flash()` sets `flashAmount` to `FLASH_STRENGTH`;
`frameBegin` decays it by `FLASH_DECAY` per tick and `Object::render` draws the object's own
sprites over themselves once more, additively, at that brightness — about eight ticks, a
sixth of a second. It is the acknowledgement a switch gives when it is pressed, and the two
counters at the bottom left of the screen give the same one when a diamond or a bomb is
collected: `Player::addInventory` is the single funnel both go through, so it calls
`Level::flashHudIcon` there, and `GS_Game`'s HUD pass draws the preset a second time under
the same additive blend. The two constants live in `object.cpp` and are `extern` so that the
icons cannot drift away from the objects.

**The diamond machine takes the block apart and puts it back together.** Sparks fly out of
the block in its own colours, sampled texel by texel through the debris mechanism; from the
cloud they leave behind, more sparks come back, taking on the colour the diamond will have
where they land and die. The block itself fades to `CONVERSION_GHOST` over the same hundred
ticks — `setConversionProgress` each tick, cleared by the block's own `frameBegin()`, so a
machine that stops pushing lets it stand full again by itself. Both ends of the flight are
computed rather than chosen: the integrator is `position += velocity; velocity *= damping`, so
a spark covers `v0·(1−dⁿ)/(1−d)` over n ticks, which says where the outward cloud ends and, read
backwards, gives the `v0` that lands an inward spark exactly on its target. **n is the number of
moves, not the lifetime**: a particle does not move in its last tick — that update only counts
down and erases — so aiming over the lifetime leaves every spark one step short, and because it
accelerates, that is the longest step of all.

**The handover is the part that has to be exact, and two off-by-one-ticks were spoiling it.**
The inward sparks live one tick longer than the conversion has left, so they are still standing
on their landing points in the last frame the block is drawn; the diamond appears in the next
one, by which time they are gone. And the block does not come back: `frameBegin()` keeps the
conversion progress once the object is dying **or scheduled to die**, the second half being the
one that matters — `disappearNextFrame()` only records the death, `update()` applies it, and
`update()` runs after `frameBegin()`, so `isAlive()` alone still answers "alive" in the tick
where the value would be cleared. Without that the finished block snapped from a 22% ghost back
to full opacity and then took half a second to fade off the new diamond, measured as a green
cast of +38 grey levels over the settled colour; it is +9 now and gone within two frames.

**Dust, not embers.** The outward sparks are many, large, slow, and carry the plain colour of
the texel they came from — `OUT_BRIGHT` and `OUT_END` are both 1, so only the opacity falls.
Glowing sparks read as welding, and the machine is handed rock, ice and grass as readily as
metal. Additive blending was never an option either: there the result depends on the
background, and the same brown would be an ember over rock and a glare over grass. The inward
motes *do* start above 1, where GL clamps to [0,1] (in the browser `clampColor()` does it
instead — see the presentation section) — but that is not a glow, it is the way
around a green cast, since a linear ramp from a blue block to the diamond's warm white passes
straight through green (measured 0.16 at t=0.6, 0.05 once over-brightened).

The sprite must be the neutral white disc at (32,32) in `particles.png` — a particle is
multiplied by its texture region, and (32,0) is a pre-coloured orange that turned every cyan
spark olive.

**An aborted conversion runs the sparks backwards**, because the block can be pushed away,
blown up or switched off in the last moment and a cloud that simply vanishes is a hole in the
middle of the motion. `DiamondMachine::abortConversion` reverses every delta and inverts the
damping — it is a factor, not a summand — and the velocity gets that inverse as well, since the
integrator shifts before it damps and the way back would otherwise be a tick out of step. The
lifetime is *mirrored*: a spark that just set off is over at once, one that was nearly home has
the whole way in front of it. It needs no extra field, because the elapsed count is in the
spark itself — its alpha is a straight line over exactly that time.

Which sparks turn round depends on what happened. The inward ones always do: they were flying
at a diamond that is not coming. The outward ones are debris that is already out, and whether
they are drawn back in depends on whether there is anything to draw them into —
`findLivingBlock()` looks for the block in the level's object list rather than through
`p_objOnMe`, which in exactly the interesting case points at nothing: a destroyed block is
deleted at the start of a tick. Blown up (or teleported away) counts as gone and they fly on
untouched; pushed aside or merely switched off and they are sucked back, aimed at the block's
*logical* cell rather than its shown one, since it is still sliding and will be there by the
time they arrive. The offset is one addend on the velocity — the same travel formula solved for
`v0` — so the return path is translated rather than distorted. Only the gravity of one variant
cannot be reversed exactly: it is added after the damping, not before, so flipping its sign is
an approximation and that spark finds a slightly different arc home.

**`Particle::id` is what makes any of this possible**, and it is the one field that stays 0
everywhere else in the game — the machine stamps its own sparks with a fresh id per conversion
and finds them again through `ParticleSystem::begin()`/`end()`. Filtering by id is an `if` at
the call site rather than a method. And `Particle` has a constructor that zeroes every member,
because the forty-nine callers of `addParticle` build one on the stack and set only what they
need; a field none of them knows about would otherwise arrive as a random number.

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

**At rest the sheet is drawn at exactly 1:1, on whole pixels.** It used to land at 0.9 of its
size, which resamples every texel of a texture whose whole point is the writing on it — and
the arrival never *finished*, because `shownAlpha` is an exponential ease towards 0.85 that
only approaches it, so the scale stayed a hair under its target and the rotation a hair over
zero for ever. Three fractions at once, and the text was soft. Once the residual falls below
`SNAP_RESIDUAL` — half a pixel over the screen diagonal, the longest lever both the remaining
travel and the remaining rotation act on — scale, angle and position are rounded to exactly 1,
exactly 0 and exactly `targetPosition`, which is a `Vec2i`. The mesh's own corners are whole
numbers anyway, so from there each texel covers exactly one pixel and is sampled at its centre.
Measured off a screenshot: 17% of the outline pixels carried the font's own colour before,
58% after. The sheet is therefore 300x400 rather than 270x360, which is exactly the height of
the play area above the status bar — the note now covers the field it is read over.

The texture belongs to the **Engine** and not to the note, and that is not tidiness: it falls
with the framebuffer object, which `Engine::exit` destroys while the GL context still stands,
whereas an `Object` is destroyed only after `main()` has returned.

**More than one note can be on screen**, which is the whole reason it is a pool
(`acquireOffscreenTexture` / `releaseOffscreenTexture`) rather than the single texture it
started as: two notes on neighbouring fields overlap for the third of a second the outgoing
one takes to fade. Sharing one texture between them was not *wrong* — each note bakes
immediately before it draws, and GL runs the commands in order — but it meant a full
render-to-texture, an FBO switch and a read-after-write stall **per visible note per frame**,
for as long as they overlapped. A note borrows a texture when it first bakes and gives it back
in `onUpdate` the moment `shownAlpha` reaches zero (and in `onRemove`, since the destructor
runs when there may be no Engine left to hand it to), so the pool never grows past the largest
number of notes visible at once — walking a row of them holds four or five, not one per note
in the level.

**The roll is geometry, not a shader.** Each end of the sheet is wound onto a cylinder of
`ROLL_TURNS` (0.50) of a turn, tessellated into `ROLL_BANDS` (48) bands, with the perspective
divide done by hand (`f = PERSPECTIVE / (PERSPECTIVE - depth)`) and a shading term per vertex.
**The viewer stands to the left of the sheet**, `VIEW_OFFSET_X` (300) pixels off the axis, so
what comes towards them also moves right — the slant the 16x16 sprite has. That is an ordinary
off-axis projection and costs one term, `e·(f − 1)` added to x; in the plane of the paper `f`
is 1 and the offset is zero, so the sheet at rest still lands pixel on pixel. Measured on a
frame at full roll, the top band's centre sits 30 px right of the bottom band's, falling to
1.5 px as the sheet flattens. **Half a turn is the hard limit, and the sheet now sits exactly on it**: this is a
painter's-order limit rather than a matter of taste, because the pass has no depth buffer, so
the only order that composes correctly is back to front, and only up to π does every further
step of paper keep moving in one direction in depth. Beyond that the far end would come back
round and still be painted in the wrong place. Radius, arc length and turn are one relation,
so at a full half-turn there are only two numbers left rather than three: a fatter bead means
rolling up more sheet. `ROLL_LENGTH` is 0.30, which leaves the middle 40% of the paper lying
flat and makes the bead 38 pixels across — 0.5 rolled the sheet up to its own middle and
looked wrong for it. The strip is
`GL_TRIANGLE_STRIP` and not `GL_QUAD_STRIP` — WebGL has no such primitive, and
`WebBuild/gl_immediate.cpp` hands the mode straight to it.

**Whether it rolls at all belongs to the artwork.** A sheet of paper rolls up; the space skin's
hint is a hard-edged display panel and rolling that would look silly. The switch is a marker
file, `hintscroll.txt`, sitting beside the `hint.png` that is *actually loaded* — its contents
are ignored, only its existence counts, and `Level::loadSkin` looks it up once and answers
through `Level::isHintScroll()`. Without it `shownUnroll` is pinned to 1.0 and the mesh
degenerates to the single flat quad.

Beside the image, and not an attribute in `tileset.xml`, because **each skin slot is chosen
separately**: `<Level skin0=… skin10=…>` is one name per entry of `p_skinFilenames`, so a level
can take its tiles from one skin and its note from another, and a flag in the tileset would be
describing a different file. Following `default_hint.png` costs nothing either — that link is
resolved by `getSkinFilename` itself, which recurses and returns the *final* path, so the
marker is looked for wherever the picture really came from. The four shipped skins need exactly
one file between them: `blocks_01` has the paper and gets it, `blocks_02` and `blocks_03` reach
that same paper through `default_hint.png` and roll too, and `space` brings its own panel and
does not. Absence meaning "no roll" is also the right default for a skin somebody else wrote.

It has to be named in the packing scripts rather than swept up as `*.txt`: `password.txt` is
deliberately packed *unencrypted* in a second pass, and a `*.txt` pattern would take it into the
encrypted set as well, where it would be the key to itself. In `pack.sh` the name is also tested
for first, because `packInto` drops only *patterns* that match nothing — a plain filename
survives an empty glob and 7za then fails on it, which is what broke the `space` archive once.

**The unrolling counts ticks, not alpha.** `shownAlpha` is an exponential ease towards 0.85
that never arrives, so a sheet driven by it would stay a little rolled up for ever.
`activeTicks` counts up while the player stands on the field and down again when they leave;
the sheet opens between tick 20, where it is at 96% of its size, and tick 40, and rolls up
over the same twenty ticks.

**Return and Escape put the note away**, without walking off the field. The sheet is 300x400
and covers the play area it is read over, so the alternative was to move — and moving is a
move you may not want to spend. `Object::dismiss()` is a virtual that answers false
everywhere except on a hint that is currently showing something; `Level::dismissDisplay()`
asks the objects on the *player's own* field and reports whether anything took the key.

The key has to be caught in `GameGUI::onKeyEvent` rather than in `Hint::onUpdate`, and the
tick order is why: `GUI::update()` runs before `p_gs->onUpdate()`, so by the time the level
saw the key the game menu would already be open. Escape therefore asks `dismissDisplay()`
first and only falls through to the menu when nothing was there to close — which is also what
makes a second Escape open the menu as always.

`dismissed` holds until the player leaves the field, or the note would simply open again on
the next tick and the key would have done nothing. Reading it again means stepping off and
back on; that is the same gesture as before and keeps Escape from turning into a toggle that
never reaches the menu.

**It rolls up before it starts to go**, which is why the roll is computed first in
`onUpdate` and `alpha` reads it: while anything is still rolled out, the sheet stays fully
opaque and in place, and only once `unroll` reaches 0 does it fade and fly back. Leaving a
note therefore takes twice as long as it used to and shows what it is doing. The path
without a framebuffer object has no roll to show, so `renderNoteFlat` scales the height to
the part that is still flat — the same silhouette without the bead, and the writing squashes
with it, which is the price of that path.

**The top edge rolls toward the viewer and the bottom edge away from it**, matching the
16x16 sprite on the field; a sheet that curled the same way at both ends would read as a
tube. `direction` is −1 at the top and +1 at the bottom, so one sign in front of `depth`
does it — but the draw order has to follow, because there is no depth buffer here. The
bottom roll goes first and from its outer end inward, since that end is now the furthest
away; then the flat sheet; then the top roll from the crease outward. Measured off a frame
at full roll: the topmost rows come out 7% wider than the middle and the bottom rows 7%
narrower, which is the perspective divide doing its work in opposite directions.

**Past the quarter turn you are looking at the back of the paper, which is blank.** The baked
texture therefore carries the sheet twice, side by side in 1024x512: the written face at x 0
and the bare paper at x 512, with a gap so no texel of one bleeds into the other. Nothing is
mirrored — the sheet curls about a horizontal axis, so left stays left. Each roll is split at
θ = π/2 into a front and a back section, because the seam has to fall on a vertex or one quad
would drag its texture across both panels; the split costs nothing visually, since at exactly
π/2 the paper is edge-on and has no projected width. That makes five sections in all, and
their order is the depth order: bottom-back, bottom-front, flat, top-front, top-back. The back
is exactly as bright as the front, because it is the same sheet of paper.

**The shading follows the surface normal, not the angle of rotation**, and that distinction is
the whole of it: on the back you are looking at the *other* face, whose normal points back at
you, so the term is `|cos θ|` rather than `cos θ` and the paper is at full brightness at both
ends of the curl. `SHADE_EDGE` (0.75) is where it dips, at the quarter turn, where the sheet
shows you its edge.

**Where it flies to is decided once**, in the tick the note opens (`activeTicks == 0`), and
never revisited. Asking again while the player is on the field means the sheet jumps to the
other side of the screen in the very tick they step off — exactly while it is rolling up and
leaving.

**It fades only while it is small.** `FADE_UNTIL` (0.5) is how far along the flight the note
reaches full opacity, and from there it is solid. A half-transparent sheet whose back is
opaque and blank contradicts itself, and there was nothing to see through it anyway; the fade
now does the one job it is good for, which is keeping the note from appearing out of nowhere
while it is still a small shape in flight. `onCollect` is the wrong place for it (it only fires once the player stands within
six pixels of centre, by which time the note is already on its way) and now does nothing at
all — it exists solely to stop `Object::onCollect` making the note disappear.

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

**`renameFile` renames where the platform can and copies where it cannot** — across a mount,
since the browser stages an upload outside the home directory, and for a member inside an
archive, which has no name on the disk of its own. The destination gives way only after the
first attempt has failed: POSIX replaces it in one atomic step and deleting it beforehand would
open a window in which neither name exists, while Windows refuses the replacement and needs the
second try. Three callers want exactly that — `retireShadowingCopies`, which said so in a
comment (*"the game's filesystem has no rename"*) and did copy-then-delete; `Campaign::save`,
whose swap otherwise wrote the whole archive a second time, megabytes for a campaign with its
music; and the progress database's crash-safety file.

`pushCurrentDir`/`popCurrentDir` maintain a search root, which is how `main.cpp` mounts
`data.zip[...]` as the asset root (the commented-out `fs.pushCurrentDir("data")` next to it
switches to loose files for development). User-writable state — saves, progress, custom levels,
screenshots, videos — lives under `getAppHomeDirectory()` = `My Documents\Blocks 5\`, never next
to the executable.

**Levels, campaigns and skins have two roots, and the game folder wins.** What ships stays
beside the executable and is read from there, so it is always exactly as new as the program;
the user directory holds only what the player made or imported.
`FileSystem::resolveContentPath("levels/skins/space.zip")` asks the game folder first and falls
back to the user directory, and `isShippedContent` — "it exists in the game folder" — is
simultaneously the definition of undeletable, un-overwritable and un-saveable-over. The paths
are absolute on purpose: a relative one would be resolved inside the mounted `data.zip`.

The order matters and the other one is wrong. User-first would let a stale copy shadow a fresh
shipped file, which is the bug this replaces: every installation used to carry a private copy
of the campaign, the four skins and the two examples, frozen at whatever version it first
installed, and that is how a skin marker went missing on a machine that had just built the
current sources. Game-first has one cost, and it is the reason both editors now refuse to save
under a shipped name: such a file could never be loaded again, because the game folder would
answer first.

**Seven files belong to the player even though they ship with the game**, and for them the
order is reversed: `FileSystem::getPlayerFiles` lists the two example levels and the five
`readme.txt`, `belongsToPlayer` makes `isShippedContent` say no for them, and
`resolveContentPath` looks in the user directory first, falling back to the game folder's copy
as a template. The two halves belong together — allowing the save while still answering from
the game folder would write a file that could never be read back. The list is hard-written
rather than a directory listing because a working tree also holds the forty-two campaign
sources, which are nothing of the kind.

Only the five `readme.txt` are *copied* on a first start, and only because nothing in the game
ever reads them: without the copy they would sit in no folder at all. The examples are not
copied. They are listed and loadable straight out of the game folder, and the player's own
version appears the moment they save one — so an untouched installation keeps getting the
newest examples, and Delete stays greyed until there is actually something of theirs to
delete. That last part is why the Manager asks `Transfer::isRemovable` ("is there a copy in
the user directory") rather than `isBuiltIn`.

**On the first start of 1.2.0 the old copies are set aside.** `retireShadowingCopies` in
`main.cpp` renames every file in the user directory whose name the game folder also has to
`<name>.bak` — renamed and not deleted, because there is no way to tell from outside whether
somebody edited one, and what is in a player's folder is theirs. `.bak` is inert everywhere:
every lister filters on the exact extension, and `convertPath` recognises an archive by
`.zip/`, not by `.zip`.

**`ProgressDB` keys on the campaign's bare filename, not on its path**, and that is what made
the move survivable. The key used to be the full path, so shifting `blocks.zip` from the user
directory into the game folder would have silently reset everyone's 42 levels — no error,
nothing in the log, just a progress bar back at zero. `keyFor` strips the directory on the way
in and on the way out, which migrates an old `progress.zip` by reading it: no separate step,
and the next save writes the short form. Case is **not** folded there, deliberately: under
Linux `Blocks.zip` and `blocks.zip` are two different campaigns, and joining their solved sets
could never be undone.

**It holds nothing.** `query()` reads the file and `markSolved()` reads it, adds and writes it
back; there is no map that lives for the process. That is what makes the Manager able to
import, merge and delete a progress at all — a copy in memory would answer from what was there
at startup and the next completed level would write it straight back over the import, a delete
would undo itself within one level, and merging would need a `clear()` the class never had.
Merging then needs no code of its own: read the imported file and mark everything in it as
solved, which `markSolved` folds into what is on the disk.

The reads are per frame — `getLevelStatus` and the progress bar are both inside
`GS_SelectLevel::onRender` — so the screen keeps the answer for as long as it is shown and
re-reads in **`onGetFocus`, not `onEnter`**: coming back from a played level is a *pop*, and
`popGameState` gives the state underneath the focus without entering it again, so a level just
solved would still be shown as unsolved. The count is clamped to the campaign's length, or a
merged database would draw the bar past its own frame and label it *45/42*.

**A save no longer destroys what it is replacing.** Writing a member into a zip rebuilds the
archive, and `File_Archived` removes the old file before the new one exists (`remove` at
`file_archived.cpp:526`), which for a one-member archive is every save — so a crash or a full
disk in that window took everything. The database is renamed to `progress.zip.saving` first and
that file deleted only once the new one stands; `query()` puts it back where the real file is
missing or unreadable, and deletes it where the real file reads. The invariant is worth stating
plainly: **the backup exists exactly while a save is in flight**, so one found lying about is
from a run that died, and leaving it would mean the next unrelated fault restores a database
months out of date. A delete takes it along for the same reason.

That parse trusts nothing, because the Manager imports this file and it is therefore a
stranger's: no root element, no `campaign` attribute and a level index that is negative or
absurd are all skipped rather than crashing. It used to run at startup from `main()`, before
`engine.init()` — no window, no toast, and in the browser a wasm trap that looks like a hang
with the offending file locked away in IndexedDB.

**A level somebody sent you is played from the level select screen, not from the editor.**
`Campaign::loadSingleLevels` builds a campaign that exists as no file: every loose `*.xml` in
the user's level folder, listed last in the campaign box under `$LS_SINGLE_LEVELS`. Opening
such a level in the editor was the only way before, and the editor gives the puzzle away by
design — `level.cpp` skips the darkness there (`if(nightVision && !inEditor)`) and
`teleporter.cpp` draws a line to every teleporter's destination.

It carries **no progress**, and that is what `isSingleLevels()` is asked about in five places:
every level is unlocked, finishing one records nothing, the run ends back at the selection
instead of at the next level, and both the *next to do* button and the progress bar — frame,
label and all — are hidden. A bar that can never move reads as a fault, not as an empty one.
Levels that have nothing to do with each other have no order to earn.

The list is sorted by the **localized title**, not by filename: that is the line the player
reads. Getting it means parsing each level's XML for the one `title` attribute
(`readLevelTitle`), which is cheaper than a `Level::load` with all its objects and skins but
is still a parse per file at dialog entry. The caption is
`formatSingleLevelCaption` — `Title (filename.xml)` — because three levels called *Unnamed
Level* are otherwise indistinguishable, while inside a campaign the filename would say
nothing but `level_2.xml`.

**The whole select screen is keyboard-operable.** Left and right step through the levels,
Home and End jump to the ends, Return plays, Shift+Right is *next to do*, and up and down
change the campaign. The last four go through `GS_SelectLevel::onUpdate` only while the
campaign list does **not** hold the focus, because those are exactly the four keys
`GUI_ListBox::onKeyEvent` handles itself; left, right and Return are unconditional, since the
list ignores the arrows and forwards Return for want of a submit button. Everything runs
through `pressButton`, which asks `isActive()` and `isReallyVisible()` first — otherwise
Return would start a locked level that the mouse cannot even click.

**One Manager button in the main menu** opens a dialog that imports, exports and deletes, on
all three platforms — `src/transfer.cpp` over `WebBuild/web_transfer.cpp` in the browser,
`GetOpenFileNameA`/`GetSaveFileNameA` under Windows and `zenity`/`kdialog` under Linux, behind
one interface: `beginImport` starts it and `pollImport` is asked each tick, so an asynchronous
dialog and a modal one look the same to the caller.

`Menu.ManagerPane` holds the five kind radios in 92px columns, the list, *Refresh*, and a
bottom row of *Import*, *Export*, *Delete* and *Close* spread over the window's own width.
The two rows span the same x=10..486 without sharing a grid: the captions decide the first
width — "Zusammenfuehren" and "Aktualisieren" are what the 92 is for — and forcing that grid on
the second would leave a hole where a fifth button would be. Import needs no selection and
comes first; Export and Delete work on the selection and grey themselves out without one.

**The progress database is the fifth kind, and it goes round the directory machinery rather
than through it.** It is one file, with one name, in the user directory *itself*, and nothing
of the sort ever ships — so `directoryFor` names it outright. An empty subdirectory would have
been the obvious answer and is a trap: `list()` would then read the game folder's own root,
where `data.zip` lies, and `remove()` would point at whatever it found there. `classify`
recognises it by a `progress.xml` inside the archive, which a zip's table of contents answers
without the password, and `install` refuses one that does not parse — the same guard a campaign
has, so a damaged file cannot destroy a good one of the same name.

`Menu.ConfirmPane`, which must stay the **last** child in `menu.xml` so it draws last and takes
the clicks, asks before a delete and before an import replaces anything. Three columns, of
which the middle one carries *Merge* and is shown only for a progress database, so *Yes* and
*No* keep their places either way; the text is wrapped, since the code sets it and a filename
can be any length. **The two buttons are renamed for an import** — *Replace* and *Cancel* —
because yes and no are no answer to a question that offers replacing and merging.

Asking before an overwrite needed somewhere to ask *from*: `install()` composed the destination
name inside itself and tested for the overwrite two lines before the copy, so no caller could
put the question first. `Transfer::targetName` and `wouldReplace` are that answer and
`install()` is built on the same two, so the name asked about and the name written cannot drift
apart. The whole import waits for the answer, `finishImport()` included — in the browser that
call deletes the staging file the bytes are in.

**`Transfer::isBuiltIn` no longer keeps a list**; it asks whether the file exists in the game
folder — and answers no for the seven files that belong to the player. Three callers, all the
same rule: an import must not take such a name, and neither editor may save under one. Delete
goes through `isRemovable` instead, which is the stricter question. They stay listed and exportable; only
*Delete* greys out. `Transfer::list` returns the union of both roots, sorted, and needs no rule
for a name in both because no path can create one. Case is the file system's problem now rather
than a hand-rolled comparison's, which is right: on Windows `Blocks.zip` *is* `blocks.zip`, and
`fileExists` says so.

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

**A scrolling texture offset is reduced to one period, and that is a phone bug.**
`wrapTextureOffset` (`util.h`) is called on all four scrollers - the menu's title clouds, and
the level's rain, snow and clouds - because each of them translates the texture matrix by an
offset that had been growing since the level began. A texture coordinate reaches the fragment
shader as a *varying*, and the shader Emscripten's GL emulation builds opens with
`precision mediump float;` with the texcoord varyings declared under it: ten mantissa bits,
which a desktop GPU implements as fp32 and a phone actually honours. The step it quantizes to
is **offset/2048 texels**, so the clouds - moving one texel a tick - drift smoothly for about
forty seconds and then go visibly steppy, and the rain, at twenty texels a tick, crosses the
same line in two seconds. Nothing is wrong on any desktop, which is what makes it hard to see.

Subtracting whole periods is **exact** under `GL_REPEAT`: it moves the finished coordinate by a
whole number and samples the same texel. Verified against the real matrix order - bind's
`1/w,1/h`, the scale, the translate and the rotate - for all four, deviation 0.000e+00 at
offsets up to 900000. Two things to keep right: the wrap goes **after** the `sin` that reads
the same offset, whose phase has to follow the unwrapped value, and the period is the
*texture's* own size, since a skin brings its own art.

**The angle those sines are given needs no such care**, and the arithmetic is worth having
once: they are `double` throughout, so one ULP at argument *A* is `A/2^52`. The snow's
argument grows at 0.2 rad/s and its sine is scaled by 500 pixels, so half a pixel of error
needs 1.7e-3 rad and arrives in about **700 000 years**; after 25 days of rain - the fastest
of them - one ULP is 9.6e-9 rad. What runs out first by a wide margin is the millisecond
counter feeding it: `Level::time` is `int` and undefined after **24.9 days** in one level,
`GS_Menu::time` and `Engine::time` are `uint` and wrap at 49.7. All three reset on entering a
level or the menu, so reaching any of them means a machine left on one screen for weeks. In
`float` the same rain argument would have a ULP of 4 radians, which is the same distinction
as the `mediump` one above, two steps further along.

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

**Writing one needs no library at all**, and `img_save.cpp` is the mirror image of that
reasoning: decoding PNG is hard, which is why stb_image is vendored; encoding it is not,
because zlib does the work and zlib is already compiled into all three builds for minizip's
sake. `compress2()` returns exactly the zlib datastream an `IDAT` chunk is defined to hold and
`crc32()` is the checksum every chunk carries, so the whole encoder is a signature, three
chunks and a filter loop. Screenshots are PNG because of it — `SDL_SaveBMP` and its 900 KB
files are gone.

Two things there are worth knowing. The per-row filter heuristic — pick the filter whose
bytes have the smallest sum of magnitudes read as signed — is twenty lines and worth them:
measured on a real 640x480 screenshot, 239 KB against 283 KB for no filtering at all, out of
a 900 KB bitmap. And **the alpha channel is not free**, which is the opposite of what it looks
like: `glReadPixels` must ask for `GL_RGBA` because that is the only combination WebGL 1
allows, but a channel of nothing but 255 does not collapse in the deflate — it pushes every
prediction one byte apart. The same frame is 330 KB as RGBA and 247 KB as RGB, so the encoder
takes a source layout and a destination layout separately and drops the channel while
building each row, before the filter ever sees it.

**The browser gets screenshots too**, and this is what unblocked them: the two reasons F11 was
refused there were `GL_BGR` and `SDL_SaveBMP_RW`, and neither is in the path any more. There is
nowhere sensible to *put* the file, though — the IndexedDB is for saved games, and filling a
player's quota with pictures they can never look at is not a trade worth making — so the bytes
go straight into their downloads through `WebTransfer::downloadBytes`, the same Blob mechanism
the Manager's export uses. It copies the heap slice (`new Uint8Array(HEAPU8.subarray(...))`)
rather than handing the Blob a view, because `ALLOW_MEMORY_GROWTH` can invalidate one at any
allocation. `$A_TOGGLE_CAPTURE_VIDEO` is still the one action `main.cpp` withholds from the
web build.

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

**Every function key belongs to the game, not to the browser.** `pre.js` swallows F1 to F24 in
the capture phase, before SDL or the browser sees them, because they are bindable actions like
any other key and the desktop build answers to all of them — a player who knows the game must
not find half of them missing, and taking a named few would be the worst of both. Left alone,
F1 opens the browser's help, F5 reloads the page and loses the level, F10 reaches for the menu
bar, F11 goes fullscreen and F12 opens the developer tools. Nothing is lost by it: Ctrl+R and
the address bar still reload, Ctrl+Shift+I still opens the tools, and fullscreen is Alt+Enter
as it is on the desktop. Whether a browser hands a page F11 and F12 at all is its own decision;
asking costs nothing where the answer is no.

**Which is why the click prompt names Alt+Enter.** A desktop browser offers no way to reach the
game's own fullscreen and nobody guesses that chord unaided, so `$WEB_FULLSCREEN_HINT` sits
under `$WEB_CLICK_TO_START` in the tooltip font — an aside, not the message. Not on a phone,
where the game takes the fullscreen itself on the first touch and there is no Alt to press;
`Engine::isPhone()` is the one C++ place that asks, and it forwards to the `b5_isPhone` in
`pre.js` that the page uses too.

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

**`touch_controls.js` carries a stamp of its own**, `touch_controls-<hash>.js`, and the hash
is the md5 of that one file rather than the payload's. Reusing the build stamp would be worse
than leaving it unstamped: it hashes the three payload files, so a pad-only edit would not
move it, the URL would not move either — and the file would then be served `immutable` for a
year instead of for a few heuristic hours. Measured: editing the pad moves
`8cb19d724ef9` → `3aa14d7f1448` while `blocks5-164c6a033fd7` stays put, so a 17 KB change
drags no part of the 13 MB payload with it. `build.sh` rewrites the name in both pages and
substitutes `%%PAD%%` into `sw.js`, the same way it already does for `blocks5.js` and
`%%BUILD%%`.

**Two files can never carry a stamp**, which is why the header half still exists:
`index.html` is where the stamps are written down, and `sw.js` is registered under a fixed
URL — a stamped one would leave the old worker alive under the old name. `WebBuild/htaccess`
ships as `.htaccess` beside `index.html`: a year of `immutable` for anything stamped,
`no-cache, must-revalidate` for everything that is not — those two plus `blocks5.html`,
`manifest.json` and the four icons — `AddType application/wasm`, and `ModPagespeed off`.
The icons and the manifest could be stamped and are not, because they change once in a few
years and the manifest would have to be generated rather than copied to name them.

**That list is every unstamped file and not a chosen few, because the gap is silent.** A
file with neither a stamp nor a rule gets a *heuristic* lifetime in the browser, a fraction
of its age, and then goes stale with nothing anywhere to say so. The pad was exactly that
before it was stamped: a new build's page and payload arrived, the on-screen pad did not, and
only a private window showed the new one. **The service worker cannot fix that and it is
worth knowing why**: a subresource the browser's HTTP cache still thinks fresh never reaches
the worker at all. Measured on a reload, the pad came back with `workerStart` 0,
`transferSize` 0 and `deliveryType` "cache" — so fetching it inside the worker with
`cache: 'no-cache'` changes nothing, and with a header in place the same measurement reads
`workerStart` 79.7 and the new bytes arrive. The worker's network-first branch is what keeps
the page working offline; freshness is the URL's job, and the header's only where there can
be no stamp.

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

**In the browser the program never ends, so nothing is ever destroyed.**
`emscripten_set_main_loop_arg(…, 1)` asks for the simulated infinite loop, which unwinds the
stack with a JavaScript `throw` — so `Engine::mainLoop` does not return, the `engine.exit()`
standing after it in `main()` never runs, and no destructor runs either: not `~Engine`, not the
game states that are locals of `main()`, not a `Level` and not an `Object`. The unwind is a JS
exception and not a C++ one, so it does not run destructors on its way out. **Anything that has
to happen must therefore hang off something that runs *during* play** — a logic tick, `onLeave`,
`onRemove` — and never off teardown. Two consequences that are visible from outside:
`config.xml` is written only where somebody asks for it (the options dialog's OK, and the CRT
pane's *Try it*) and never on quit, so a browser player who never opens the options has the
language detected afresh at every start; and every GL object the Engine owns is simply left to
die with the page.

**The flag is load-bearing and must not be tidied away.** Emscripten ends the call with
`throw "unwind"`, and `callMain` swallows it without restoring `__stack_pointer` — so the
abandoned frames stay above it and every later `requestAnimationFrame` allocates below them,
which is exactly what keeps `main()`'s locals alive. Its three game states are such locals, and
`Engine::registerGameState` keeps raw pointers to them. Passing 0 instead breaks two things at
once: `mainLoop()` would return, so the `engine.exit()` after it would tear the engine down
*before the first frame* and the loop would then run against the wreckage; and `main()` would
return, destroying the game states the Engine still points at. Getting rid of the flag is
therefore a restructure — game states off the stack, `exit()` moved into the quit path — and not
a one-word change. To persist settings in the browser, a `pagehide` handler calling
`saveConfig()` is the smaller answer, and it catches a closed tab, which the Quit button never
sees.

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

**A mouse-move event has to mean the mouse moved**, which is not the same question as whether
`cursorPos` changed. That position comes from `Engine::getCursorPosition()` and therefore
through the CRT filter's barrel distortion, so anything that changes the warp moves the cursor
in game space with the hand perfectly still — and the one control that changes the warp is the
curvature slider, which is dragged with the mouse. The synthetic `onMouseMove` set a new slider
value, the value set a new curvature, and the loop closed: measured with the hand held on the
handle, the slider took 249 values in six seconds, flipping between 0 and 1 at the logic rate.
The step is what makes it reach that far — `getOverscan()` is deliberately zero at curvature 0
and its full 1.3% the moment the slider leaves the stop, which a third of the way out from the
centre is a pixel and a half, and the bar turns 1.7 pixels into one unit. So `GUI::update()`
asks for both: the window's own cursor position (`Engine::getRawCursorPosition()`) must have
changed **and** the game-space one must have landed on another pixel. `noMoveCounter`, which times the tooltips,
reads the same answer.

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

**Text written to a fixed place has to be measured first.** `Font::renderText` neither wraps
nor clips, so a level whose title is longer than the space kept for it simply draws over
whatever is beside it — in the select screen across the description column and off the right
edge, in the status bar across the Menu button. `Font::fitText(text, maxWidth)` cuts it down
and ends it in three dots; `Font::adjustText` is the other answer, and wraps instead, which is
what the multi-line help pages want. Both skip over `<h>…</h>`: it draws nothing, so it must
not count toward a line's width, and a hard break landing inside it turned the markup into
visible text — `<h>Kopf</h>` came out as `<h>Kopf<` and `/h>`, which is one long headline away
in pages that already use `<h>`. The two callers keep the level *number* and the *filename*
whole and shorten only the title, since those are what tells two levels called *Unnamed Level*
apart: the caption is measured once with an empty title to learn what the frame costs, and the
title gets the rest. A second pass over the finished caption is the backstop for a filename so
long that even the frame does not fit.

The cut may not land inside `<h>…</h>`, so `fitText` drops a half-cut tag entirely and closes
whatever it left open. **`<h>` ends with the string it began in**, and both `measureText` and
`buildText` now enforce that with a counter: the option stack they push on belongs to the
`Font` and not to the text, and a string is laid out on its own, so markup could
never have carried across a call anyway. An unclosed `<h>` used to leave `italic` set and an
entry on the stack for the rest of the run; an extra `</h>` used to pop the *caller's* entry,
or `top()` an empty stack — a level titled `</h>Hello` crashed the game, and a level title is
a file from a stranger.

**`measureText`'s position array is indexed by byte**, one entry per byte of the string and one
behind it — `text.length() + 1`, always. The bytes of `<h>` and `</h>` get an entry each even
though they draw nothing, all carrying the cursor the tag stands at. That is what the three
callers need and already assumed: the edit boxes look a position up under the same byte index
their caret uses, and `GUI_MultiLineEditBox` reads `[i + 1]` to size a selection. One entry per
loop pass instead left the array short — two per `<h>`, three per `</h>` — so a caret at the end
of such a text read past the vector. Typing `<h>abcdef` into the level editor's title field and
pressing End put the caret 190 px right of the last letter, which is whatever stood one past the
end.

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
elsewhere, and answers only `de` or `en` — every one of the 386 IDs in `languages.txt` has
an English body and a German one and nothing else, so detecting `fr` would give a wholly
English game that merely believed otherwise. The one `§fr:` and the one `§es:` in that file
are the lines of its own header explaining what the tags mean. It runs only when
`config.xml` has no `<Language>`.

Nothing ships a `config.xml` template — not the installer, not the web build. The game writes
the file itself on exit, which is what leaves the detection a chance to run at all.

**Localization.** Any user-facing string starting with `$` is an ID resolved against
`data/languages.txt` by `Engine::localizeString` / the free `loadString` helper. In that file a
`$ID` line is followed by per-language bodies tagged `§en:`, `§de:`, `§fr:`, `§es:` — that
prefix is the section sign, 0xA7 in Latin-1, not the pilcrow. A separate character, `¶`
(0xB6), inserts a newline inside a body. Missing translations fall back to English. Level titles,
tooltips and menu captions in XML all use these IDs.

**No string names a key.** A message that writes "(F5)" or "Return/Enter" into its text is a
lie to everyone who rebound anything, so `%BINDING{$A_RESTART_LEVEL}` stands there instead and
expands to whatever that action is bound to now: both keys separated by a slash, one on its
own, or the word for unassigned. `%BINDING_OPTIONAL_RIGHT{…}` is the same with a leading space
and *nothing at all* when the action is unbound, which is what a button caption naming its own
shortcut wants — "Restart Level" and not "Restart Level ". `Engine::expandBindings` does it
once, on the finished text, which is why `localizeString` is a shell around
`localizeStringRaw`: the raw half recurses through the `$ID` lookup and the English fallback,
and expanding on the way out of each of those would be the same work several times over.
`verify.py`'s `bindings` check reads every marker against the `registerAction` calls in
`main.cpp`, because an action that does not exist expands exactly like an unbound one and would
otherwise be found by a player.

**`<k>…</k>` is the keycap**, and the font draws the frame. It cannot go in the glyph batch —
it carries no texture — so the rectangles are collected while the text is laid out and drawn
once the batch is closed, which also carries them through the two shadow passes with the
glyphs; a keycap without the same shadow would look pasted on.

**The frame is drawn on exactly rows `capTop`..`capBottom` of a glyph cell**, two optional
`<Font>` attributes, and nothing about it is derived from the line. `lineHeight` and `offset`
describe the line a font is *set* at, and the ink is free to sit elsewhere in either direction:
the note's font ends its letters five rows above the foot of its line box, so a frame drawn on
the line box sits under the word instead of around it, with its top edge through the capitals —
while the tooltip font's letters are *taller* than its line, since `Backspace` reaches a row
above the capitals and a row below the baseline and a ten-row line has room for neither.

**That second case is why the frame carries its own height rather than the line's.** A frame
fixed at the line height and merely centred cannot be placed in a font whose ink does not fit
inside the line: only the sum of the two attributes is read, so every pair with the same sum
gives the same frame and the next sum moves it a whole row — always one row too high or one too
low, with no third option. Saying the frame outright is also what makes `verify.py`'s
`font_metrics` check a straight comparison against the measured ink.

They default to the line box, which is what `font.xml` and `credits_font.xml` measure out to
anyway, so those two are unchanged. Three files carry them: `tooltip_font.xml`, at rows 1..12,
and the two skins that bring a `hintfont.xml`. Keeping two keycaps on neighbouring lines apart
is the font's own business now, since its author is the one saying how tall the frame is — and
it is not a rare case: two rows of the help table and any wrapped line of a hint note have
keycaps directly above one another.

**Data, and not the image measured at every start.** Where a font's letters sit is a constant
of the art, and a statistic recomputed at load would move every keycap in the game by a pixel
because somebody redrew one glyph — silently, with nothing in any diff to point at.
`verify.py`'s `font_metrics` check is the other half of writing it down: it reads the first and
the last inked row of every printable character out of the font's PNG, takes the **mode** of
each — the top of a capital and the line the writing sits on, where a brace reaches higher and
a comma lower than anything a key is ever called — and reports a font whose frame would cut
into its letters or sit off to one side of them, naming the two numbers to write. It is the
same arrangement as the committed `.ico`: the file is the source of record and the check is
what stops it going stale. (`read_png` in `WebBuild/make_icon.py` learned the narrow bit depths
for it — `credits_font.png` is a two-colour palette at one bit.)

**A keycap is an atom to `adjustText`.** A box cannot be broken across two lines, so the whole
`<k>…</k>` run moves down together, the way any typesetter treats an inline box — and the
renderer is then never asked to draw half a frame. That is the whole answer to line breaks
inside a keycap, and it is why the run is measured rather than walked character by character:
the padding either side belongs to its width.

**The right side of the frame carries the slant.** An italic glyph leans right — its top is
drawn `options.italic` pixels further along than its foot, while the cursor advances by the
upright width — so a frame that ends where the cursor does cuts the last letter of the key
name. That is every speech balloon, which sets italic for the whole text: the hotel's
`[Enter] / [Num Enter]` had the *r* of each word touching the frame. The advance after `</k>`
grows by the same amount, or the following word would move into the frame instead; the left
side needs nothing, since the first letter's foot still stands on the cursor.

**Between keycaps that belong together stands a half space** — `HALF_SPACE` in `font.h`, half
of that font's own space and a space in every other respect: measured like one, and a line
breaks at one and replaces it exactly as a break replaces a space. Each keycap already stands
off its own frame, so a full space either side of the slash leaves it adrift between the two
keys instead of the pair reading as one binding, and the same holds for the plus of a chord:
`<k>Alt</k>·+·<k>Enter</k>`. It is a byte rather than an element like `<k>` because breaking is
a matter of characters: `adjustText` searches backwards for the last one it may cut at, and an
element would have to be taught to be a break as well as to be skipped over.

The byte is the **middle dot**, `\xB7` — the character an editor shows a space as, and the
third of this file's meaningful bytes beside `§` and `¶`. It has to be a printable one because
the chords are written out by hand in `languages.txt` (a `%BINDING{…}` cannot say *Alt*), and a
control character there would be invisible to whoever edits the line. `Engine::getBindingMarkup`
writes the same byte around its slash. The plus that joins a key to a *word* keeps its full
space — `%BINDING{$A_PLANT_BOMB} + direction` — so the help table shows the hierarchy: tight
where keys bind to each other, loose where prose follows.

**A keyboard has two Enter keys and the game tells them apart nowhere.** `isReturnKey`
(`util.h`) is the one place that says so, and everything reading the SDL key itself goes
through it: confirming a dialog, playing the selected level, leaving the credits, the level
editor's settings, and Alt+Enter for the fullscreen. The named actions never needed it — a
binding has a primary and a secondary, and `$A_SAVE_IN_HOTEL` has used both since it was
written. Both are called `Enter` in both languages — `Enter` and `Num Enter` — which is the prefix
the other seventeen keypad keys already carry. It is what a PC keycap prints (a German board
prints the hooked arrow and no word at all, so there is nothing to copy off the cap) and it is
the word every neighbouring language borrowed. `Return` was SDL's own name for the key, and
since the fallback capitalises SDL's name, the English entry for it was replacing nothing.

**`VirtualKey::niceName` is what a player reads**, as against `name`, which is SDL's, and `id`,
which config.xml holds and can therefore never be translated. It is a `$ID` and not the
finished text because the language can change while the game runs. The table in `engine.cpp`
spells each id out rather than composing it from the key name, so that `verify.py`, which
collects `"$…"` literals from the source, catches one that `languages.txt` does not have; a key
with no entry keeps SDL's own name with the first letter raised, which is all `F5` needs. A
joystick keeps a device word in front, localized separately, because `B3` beside a keyboard key
would say nothing about where it is.

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
- **A rename goes through a tool that parses the code, never through a text
  substitution.** `clang-rename` and `clang-change-namespace` are installed
  (LLVM 18, `/usr/lib/llvm-18/bin/`), and `sh Tools/compile_db.sh` writes the
  `compile_commands.json` they need — asking `LinuxBuild/build.sh flags` for
  the real compile flags rather than keeping a copy of them, because a database
  with its own idea of the include paths has a refactoring tool parsing a
  different program from the one that ships. The file is a build product and is
  gitignored.

  ```
  sh Tools/compile_db.sh
  clang-rename-18 -i --qualified-name='Texture::unbind' --new-name='...' Blocks5/src/*.cpp
  ```

  The reason is not tidiness. A `sed` over `GLState::` → `GL::` also rewrites
  `GLState::GLState`, the constructor of the struct of that name — measured:
  `clang-rename` asked for the same namespace rename leaves that line alone,
  and `sed` breaks it. The same shape waits wherever a member, a local or a
  word inside a comment or a string literal shares a name with the thing being
  renamed. What saves a blind replacement here is that `Tools/syntax.sh`
  compiles all 123 sources in seconds, so the mistake is a compile error
  rather than a silent one — but that is a backstop, not a method, and it
  catches nothing that still compiles.

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

- **Comments are in English, and so is everything the build and test tools print.** The tree
  was commented in German until 1.2.0 and the mixture with English code read badly; the sweep
  that changed it moved no code at all, which is what made a change across 286 files
  reviewable. German survives in exactly three places, all of them data rather than prose:
  `data/languages.txt`, the inline `"\xA7" "de:…"` strings, and the two word lists in
  `verify.py`'s `comments` check together with the two faults `selftest.py` injects into it.

  **That check reads further than the other eighteen**, and the reason is a file it did not
  catch: `WebBuild/htaccess` was wholly German through the whole sweep, because it has no
  extension and `source_files()` walks `.cpp`, `.h` and `.c` under `Blocks5/src`, `WebBuild`,
  `PWEncrypt` and `ShowUserDir` — never `LinuxBuild`, and never a script. `prose_files()` is
  the second list: the sources plus every `.js`, `.sh` and `.py` in `LinuxBuild`, `WebBuild`
  and `Tools`, plus `htaccess` by name, each with the marker its comments begin with. Only the
  language half uses it; the density guard stays on the sources, since a shell script has no
  ratio worth judging. The
  shipped `readme.txt` files are English, and always have been — the German that survives
  is the three places above and nothing else.

  A shared glossary settled the vocabulary, and its traps are worth knowing before writing a
  comment that reaches for the obvious word: `uebersetzen` in the filter code is *compile*,
  `Zeiger` is the mouse *cursor* almost everywhere but a real pointer in `hint.cpp`, `massiv`
  is *solid* (`OF_MASSIVE` means impassable), `Ebene` is *layer* — *level* would collide with
  the class — and `Bild` is a *frame*, a *picture* or an *image* depending on which the
  sentence means, which is the distinction the whole browser timing argument rests on.
- **Every source file is pure ASCII** — `Blocks5/src`, `WebBuild`, `PWEncrypt` and
  `ShowUserDir`, all of it. Umlauts are written `ae oe ue ss` (`AE OE UE SS` inside an
  all-caps word), so the encoding of these files no longer matters to anything: ASCII is a
  subset of UTF-8, of Latin-1 and of every codepage, and none of them needs a BOM or a
  `/utf-8` switch. Keep it that way — one umlaut typed into a comment puts the tree back to
  being encoding-dependent.
- **The three bytes that carry meaning are written as escapes.** `data/languages.txt` is
  Latin-1 and shipped that way; the game parses it with `'\xA7'` (the section sign, §) in
  `engine.cpp`, `'\xB6'` (the pilcrow, ¶, a line break) in `font.cpp` and `'\xB7'` (the middle
  dot, ·, a half space) in `font.h`; a few inline localized strings
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
