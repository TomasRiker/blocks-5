# SDL 1.2 -> SDL 3 migration: the plan

**Status: not being done.** The game works, 1.2.0 has shipped, and the owner decided the risk
was not worth taking now - see ROADMAP item 37. This document exists so that the decision can be
revisited from evidence rather than from memory, and so the survey behind it is not lost. It may
never be executed; that is a fine outcome for a plan.

**There is no licence pressure to do it.** An earlier reading of this said there was. Blocks 5 is
GPL v3 (`LICENSE.txt`) with its complete source published, which more than satisfies what LGPL 2.1
section 6 asks of the statically linked SDL 1.2 and shine - see the LICENCE section of
`Blocks5/libs/sdl-1.2.15/PROVENANCE.txt`. SDL3 being zlib-licensed is a tidiness, not a fix.

---


Blocks 5 ("Bob's Amazing Adventures"), branch `claude/review-hqxk2p`.
Written against `SURVEY-DIGEST.md` — ten surveyed areas, each adversarially verified, 24 claims refuted, 13 left uncertain, plus a completeness critic. **Where this plan states a fact, the survey measured it. Where it says *unproven*, nobody ran it.** Those two words are used strictly.

**This is the second edition.** The first was attacked by a reader with the survey in hand, and the attack found the plan breaking its own rules in two places. Three of the four checks it invented were **green by construction** and one was **red for the wrong reason** — which is the exact failure abort criterion 12 exists to stop, applied to everything except the plan's own checks. And a handful of Windows sentences read as settled when nothing had been run, which breaks the honesty rule in the paragraph above. Both are repaired. §4 Stage 1 now states, for **every** new rule: what fault makes it fire, whether that fault is reachable, and whether the rule is green on today's tree. Where a rule is red on the SDL 1.2 tree it says so, and says which stage turns it green.

Three corrections of fact are folded in, established after the first edition: the shipped cmake is the **32-bit** build and it is a **directory of 1,715 files**, not one binary; **`-A Win32` is mandatory**, not optional (documentation-derived, not measured here); and the browser payload grows by about **65%**.

---

## 1. What this buys and what it costs

### What it buys

**A licence problem goes away.** SDL 1.2.15 is LGPL 2.1 — the banner is at `Blocks5/libs/SDL-1.2.15/include/SDL.h:5-17` — and it is **statically linked into `blocks5.exe` today**. That is the identical constraint CLAUDE.md cites for keeping `OpenAL32.dll` dynamic ("LGPL v2 and must stay dynamically linked"), applied inconsistently. Its `PROVENANCE.txt` is one of only three of the twelve that never mentions a licence at all. SDL3 is zlib: "Permission is granted to anyone to use this software for any purpose, including commercial applications." The migration removes a real static-linking licence problem rather than creating one. This is the strongest argument for the whole exercise and the survey found it last; it belongs in the changelog, not only here.

**The browser stops running on a JavaScript reimplementation.** Emscripten's SDL 1.2 is a ~134 KB hand-written JS library that `WebBuild/platform_stubs.cpp` exists solely to patch. SDL3's Emscripten backend is the same C the desktop runs.

**2.4 MB, 170 tracked files, 67 `ClCompile` entries and two vendored patches leave the tree** — and with them the `#undef UNICODE` guard in `SDL_win32_main.c` and the `#pragma pack` bracket in `SDL_syswm.h`. SDL3 has no `SDL_syswm.h`, so the second bug class cannot recur.

**Some Windows-only code becomes portable and therefore testable.** `rememberWindowPlacement`, `restoreWindowPosition` and `isWindowMaximized` (`engine.cpp:2090`, `:2134`, `:2163`) are wholly inside `#ifdef _WIN32` today. `SDL_GetWindowPosition` / `SDL_SetWindowPosition` / `SDL_MaximizeWindow` / `SDL_GetWindowFlags` are cross-platform, so a region nothing here can exercise becomes a region `smoke.sh` can.

**SDL 1.2 has had no upstream since 2013.**

### What it costs

Seven of ten surveyed areas came back **hard** after adversarial review, and that review found silent-failure defects *in areas that had already been surveyed*. The honest cost is roughly **39–55 focused engineer-days**, of which about a third is spent before SDL3 is linked into anything, plus **repeated** access to a Windows machine with MSVC v145 — not one borrowed hour. (The figure agrees with the table under §Estimate; the first edition's §1 said 35–45 and its table said 36–52, and the table was the honest half.)

**The browser payload grows by about 65%, and that is the largest player-visible cost in the document.** Measured twice, independently:

- With the project's own link flags (`-O2 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=50331648`), SDL 1.2 against SDL3-static on the same trivial program: wasm 8,124 → 734,083 B, JS 54,376 → 186,726 B. **+725,959 wasm, +132,350 JS.**
- On a second trivial program with the same flags: wasm 17,984 → 699,233 B (the port) and 699,239 B (a direct build of the vendored tree). **+681 KiB**, corroborating the first.

The shipped `WebBuild/build/blocks5-540157f099f2.wasm` is 1,107,991 B, so this is **about +65% on the payload a phone downloads once per build** — a payload the service worker `addAll`s all-or-nothing on install and `.htaccess` then caches `immutable` for a year. Against that sits one measured saving in the same area: **startup heap allocation drops from 65,656 to 16,024 bytes**, so the 48 MiB `INITIAL_MEMORY` is not endangered. Those two numbers belong in the same paragraph, because they are the two halves of the same decision: SDL3 makes the browser build *cheaper to run* and *dearer to fetch*, and on a phone-first web build the fetch is the half the player feels. An owner who is not willing to pay 700 KiB should decide that here, at §1, and not after Stage 5 has put 30 MB into git history.

Two new maintenance obligations that the repo does not have today:

- **A cmake in `Tools/`, ~17.8 MB across 1,715 files.** It is **not** a single self-contained binary the way `Tools/7za.exe` is: `cmake.exe` needs `share/cmake-4.2/Modules`, `Templates` and `Licenses` beside it, so `Tools/` gains a **directory**, not a file. Measured on the 32-bit windows-i386 build of 4.2.0: `bin/cmake.exe` 11,435,088 B + `Modules` 5,532,854 B (1652 files) + `Templates` 784,444 B (61 files) + `Licenses` 7,816 B (1 file) = **17,760,202 B in 1,715 files**. That is not a 15× step up on the tracked-binary payload; it is a 15× step up in *bytes* and a ~1,700× step up in *tracked files*, and the second number is the one a `git status` and a code review feel.
- **A vendored SDL3 tree, 30,024,663 B across 1479 files**, permanent in git history from the commit that lands it. The working tree is revertible; the history is not. This plan gates that merge accordingly (§4, Stage 5).

`LinuxBuild/build.sh` gains cmake, a generator and **libxi-dev** as hard prerequisites where today it needs only `g++` and `libsdl1.2-dev`.

### The one-DLL position

**Exactly one DLL ships beside the executables: `OpenAL32.dll`.** That is not negotiable and the migration keeps it. Verified as far as this environment can: a 32-bit mingw executable linked against a static `libSDL3.a` imports `ADVAPI32, GDI32, IMM32, KERNEL32, OLEAUT32, SETUPAPI, SHELL32, USER32, VERSION, WINMM, msvcrt, ole32` — **and no SDL3.dll**. The installer needs no change either: `Blocks5/setup/Blocks 5.iss:52` is one `Source: "..\stage\*"` line and `Blocks5/stage.bat:10` copies one DLL. `Tools/cmake/` is a build tool and never reaches `stage/`. The MSVC half of that is unproven (§6).

### What is not oversold

- Not one MSVC compile happened anywhere in the survey. Every Windows build claim in this plan is either read from source or measured under mingw.
- The Windows live-resize path was read, never run. Two of its four hazards are **unanswered**, not defused — see Decision 1 and Stage 8.
- The browser has been proven to reach **parity of failure** with SDL 1.2 in a test program, not parity of success in the real game.
- `mobile.js`'s CDP touch has been proven only as far as a bare canvas receiving trusted `pointerdown`/`pointerup`.

---

## 2. The two decisions already made, and the traps each brings

### Decision 1 — main callbacks

The game moves to `SDL_AppInit` / `SDL_AppIterate` / `SDL_AppEvent` / `SDL_AppQuit` with the `*appstate` pointer. This plan honours that in substance and departs from it in one mechanical detail, **which needs the owner's sign-off**:

> **We do not define `SDL_MAIN_USE_CALLBACKS`.** `main.cpp` keeps its own `main()` with the `__try`/`__except` at `:614` intact, includes `<SDL3/SDL_main.h>` *after* `#include "pch.h"`, and calls `SDL_EnterAppMainCallbacks(argc, argv, AppInit, AppIterate, AppEvent, AppQuit)` from inside the `__try`.

This is not a dodge of the decision — all four callbacks and the appstate pointer are exactly as specified. It is a choice of entry mechanism, and it is available because **`SDL_EnterAppMainCallbacks` and `SDL_RunApp` are declared unconditionally** (`SDL_main.h:581`, `:608`, both *after* the `#endif /* SDL_MAIN_USE_CALLBACKS */` at `:484`).

**What was exercised, and what was not.** The survey ran **a variant** of this shape end to end: `exp/web/cb2.cpp` used `SDL_MAIN_HANDLED` + its own `main` + `SDL_EnterAppMainCallbacks` through the full MAIN-ENTER / APPINIT / FIRST-ITERATE / TERMINATING lifecycle in Chromium. That is not the spelling above, and the difference is not cosmetic: `SDL_main.h:262` renames `main`→`SDL_main` whenever `SDL_MAIN_NEEDED`, `SDL_MAIN_AVAILABLE` **or** `SDL_MAIN_USE_CALLBACKS` is defined, and `:681` includes `SDL_main_impl.h` only when `SDL_MAIN_HANDLED` is *not* defined. `SDL_MAIN_AVAILABLE` is set for `SDL_PLATFORM_WIN32` at `:158`. So on Windows our spelling takes **both** branches that `cb2.cpp` skipped — our `main` becomes `SDL_main`, and SDL supplies the real `main`/`WinMain` that calls `SDL_RunApp(argc, argv, SDL_main, NULL)`. On Linux and Emscripten neither macro is set, so nothing is renamed and no implementation is included, and the same source has to work unchanged on all four targets. **Confirmed claim [13] exists precisely because these two macros interact badly**, so the shape is not assumed: Stage 0 now proves it on three of the four builds *here* (§3, E-ENTRY-LOCAL) and E-MSVC-7 settles MSVC.

It buys two of the six named costs, **conditionally on E-MSVC-7**:

- **The SEH handler keeps working.** SEH is *dynamic* scope, and natively the generic callbacks loop is synchronous — `exp/app/cb` ran for 2000 ms and returned. So the `__except` filter covers every `SDL_AppIterate` for the life of the run, and no gameplay crash stops being reported.
- **The `/Yu` trap has no way in.** Nothing has to be written above `#include "pch.h"`, so MSVC's rule that `/Yu` discards preceding text cannot bite, and SDL testing the macro with `#ifdef` rather than `#if` (so that even `#define SDL_MAIN_USE_CALLBACKS 0` selects callbacks and dies with "redefinition of SDL_main") has nothing to test.

**Both can come back, and the plan must not read as though they cannot.** If E-MSVC-7 fails — the Release Windows-subsystem link, or the `__except` filter under `SDL_RunApp`'s own frame — the recorded alternative is to define `SDL_MAIN_USE_CALLBACKS` as a **build-system preprocessor definition** in all four builds, never as a `#define` in a source file, with an `#ifndef … #error` backstop placed *after* the pch include and a `verify.py` rule forbidding the macro in any `.cpp`/`.h`. That alternative brings cost 9 straight back: the `__try` at `main.cpp:614` then covers nothing that runs during play, and `SetUnhandledExceptionFilter(expFilter)` becomes the **primary** crash reporter rather than a second one — which changes what §6 item 5's crash test is proving, and makes it mandatory rather than confirmatory. Say that here rather than only in §6.

The other four costs are answered in the stages and are **not** made cheaper by this choice, because `SDL_EnterAppMainCallbacks` sets `SDL_HasMainCallbacks()` just as the macro does:

| Cost | Answer | Stage |
|---|---|---|
| Seven `GS_*` locals of `runTheGame()` (`main.cpp:586-592`) with `Engine::registerGameState` holding raw pointers | Move onto a heap `AppState` **on SDL 1.2**, one stage before callbacks exist | 4 |
| The measured accumulator bug — the hint alone gives 101 iterations and **15** logic ticks where 100 were wanted | Move the accumulator to `dt = end - lastFrameEnd` **on SDL 1.2**, with a tick-rate assertion armed in Stage 1 | 1, 4 |
| `Engine::flushInput` hollow for the key grab, real for `transfer.cpp:495` | Split into two named functions with two jobs, **on SDL 1.2** | 4 |
| `SDL_IterateMainCallbacks(false)` during the Win32 live resize vs. our own `SDL_PumpEvents` at `engine.cpp:3216` / `:3382` | One `reentryDepth` counter; **`hookWindowProc` is not deleted** | 4, 8 |
| Crash reporting under the macro form (cost 9) | **Not answered unconditionally.** Answered by the entry mechanism *if* E-MSVC-7 passes; answered by `SetUnhandledExceptionFilter` as primary if it does not | 0, 4, 8, §6 |

**The recorded retreat:** a classic `main()` with `emscripten_set_main_loop_arg` has been **proven** to work against SDL3 in the browser — 45 frames, 7 events, `driver=emscripten`, WebGL 1.0, `simulate_infinite_loop` correctly not returning. Callbacks are a choice, not a requirement, and the only SDL3 coupling to them anywhere is `SDL_GL_SetSwapInterval` (`src/video/emscripten/SDL_emscriptenopengles.c:56-63`), which this game never calls. That is why the callback adoption is **Stage 8**, taken from a working SDL3 game, revertible in one commit.

### Decision 2 — a shipped cmake (≥ 4.2) driving a CMake build of a static SDL3

Accepted. **4.2 is required, not preferred:** Kitware added the "Visual Studio 18 2026" generator only in 4.2.0 (verified against `Help/release/4.2.rst`; the 4.1.0 notes have no such line), and CLAUDE.md says this repo builds with v145. A 3.28 would silently cap the project at VS 2022.

**Ship the 32-bit windows-i386 build, and ship it as a directory.** Measured on 4.2.0:

| | windows-i386 (ship this) | windows-x86_64 |
|---|---|---|
| `bin/cmake.exe` | 11,435,088 B | 12,899,920 B |
| `share/cmake-4.2/Modules` | 5,532,854 B (1652 files) | same |
| `share/cmake-4.2/Templates` | 784,444 B (61 files) | same |
| `share/cmake-4.2/Licenses` | 7,816 B (1 file) | same |
| **minimal tree** | **17,760,202 B, 1,715 files** | **19,225,034 B** |

The 64-bit build is **bigger**, and 32-bit runs on every Windows this game targets, so i386 is the right choice on size alone. `Help/`, `doc/`, `man/`, `share/vim`, `share/emacs`, `share/aclocal`, `share/bash-completion`, `cmake-gui.exe`, `cpack.exe` and `ctest.exe` are all droppable. **`bin/cmcldeps.exe` (1,577,032 B) is dropped too**, and that is a trap with a condition on it: cmake needs it for `/showIncludes` dependency scanning under the **Ninja** generator with MSVC, and not under the Visual Studio generator, which is what we use. If anyone ever forces `-G Ninja` on Windows, `cmcldeps.exe` has to come back. E-MSVC-1 confirms it is not wanted on the path we actually take.

**Two separate facts, not one.** The byte counts above are from a *downloaded and measured* `cmake-4.2.0-windows-i386.zip`. The "the trimmed tree configures SDL3" claim is from a *different* tree: `cmake-4.2.0-linux-x86_64`, assembled down to the same four components and used to configure SDL3 under Linux. **No Windows cmake has run anywhere.** Whether the trimmed *Windows* layout configures SDL3 under MSVC is E-MSVC-1's job and is **unproven**.

| Trap | Answer | Stage |
|---|---|---|
| `Build.bat /rebuild` has no concept of a cmake build directory | Put the build tree at **`Blocks5\obj\sdl3\`** — outside the vendored source, gitignored like `data.zip` — so `/rebuild` is one `RD /S /Q` on a directory `Build.bat` owns by name; add it to `:doclean`'s `rmdir` list (`Build.bat:388-427`) so `/clean` reports it. Add `/nosdl` mirroring `/nodata`. | 5 |
| **`-A Win32` is mandatory** | CMake's own documentation for the Visual Studio generators says "The default target platform name (architecture) is that of the host", so on any 64-bit Windows the generator defaults to **x64** whichever cmake binary is used — a 32-bit `cmake.exe` does *not* imply a 32-bit target. Omitting `-A Win32` silently produces a 64-bit `SDL3-static.lib` against a solution that has only `Debug\|Win32` and `Release\|Win32`. It fails loudly (LNK1112), so it costs an hour rather than a week, but it is the single flag that decides the architecture and it must be in the invocation. | 5 |
| Both configurations, so the cmake step runs twice | **Configure once into a multi-config tree, build twice.** SDL3's CMake is multi-config clean — the survey built both from one configure with Ninja Multi-Config (Release 5,439,012 B, Debug 17,467,194 B, per-config subdirectories, no `DEBUG_POSTFIX`). The Visual Studio generator is natively multi-config. State the disk cost in `Build.bat /?` rather than let it be discovered. **No configure time is quoted here**: every configure timing in the survey (17.98 s, 18.0 s, 20.9 s, 22.5 s) is Linux or mingw-cross with Ninja, and an MSVC + Visual Studio-generator configure runs a different set of compiler probes. §6 item 4 asks the owner for the real number. | 5 |
| `/MT` via CMP0091 | Pass `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>`. A `CMAKE_PROJECT_INCLUDE` probe confirmed `CMP0091=NEW` inside SDL3's *own* top-level scope and the variable arriving unmangled, under both CMake 3.28.3 and 4.2.0; SDL3 offers no runtime option of its own (`grep` for `/MT`, `/MD`, `MSVC_RUNTIME_LIBRARY` over its whole CMake returns empty). **UNPROVEN under real MSVC.** Acceptance test: `dumpbin /directives` must show `LIBCMT`/`LIBCMTD` and never `MSVCRT`. | 0, 5 |
| The library is not called `SDL3.lib` | Under MSVC the static library is **`SDL3-static.lib`**: `CMakeLists.txt:3710-3716` sets `sdl_static_libname` to `SDL3-static` when the static prefix is empty and the suffix is `.lib`, applied at `:4011` via `OUTPUT_NAME`, and the in-source comment says it exists to avoid a clash with the DLL import library. So `Blocks5.vcxproj` needs `SDL3-static.lib` in `AdditionalDependencies` at `:78` and `:106`, the per-configuration output directory under `Blocks5\obj\sdl3\` added to `AdditionalLibraryDirectories` at `:79` and `:107` (today that is `libs\bin` and nothing else), and `libs\SDL-1.2.15\include` replaced by `libs\SDL3-3.4.2\include` in `AdditionalIncludeDirectories` at `:67` and `:94`. The exact per-configuration path the Visual Studio generator writes is E-MSVC-1's to record. LNK1104 is loud, so this is cheap — but it is a confirmed fact and it belongs in the document that carries them. | 5 |
| Vendoring location | **`Blocks5/libs/SDL3-3.4.2/` and nowhere else.** Forced, not aesthetic: `verify.py`'s `source_files()` walks `Blocks5/src`, `WebBuild`, `PWEncrypt`, `ShowUserDir` and skips any path with a `libs` component (`:57`); `prose_files()` does the same. Anywhere else puts **1147 `.c`/`.h` files, 15 of them non-ASCII**, in front of `encoding`, and every SDL build script in front of `comments`. `check_project_files()` only lists `Blocks5/src`, so it neither checks new entries nor breaks when the 67 leave. Verified: zero basename collisions between `Blocks5/src` and any SDL3 directory. | 5 |
| The next Visual Studio | **Omit `-G` and let cmake auto-detect**, plus a new `/sdl3gen:"<name>"` flag mirroring the existing `/toolset:`. Forward `/toolset:vNNN` as `-T vNNN`. Pinning a generator reintroduces exactly the hardcoded-version problem `Build.bat:33-36` exists to prevent. The recorded cost is a binary bump per VS major *when Kitware ships support*, made as a choice rather than as a standing obligation; the recorded escape hatch is the fourth mechanism (§8). | 5 |
| **`Build.bat` and cmake can pick different toolsets** | `Build.bat` passes no `/p:PlatformToolset`, so MSBuild uses `$(DefaultPlatformToolset)` of whichever MSBuild it found; with `-G` omitted, cmake independently auto-detects *its* newest Visual Studio. On a machine carrying VS 2022 and VS 2026 those can differ, and the `-T vNNN` forwarding only fires when `/toolset:` is given, which is the unusual case. **Derive the generator from the same `vswhere` result `Build.bat` already resolves at `:190` for its banner, and print both the MSBuild toolset and the cmake generator in that banner.** Then a disagreement is visible in every build log instead of showing up as an inexplicable link error. | 5 |
| The link line | Add **`gdi32.lib, imm32.lib, oleaut32.lib, uuid.lib, version.lib, setupapi.lib`** explicitly at `Blocks5.vcxproj:78` and `:106`. The per-library drop test proved all eight load-bearing (gdi32 68 undefined refs, imm32 45, ole32 22, setupapi 8, uuid 6, winmm 4, version 3, oleaut32 1); today's line carries only `winmm` and `ole32`. Do **not** rely on the inherited `%(AdditionalDependencies)` default — nobody in this environment can inspect it. Listing a lib twice is harmless; omitting one is an LNK2019 on a machine we cannot reproduce. `DECLSPEC=` at `:68`/`:95` goes with SDL 1.2; `dxguid.lib` waits for one confirming MSVC link, because mingw's `libdxguid.a` and MSVC's `dxguid.lib` partition symbols differently. | 5 |
| **Building from the Visual Studio IDE stops working** | CLAUDE.md documents the path: *"open `Blocks5.sln` in Visual Studio and build all three projects."* After Stage 5 there is no fourth project, no project reference and no build ordering, so an IDE build fails to link until someone has run `Build.bat` at least once. **Decide it rather than discover it.** Either (a) document the IDE path as "run `Build.bat /nodata` once first", which is a real regression in convenience, or (b) take the fourth mechanism (§8) — SDL's own `.vcxproj` as a fourth project — which *keeps* the IDE path working and needs no cmake at all. This is an argument in the choice between the two mechanisms, not only a documentation fix, and it is the strongest argument the fourth mechanism has. Recommended: (a), with the sentence written into `Build.bat /?`, CLAUDE.md and `README.md` in Stage 9 — but the owner should see that (b) exists and buys this back. | 5, 9 |

---

## 3. What must be proven before anything is written

Eight experiments. Three run here; five need the owner's Windows machine. **None of them touches `/home/user/blocks-5`.** Results go into a new `SDL3-MIGRATION.md` at the repo root — a decision log, not a duplicate of CLAUDE.md — each with the exact command that produced it.

The one repo artefact of this stage is `Tools/msvc_probe/`: the Windows probes committed as a directory with a README the owner runs in one sitting, so they can be re-run after a change rather than reconstructed.

### E-GL-WEB — the cheapest kill switch, and it goes first *(here, ~1 day)*

A ~200-line Emscripten program linked against a scratchpad SDL3 that exercises the game's **actual** GL usage, not a triangle: `SDL_CreateWindow` + `SDL_GL_CreateContext`, then

```c
EM_ASM({ Browser.useWebGL = true;                                  /* FIRST */
         Browser.moduleContextCreatedCallbacks.forEach(function(f){ f(); }); });
```

then display lists, `glBegin`/`glEnd` quads, a texture-matrix scroll, an FBO with a packed depth-stencil built through `SDL_GL_GetProcAddress`, `glScissor`, and a stencil pass. Run in Chromium/swiftshader and read the frame back with `glReadPixels`.

**Why it is first:** the survey proved the two-statement fix only to *parity of failure* — SDL1 and SDL3 both then hit the same `numVertices must be an integer` assertion in a test program. Everything in the browser plan is downstream of this and nothing has been spent yet.

**Stop if:** the frame does not come back correct. Then no vendoring happens, 30 MB stays out of git history, and the web target is reconsidered separately from the desktop one.

### E-ENTRY-LOCAL — the entry-point shape, on the three builds that can run it *(here, ~half a day)*

**New in this edition, and it replaces a piece of evidence the first edition cited wrongly.** Decision 1's exact spelling — own `main`, `#include <SDL3/SDL_main.h>` *after* the pch include, `SDL_EnterAppMainCallbacks` inside it, **no** `SDL_MAIN_HANDLED` — is not what `exp/web/cb2.cpp` ran. Three of the four builds can run the real spelling here, today, for free:

1. **Linux** (`g++`, native SDL3): compile, link, run. `SDL_MAIN_NEEDED`/`SDL_MAIN_AVAILABLE` are unset, so `SDL_main.h` renames nothing and includes no implementation — confirmed claim [31] measured exactly that with `-dM -E`. What this proves is that the include is *harmless* where it does nothing, which is the property the shared source needs.
2. **Emscripten** (`em++`): same, in Chromium, through the APPINIT / FIRST-ITERATE / TERMINATING lifecycle.
3. **mingw** (`i686-w64-mingw32-g++`), and this is the one that matters: `SDL_PLATFORM_WIN32` sets `SDL_MAIN_AVAILABLE` at `SDL_main.h:158`, so on this target — and only on this target — `:263` renames our `main` to `SDL_main` and `:683` pulls in `SDL_main_impl.h`, which supplies the real `main`/`WinMain` calling `SDL_RunApp(argc, argv, SDL_main, NULL)`. **Compile it, link it as both a console and a `-mwindows` GUI executable, and run both under wine** (wine-9.0 is on this box) against the 32-bit `libSDL3.a` the survey already built. A "redefinition of `SDL_main`", a missing `WinMain`, or a program that starts and does nothing shows up here for nothing.

**What it does not prove, said plainly:** wine is not Windows, mingw is not MSVC, GCC's `__try` is not MSVC's SEH, and `/Yu` does not exist here. The Windows-subsystem link, the precompiled-header interaction and the `__except` filter remain **E-MSVC-7's**, and E-MSVC-7 stays mandatory. What this buys is that a shape which fails on all three cheap targets is never carried to the owner's machine at all.

### E-MSVC-1…5 — the Windows facts *(owner, ~2 hours, gates Stage 5)*

1. Configure and build SDL3 3.4.2 static with the candidate vendored 32-bit cmake 4.2.x and real MSVC, **both configurations from one configure**, through a multi-config generator, with `-G` omitted and **`-A Win32` given**. Record the wall time (nobody has an MSVC configure number) and the exact per-configuration path `SDL3-static.lib` lands in. Confirm `cmcldeps.exe` was not wanted.
2. `dumpbin /directives SDL3-static.lib | findstr /i msvcrt` → must print **LIBCMT / LIBCMTD**, never MSVCRT. This is the only available proof of the `/MT` mechanism.
3. Link `blocks5.exe` against it with the corrected link line; `dumpbin /dependents` must list **only** Windows system DLLs plus `OpenAL32.dll`, and no `MSVCR*`/`VCRUNTIME*`.
4. One link with `dxguid.lib` removed.
5. Confirm MSBuild does not hit the `pch.obj` collision the critic found. SDL's own `VisualC/SDL/SDL.vcxproj` compiles `src/core/windows/pch.c` as its `/Yc` unit; `Blocks5.vcxproj:718` compiles `src/pch.cpp` as its own. The cmake route puts them in separate projects with separate `$(IntDir)` and should avoid it entirely — worth confirming rather than assuming, because it is also the reason option (a) is dead.

### E-MSVC-6 — the `/Yu` rule *(owner, minutes)*

One TU with `#define SDL_MAIN_USE_CALLBACKS` written **above** `#include "pch.h"` and an `#ifdef` test below it. The critic rated this *medium confidence, untested*. Our chosen entry mechanism makes it moot, but it is a fact worth having written down, and it is the fact the fallback in Decision 1 turns on.

### E-MSVC-7 — the entry point *(owner, ~30 min)*

Compile and link a stub with `main.cpp`'s shape: own `main()` inside a `__try`, `#include <SDL3/SDL_main.h>` after the pch include, `SDL_EnterAppMainCallbacks` inside the `__try`, no `SDL_MAIN_HANDLED`. **Both** the Debug Console-subsystem and Release Windows-subsystem configurations must link and run. Then crash it deliberately and confirm the `__except` filter fires *from inside an `SDL_AppIterate`*, not merely from `main`. This is the one thing standing between the plan's callbacks choice and a rewrite, and E-ENTRY-LOCAL will already have removed the cheap ways for it to fail.

### E-WIN-RESIZE — the live-resize path *(owner, ~1 hour, 60 lines)*

A program under `SDL_EnterAppMainCallbacks` with a re-entrancy depth counter that pumps and paints from inside `SDL_AppIterate`, **with a window procedure chained in front of SDL's exactly as `Engine::hookWindowProc` does it**, because that is the configuration the game will actually be in. Drag the border; hold still; open a window menu. Log, and report each separately:

1. Does the `WM_TIMER` path re-enter `SDL_AppIterate`, and at what depth?
2. Does our own `SDL_PumpEvents` inside `DefWindowProc`'s modal loop misbehave?
3. **How many key-ups arrive from the `SDL_ResetKeyboard()` at `SDL_windowsevents.c:1879`, with keys held down when the drag starts?** Our `WM_ENTERSIZEMOVE` case at `engine.cpp:2190-2196` ends in `break`, which chains to `CallWindowProc(p_sdlWindowProc, …)`, so SDL's own case runs regardless of what we do — keeping the hook does **not** prevent this.
4. **What does `WM_ENTERMENULOOP` do?** It reaches `SDL_windowsevents.c:1855-1856` — the same case — untouched by our hook, so a window menu now starts SDL's 10 ms timer and resets the keyboard as well.
5. Do SDL's 10 ms timer and our 15 ms timer coexist on one HWND without either being clobbered? `SetTimer` is keyed by id, so this is a question about the two ids, not a certainty either way.

**This decides whether `hookWindowProc` can ever be deleted — and until it says yes, it stays.** Items 3 and 4 are **unanswered behaviour changes** that keeping the hook does not address; see Stage 8.

### E-KEY-TABLE — the frozen vocabulary *(here, ~half a day)*

Generate the candidate scancode → id table and diff the ids it mints against `SDL_GetKeyFromScancode` under the **dummy** driver and under **X11**. The frozen table must be identical in both; `SDL_GetKeyFromScancode` measurably is not (12 scancodes mint different ids on the same machine). That property — layout independence — is what SDL 1.2's `MapVirtualKeyEx(…, hLayoutUS)` gave and what the table exists to preserve.

### Golden baseline *(owner, ~15 min)*

Build today's tree with `Build.bat` on the v145 box. Archive `Release/blocks5.exe`, its `dumpbin /dependents` listing, and **the `config.xml` a fresh first start writes**. That config file is what Stage 3's golden-binding assertion is written against, and it must come from real SDL 1.2 — this Linux box runs **sdl12-compat** (`sdl-config --version` → 1.2.68), so a baseline recorded here would record sdl12-compat's mapping instead.

---

## 4. The stages

Ten stages. **Eight ship on their own. One is atomic and says so. One writes no game code.** Stages 1–4 land on `master` while the tree is still SDL 1.2 — that is what lets `verify.py`, `syntax.sh`, `smoke.sh` and `smoke.js` prove that nothing changed, which they cannot do in the same commit as a library swap.

---

### Stage 0 — Prove first *(no game code)*

**Goal.** Answer the eight things in §3 before any mechanism is chosen, and record what "correct" looks like while SDL 1.2 is still producing it.

**Work.** `Tools/msvc_probe/` (six probes plus a README) and `SDL3-MIGRATION.md` (the decision log, including the licence fact, the browser payload numbers and the fourth-mechanism retreat). E-GL-WEB, E-ENTRY-LOCAL and E-KEY-TABLE run in the scratchpad and leave nothing in the tree.

**Proven by.** Eight recorded answers, each with its command. An archived `Release/blocks5.exe`, its dependents listing and a first-start `config.xml`.

**Ships?** Nothing to ship — one markdown file and one `Tools/` subdirectory, neither on any build path.

**Revert.** Delete two paths.

---

### Stage 1 — Arm the checks and record the golden baseline *(still SDL 1.2)*

**Goal.** Make the checks and the three harnesses capable of catching the migration's *known* silent failures **before any SDL3 code exists**. This is the critic's central warning executed: a check written after the migration proves nothing about what the migration changed.

#### How every new rule is specified, and why the first edition's were not

The first edition invented four checks and did not ask of them what it asks of every stage. Measured against the tree, three were **green by construction** — they could not have gone red on any fault anyone would commit — and one was **red on the SDL 1.2 tree for a reason that had nothing to do with SDL**. That is abort criterion 12 turned inward, and it would have failed the plan.

So every rule below is written out in four parts, and a rule that cannot fill all four is not written at all:

> **What it forbids · what fault makes it fire · is that fault reachable · is the rule green on today's tree.**

Two mechanisms carry the weight that greps cannot, and both are cheap because Stage 2 and Stage 3 are cutting those seams anyway:

- **`Ticks` is a strong type** (Stage 2), so assigning a tick value to a `uint` is a compile error on all four compilers rather than a grep's opinion.
- **`KeySlot` is a strong type** (Stage 3), so passing a keycode where a slot index is wanted is a compile error rather than a silently wrong index space.

Where a strong type does the work, the grep beside it is described as a **backstop against the one thing the type cannot see — an explicit cast** — and is not credited with more.

**Three of the rules below are red on the SDL 1.2 tree**, at sites Stage 3 or Stage 6 rewrites. That is not a reason to weaken them. Each is armed with a **named, self-retiring exemption**: the exemption lists the exact file and line, gives the reason, and **the check fails if the exempted site no longer matches** — so it cannot outlive the code it excuses, and the stage that deletes the code must delete the exemption in the same commit. That is the difference between an exemption and a hole.

#### `Tools/verify.py` — 17 checks become 19 here, 21 at Stage 2, 23 at Stage 3

**`sdl_traps`** — a family of grep-shaped rules, one per proven silent failure a grep can actually see. Two rules are armed here; two more join at Stage 3 (below).

*Rule T1 — `SDL_PIXELFORMAT_RGBA8888` must not appear in `Blocks5/src`, `WebBuild`, `PWEncrypt` or `ShowUserDir`.*
**Fires on:** anyone writing that constant where `SDL_PIXELFORMAT_RGBA32` is meant.
**Reachable:** yes, and likely — it is the name a migration guide reaches for, the masks at `img_load.cpp:51` and `texture.cpp:66`/`:173` are `RGBA32` = `ABGR8888` on little-endian, and getting it wrong swaps red and blue in every texture in the game while compiling and running perfectly.
**Green today:** yes — the token appears nowhere.
**Selftest:** inject `SDL_PIXELFORMAT_RGBA8888` into `texture.cpp`, expect a finding, restore byte-for-byte.

*Rule T2 — `SDL_SetWindowFullscreen` may appear **only** inside a `#ifndef __EMSCRIPTEN__` region.*
This is the **inverse** of what the first edition wrote, and the inversion is the whole point. The first edition forbade the call "in `WebBuild/` or inside an `__EMSCRIPTEN__` branch", which is not the shape the bug takes: the realistic fault is somebody porting `applyWindowStyle` and writing an **unguarded** `SDL_SetWindowFullscreen` in `engine.cpp`, on a path the browser reaches. That is neither in `WebBuild/` nor inside an `__EMSCRIPTEN__` branch, so the old rule passes and the on-screen pad vanishes — the exact regression `mobile.js` cannot see, because the pad still reports a full-size `getBoundingClientRect` while invisible. An allow-only rule has no such gap: everything is forbidden except the one region where the call is correct.
**Fires on:** any `SDL_SetWindowFullscreen` not inside a `#ifndef __EMSCRIPTEN__` (or `#if !defined(__EMSCRIPTEN__)`) region.
**Reachable:** yes — Stage 6 and Stage 7 both touch the fullscreen path, and `SDL_emscriptenvideo.c:748` promotes the **canvas**, which hides the pad.
**Green today:** vacuously — the symbol is an SDL3 name and appears nowhere. That is acceptable *because the selftest proves the rule fires*, which is the standard the first edition failed to apply to itself.
**Cost the plan must own:** "inside a `#ifndef __EMSCRIPTEN__` region" is **not a grep**. It needs a small preprocessor-region scanner — roughly 40 lines tracking `#if`/`#ifdef`/`#ifndef`/`#elif`/`#else`/`#endif` nesting per file and the condition text of each open level. That scanner is shared with `foreign_ifdef` below, which needs the same walk, so it is written once.
**Selftest:** inject an unguarded call into `engine.cpp`, expect a finding; inject one inside a correct `#ifndef __EMSCRIPTEN__` block, expect none.

*Two rules the first edition had and this one deletes, with the reason:*

- **"`SDL_GetKeyboardState` must not be assigned to a non-`const bool*`"** is deleted as theatre. Under SDL3 it is a **hard compile error** on every compiler — the survey ran it: `g++ -c` on `Uint8* p = SDL_GetKeyboardState(0);` gives *"cannot convert 'const bool*' to 'Uint8*'"*. A grep that duplicates a loud failure buys nothing and costs an exemption, because the assignment is **correct today at four sites** (`gs_menu.cpp:150`, `engine.cpp:3218`, `engine.cpp:3416`, and the Emscripten arm of the first). Guarding a compile error is not a check; it is decoration.
- **"no direct `keyData[` / `keyHeld[` / `buttonData[` indexing outside `engine.cpp`"** is deleted and **replaced by the `KeySlot` type plus rule T3** (Stage 3). Measured: there are 39 such subscript sites in `engine.cpp`, 3 in `engine.h`, and **zero anywhere else** — every writer and reader is already inside the file the rule exempted, so the rule guarded nothing on the day it was written and would guard nothing after Stage 3. Worse, the only two other hits in the tree are `gs_menu.cpp:316-317`, where `keyData` is an unrelated local `std::unordered_map` in the demo *loader* — so as specified the rule is a **false positive on the SDL 1.2 tree**, which under abort criterion 11 is a stop, or in practice an exemption added on day one. The writer that actually needs guarding is `Engine::setKeyData`'s callers, and the old rule did not name them.

**`foreign_ifdef`** — any `#if` / `#ifdef` / `#ifndef` / `#if defined` in our own sources on a macro that is neither defined in our own sources nor on a short allowlist of toolchain macros. This is landmine 1 *generalised* rather than patched, and it is the check that would have caught `SDL_VIDEO_DRIVER_X11` before anyone knew to look. **Two things about it are load-bearing and the first edition had both wrong.**

*(a) It needs a fifth base, and it must not be `source_files()`.* `verify.py`'s `source_files()` walks `Blocks5/src`, `WebBuild`, `PWEncrypt`, `ShowUserDir` (`:53`), and **`LinuxBuild` is not one of them** — `prose_files()` exists precisely because of that gap and its own docstring says so. The landmine lives at `LinuxBuild/linux_window.cpp:9, 21, 62`. A `foreign_ifdef` built on `source_files()` — the natural choice, and what every other structural check uses — returns **all clear on a tree with the landmine fully armed**. Adding `LinuxBuild` to the shared helper would change the surface of `encoding`, `style`, `ctor_init`, `naming` and `comments` all at once, which is its own decision and not one to take by side effect. So **`foreign_ifdef` carries its own base list**: `Blocks5/src`, `WebBuild`, `PWEncrypt`, `ShowUserDir`, `LinuxBuild`. Written down as a list in the check, with a comment saying why it is not `source_files()`.

*(b) The "defined in our tree" search must be pinned to that same list, or Stage 5 disarms the check.* The rule is "a macro that is neither defined anywhere in our tree nor on the allowlist", and *"grep the tree for a `#define`"* is the obvious implementation and the wrong one. After Stage 5 vendors SDL3, `SDL_VIDEO_DRIVER_X11` **is** defined in the tree, three times over:

- `Blocks5/libs/SDL3-3.4.2/include/build_config/SDL_build_config.h.cmake:427` — `#cmakedefine SDL_VIDEO_DRIVER_X11 1`;
- `Blocks5/libs/SDL3-3.4.2/include/build_config/SDL_build_config_macos.h:191-195` — the `_DYNAMIC` family;
- and, once `LinuxBuild/build.sh` has run, `Blocks5/obj/sdl3/include-config-release/build_config/SDL_build_config.h:427` — a real `#define SDL_VIDEO_DRIVER_X11 1`. Verified against a generated tree in the scratchpad.

So a whole-tree definition search goes quiet **at exactly the stage that makes the landmine live**. The search reads the same five bases and nothing else — never `Blocks5/libs`, never `Blocks5/obj`, never a generated build_config.

*(c) A commented-out `#define` counts as a definition site*, and this one is not pedantry either. This tree's idiom for a switch you turn on by hand is a commented define: `engine.cpp:842` `// #define RECORD`, `engine.cpp:1572` `// #define PROFILE_ENGINE_UPDATE`, and five more of the `PROFILE_*` family. If the search ignores them, `foreign_ifdef` reports **thirteen findings on day one** and its allowlist gets stuffed with them — which is how an allowlist stops meaning anything. Accept `//`-commented defines; reject nothing else.

**Fires on:** an `#ifdef` on a macro that comes from outside our sources — a library's private config, a build flag nobody sets, a typo in a macro name.
**Reachable:** it is *live*. **Green today: no — and this was measured, not assumed.** The rule as written above was implemented and run over the five bases. It reports **18 sites across 10 macros**, not the three the earlier edition of this plan claimed without running it. That claim was the plan committing its own abort criterion 12, and the measurement is the correction:

| macro | sites | what it is | disposition |
|---|---|---|---|
| `SDL_VIDEO_DRIVER_X11` | 3 — `LinuxBuild/linux_window.cpp:9, 21, 62` | **the landmine** | fixed in Stage 2 |
| `BLOCKS5_TEST_HOOKS` | 4 — `engine.cpp:1580`, `testhooks.cpp:11`, `testhooks.h:23`, + 1 | defined by `-D` on the build line, never in a source | **allowlist**: build-system macros |
| `STRESS_TEST`, `EDGY_CONNECTIONS`, `CHECK_IF_IT_REALLY_IS_A_LEVEL` | 1 each — `engine.cpp:1650`, `pin.cpp:124`, `gs_leveleditor.cpp:763` | hand-thrown switches, deliberately never defined | **allowlist**, named individually with that reason |
| `APSTUDIO_INVOKED`, `APSTUDIO_READONLY_SYMBOLS` | 1 each — `resource1.h:9, 10` | written by the VS resource editor | **allowlist**: generated file |
| `CURRENT_THREAD_VIA_EXCEPTION` | 1 — `stackwalker.h:177` | vendored third-party | `stackwalker.*` is already `VENDORED` in `verify.py` — exclude it |
| `GL_ES`, `GL_FRAGMENT_PRECISION_HIGH` | 5 — `u_crt.cpp:93, 94`, `u_sharpfit.cpp:39, 40`, `upscaler.cpp:13` | **not C++ at all** — GLSL inside C++ string literals | see below |

**The last row is a design requirement, not an exemption.** `"#ifdef GL_ES\n"` is a *shader* preprocessor directive living inside a C++ string, and a line-based scanner cannot tell it from the real thing. The fix is that the check must skip string literals — allowlisting these five would hide the next real one written in a shader-adjacent file. That is roughly twenty more lines in the scanner, shared with rule T2's `#ifndef __EMSCRIPTEN__` region test, and it is the single most important thing to get right in this check.

So Stage 1's honest claim is *"19 checks; `foreign_ifdef` red at the three `SDL_VIDEO_DRIVER_X11` sites, with the other 15 dispositioned above"* — not "all clear", and not "three findings".
**Allowlist:** `_WIN32`, `__EMSCRIPTEN__`, `__linux__`, `_DEBUG`, `NDEBUG`, `_MSC_VER`, `_M_IX86`, `_M_X64`, `_M_IA64`, `_WIN32_WINNT`, `__cplusplus`, `BLOCKS5_TEST_HOOKS`. Twelve entries, each with a reason beside it, and adding a thirteenth is a decision somebody has to defend in a diff.
**Selftest:** inject `#ifdef SDL_VIDEO_DRIVER_COCOA` into `engine.cpp`, expect a finding; **and a second case that is the real one** — add a `#define SDL_VIDEO_DRIVER_COCOA 1` under `Blocks5/libs/` and confirm the finding **still fires**, which is the only way to prove (b) was implemented rather than intended.

**`tick_width`** — *armed in Stage 2, not here*, because it is red on today's tree: `font.cpp:189` and twelve other sites assign `SDL_GetTicks()` to a `uint`, which is correct under SDL 1.2. It is written here, with its selftest case, and registered in the check list by Stage 2 as part of that stage's proof.

Its **claim is also reduced**, because the first edition credited it with a job it structurally cannot do. A grep cannot see `if(now >= grabDeadline)` where `grabDeadline` is a `uint` member declared in another file, and `sdl_contract.cpp`'s `static_assert(sizeof(Ticks) >= sizeof(decltype(SDL_GetTicks())))` says nothing about storage. **The strong `Ticks` type is the mechanism** (Stage 2); `tick_width` is the backstop against the one thing the type cannot see:

*Rule — no narrowing cast of a tick value.* `static_cast<uint>` / `static_cast<Uint32>` / `static_cast<unsigned>` / `(uint)` / `(Uint32)` applied to `getTicks()`, to a `Ticks`-typed expression, or to any identifier ending in `Ticks`.
**Fires on:** somebody silencing a `Ticks`-to-`uint` compile error with a cast instead of widening the storage.
**Reachable:** yes, and it is the *likeliest* failure of the whole Stage 2 mechanism, because a cast is what a hurried fix looks like and the compiler then says nothing at all.
**Green after Stage 2:** yes, by construction of that stage's work.

**`sdl_funnel`** — *also armed in Stage 2*, for the same reason: `SDL_GetTicks` has 13 call sites today, so the rule is red until Stage 2 routes them. `SDL_Init`, `SDL_GetTicks`, `SDL_SemWaitTimeout` / `SDL_WaitSemaphoreTimeout` appear nowhere outside `sdlwrap.cpp`.
**Fires on:** a new direct call site — and there will be one, because Stage 7 touches `videorecorder.cpp` and Stage 8 touches the loop.
**Reachable:** yes.
**Why a funnel and not a grep for the polarity itself:** a rule saying "`if(SDL_Init(` must be negated" *cannot be green on the SDL 1.2 tree it is armed against*, because under SDL 1.2 the un-negated form is correct. The two polarity inversions get a funnel, not a rule; `sdl_funnel` is what keeps the funnel intact.

*`Tools/verify.py` — one repair.* `check_version`'s regex is `p_localVersion\s*=\s*"([\d.]+)"`, which silently fails to match `1.3.0-beta`; the remaining seven places then agree, `len(values)==1`, and the check returns clean while no longer covering `main.cpp` at all. Either fix the regex or forbid a non-numeric suffix. (This plan forbids the suffix — Stage 9.)

#### `LinuxBuild/test/harness.sh`

*Fix the vacuous repeat tests.* Xvfb's measured autorepeat is **delay 660 ms / interval 40 ms** and `b5_hold` is 400 ms, so `smoke.sh:50-64` and `:195-202` pass today without ever generating a repeat — and will keep passing after `SDL_EnableKeyRepeat(140,60)` is gone. Add a ten-line Xlib helper calling `XkbSetAutoRepeatRate(dpy, XkbUseCoreKbd, 100, 40)` from `b5_start` (verified: the setting sticks, SDL3 does not reset it, and the same 400 ms hold then produces 9 key-downs) and assert a repeat actually occurred. Do **not** use the shell-only alternative — five `xdotool keydown` 60 ms apart produce **one** SDL key-down, because X coalesces a repeated fake press of an already-down key.

*A third `b5_stale` input.* `harness.sh:46-49` already refuses to run a binary older than `Blocks5/src` and a `data.zip` older than `Blocks5/data`, which is the same rule `WebBuild/build.sh`'s exit code enforces and for the same reason. After the migration the harness has a **third** input it does not know about: the SDL3 archive and the flag string it was configured with. Change a `-DSDL_X11_*`, rebuild the game, run the harness, and it happily certifies a binary linked against the previous library. Add `b5_stale "$B5_EXE" "<the SDL3 archive>"` and a comparison against the stamp file Stage 5 writes. Write it **now**, inert, so that Stage 5 has nothing to remember.

*Two more.* Export `XDG_RUNTIME_DIR` (or set `SDL_VIDEO_DRIVER=x11`) ahead of the flip: SDL3 dlopens `libwayland-client`, which prints an unsolicited lowercase `error: XDG_RUNTIME_DIR is invalid or not set` on every headless run. `smoke.sh:275`'s case-sensitive `grep -q "ERROR"` misses it today but SDL3's own log handler prefixes `ERROR: `, so that grep's meaning changes and the log must be clean first. And **keep `b5_click`'s move/settle/press/hold/release shape**, rewriting its comment with the measured reason: under SDL3 on Xvfb, `xdotool click 1` delivers **two** button-down/up pairs (6 for 3 clicks, three separate runs) where the harness's own shape delivers exactly one at every hold from 0 to 250 ms. SDL2 did not double.

#### `LinuxBuild/test/smoke.sh` — six new assertions, all green on SDL 1.2

1. **`-nofbo` pins the window to 640×480**, and a maximize request does not change it. `-nofbo` occurs in the whole test tree exactly once, as a *comment* at `harness.sh:99`, so `LinuxWindow::setFixedSize` and `engine.cpp:2417` are exercised by nothing anywhere.
2. **Fullscreen geometry with `B5_WM_PID` mandatory.** The assertion at `:213-236` exists but is inside `if [ -n "$B5_WM_PID" ]`; make its absence a failure, not a skip.
3. **`<Window fullscreen="1">` round-trips through `config.xml`** after Alt+Return. This is the one `<Window>` attribute testable here — `positionX`, `positionY` and `maximized` are all inside `#ifdef _WIN32`.
4. **The fourteen default binding ids, asserted as literal strings.** Not by round-trip: this box runs sdl12-compat, so a round trip records *its* mapping rather than real SDL 1.2's. `secondary="key:KP4"` and its three siblings must appear verbatim.
5. **Logic-tick rate.** Extend `Blocks5/src/testhooks.cpp`'s dump with `ticks` and `frames` counters and assert ticks/elapsed ≈ 50 Hz over a 10-second run. This is the assertion that later catches the accumulator bug — 15 ticks where 100 were wanted — and it must be passing *before* anything changes.
6. **Text input actually reaches an edit box.** Open the level editor's title field through `clickPath`, type a known string with `b5_key`, and assert the field's contents through the hook. Nothing in this tree has ever typed into an editor, and `SDL_StartTextInput` is the single easiest way to ship a build whose editors look fine and accept nothing.

*The video recorder.* `videorecorder.cpp` is compiled by exactly one of the four checks and exercised by **none** (`smoke.sh` presses F11 at `:249` and never F12). Record ~2 s with F12 and **assert the frame count against the elapsed wall time**. This has to be specific: an inverted semaphore wait produces ~2.2× the frames, each duplicate clamped to a full frame's duration; a check that only asks whether an MP4 was written passes on the broken build.

*Say what the suite now costs.* Assertion 1 is a **second full harness run** with `B5_ARGS=-nofbo` — `B5_ARGS` exists and `harness.sh:97-99` names `-nofbo` in its comment, so it is feasible, but `b5_start` tears down and rebuilds Xvfb and the wait loop allows 60 s for the window plus 60 s for the hook, because under llvmpipe the start takes half a minute. Two of those, plus the fullscreen `sleep 3`s, plus the new text-input, tick-rate (10 s) and F12 (2 s) steps, is a materially longer run than CLAUDE.md's *"they take about half a minute together"* describes. **Measure it in Stage 1 and write the new number into `LinuxBuild/README.md` and CLAUDE.md's "Checking a change" section**; the estimate to beat is three to four minutes for `smoke.sh` alone, and if it lands much past that, split the `-nofbo` run into a second script the owner can run separately rather than let the main one rot from slowness.

#### The screenshot comparison, specified

The first edition leaned on "a screenshot diff against the recorded baseline" three times, plus abort criterion 7, and **as stated it is impossible.** Two independent reasons:

- **The menu animates and is not reproducible.** `smoke.sh` presses F11 at `:250`, after the walk has returned to `GS_Menu`, and the main menu runs the title demo — bombs, lasers, particles. The particle velocities come from the global `MTRand mt` at `util.cpp:11`, and `MersenneTwister.h:262` seeds a default-constructed `MTRand` from `/dev/urandom` or `hash(time(NULL), clock())`. **No two runs produce the same frame**, whatever the tick alignment.
- **A scale change cannot appear in the file at all.** Screenshots read `GL_COLOR_ATTACHMENT0` at 640×480 and never see the window size, which is CLAUDE.md's own rule. A "screenshot diff" could never have caught a scale change.

So the one diff is replaced by four assertions, each naming the fault it catches:

**S1 — the red/blue swap, animation-invariant.** Take per-channel means over the whole 640×480 frame for the new shot `S` and the Stage 1 baseline `B`, and assert `|R̄ₛ−R̄ᵦ| + |B̄ₛ−B̄ᵦ| < |R̄ₛ−B̄ᵦ| + |B̄ₛ−R̄ᵦ|` — the new shot must be closer to the baseline than to a red/blue-swapped copy of it. The menu's colour balance is dominated by sky, grass and stone and is stable across frames; the swap moves it by tens of levels. **Additionally** assert each channel mean within a tolerance of the baseline's — and **measure that tolerance in Stage 1** by taking ten menu shots on the SDL 1.2 build and recording the observed spread, rather than guessing a number. If the spread is wide enough that the second half is useless, keep only the first half, which needs no tolerance at all.

**S2 — one frame that really is comparable.** Take a second screenshot from a state that runs no logic: the **level editor** with its default level open (`gs_leveleditor.cpp:1242` loads `level_default.xml`, and the editor never calls `Level::update()`, so no object moves and no particle is spawned). Stage 1 already opens the editor for the text-input assertion, so this costs one keypress. Compare **pixel-exact**. And prove it is stable first: take it three times on the SDL 1.2 build in Stage 1, and **if the three are not byte-identical, delete this assertion rather than loosen it** — a tolerance invented to make a flaky check pass is the thing abort criterion 11 forbids.

**S3 — scale.** Not a screenshot question. Assert `computePresentRect`'s output through the test hook against the window size, plus the `-nofbo` 640×480 pin.

**S4 — cursor offset.** Not a screenshot question either. Click a known window coordinate and assert **which element** was hit, through the same hook `clickPath` uses. `smoke.js` gets the browser twin (Stage 7); the native side goes through `$B5_TEST_DIR/request`.

Abort criterion 7 is rewritten against S1–S4 (§8).

#### The rest of Stage 1

*`WebBuild/test/smoke.js`* — assert a frame actually rendered (a counter in `test_hooks.cpp`'s dump), not merely that the module initialised. *`WebBuild/test/mobile.js`* — replace the on-screen-pad `getBoundingClientRect` measurement with `document.elementFromPoint` on the pad's centre while fullscreen. The pad reports a full-size rect **while invisible**, which is exactly why the current test cannot see the canvas-promotion regression at all.

*`Tools/syntax.sh`* — parameterise the SDL include set behind one variable (default SDL 1.2), and record in the header that `-DDECLSPEC=` becomes inert under SDL3, which spells it `SDL_DECLSPEC` and defaults it to nothing for a consumer.

*Three stale prose sites* that name no SDL symbol and would survive a rename sweep untouched: `harness.sh:152` (quotes the game's `Initializing SDL …` log line as the wedged-X-server signature), `smoke.sh:211`, `smoke.sh:267` (explains `xdotool windowclose` in terms of SDL 1.2 internals).

*Golden artefacts, recorded to the scratchpad:* the fourteen config ids, the menu screenshot **and its measured ten-shot channel spread**, the editor screenshot **and its three-run stability result**, a 2 s MP4's frame count, `verify.py`'s output including the three `foreign_ifdef` findings, `syntax.sh`'s 120-file pass, and the measured `smoke.sh` wall time.

**Proven by.** `python3 Tools/verify.py` — **19 checks; `foreign_ifdef` red at exactly the three `SDL_VIDEO_DRIVER_X11` sites (`LinuxBuild/linux_window.cpp:9, 21, 62`) once the allowlist and the string-literal skip in the table above are in place, and no others.** `python3 Tools/selftest.py` — **25 injected faults, 25 fired**, every file restored **byte-for-byte and mtime-for-mtime**. `sh Tools/syntax.sh` — 120 files, no output. `LinuxBuild/build.sh hooks` + `smoke.sh` green with all new assertions. `WebBuild/build.sh hooks` (exit via `${PIPESTATUS[0]}`) + `smoke.js` + `mobile.js` green.

Then the part that makes any of it mean something: **break each guarded thing and watch the assertion fail.** Stub `LinuxWindow::setFullScreen` to `return false` → `smoke.sh` must fail. Remove the `-nofbo` clamp → the pin assertion must fail. Overwrite a default binding id → the golden assertion must fail. Swap the channels in `img_save.cpp` → S1 must fail. Restore byte-for-byte. A smoke assertion proven only *green* is a grep that may never have matched — which is the very defect this stage is fixing.

**Ships?** Yes. Nothing outside `Tools/` and the two test directories changes; the game is byte-identical.

**Revert.** One commit.

**Risk.** Low, and the realistic risk is the opposite of failure: a new assertion comes back red on SDL 1.2. `foreign_ifdef` **will**, by design. Anything else that does is a bug found for free and must be fixed or knowingly waived **here**, not blamed on SDL3 later.

---

### Stage 2 — Defuse the three landmines and the version-neutral hazards *(still SDL 1.2)*

**Goal.** Turn every construct the survey proved will fail *silently* after a mechanical header rename into something that is either impossible or loud — using changes that are correct under the library the tree ships today.

**Work.**

**Landmine 1 — `SDL_VIDEO_DRIVER_X11`.** `LinuxBuild/linux_window.cpp` wraps its whole implementation in `#ifdef SDL_VIDEO_DRIVER_X11` at lines **9, 21 and 62**. That macro comes from SDL 1.2's *installed* config (`/usr/include/SDL/SDL_config.h:183`); SDL3 does not publish it to consumers, proven with an `#ifdef`/`#error` probe. After a header rename both functions compile to `return false` with **no error, no warning, no link failure**, killing Linux fullscreen (`engine.cpp:2491`) and the `-nofbo` 640×480 pin (`engine.cpp:2417`). Replace the macro with `B5_HAVE_X11`, which `LinuxBuild/build.sh` defines after probing for `X11/Xlib.h`, and put `#else` / `#error "the build must say whether X11 is available"` on the outermost one so an undecided build fails at compile time. Add a startup log line naming which implementation is live, so `smoke.sh` can see it. **This is what turns `foreign_ifdef`'s three Stage 1 findings green**, and `verify.py --only foreign_ifdef` clean is this stage's proof for it. Note for Stage 6: `info.info.x11.lock_func` / `unlock_func` (`linux_window.cpp:30, 52, 66, 84`) have **no SDL3 successor** and go with the rewrite.

**Landmine 2 — the tick width, and `Ticks` is a strong type.** New `Blocks5/src/sdlwrap.h` + `sdlwrap.cpp`. The first edition wrote `typedef Uint64 Ticks;`, which is a documented intention and nothing more: a `typedef` assigns to a `uint` in silence on all four compilers, and the widening class then depends on a grep that cannot see storage. Make it a type the compiler enforces:

```cpp
enum class Ticks : Uint64 { };
inline Uint64 ms(Ticks t)                  { return static_cast<Uint64>(t); }
inline Ticks  operator+(Ticks a, Uint64 d) { return Ticks(ms(a) + d); }
inline Uint64 operator-(Ticks a, Ticks b)  { return ms(a) - ms(b); }  // a difference is a duration
const  Ticks  TICKS_NEVER = Ticks(~0ULL);
Ticks getTicks();
```

A scoped enumeration compares against its own type for free, so `<`, `<=`, `>`, `>=`, `==`, `!=` need no overloads and only `+`, `-` and (for the CRT flicker) an explicit `ms()` at the one modulo do. **Assigning a `Ticks` to a `uint` is then a hard error on gcc, mingw, emcc and MSVC alike** — the whole widening class becomes build breakage in Stage 2, before SDL3 is anywhere near the tree, instead of a check that has to keep being right. `tick_width` (Stage 1, armed here) is demoted to its one honest job: catching the explicit cast somebody reaches for to silence the error.

**The cost, stated rather than discovered:** every `printf`-family format that prints a tick value has to change — `%u` becomes `%llu` with an explicit `ms()` — and each of the thirteen call sites and seven storage sites needs looking at once rather than being swept. That is the point; a sweep is what leaves `font.cpp` behind. Budget half a day for it inside this stage's estimate.

Alongside, and for the same reason the survey gave, **typedef the three thread primitives here too**: `SDL_sem`, `SDL_Thread` and `SDL_mutex` are all renamed or poisoned in SDL3 (`SDL_oldnames.h:1099-1115`), so a funnel that wraps the *calls* and leaves the *types* buys less than it claims. Nine member declarations move at the flip otherwise — `audiocapture.cpp:56, 282, 283, 765, 766`, `videorecorder.cpp:104, 105`, `streamedsound.h:57, 66`. Give them `B5Semaphore`, `B5Thread`, `B5Mutex` in `sdlwrap.h` now, one line each under SDL 1.2.

Then widen every storage and comparison site — **thirteen call sites and seven storage sites across eight files**, and it is one decision about `uint` (`typedefs.h:4`), not a sweep:

`cf_rewind.h:35 startTicks` (written `cf_rewind.cpp:88`, read `:294`; measured, **934 of 1750 frames** give a different REWIND blink state at 2³² ms of uptime) · `resource.h:56 timestamp` · `sound.h:45 lastInstanceCreatedAt` · `font.h:76 lastTimeUsed` · `engine.h:461 grabDeadline` · `engine.h:513 frameTime` · `engine.cpp:934/1327/1344` · `manager.h:35 newestTimestamp`.

**And `font.cpp:208`, which the first edition got wrong on the one item its own register calls a crash.** It wrote *"`uint minTime = ~0;` — write `~0ULL`"*, and `uint minTime = ~0ULL;` is **still a `uint` and still `0xFFFFFFFF`**: the initialiser is truncated straight back and nothing changes. The survey's wording was *"widen `minTime` with it (**or** write `~0ULL`)"*, where the parenthetical only makes sense as *widen the declaration and initialise with `~0ULL`*. **The declaration is what moves:**

```cpp
Ticks minTime = TICKS_NEVER;                                    // font.cpp:208
std::unordered_map<std::string, StringCacheEntry>::iterator oldestEntry = stringCache.begin();
```

Two edits, not one. The first is the fix: `font.h:76 lastTimeUsed` becomes `Ticks`, and with `TICKS_NEVER` at 64 bits every entry satisfies the comparison at `:212` again. The second is the belt: `oldestEntry` is **default-constructed and never assigned** and then dereferenced unconditionally at `:220-221` (`listIndex = oldestEntry->second.listIndex; stringCache.erase(oldestEntry);`). Widen the member and leave the sentinel narrow and, past 49.7 days of uptime, no entry satisfies `lastTimeUsed < 4294967295`, the loop assigns nothing, and the erase runs on garbage. **A crash, not a degradation** — and the crash class survives the sentinel fix by one refactor, because the loop still assigns nothing if the map is ever empty. Initialising the iterator costs nothing and closes it. (The survey cites this as `font.cpp:206-218`; the tree has drifted two lines and the declaration is at `:208`. The same trap sits one file away in `manager.h:35 newestTimestamp = 0;`, which is only a wrong-newest rather than a crash because of the `!p_newestResource ||` short-circuit at `:39`, and which the first edition listed correctly.)

**Say plainly what this is and is not.** It is *preparation*, not a bug-fix release. The measurement is on record: at `now = 2³²+1000` and at 60 days, the SDL3 form expires immediately (1) and the **SDL 1.2 form does not** (0). Only in a narrow window either side of the wrap do *both* forms misbehave. Under SDL 1.2 the widening is behaviour-preserving everywhere the survey measured, and that is exactly what makes it safe to land now.

`util.cpp:396-424`'s `getExactTime()` and `util.h:74`'s `uint getExactTimeMS()` carry the same 49.7-day question and feed `engine.cpp:1261/1644/2661`. Widen them here. **Do not** replace them with `SDL_GetTicksNS()` yet: the Linux branch returns absolute `CLOCK_MONOTONIC` seconds while the Windows branch is relative to first call, so any caller treating the value as absolute would change behaviour. That check comes first, in Stage 7 or never.

**Landmine 3 — GLU on Windows.** `Blocks5/src/pch.h:21-23` guards `#include <GL/glu.h>` with `#ifndef _WIN32`. SDL 1.2's `SDL_opengl.h:46-47` includes `<GL/glu.h>`; SDL3's does not. So on Windows the game's GLU declarations arrive *solely* through SDL, and after the rename **13 call sites in six files** stop compiling — `cf_camera.cpp` (2), `cf_cube.cpp` (2), `cf_slices.cpp` (2), `cf_zoom.cpp` (2), `gs_credits.cpp` (2), `engine.cpp` (3 × `gluOrtho2D`). **Delete the `#ifndef _WIN32` / `#endif` guard entirely so the include is unconditional.** Linux and Emscripten already take it (and `WebBuild/gl_compat.cpp:8` includes it explicitly besides), and `glu32.lib` is already on the link line at `Blocks5.vcxproj:78` and `:106`. Reproduced with exactly `Tools/syntax.sh`'s compiler and flags.

**New `Blocks5/src/sdl_contract.cpp`** — compile-time enforcement that no grep can match. **It goes in `Blocks5/src` and in `Blocks5.vcxproj` and `.filters`, not in `Tools/`**, and that placement is the whole point. The first edition put it under `Tools/`, where `verify.py`'s `project_files` check does not require it in the project and **MSVC never compiles it** — so its `glu*` calls and its `SDL_GL_GetProcAddress` shape would have been checked by mingw and never by the toolchain that ships, while Stage 5's "proven by" cited it as the proof. In `Blocks5/src` it is enforced by `project_files` and seen by all four compilers. Contents:

- `static_assert(sizeof(Ticks) >= sizeof(decltype(SDL_GetTicks())))` and `static_assert(!std::is_convertible<Ticks, unsigned int>::value)` — the second is the one that proves the strong type is still strong;
- `#if !defined(B5_HAVE_X11) && !defined(B5_NO_X11) #error` on the Linux path;
- three `gluPerspective` / `gluLookAt` / `gluOrtho2D` calls, so GLU resolution is proved by every compiler in the loop;
- the exact `void* p = SDL_GL_GetProcAddress(…)` shape from `glextensions.cpp:50` and `engine.cpp:597`, which becomes a compile error under SDL3's `SDL_FunctionPointer` return type.

**And it must actually link, not merely compile.** A never-called function is a candidate for `/OPT:REF`, so the proof that `glu32.lib` is on the line would evaporate at exactly the moment it mattered. Put the calls in one exported `void sdlContractProbe()` and call it once from `runTheGame` behind `if(getenv("B5_CONTRACT_PROBE"))` — a condition no compiler can fold away, no player ever satisfies, and the linker must therefore resolve.

**Five `SDL_PushEvent` sites push a partly uninitialised `SDL_Event`** — `engine.cpp:1033`, `gs_game.cpp:228`, `gs_leveleditor.cpp:918`, `gs_menu.cpp:191`, `:513`. Add `SDL_zero(event)` before setting `.type`. Harmless under SDL 1.2; SDL3 reads `common.timestamp`, runs the filter and two watch lists, and consults `ShouldDispatchImmediately` against stack garbage.

**`videorecorder.cpp:430-436`** — extend the guard to `if(!p_impl->p_semaphore || !p_impl->p_thread)`. Today a NULL semaphore made `SDL_SemWaitTimeout` return −1 and the `continue` treated it as "go round again"; **SDL3 returns `true` for a wait on a NULL semaphore** (measured), so the corrected wait would fall straight through and run H.264 encode with no pacing at all. `audiocapture.cpp:596-602` already checks both; make the recorder match.

**Name the two semaphore call sites, because one of them is a hard error nobody listed.** There are **two** `SDL_SemWaitTimeout` calls in the tree, not three: `videorecorder.cpp:199` and **`streamedsound.cpp:339`**. (The survey's grep count of 3 included the comment at `streamedsound.cpp:332`; the count of 2 for `SDL_MUTEX_TIMEDOUT` is that comment at `:333` plus the code at `:339`.) The second site is the interesting one: it reads `if(SDL_SemWaitTimeout(p_stopSignal, 10) != SDL_MUTEX_TIMEDOUT) break;`, and **`SDL_MUTEX_TIMEDOUT` exists nowhere in SDL3's headers, not even in `SDL_oldnames.h`** — compiled both ways, with and without `-DSDL_ENABLE_OLD_NAMES`, it is `'SDL_MUTEX_TIMEDOUT' undeclared` each time. Loud, therefore cheap; but it is the site the funnel has to carry and the comment at `:332-333` states the *old* polarity in words, so both move together in Stage 6. Route both calls through `semWaitTimeout()` here and rewrite that comment to say what the funnel guarantees rather than what SDL 1.2 returned.

**Proven by.** All checks and all three harnesses, green and *identical* to Stage 1 — **except that `foreign_ifdef` now reports nothing**, which is this stage's headline. `verify.py` — **21 checks** (`tick_width` and `sdl_funnel` registered here), all clear. `selftest.py` — **25 cases, 25 fired.** `sh Tools/syntax.sh` is the specific proof of the GLU change and of `sdl_contract.cpp`. A scratchpad harness that offsets the tick base past 2³² and asserts the REWIND blink state matches the 32-bit reference for all 1750 frames of the effect. Then the negative proof: revert each of the three landmine fixes in the scratchpad and confirm the corresponding check or compile fails.

**Ships?** Yes.

**Revert.** Six small independent commits; any one reverts alone.

**Risk.** Low–moderate. The strong `Ticks` type will produce a burst of compile errors on first introduction — that is it working — and the widening will produce `-Wconversion`-class findings that none of the four checks emits today (none of them passes even `-Wall`). Do a one-off `-Wall -Wextra` sweep before and after and compare counts, exactly as CLAUDE.md already prescribes for `syntax.sh`.

---

### Stage 3 — Cut the seams and freeze the binding vocabulary *(still SDL 1.2)*

**Goal.** Move the many-site changes to few-site changes, and settle the survey's *"single largest open design question in the whole exercise"* — while SDL 1.2 is still present to translate from. This stage changes no behaviour and must prove it.

**Work — the seams.**

| Seam | Sites today | After |
|---|---|---|
| Native window handle | 14 `SDL_GetWMInfo` + 14 `SDL_VERSION(&info.version)` pairs (`engine.cpp` 11, `transfer.cpp` 1, `linux_window.cpp` 2) | `Engine::getWin32Hwnd()`, `getX11Display()`, `getX11Window()` — three functions. `transfer.cpp:475` needs the HWND for `GetOpenFileNameA`; that is a hard requirement. In SDL3 they become `SDL_GetPointerProperty(SDL_GetWindowProperties(w), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL)` and friends. Record separately that `SDL_VERSION` is an **object-like** macro in SDL3 expanding to `SDL_VERSIONNUM(3,4,2)`, so `SDL_VERSION(&info.version)` becomes "called object is not a function" — loud, but 14 of them. |
| Pixel format | 18 `->format->` dereferences on **10 lines** (`texture.cpp:81, :188, :230-235`; `img_load.cpp`; `engine.cpp:494-497`) plus 2 in `WebBuild/platform_stubs.cpp:47` | `surfaceBytesPerPixel(s)`, `surfaceChannelShift(s, ch)`. **Do not touch `engine.cpp:582`** — it dereferences `p_display->format->` where `p_display` is `SDL_SetVideoMode`'s return; SDL3 has no display surface, so that line is a rewrite owned by Stage 6, not a swap. (Note also that SDL3 dropped `Rloss`…`Aloss` entirely.) |
| Cursor visibility | 7 sites (`engine.cpp:558, :1275`; `gs_credits.cpp:282, :288`; `gs_menu.cpp:322`; `gs_game.cpp:545, :649`) | `Engine::showCursor(bool)` + `Engine::isCursorVisible()` backed by a member. `engine.cpp:1275`'s `if(SDL_ShowCursor(-1))` is SDL 1.2's *query* form and has no same-named SDL3 successor. |
| GL proc address | `glextensions.cpp:48-51`'s `getProc()` returns `void*`; `engine.cpp:597` assigns to `void*` | A function-pointer typedef. SDL3 returns `SDL_FunctionPointer`. The other 25 sites already `reinterpret_cast` to `PFNGL*` types and are fine — all eight families the game uses were verified to resolve from SDL3's `SDL_opengl_glext.h`, so `glextensions.h` needs nothing. This file decides whether the FBO and the whole upscaler chain exist, and no survey area owned it. |
| **Key slot** | every writer/reader of `keyData[]`, `keyHeld[]`, `buttonData[]` — 39 subscripts in `engine.cpp`, 3 in `engine.h`, plus **`Engine::setKeyData`'s two callers in `gs_menu.cpp`** | **`enum class KeySlot : int`**, minted only by `Engine::keySlotFor(const SDL_KeyboardEvent&)` and by the frozen table. Today `keySlotFor` returns `KeySlot(event.keysym.sym)` and nothing changes; in Stage 6 this one function changes index space. See below — the type is the mechanism, not the grep. |
| **Keyboard VK lookup** | `Engine::getKeyboardVK(SDLKey)` — **22 calls on 15 lines**: 21 in `main.cpp:531-569` and one at `engine.cpp:3468`, declared `engine.h:272` | The first edition missed this one entirely, and it is the *only* public VK entry point with callers — its "five fewer `SDLKey` signatures to retype" counted four dead functions and not this. It takes an `SDLKey`, a type SDL3 does not have. It becomes `int getKeyboardVK(KeySlot)` and the fourteen default bindings in `main.cpp` name their keys through the frozen table. |
| Modifiers | 18 `KMOD_*` tokens on 9 lines in five files (`engine.cpp:1027, :1043`; `gs_leveleditor.cpp:465-466`; `gui_editbox.cpp:145, :148`; `gui_element.cpp:197`; `gui_multilineeditbox.cpp:154, :157`) plus two `SDL_GetModState()` on two of those same lines | `Engine::keyModState()`. The unprefixed spellings are *poisoned* in SDL3 (`SDL_oldnames.h:1008`) so they fail loudly — but that is **twenty** edits, not the "32" the first edition wrote. 32 is the *keycode-constant* figure, used correctly again in Stage 6; counting it twice inflated this row and obscured that the edit-box half of the modifier work belonged to no survey area at all. |

Free deletions the survey found: `Engine::setKeyDown`, `setKeyPressed`, `setKeyReleased` (`engine.cpp:3033`, `:3041`, `:3049`) and `getVKs()` (`engine.h:269`, `engine.cpp:3104`) have **zero callers anywhere** — four fewer `SDLKey` signatures to retype. **`Engine::setKeyData` (`engine.cpp:3057`, declared `engine.h:248`) is a fifth of the same family and is *not* dead** — it has two live callers, and they are the next item.

**Work — `gs_menu.cpp`'s title-screen demo playback.** *(New in this edition. It is a `keyData` writer outside `engine.cpp`, on the path that runs at every single start, and the first edition used the demo as a **witness** for a different change — "the title-screen demo playing proves the `AppState` move" — while leaving its own port unwritten.)*

Three things move, and one of them is silent:

1. **`translateRecordedKey`'s table** (`gs_menu.cpp:34-37`) is keycode-valued — `{9, SDLK_TAB}, {273, SDLK_UP}, {274, SDLK_DOWN}, {275, SDLK_RIGHT}, {276, SDLK_LEFT}, {278, SDLK_HOME}, {304, SDLK_LSHIFT}, {306, SDLK_LCTRL}` — and its output is handed straight to `setKeyData` as a slot index at `:212`. Retarget the right-hand column to the frozen table's scancode slots. Loud once `setKeyData` takes a `KeySlot`.
2. **The fall-through at `:47` is `return recorded;`** — a raw SDL 1.2 number used directly as an index. **This is the silent half.** Replace it with *ignore*: return "no slot" and skip the write. The survey parsed the shipped `Blocks5/data/demo1.dat` to make that safe rather than asserting it — *"distinct recorded key numbers: [9, 273, 274, 275, 276, 278, 304, 306], records: 2096"*, which is **exactly** the eight numbers already in the table, so nothing in the shipped demo takes the fall-through at all. Ignoring it is provably lossless here, and a future recording made on a different build is exactly the case that must not write to an arbitrary slot.
3. **The clear loop at `:197-200`** — `for(int i = 0; i < SDLK_LAST; i++) engine.setKeyData(static_cast<SDLKey>(i), 0);` — becomes `Engine::clearAllKeyData()`. `SDLK_LAST` is the loud half (it does not exist in SDL3) and the comment above it, which explains that Emscripten counts to 1536, goes with it.

The two `p_keyStates[SDLK_c]` / `[SDLK_LSHIFT]` / `[SDLK_RSHIFT]` cheat tests at `:154-162` move here as well rather than waiting for Stage 6, because they are the other keycode-as-index sites in the same function and Stage 6 has enough to carry. Under SDL 1.2 they route through the same frozen-table lookup and behave identically; under SDL3, `SDLK_LSHIFT` is `0x400000e1` and indexing a 512-entry array with it is a **~1 GB out-of-bounds read**.

**Work — two more `sdl_traps` rules, armed here because this stage is what makes them green.**

*Rule T3 — `key_index_space`: no `SDLK_*` or `SDL_SCANCODE_*` constant may appear as an array subscript (`[SDLK_`, `[SDL_SCANCODE_`) or as a loop bound (`< SDLK_`, `<= SDLK_`, `< SDL_SCANCODE_`) outside `Blocks5/src/keytable.h` and `Engine::keySlotFor`.*
**Fires on:** a new site that indexes a key-state array by keycode — the fault that is "compiles and runs wrong", the expensive kind.
**Reachable:** yes, demonstrably — the tree has **six** subscript sites today (`gs_menu.cpp:154, 155, 156, 160, 161, 162`) plus **two** loop bounds, `gs_menu.cpp:198` and — the one the first edition missed — **`engine.cpp:313`, `for(int k = 0; k < SDLK_LAST; k++)`, the VK-table construction loop in `Engine::init`**. That site is in neither of T3's permitted homes and survives until Stage 6 switches the table to `keytable.h`, so **T3 is registered at Stage 6, not Stage 3**; registering it earlier makes Stage 3's own proof gate fail. Stage 6 is a stage full of opportunities to write a ninth.
**Green today: no** — it is red at exactly the sites this stage rewrites, and green the moment they are rewritten. That is the check being armed by the work rather than in spite of it.
**One self-retiring exemption:** `WebBuild/platform_stubs.cpp:95-237` (the range starts at the `static const char* keynames[SDLK_LAST];` declaration on `:95`, not at `:103` — 133 `[SDLK_` hits run `:95` to `:237`) is SDL 1.2's own key-name table, ~130 legitimate `keynames[SDLK_x]` subscripts, and it is **deleted whole in Stage 6**. The exemption names the file and the reason, and the check **fails if that file no longer exists** — so Stage 6 must delete the exemption in the same commit that deletes the file.
**Permanent:** unlike the rule it replaces, this one is not retired in Stage 9. It is the thing that stops a keycode being used as a slot index ever again.

*Rule T4 — `keytable.h` must contain no `LMETA` or `RMETA` row and must mint no `key:LMETA` / `key:RMETA` id.*
The first edition wrote this as *"`SDLK_LMETA`/`SDLK_RMETA` must not appear"*, tree-wide, which is **red at five sites today**: `engine.cpp:199-200` (the 136-row key-name table that `keytable.h` replaces) and `platform_stubs.cpp:224-225` plus a comment at `:227`. Scoping it to the one file the rule is about makes it green the moment that file exists and aims it at the fault that is actually likely.
**Fires on:** somebody transcribing the old 136 rows into `keytable.h` mechanically — which is the *default* thing to do, since `engine.cpp:199-200` has them side by side with `LSUPER`/`RSUPER`.
**Reachable:** yes, and invisible otherwise. They exist in SDL3 (`SDL_keycode.h:316-317`) as `0x2000xxxx` extended keycodes that are neither ASCII nor `SDLK_SCANCODE_MASK`-tagged, they mean a **different physical key**, and any `k & ~SDLK_SCANCODE_MASK` arithmetic mangles them. They compile. No compiler and no other check can see this.
**Widened in Stage 9** to the whole tree, once `engine.cpp`'s old table and `platform_stubs.cpp` are both gone.

**Work — the vocabulary. The decision, on record:**

> **`config.xml` binding ids are minted from a frozen scancode-keyed table of our own, checked into `Blocks5/src/keytable.h`, and never from `SDL_GetKeyFromScancode`.**

The reasons are measured, not aesthetic. SDL 1.2's Windows keysym is *deliberately* layout-independent — `SDL_dibevents.c:598` maps through `MapVirtualKeyEx(scancode & 0xFF, 1, hLayoutUS)`, explicitly the US layout. `SDL_GetKeyFromScancode` is keymap-driven: **12 scancodes mint a different id under the X11 backend than under the dummy driver on the same machine**, and on a German Windows the grave key would mint `key:CARET` where a US one mints `key:BACKQUOTE`. `SDL_GetDefaultKeyFromScancode`, which would fix this, is `static` and not public API. A frozen table therefore *preserves* what the id has always meant rather than making it depend on the player's layout.

Three rules go into the file's header comment and into a new `vk_ids` check:

1. **A scancode with no keycode mints `key:sc#<n>`, never `key:UNKNOWN`.** Under the naive design, 19 named scancodes (NonUSBackslash, International1-9, Language1-9) share the single id `key:UNKNOWN` because their default keycode is `SDLK_UNKNOWN` and that is row 0 of the table — and `getVKFromId` (`engine.cpp:3131-3141`) returns the first match. Today exactly one VK carries that id.
2. **`SDLK_LMETA` / `SDLK_RMETA` are struck from the table by hand**, enforced by rule T4.
3. **The legacy alias ids go in a separate, read-only lookup consulted only on read.** That makes the ordering fragility *structural* instead of accidental: today the only reason a fresh grab on a digit writes `key:1` rather than `key:EXCLAIM` is that `updateKeyGrab` (`engine.cpp:3474-3483`) returns the first VK in list order and the aliases happen to be appended last. (The alias *count* is not a design fact — 20 under the dummy driver, 18 under X11 — so the plan says "the legacy aliases", never a number.)

**Land the READ half now, on SDL 1.2**, in `getVKFromId` and `resolveActionKeys` (`engine.cpp:3530-3548`). **Four legacy vocabularies must resolve and none may be deleted:**

- the 136 current id spellings;
- the read-only aliases;
- the **`key:#N` escape hatch** — `keyboardVKId` (`engine.cpp:210-219`) writes `key:#%d` for any of the 187 unnamed SDL 1.2 keyboard slots, and that includes `SDLK_WORLD_0..95` (160-255), which is where SDL 1.2 on Windows puts the accented keys. A German player who bound an umlaut has **`key:#246`** in their file;
- **the bare integers at `engine.cpp:3879-3883`.** Deleting that branch is *destructive, not cleanup*: `main.cpp:325-410` still carries live migration branches for `not_played`, `<= 1.0.7`, `1.0.71`, `1.0.72`, `1.0.73`, and `readme.txt`'s changelog runs back to 1.1.0. A player upgrading 1.1.2 → 1.3.0 has bare integers, `QueryIntAttribute` is the only thing that reads them, and deleting the branch loses every binding on first start and writes the loss back as an empty string. **Translate it** — a one-time SDL-1.2-keycode → new-VK table run at the `main.cpp:314` version hook. (`git tag` is empty in this checkout, so it is not even established that 1.2.0 has shipped. If it has not, this population is *everyone* — an owner question, §6.)

**Keep all five `$VK_KEYBOARD_KP_*` special cases** at `engine.cpp:239-241` and in `languages.txt`. The survey **refuted** deleting them: `$VK_KEYBOARD_KP_PERIOD` is *"Num ."* in English and **"Num ,"** in German — a real translation, because a German keypad prints a comma — and the ten `$VK_KEYBOARD_KP0..KP9` rows that stay use the same house style, so deleting the five would put "Keypad +" beside "Num 0" in one list. Only the comment's stated reason (SDL 1.2's bracket names) goes stale.

**Joystick ids — decide now, apply in Stage 7.** `Joystick1 B3` is built from the enumeration ordinal and the raw button index. SDL 1.2 on Windows enumerates through winmm/DirectInput; SDL3 has **no winmm driver at all** and enumerates through RawInput/XInput/WGI plus 26 HIDAPI device drivers that deliberately remap a known controller into gamepad-standard order. The id resolves and means a different physical button, with no error anywhere. **Decision: on the first start of the SDL3 version, clear every joystick binding and raise a toast naming the actions that lost one.** That is not politeness — `repairLostBindings` (`engine.cpp:3553-3564`) returns at the first action that still has a key, so a player who bound half their actions to a pad gets half an unbound game with no other signal. The same clear also defuses the latched-axis hazard, where an XInput trigger resting at −32768 leaves a bound action permanently down (measured: unchanged after ten update cycles and after a half-press-and-release).

**Two more decisions, recorded here because both are now choices rather than accidents.**

- **`flushInput`'s mouse range.** The plan widens the keyboard range to 0x300..0x303 (below). The mouse half needs a decision the first edition never made: SDL 1.2's `SDL_EVENTMASK(SDL_MOUSEBUTTONDOWN)` **did** flush the wheel, because the wheel arrived as buttons 4 and 5 through `buttonData` (`gui.cpp:301-302`). `SDL_FlushEvents(SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_BUTTON_UP)` stops at 0x402 and leaves `SDL_EVENT_MOUSE_WHEEL` (0x403) **queued**, so "matching today's behaviour" is backwards. **Decision: extend the range to 0x403 and flush the wheel, matching today.** A wheel tick surviving a modal file dialog and arriving in the Manager's list afterwards is a small thing, but it is a behaviour change and it should be one somebody chose.
- **Key repeat is no longer the game's to set.** `SDL_EnableKeyRepeat(140, 60)` has no SDL3 successor; the OS rate takes over, feeding **seven** `GUI::isKeyRepeat()` consumers — `options.cpp:170`, `gs_game.cpp:124`, `gui_listbox.cpp:176`, `help.cpp:36`, `gui_editbox.cpp:180`, `gs_leveleditor.cpp:426`, `gs_campaigneditor.cpp:85`. Every menu list, every edit box and the level editor change repeat feel **for every player on every platform**, and there is no way to put it back. The first edition had this only in the §7 documentation list. It is a **player-visible behaviour change with a changelog line** (Stage 9) and, if the owner wants the old feel back, an implementation decision: time the repeats in `GUI` off the logic tick instead of trusting `event.key.repeat`, which is a real piece of work and belongs in Stage 7 or nowhere.

**New checks.** `vk_ids` (no two rows mint the same id; only scancode 0 may mint `key:UNKNOWN`; every id `main.cpp`'s fourteen default bindings use is covered) and `main_entry` (`<SDL3/SDL_main.h>` included by exactly one TU; `SDL_MAIN_USE_CALLBACKS` never written as a `#define` in a source file).
**`vk_ids` fires on:** two table rows colliding, which is the `key:UNKNOWN` failure and which `getVKFromId`'s first-match resolution turns into "your binding is on a different physical key". **Reachable:** it is the *default* outcome of the naive design — measured, 19 rows collide. **Green:** the moment `keytable.h` is written correctly.
**`main_entry` fires on:** a second TU including `SDL_main.h`, or the macro appearing in a source file — the `/Yu` trap's only remaining route in. **Reachable:** yes, if the E-MSVC-7 fallback is ever taken carelessly. **Green:** vacuously today, with a selftest case that proves it fires.
Checks 21 → **23**; selftest 25 → **29** (`vk_ids`, `main_entry`, T3, T4).

**Proven by.** The whole Stage 1 suite green and **identical** — same fourteen ids, same tick rate, S1 and S2 passing. Plus a **migration corpus** in the scratchpad, added to the smoke suite: synthetic `config.xml` files (current ids; alias ids; `key:#246`; bare integers; joystick bindings) each dropped into the user directory, the game started under the harness, and the resulting `config.xml` plus `log.txt` asserted — every binding either carried over or named in the log. Plus a **title-demo assertion**: start the game, wait 30 s in the menu, and assert through the hook that the demo moved a player — the shipped demo's first `$A_LEFT` arrives well inside that, and it is the only thing that would catch the `gs_menu` port going wrong. Plus `verify.py --only ctor_init` for the new `Engine` members.

**Ships?** Yes.

**Revert.** Seam by seam; the vocabulary half is three commits (read-side lookup, unreferenced header, dormant migration hook) and each reverts alone.

**Risk.** Low in code, moderate in judgement. The frozen table is a permanent contract — once shipped, ids cannot be re-spelled without a second migration — so it deserves review before it lands. The one thing it cannot promise is that a config copied between machines with different physical layouts lands on the same *printed* key; it lands on the same *position*, which is what SDL 1.2 also did. The `KeySlot` seam is the one to be careful about, and it is careful by construction now: miss a writer and the compiler says so, which is why the type replaced the grep.

---

### Stage 4 — Program lifetime and the loop, reshaped *(still SDL 1.2)*

**Goal.** Do the entire callbacks restructure while SDL 1.2 is still underneath, so that Stage 8 is a change of *who calls the functions*, not of what the functions are. Four of the six named callback costs are paid here, against a game `smoke.sh` can still drive.

**Work.**

**The seven `GS_*` locals** of `runTheGame()` (`main.cpp:586-592` — `menu`, `selectLevel`, `game`, `levelEditor`, `campaignEditor`, `credits`, `loading`) move into a heap-allocated `AppState` held by a `std::unique_ptr`. `Engine::registerGameState` (`engine.cpp:2831`) stores raw pointers into a map from the `GameState` base constructor (`gamestate.cpp:9`), so the map outlives the objects the moment `main` returns — **proven in the browser**: MAIN-RETURNING and LOCAL-DTOR both print before the first `SDL_AppIterate`.

**`Engine::mainLoop()` splits** into `Engine::appIterate()` plus the quit path, with every piece of cross-frame state moved onto `Engine`: `timeToProcess`, `timeProcessed`, `firstEventRecorded`, `lastFrameEnd`, and **`FILE* p_out`** (`engine.cpp:917`, the RECORD demo recorder's file, `fclose`'d at `:1364`). No survey area listed that last one and a RECORD build stops compiling without it. Keep the classic `for(;;)` calling `appIterate()` — the browser branch already has this shape.

**The accumulator moves to the wall-clock-between-iterations scheme** the Emscripten branch already uses at `engine.cpp:1336-1341`: `dt = end - lastFrameEnd`. Today's `dt = end - start` is measured *inside* the iteration (`engine.cpp:1332`) and the `SDL_Delay` sits inside that span, so the sleep is counted. Measured: with `SDL_HINT_MAIN_CALLBACK_RATE=50` the current form gives **101 iterations and 15 logic ticks** where 100 were wanted — the game at one seventh speed, with nothing in any diff to point at; the wall-clock form gives 101/99 with the hint and 637/99 without. Doing it here also removes an `#ifdef` and makes the native and browser branches the same code. **Stage 1's tick-rate assertion, passing since before anything changed, is what proves it.**

**A `reentryDepth` counter on `appIterate()`** — one mechanism answering three hazards at once. Entering while already inside skips `SDL_PumpEvents`, skips the logic ticks, skips the inactive-window `SDL_Delay(50)` at `engine.cpp:1144`, and does only the re-present. That reproduces `repaintDuringSizeMove`'s existing behaviour portably and costs nothing today. It covers: the re-entrant `PeekMessage`/`DispatchMessage` from inside a window procedure that is itself inside `DefWindowProc`'s modal loop; the new fact that logic ticks would run during a drag, which `repaintDuringSizeMove` never did; and a 50 ms sleep landing inside SDL's 10 ms `WM_TIMER`.

**`Engine::flushInput()` splits into two functions with two jobs.** `discardPendingInput()` keeps the `SDL_PumpEvents` + `SDL_PeepEvents` for `transfer.cpp:495`, where the events are in the OS queue after a modal file dialog and the pump is what pulls them in. For the key-grab caller at `engine.cpp:1596`, replace "swallow what has piled up" with an explicit **gate**: a flag on `Engine` that the event dispatch consults and that bins key and text events while a grab runs. Measured, the peep is a no-op for that caller under callbacks — `SDL_DispatchMainCallbackEvents` drains the whole queue into `SDL_AppEvent` before `SDL_AppIterate`, so `flushInput-swallowed=0` — whereas the gate acts in exactly the right place. (In Stage 6 the surviving peep's keyboard range widens to `SDL_EVENT_KEY_DOWN..SDL_EVENT_TEXT_INPUT`, 0x300..0x303 — SDL 1.2 carried the typed character *inside* the key event via `SDL_EnableUNICODE`, SDL3 does not, so 0x300..0x301 alone leaks typed characters past a grab into whatever edit box has focus — and its mouse range widens to 0x403 per Stage 3's decision.)

**Install `SetUnhandledExceptionFilter(expFilter)`** at the top of `runTheGame`, under the same `#if defined(_WIN32) && !defined(_DEBUG)` guard, **keeping the `__try` alongside it**. It is strictly a second reporter, not a replacement: it also covers the video encoder and the audio capture thread, which the `__try` never did. Verified that SDL3 does not compete — its only Windows exception-handler use is an `AddVectoredExceptionHandler` installed and removed inside `SDL_SYS_SetupThread` (`src/thread/windows/SDL_systhread.c:145`) purely to raise the 0x406D1388 thread-naming exception.

Because our entry mechanism keeps the `__try`, this filter is belt-and-braces rather than the primary — **unless E-MSVC-7 fails**, in which case the macro form takes over, the `__try` covers nothing that runs during play, and this filter becomes the only crash reporter the game has. **Verify it accordingly:** with both installed the inner `__try` wins, so a Release crash test here confirms the mechanism *being kept*, not the one being added. The filter's own proof is a Release build with the `__try` temporarily removed — an owner action, once (§6 item 5), and it is mandatory rather than confirmatory if E-MSVC-7 came back red.

**Proven by.** Stage 1's tick-rate assertion reads the same before and after — that is the specific proof of the accumulator change. `smoke.sh`'s full menu/options/manager walk proves the `AppState` move, and Stage 3's title-demo assertion rides along. `verify.py --only ctor_init` covers the new `Engine` members.

**Ships?** Yes, but this is the largest pre-flip diff and it touches `engine.cpp`'s loop region. Land it whole; do not interleave it with Stage 3.

**Revert.** One commit.

**Risk.** Moderate — the highest of the four pre-flip stages. The accumulator is the one to watch: it is a real change to the logic clock, exactly the shape that reads as "the game feels slightly off" rather than as a bug, and the only thing between it and a shipped regression is an assertion that was written first for that reason.

---

### Stage 5 — Vendor SDL3 and build it everywhere, consumed by nothing

**Goal.** Every build path produces a static SDL3, on all three platforms, while the game still links SDL 1.2. All the build-plumbing risk is paid where a failure cannot break the game.

**This is the point of no return for git history.** The 30 MB is permanent once merged. Develop this stage on the same branch as Stage 6 and **merge to `master` only after Stage 6's reconnaissance gate returns its three passes** (§Stage 6). Until then it costs nothing but the work.

**Work.**

**Vendor at `Blocks5/libs/SDL3-3.4.2/`** — `src` + `include` + `cmake` + `build-scripts` + `CMakeLists.txt` = **30,024,663 B**. Write `PROVENANCE.txt` in the house style naming the **zlib** licence explicitly, the tag `release-3.4.2-0-g683181b47`, what is compiled, and the two local changes below.

**Two vendored files, documented like every other:**

1. `src/dynapi/SDL_dynapi.h` gets `#if defined(SDL_STATIC_LIB) / #define SDL_DYNAMIC_API 0 / #elif` ahead of the platform ladder. Measured: `libSDL3.a` 5,439,012 → 4,808,150 B, exe −461 KB, and the `SDL3_DYNAMIC_API` environment override — a DLL-injection path in a statically linked game — stops existing. (Optional and deferred to Stage 9: trimming GPU/Vulkan/camera/render/dialog/haptic/power/sensor/hidapi cuts a further 36%, verified, and changes the link set only by making `oleaut32` optional. Not in the landing whose MSVC story is still fresh.)
2. **`SDL_build_config.h` for the browser, 15,242 B — and it belongs to the emsdk, not to SDL.** The file is `emsdk/upstream/emscripten/tools/ports/sdl3/SDL_build_config.h`, Emscripten's own, pinned to the emsdk that produced it — **6.0.8** here. `PROVENANCE.txt` must say so by version, because it is the one vendored file whose upstream is a toolchain rather than a library, and because of what it decides: it defines `SDL_THREADS_DISABLED 1` **and** `SDL_THREAD_PTHREAD` together, and its own header comment says it was modified from SDL's original *"only WRT to SDL_THREAD_PTHREAD"*. So the moment `-pthread` appears in `WebBuild/build.sh`, the port switches to `thread/pthread`, `SDL_CreateThread` starts **succeeding** on the web, and `streamedsound.cpp` would start a real decoder thread against a main-thread-only Emscripten OpenAL. Write that consequence into the file's `PROVENANCE.txt` entry and into `WebBuild/README.md`, and treat "the emsdk moved" as a reason to re-diff this file rather than assume it.

**`Tools/cmake/`** — the cmake 4.2.0 minimal **windows-i386** layout: `bin/cmake.exe` plus `share/cmake-4.2/{Modules,Templates,Licenses}`, **17,760,202 B in 1,715 files**. Document in `Tools/README.md` what that is next to `7za.exe` and `optipng.exe`: 15× the bytes and ~1,700× the file count, and a directory rather than a binary. Note that `cmcldeps.exe` is deliberately absent and is needed only under the Ninja generator with MSVC.

**`Build.bat`** — the `:sdl3build` section per Decision 2's table (**`-A Win32` mandatory**, `-DCMAKE_MSVC_RUNTIME_LIBRARY=…`, `-G` omitted, `/sdl3gen:` and `/toolset:` forwarding, the vswhere-derived banner line naming both the MSBuild toolset and the cmake generator), plus a **stamp file** recording the exact flag string. Without it, `cmake --build` is 0.042 s when clean but the *configure* is skipped entirely once the build directory exists, so a changed `-DSDL_X11_*` or `-DCMAKE_MSVC_RUNTIME_LIBRARY` is silently ignored. The same stamp rule goes into the other two build scripts, and `harness.sh`'s third `b5_stale` (Stage 1) reads it.

**`Blocks5.vcxproj`** — the six missing link libraries plus `SDL3-static.lib`, the SDL3 include directory at `:67`/`:94`, and the per-configuration library directory at `:79`/`:107` (Decision 2's table).

**`LinuxBuild/build.sh`** — cmake + build of the vendored tree with **eight** of the nine `-DSDL_X11_*=OFF`. **Not `SDL_X11_XINPUT`:** all nine off configures cleanly and then fails 20 s later with **11 compile errors in `src/video/x11/SDL_x11xinput2.c`** ("unknown type name `XIRawEvent`" at `:84`), an upstream guard bug. So **`libxi-dev` is a hard, non-negotiable prerequisite**, and the header comment must say so beside `libx11-dev` and `libxext-dev`, along with cmake and a generator. Fall back to Unix Makefiles where ninja is absent. `-lX11` can go — verified, `libX11` arrives only transitively through `libGL → libGLX` — but **say why**, because it comes back if anyone ever builds SDL3 with dynamic X11 off.

**Keep the SDL3 objects out of both md5-keyed object caches, and that is one finding stated once for two files.** `LinuxBuild/build.sh:79` and `WebBuild/build.sh:57` are the same line in different clothes — `local o="$OUT/obj/$(echo … | md5sum | cut -c1-12)-$(basename "$1").o"` — and in both scripts `$OUT` is `build/` or `build-test/` depending on the `hooks` argument. Left in the cache, Linux builds all 320 SDL3 objects twice and the browser builds all 181 twice, once for each output directory, for no benefit at all: SDL3 is identical in both, because `-DBLOCKS5_TEST_HOOKS` is ours and never reaches it. **Put the SDL3 archive in one shared location outside `$OUT` in both scripts.**

**`WebBuild/build.sh` and `build_asan.sh`** — the **direct-compile route**. Replay `tools/ports/sdl3.py`'s own glob list against the vendored tree: exactly **181 files**, `emcc -c -O2 -sUSE_SDL=0`, 16.7 s on four cores, zero errors, `emar` → a 1,923,314 B archive, and a wasm **within 16 bytes** of the port's with identical browser output.

The port itself is **refused**, for three measured reasons: `-sUSE_SDL=3` re-fetches a 17 MB github zip on *every* `emcc` invocation and gets **HTTP 403** here even with the archive already cached; `EMCC_LOCAL_PORTS` is described by Emscripten's own `tools/ports/__init__.py:305-307` as *"a hacky way… not tested"* and re-copies the whole 29 MB tree whenever mtimes look newer; and the port prints `-Wexperimental` on all ~170 compiles plus the link, into a build script that pipes the link through `tail -30`.

**`Tools/syntax.sh`** — the SDL3 arm behind the Stage 1 variable: `-I$LIBS/SDL3-3.4.2/include -I$LIBS/SDL3-3.4.2/include/SDL3`. `-DDECLSPEC=` stays for now (verified inert) and is deleted in Stage 9.

**`Tools/sdl3probe`** — ~60 lines each build system compiles and, on Linux and in the browser, runs: `SDL_Init`, `SDL_CreateWindow(OPENGL|RESIZABLE)`, `SDL_GL_CreateContext`, one immediate-mode quad, `glGetError`, report driver/version/stencil. This is the artefact that proves the three libraries *work* rather than merely link.

**A link-time assertion in all three builds that the one-DLL rule holds:** on Windows, no `SDL3.dll` and no `MSVCR*`/`VCRUNTIME*` in the import table (`objdump` here, `dumpbin` on the owner's machine); on Linux, no `libSDL3` in `ldd`.

**Proven by.** With the SDL 1.2 arm selected, all checks produce byte-identical output to Stage 4 — the game has not changed. With the SDL3 arm: **`syntax.sh` compiles `Blocks5/src/sdl_contract.cpp` clean under mingw against SDL3 headers** — and note what that is and is not: mingw is the compiler that reproduced the GLU break and is the only one here that sees the Windows-only code, but **it is not MSVC, and `sdl_contract.cpp` being in `Blocks5/src` and in the vcxproj is what makes the owner's build compile it too**, which is the half the first edition did not have. Linux builds `libSDL3.a` (320 objects, 7,174,140 B) and `sdl3probe` prints `driver=x11`, GL 4.5 compatibility, `glerr=0`, `stencilbits=8` under Xvfb; the browser builds the 181-object archive and `sdl3probe` boots in Chromium. Owner: E-MSVC-1…5.

**Ships?** Yes — the game does not consume any of it.

**Revert.** The working tree reverts cheaply. **The history does not.** That asymmetry is the reason for the merge gate.

**Risk.** Moderate-to-high, and all of it Windows. Linux and the browser were measured end to end during the survey.

---

### Stage 6 — THE FLIP *(atomic; not shippable mid-work)*

**Goal.** Replace SDL 1.2 with SDL3 everywhere, keeping a **classic `main()`** and `emscripten_set_main_loop_arg`. This is the one stage that cannot be thin, and pretending otherwise would be dishonest. Stages 1–5 have made it as small as they could.

**Why there is no dual-library switch, on record so nobody re-proposes it.** The divergence is in *types*, not spellings: `SDL_KeyboardEvent` has no `keysym` member at all (three shims across 40+ sites for `.sym`/`.mod`/`.unicode`); `SDL_GetKeyboardState` returns `const bool*` indexed by **scancode** where `SDL_GetKeyState` returned `Uint8*` indexed by **keycode**, so a shim would translate index spaces per call; `SDL_Surface::format` is an enum, not a struct pointer; `NUM_KEY_SLOTS = SDLK_LAST` does not exist; and the seven `onKeyEvent(const SDL_KeyboardEvent&)` virtuals (`gui_element.h:42`, `gui_listbox.h:30`, `gui_editbox.h:19`, `gui_multilineeditbox.h:21`, `help.h:14`, `options.h:15`, `gs_campaigneditor.cpp:79`) plus `Engine::QueuedKeyEvent`'s by-value member change shape in a way no macro can reach. A switch would be a second engine. Revertibility here comes from git and from the fact that eight of ten stages are independently revertible.

**Also refuse `-DSDL_ENABLE_OLD_NAMES`.** It covers 29 of the 47 missing keycode constants and would turn loud breaks into quiet ones. A compile error is the cheapest diagnostic available on a platform whose compiler nobody here can run.

**Structure: one branch, one squashed commit, four internal gates.**

- **Gate 0 — reconnaissance, time-boxed to two days, deleted.** Port the tree crudely on a scratch branch that will never be merged, to answer three questions and produce a cost inventory: does the **real game** reach `GS_Menu` in Chromium (via the test hook: a dump that comes back with a game state and a non-empty element list, waiting on the reported state, ~30 s under swiftshader — never a guessed interval); does `LinuxBuild/test/smoke.sh` complete; does it compile and link under MSVC with the Stage 5 plumbing (owner). Confirm the survey's counts while there: 18 `->format->` on 10 lines, 32 renamed-constant sites outside `engine.cpp`, **20 modifier sites** (18 `KMOD_` tokens on 9 lines plus 2 `SDL_GetModState()`), 14 `SDL_GetWMInfo` + 14 `SDL_VERSION`, 8 `#include <SDL…>` lines in the whole non-vendored tree. **Then delete the branch in the session that answers its questions** — its value is being allowed to be wrong, and a spike that must be defended is a spike that gets kept. It is two days and not a week because E-GL-WEB already answered the GL half in Stage 0; this only has to answer whether the *real game* gets there.
- **Gate 1** — compiles on Linux. **Gate 2** — runs on Linux, `smoke.sh` green. **Gate 3** — runs in the browser (`smoke.js`, `mobile.js`) and `syntax.sh` passes. Nothing lands until all three are green.

**Work.** (Every item below is a survey finding, not a guess.)

*Includes and init.* The eight `#include <SDL…>` sites — `pch.h:18-20`, `engine.cpp:12`, `transfer.cpp:14`, `linux_window.cpp:6-7`, `platform_stubs.cpp:2` — become `<SDL3/SDL.h>`, `<SDL3/SDL_thread.h>`, `<SDL3/SDL_opengl.h>`, and four build systems must agree on the path. `SDL_INIT_TIMER` goes (SDL3 raises the Windows timer resolution unconditionally from `SDL_InitTicks`, `SDL_timer.c:549-591`; the behaviour now hangs off `SDL_HINT_TIMER_RESOLUTION`). **The two polarity inversions are now two one-line bodies in `sdlwrap.cpp`** — `sdlInit` and `semWaitTimeout` return the bool directly — and nothing else in the tree can get them wrong. That is what Stage 2 bought: `gcc`/`mingw` with `-Wall -Wextra -Wconversion` are **all silent** on both un-negated forms.

*Window and GL.* `SDL_SetVideoMode` → `SDL_CreateWindow(SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE)` + `SDL_GL_CreateContext`, with `SDL_DestroyWindow`/`SDL_GL_DestroyContext` added to `Engine::exit` (nothing calls them today because there is no `p_window`). Add **`SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 0)` beside the existing seven at `engine.cpp:461-467` and nowhere earlier** — `SDL_VideoInit` calls `SDL_GL_ResetAttributes` (`SDL_video.c:709`), wiping anything set before `SDL_Init`. Without it SDL 3.4.2 **cannot create a GL window on llvmpipe under Xvfb**, which is `smoke.sh`'s own machine. Do not set `SDL_OPENGL_FORCE_SRGB_FRAMEBUFFER=1`; it overrides the attribute. `engine.cpp:582`'s five `p_display->format->` dereferences are **deleted**, not ported — there is no display surface — and whatever is worth keeping comes from `SDL_GL_GetAttribute`. `SDL_WM_SetCaption` (`engine.cpp:308`) and the icon block (`engine.cpp:469-513`) both **move** below window creation; the icon block is a **rewrite**, not a shrink — `SDL_CreateRGBSurface`, `SDL_SetAlpha` and `SDL_SWSURFACE` are all absent, `format` is an enum, and `SDL_SetWindowIcon` takes the loaded surface directly, so the convert-and-blit dance and the 29-line AND-mask loop both go.

*Keyboard.* `keyData[]` / `keyHeld[]` / `NUM_KEY_SLOTS` move to **scancode** index space (`SDL_SCANCODE_COUNT` = 512) through Stage 3's `KeySlot` type and `keySlotFor` seam — the change that would compile-and-run wrong rather than fail loudly, and which the strong type turns into a compile error at every site that gets it wrong. Delete `keyHeld[]` outright (SDL3 gives `event.key.repeat`; its only reader is `engine.cpp:1019`). Fix the 32 renamed-constant sites (`gs_game.cpp:336`, `gs_leveleditor.cpp:473-508`, `gui_editbox.cpp:189-211`, `gui_multilineeditbox.cpp:199-221`, `main.cpp:531-540`) — all compile errors, all real edits. Switch the VK table to `keytable.h`. Decide which space `isReturnKey` (`util.h:92`) lives in, since the Alt+Enter swallow at `engine.cpp:1043-1053` and the swallowed-return release test at `:1076` must agree. **`gs_menu.cpp`'s cheat tests and demo playback were already moved in Stage 3** and need nothing here beyond `keySlotFor` returning the new space.

*Text input.* **Call `SDL_StartTextInput(window)` once at init.** Deleting `SDL_EnableUNICODE(1)` and adding a `TEXT_INPUT` case is *not* sufficient: without it the level editor's title field, the campaign editor and the hint editor accept nothing at all, silently, with no compiler complaint. At `gui_editbox.cpp:220` and `gui_multilineeditbox.cpp:230`, decode the UTF-8 event text back to a Latin-1 byte and drop anything above U+00FF — the game's text pipeline is byte-per-glyph Latin-1 end to end (`font.xml` has 256 `<Character>` entries) and player-authored level titles are on disk in that encoding.

*Mouse.* `SDL_GetMouseState` takes `float*`; the three `cursorPosition = Vec2i(event.button.x, …)` / `event.motion.x` assignments at `engine.cpp:1103, 1108, 1116` are compile errors. The wheel becomes its own event, and **the sign inverts**: `gui.cpp:301` maps WHEELUP to −1 while `SDL_MouseWheelEvent.integer_y` is positive away from the user, so it must be `wheel = -integer_y` or every list scrolls the wrong way. Size the button array **8–16, not 6**: X11 maps buttons above 7 down (`SDL_x11events.c:1139-1142`), so a mouse with side buttons delivers SDL button 6+, which today lands in a 323-slot array and satisfies `wasAnyButtonPressed` — leave-the-pause. Delete `engine.cpp:828-829`'s `SDL_GetCursor`/`SDL_FreeCursor` rather than repairing the leak: `SDL_QuitMouse` destroys every cursor already.

*Surfaces and IO.* Port `img_load.cpp`/`file.cpp` to `SDL_IOStream`: `SDL_INIT_INTERFACE`, five callbacks, `SDL_OpenIO`, `SDL_LoadFile_IO`, `SDL_CreateSurface` + `SDL_SetSurfaceBlendMode(src, NONE)` — without that last one a blit composites and turns (255,255,255,0) into (0,0,0,0) in 552 of the first tile's 1024 bytes. Four things a mechanical port gets wrong:

- the close callback at `file.cpp:132` must **keep** `FileSystem::inst().closeFile(p_file)` and lose only `SDL_FreeRW` — dropping it leaks a `File` and, for `File_Archived`, its whole decompressed buffer on every texture load;
- `File_IOSeek` must return **−1 on failure**. Today `File_RWSeek` (`file.cpp:82-102`) ignores the bool from `File::seek` and returns `p_file->tell()` unconditionally; under SDL3 that lie is load-bearing in `SDL_GetIOSize`'s fallback and `SDL_IsPNG`'s Tell/Seek round trip, and `File_Archived::seek` returns false without moving the pointer;
- an empty file needs an explicit `if(!p_data || numBytes == 0)` guard — `SDL_LoadFile_IO` returns a **non-NULL one-byte buffer with size 0**, and the tree holds seven zero-byte `.png` marker files;
- close the `SDL_OpenIO` NULL hole while the file is open: `if(!io) FileSystem::inst().closeFile(this);`. The same hole exists today via `SDL_AllocRW`, so it is not a regression — but this rewrite is the only moment it will ever be cheap.

**Use `SDL_PIXELFORMAT_RGBA32`, never `SDL_PIXELFORMAT_RGBA8888`.** The masks at `img_load.cpp:51` and `texture.cpp:66/:173` are R=0x000000ff G=0x0000ff00 B=0x00ff0000 A=0xff000000, which is `RGBA32` = `ABGR8888` on little-endian. The same-looking name swaps red and blue in every texture in the game and, through `Texture::getPixel`, in every diamond-machine spark. It compiles and it runs. `sdl_traps` rule T1 forbids the wrong one and **S1**, the channel-mean comparison against the Stage 1 baseline, is what catches it if the rule is ever relaxed.

*Threads.* `SDL_CreateThread` takes three arguments. Keep `WebBuild/videorecorder_stub.cpp` and `streamedsound.h`'s compile-time `#ifdef` — `SDL_CreateThread` still returns NULL on the web (measured), and a runtime `if(!p_thread)` check would silently start a real decoder thread if the web build ever gained `-pthread`, against a main-thread-only Emscripten OpenAL. The three thread types were typedef'd in Stage 2, so the nine member declarations do not move here.

*Browser.* **The GLImmediate fix is two statements and the order matters** — `Browser.useWebGL = true;` **first**, then fire `Browser.moduleContextCreatedCallbacks`, in one `EM_ASM` immediately after `SDL_GL_CreateContext`. `libglemu.js:2847`'s `if (!Browser.useWebGL) return;` runs *before* the matrices are allocated at `:2868-2876`, so firing the callbacks alone reaches `GLImmediate.initted=true`, prints its reassuring warning, and still throws the identical TypeError at `_emscripten_glLoadIdentity`, with no compile or link error. Measured both ways in Chromium.

**Never call `SDL_SetWindowFullscreen` in the web build** — `SDL_emscriptenvideo.c:705-747` promotes the **canvas**, which is precisely what makes the on-screen pad, its sibling, vanish. `Module.b5_setFullscreen` stays on `<html>`, and rule T2's allow-only-inside-`#ifndef __EMSCRIPTEN__` form is what holds it. Record alongside: SDL3 installs a document-level `fullscreenchange` handler (`SDL_emscriptenevents.c:1289`) that matches only the canvas id, so when the game takes the fullscreen on `<html>` SDL never learns and `SDL_GetWindowFlags` permanently disagrees with the browser — **nothing may read it there**. Never pass `SDL_WINDOW_FILL_DOCUMENT` either; it reparents every other body child into a hidden div.

**Canvas-size ownership gets one owner: `WebBuild/pre.js`.** SDL3 sizes the canvas to 1×1 to probe CSS at window creation and then writes `floor(css × pixel_ratio)` itself (`SDL_emscriptenvideo.c:597-605`, `SDL_emscriptenevents.c:550`), so whatever `b5_fitCanvas` did before `SDL_CreateWindow` is overwritten — **C must call `b5_fitCanvas` immediately after window creation**, and whenever the per-frame poll at `engine.cpp:936-946` sees a change, call `SDL_SetWindowSize` so `window->w` tracks the backing store. SDL scales pointer coordinates by `window->w / css_w` (`SDL_emscriptenevents.c:717-719`), so a disagreement is a systematic click offset — exactly the failure S4 exists to catch. **Keep the per-frame poll**: SDL 3.4.2 observes neither `ResizeObserver` nor `visualViewport` (grep over its whole `src/` returns nothing), and `visualViewport` is how a phone reports the address bar sliding away.

**Delete `WebBuild/platform_stubs.cpp` whole** — its `int SDL_LockSurface(SDL_Surface*)` is a *conflicting declaration* against SDL3's `bool`, a compile error rather than dead code — and remove it from `WebBuild/build.sh:40` **and `build_asan.sh:33`**, along with `-Wl,--wrap=SDL_CreateRGBSurface` at `build.sh:140` **and `build_asan.sh:84`**. **Delete rule T3's self-retiring exemption in the same commit**, which the check will insist on. Then **explicitly `SDL_HideCursor()` in the browser**: SDL3's Emscripten backend implements cursors for real (`SDL_emscriptenmouse.c:72`), so dropping the stubs would otherwise put a browser arrow on the canvas beside the one the game draws, and neither `smoke.js` nor `mobile.js` would catch it. Set `SDL_GL_ALPHA_SIZE` to 0 in the web build or make `shell.html`'s canvas background opaque — SDL3 sets the WebGL context's alpha from `gl_config` (`SDL_emscriptenopengles.c:92`), which SDL 1.2's JS did not.

*Two deletions and one refusal.* Delete `engine.cpp:1024-1036`'s Alt+F4 workaround (SDL3's win32 backend generates `CLOSE_REQUESTED` itself and the last window's close becomes `SDL_EVENT_QUIT`, both on by default) and the `#ifdef __EMSCRIPTEN__` `SDL_WINDOWEVENT` block at `engine.cpp:982-1002` (SDL3 has no `event.window.event` field). **Keep the `SDL_PumpEvents` at `engine.cpp:3216`**, renaming `SDL_JoystickUpdate` to `SDL_UpdateJoysticks`: the deletion's stated justification is false in exactly the live-resize case, and even on the normal path it is a behaviour change, since `Engine::update()` runs 0..N times per iteration and today each caught-up tick sees a fresh snapshot where after deletion all N would share one.

*Accept and record one deliberate loss.* `gs_leveleditor.cpp:485-489`'s Shift+1..5 palette shortcuts switch on the event keycode. Today Emscripten's SDL 1.2 derives the keysym from the unshifted DOM `keyCode`, so Shift+1 arrives as `SDLK_1`; under SDL3's Emscripten backend it arrives as `'!'` (`SDL_emscriptenevents.c:430-435`) and the case never fires. Scancode-keying `keyData` does **not** fix it — this is the event-keycode path. Either match on `event.key.scancode` there, or take the loss and put it in the changelog. (Owner decision, §6.)

**Proven by.** All four checks — `verify.py` (**23 checks**), `syntax.sh` (120 files plus `sdl_contract.cpp`; the only compiler here that sees the Windows-only code and the one that reproduced the GLU break), `LinuxBuild/build.sh`, `WebBuild/build.sh` with its `${PIPESTATUS[0]}` test intact — and all three harnesses: `smoke.sh` (including the `-nofbo` pin, the fullscreen geometry, the text-input step, the tick rate, the F12 frame count and the title-demo assertion), `smoke.js`, `mobile.js`. **Beyond green**, the Stage 1 golden artefacts must match: the same fourteen config ids, the same tick rate, a 2 s recording with the right frame count, and **S1–S4 in place of a screenshot diff** — S1's channel-mean comparison, S2's pixel-exact editor frame if Stage 1 proved it stable, S3's present-rect assertion and S4's click-offset assertion. Owner: `Build.bat` produces a running `blocks5.exe` whose `dumpbin /dependents` still lists exactly `OpenAL32.dll` plus system DLLs.

**Ships?** No, not mid-work. It lands as one squashed commit when all three gates are green.

**Revert.** One revert, back to a running SDL 1.2 game. Not revertible in pieces — that is the honest cost of this stage.

**Risk.** The highest of the plan, concentrated in three places nothing here can test: MSVC, the Windows live-resize path, and the phone. Second-order risk is duration. **If the gates are not all green inside the time box, land a smaller flip**: the browser's on-screen pad, the video recorder and the joystick can each be reduced to a named, documented degraded state behind a TODO. A running SDL3 game with two features degraded is a far better position than an un-runnable tree.

---

### Stage 7 — Restore what SDL3 changed

**Goal.** Port everything whose failure a player notices and a compiler does not, last, against a working game so each change can be compared with the frame before it. Nothing here is load-bearing for the build; anything that cannot be proven can be deferred without blocking the rest.

**Work.**

**`fixWindowSize` is three calls in this order, and the order is not cosmetic:** `SDL_RestoreWindow`, then `SDL_SetWindowMaximumSize(640,480)`, then `SDL_SetWindowResizable(false)`. Measured: `SDL_SetWindowResizable(false)` **alone left a 1000×800 window at 1000×800** — the flag cleared, the size did not change — and that is the real `-nofbo` scenario, because `restoreWindowPosition` replays a large remembered window before the framebuffer decision is taken. Only `SDL_SetWindowMaximumSize` shrinks it, and that is documented generic behaviour (`SDL_video.c:3375-3380` ends with `return SDL_SetWindowSize(window, w, h)` clamped to max), not luck. The ordering: `WM_GETMINMAXINFO`'s non-resizable branch pins Windows to whatever size the window has when resizable is cleared, and `SDL_windowsevents.c:1790` sets `force_ws_maximizebox` while maximized, which ORs `WS_MAXIMIZEBOX` back in. Add `SDL_SetWindowMinimumSize(640,480)` at creation: `handleResize` becomes book-keeping only *because something else took over the enforcement*.

**Drive `displaySize` off `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` as well as `RESIZED`.** `SDL_windowevents.c:133-135` suppresses `RESIZED` when the logical size is unchanged while `SDL_CheckWindowPixelSizeChanged` still runs, so on any platform where logical and pixel size diverge a pixel-size change fires only the former. `displaySize` must mean pixels — `presentFrame`, the single `glViewport`, the recorder and `getCursorPosition` all depend on it.

**Window placement becomes portable.** `SDL_GetWindowPosition` / `SDL_SetWindowPosition` / `SDL_MaximizeWindow` / `SDL_GetWindowFlags` replace the `#ifdef _WIN32` block. There is no `rcNormalPosition` equivalent and the plan's answer is "record the floating rect only when not maximized", which follows from event order: SDL3 sets `SDL_WINDOW_MAXIMIZED` **before** it sends the accompanying MOVED/RESIZED, and the structural cause is in the common layer (`SDL_windowevents.c:155-161`, inside `SDL_SendWindowEvent`), so the reasoning is platform-independent.

**Say where the measurement came from, because it is not the platform this matters on.** The `flags=6a2` reading — the maximized bit already set at MAXIMIZED dispatch — was taken by `adv/big.c` under **Xvfb + openbox**. The region being rewritten, `rememberWindowPlacement`, is Windows-only. So: **measured on X11; the mechanism is in code both platforms share; the Windows ordering is unverified**, and §6 item 7 is what settles it by hand. If Windows turns out to dispatch the other way round, the fallback is to record the floating rect on a one-frame delay rather than in the event, which is uglier and works either way.

**DPI — a decision, not a discovery.** `WIN_InitDPIAwareness` (`SDL_windowsvideo.c:571-584`) reads no hint and falls into `WIN_DeclareDPIAwarePerMonitorV2`, called unconditionally from `WIN_VideoInit` at `:607`. SDL 1.2 is DPI-unaware, so today a 150% display hands the game a virtualised 1280×720; under SDL3 the same machine reports 1920×1080, `getDefaultWindowSize` (`engine.cpp:2080-2088`) jumps from 1× to 2×, and a restored `<Window sizeX="1280" sizeY="960">` is a **physically smaller window**. `SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "unaware")` preserves today's behaviour exactly, and is the default this plan recommends so the migration changes one thing at a time. Going DPI-aware means re-deriving CLAUDE.md's 120px-margin argument, which is written against the unaware numbers. **Owner decides, on a real high-DPI display.**

**Keyboard cosmetics.** SDL3's key names are already capitalised, so `keyboardNiceName`'s `toupper` fallback and its comment go. **Guard the fallback against non-ASCII:** `SDL_GetKeyName` ends in `SDL_UCS4ToUTF8` for any layout key above 0x7F, so on a German layout an umlaut key's fallback name becomes two UTF-8 bytes where `Font::renderText` and `languages.txt` are Latin-1 — two garbage glyphs on a key button, invisible to every check. Fall back to the frozen id instead. Separately, accept or reject SDL3's changed wording for keys not in `p_keyDisplayNames` ("Left Shift" where SDL 1.2 said "left shift") as a decision rather than a surprise.

**Key repeat — decide it or accept it.** Stage 3 recorded that the rate is no longer the game's to set and that seven `isKeyRepeat()` consumers change feel. This is where the work would go if the owner wants the old 140/60 back: time the repeats in `GUI` off the 20 ms logic tick and ignore `event.key.repeat` for the GUI's own purposes. It is perhaps half a day and it touches one file. **Default recommendation: accept the OS rate**, because it is what every other application on the machine does and a player's own accessibility settings then apply — but it goes in the changelog either way (Stage 9).

**Apply the joystick migration** decided in Stage 3: clear every joystick binding on the first start after upgrade, raise the toast naming the actions that lost one, and set `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS` deliberately or accept that cached axis and button state stops updating without keyboard focus, which SDL 1.2's winmm polling did not care about.

**Make every unresolved binding loud.** Today `getVKFromId` returns −1, `resolveActionKeys` clears the string, `getVKId(-1)` returns empty and `saveConfig` writes it back — with **nothing logged anywhere**. Add a `printfLog` line and one `Engine::showToast(TOAST_ERROR, …)` naming how many bindings could not be carried over.

**Browser touch and cursor.** Prove `mobile.js`'s one real-touch check end to end — CDP's injected touch does produce the trusted `pointerdown`/`pointerup` SDL3 listens for on a bare canvas, but it has never been driven through SDL3's handler into this GUI, and it is the only touch test that exists. Prove `Module.b5_setFullscreen` on `<html>` coexists with SDL3's canvas-level fullscreen **with the pad present**, using Stage 1's `elementFromPoint` check. Fix `Emscripten_ShowCursor` being a no-op while mouse focus is NULL (`SDL_emscriptenmouse.c:78`) against `Engine::updateCursorSize`'s early return on an unchanged scale (`engine.cpp:4450`) — a 1×→2× change during a fullscreen toggle or resize is otherwise applied to nothing and never retried.

**`util.cpp:396-424`'s `getExactTime()`** may become `SDL_GetTicksNS()` here — **only after** checking that no caller treats the Linux branch's absolute `CLOCK_MONOTONIC` seconds as absolute rather than as a difference. Otherwise leave it.

**Proven by.** Stage 1's assertions, now covering rewritten code. `smoke.js`/`mobile.js` including **S4's browser twin**: click a known page coordinate and assert through the hook that the expected element was hit — the only thing that catches a canvas-size disagreement scaling pointer coordinates. S1 and S2 against Stage 6's captures. Owner: window placement across a restart and across a second monitor to the left of the first (negative coordinates); a DPI display; a real controller; a real phone.

**Ships?** Yes, item by item.

**Revert.** Item by item; nothing here is structural.

**Risk.** High in the sense that this is where a defect reaches a player rather than a build, and much of it cannot be exercised from here.

---

### Stage 8 — Adopt the callback entry points

**Goal.** The owner's decision, on top of a working SDL3 game, so the retreat is a revert of one commit.

**Work.** By this point Stage 4 has already moved the state, fixed the accumulator, split `flushInput`, installed the re-entrancy guard and installed the unhandled-exception filter. What remains:

- **`main.cpp`:** keep `int main()` and its `__try`; add `#include <SDL3/SDL_main.h>` **after** `#include "pch.h"`; call `SDL_EnterAppMainCallbacks(argc, argv, AppInit, AppIterate, AppEvent, AppQuit)` from inside the `__try`. Write the `#ifdef`-not-`#if` fact and the `/Yu` rule into the comment beside it, so nobody later "tidies" this into the macro form. `main_entry` (Stage 3) enforces the single `SDL_main.h` include and the macro's absence from every source file. E-ENTRY-LOCAL (Stage 0) will have run this exact shape on Linux, Emscripten and mingw-under-wine; E-MSVC-7 will have run it on MSVC. **If E-MSVC-7 came back red, this bullet is instead the build-system `SDL_MAIN_USE_CALLBACKS` definition** and §6 item 5's crash test stops being confirmatory.
- **Wire the four callbacks onto Stage 4's shapes.** `SDL_AppInit` does what `runTheGame`'s head did and allocates the `AppState`; `SDL_AppIterate` is `Engine::appIterate()`; `SDL_AppEvent` is the loop's event switch plus the key-grab gate; `SDL_AppQuit` calls `engine.exit()` and deletes the appstate. **`SDL_AppQuit` runs even when `SDL_AppInit` returned `SDL_APP_FAILURE`** (`SDL_main_callbacks.c:100-116`, then `SDL_QuitMainCallbacks(rc)` unconditionally), so the delete must tolerate a NULL appstate — `Engine::exit()` already guards on `initialized` at `engine.cpp:764`, but say so rather than discover it.
- **The hint, and only now.** `SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, …)` from inside `SDL_AppInit` (verified to take effect — `SDL_AddHintCallback` fires immediately) and delete the `SDL_Delay` at `engine.cpp:1343`. Natively the generic loop does not throttle and the game requests no vsync, so without the hint it free-runs at ~610 kHz. The accumulator has been on the wall-clock scheme since Stage 4 and `smoke.sh`'s tick-rate assertion has been passing since Stage 1.
- **Browser.** `emscripten_set_main_loop_arg` and the `simulate_infinite_loop` flag go away, and with them the whole "no destructor ever runs in the browser" constraint — **`SDL_AppQuit` is called on `SDL_APP_SUCCESS`** (verified end to end), so the Quit button finally writes `config.xml`. Add an `SDL_EVENT_TERMINATING` case (delivered synchronously from `beforeunload`; `SDL_AppQuit` is **not** reached on a tab close), and record that `SDL_AppEvent` can be entered re-entrantly and from a non-main thread (`SDL_main_callbacks.c:28` says so outright) — in the browser it is the main thread's `beforeunload`, so it is fine here, but the constraint belongs in the comment. Treat `WebBuild/web_bluescreen.cpp` as a **rework, not a deletion**: its `emscripten_cancel_main_loop` at `:105` is redundant only if the `SDL_QUIT` case returns `SDL_APP_SUCCESS`, and if it does, `engine.exit()`/`SDL_Quit()` then tear the Emscripten video backend and the GL context down under a DOM overlay that must stay clickable for 700 ms and then reload — while SDL's own `Emscripten_VideoQuit` unregisters its callbacks alongside `web_bluescreen.cpp:103-104` unregistering ours. **Test it, do not reason about it.**
- **State the frame's new shape rather than discover it:** today one iteration is render → poll → logic; under callbacks SDL drains the queue into `SDL_AppEvent` first, so it becomes **events → render → logic**. `handleResize` still lands before `presentFrame` either way.

**`Engine::hookWindowProc`, `repaintDuringSizeMove` and the 15 ms timer stay — and here is exactly what that does and does not answer.** The first edition listed four hazards and presented keeping the hook as the answer to them. Read against the code, it answers one:

| Hazard | Keeping the hook | Status |
|---|---|---|
| The live-resize repaint path was read and never run, and `SDL_OnWindowLiveResizeUpdate` calls `SDL_IterateMainCallbacks(**false**)` — SDL deliberately does not pump | **Answered.** Our own repaint keeps working exactly as today, and Stage 4's `reentryDepth` makes the re-entrant case cheap | answered |
| The interval is `USER_TIMER_MINIMUM` = **10 ms**, not the 15 ms our code uses, so there are now two timers on one HWND | **Partly.** `SetTimer` is keyed by id, so two ids coexist — but nobody has run it, and two repaint paths on one window during a drag is exactly the shape that makes `repaintDuringSizeMove`'s *"borrows `displaySize` and must put it back"* dangerous | **E-WIN-RESIZE item 5** |
| **`WM_ENTERSIZEMOVE` calls `SDL_ResetKeyboard()`** (`SDL_windowsevents.c:1879-1880`), posting key-ups for everything held | **NOT answered.** `engine.cpp:2190-2196` handles `WM_ENTERSIZEMOVE` with `break`, which falls through to the chained `CallWindowProc(p_sdlWindowProc, …)`, so SDL's own case runs **regardless of what we do**. Keeping the hook preserves our repaint *in addition to* SDL's behaviour; it does not suppress it | **UNANSWERED** — E-WIN-RESIZE item 3 |
| **`WM_ENTERMENULOOP` fires the same case** (`:1855-1856`) | **NOT answered**, for the identical reason: it reaches SDL untouched. Opening a window menu now resets the keyboard and starts SDL's timer, which SDL 1.2 never did | **UNANSWERED** — E-WIN-RESIZE item 4 |

**The plan has an honesty rule and these two sentences broke it.** They are unanswered behaviour changes, not defused ones, and E-WIN-RESIZE is told to measure both by name and to report the key-up count with keys actually held. If it comes back showing that a drag or a window menu drops held keys, the fix is ours and it is small — the `SDL_ResetKeyboard` arrives as ordinary key-up events through the normal dispatch, so `Engine` can ignore key-ups between `WM_ENTERSIZEMOVE`/`WM_ENTERMENULOOP` and `WM_EXITSIZEMOVE`/`WM_EXITMENULOOP` — but it is a work item that does not exist until the measurement says so, and it must not be written blind.

One further fact recorded beside the hook rather than acted on: the public `SDL_AddEventWatch` list is **second in line** behind SDL's two internal window-event lists (`SDL_windowevents.c:242-243` calls `SDL_DispatchEventWatchList` directly, and the public list is reached only via `if (post_event && SDL_EventEnabled(windowevent))` at `:245-256`), so a watch stops firing the moment anything calls `SDL_SetEventEnabled(SDL_EVENT_WINDOW_EXPOSED, false)`. That is why an event watch is not a replacement for the hook. Note the duplication in a comment; **delete ours only if E-WIN-RESIZE shows a real drag behaving, and only after the two unanswered hazards have answers.**

**Proven by.** All four checks unchanged from Stage 7, plus two measurements only this stage can make: `smoke.sh`'s tick-rate assertion over a 10-second run must give 480–520 logic ticks (the accumulator bug's signature is ~70), and a new `smoke.js` assertion that `config.xml` is written when the browser Quit button is pressed — impossible before, and the visible proof that `SDL_AppQuit` runs. Plus `syntax.sh` for the Windows entry point. Owner: a Release build crashed deliberately must still write a StackWalker trace, **and E-WIN-RESIZE's five answers on the real drag path.**

**Ships?** Yes.

**Revert.** One commit, back to Stage 6/7's classic-`main` SDL3 game. **This is the recorded retreat** and it is why this stage is eighth: callbacks are a preference, SDL3 is the goal.

**Risk.** Moderate, almost all Windows-only. Two of the four live-resize hazards have a defusal in the tree by the time this stage runs; two do not, and this stage is where they get measured.

---

### Stage 9 — Delete SDL 1.2 and rewrite the documentation

**Goal.** Remove the second library, and leave CLAUDE.md describing the game that now exists.

**Work.** Delete `Blocks5/libs/SDL-1.2.15` (170 tracked files, 2,003,347 B), the 67 `ClCompile` entries in `Blocks5.vcxproj` and the 67 in `.filters`, `DECLSPEC=` at `:68`/`:95`, and `dxguid.lib` at `:78`/`:106` if E-MSVC-4 cleared it. Remove the SDL-selection switch from `syntax.sh`, `LinuxBuild/build.sh` and `WebBuild/build.sh` — one library, one path.

**Widen `sdl_traps` rule T4** from `keytable.h` to the whole tree, now that `engine.cpp:199-200`'s old key table and `platform_stubs.cpp` are both gone. **Retire nothing else.** In particular rule T3 (`key_index_space`) is permanent: it is what stops a keycode being used as a slot index in some future edit, and the first edition's plan to retire the rule it replaced is exactly why that rule was worthless.

Version bump in the four places `verify.py`'s `version` check knows: `p_localVersion` in `src/main.cpp`, `AppVersion`/`OutputBaseFilename` in `setup/Blocks 5.iss`, the banner and changelog in `readme.txt`, and `FILEVERSION`/`PRODUCTVERSION` plus the two string values in `src/resources.rc` — the one that was missed before and sat at 1.1.1 through the whole of 1.1.2. **Plain numeric, no suffix** (see the `check_version` repair in Stage 1).

**Changelog entries for the player-visible changes** — four, not three:

1. the binding vocabulary (ids are now scancode-derived and stable across layouts; bindings carry over, and any that could not are named in a toast and in `log.txt`);
2. the joystick clear on first start;
3. the DPI decision, whichever way it went;
4. **key repeat now follows the operating system's rate**, everywhere — menus, lists, edit boxes and the level editor;

and the browser Shift+1..5 palette loss if that was taken.

Documentation is §7 below.

**`BASELINE` in `verify.py` stays at `95660bb`.** Moving it forward is the tempting wrong answer: it would silently exempt the entire migration from `style`, `ctor_init` and the comment-density guard, which is precisely the class of change those three exist to judge. `engine.cpp` is already 1939 of 4458 lines outside the exemption and `engine.h` 348 of 600; every line the migration touches joins that set, and `ctor_init` firing on every new `Engine` member (`SDL_Window*`, `SDL_GLContext`, the rebuilt VK table) is the check doing its job. Budget the findings per stage rather than suppressing them.

**Proven by.** All checks green on a tree with exactly one SDL — **23 checks, 29 selftest cases.** `git grep -c 'SDL-1.2'` returns nothing outside `ROADMAP.md` and `FINDINGS.md`, which are historical logs and stand. Owner: a clean-clone `Build.bat` run producing a `stage/` tree whose only DLL is `OpenAL32.dll`.

**Ships?** Yes. **Revert.** Not usefully — this stage exists to remove the retreat. Take it only when Stages 6–8 have been running for a while.

**Risk.** Low technically, high in the sense that CLAUDE.md is the file this project is steered by.

---

## 5. The silent-failure register

**This is the most important section in the document.** SDL3 does not break this codebase loudly. Every row below compiles, links, and in most cases runs; the second column says why nobody would notice; the third names the specific thing that catches it and the stage that arms it.

**Two disciplines apply to the third column, and the first edition broke both.** A row may not name a check unless that check has been shown to fire on that fault (§4 Stage 1), and a row may not name a *compile error* as though it were a check — a compile error is better than a check and should be said so, not dressed up as one. Where a strong type does the work, the row says so.

### A. Killed before SDL3 arrives (Stages 1–4, on SDL 1.2)

| # | Failure | Why it is silent | Caught by | Armed |
|---|---|---|---|---|
| 1 | `#ifdef SDL_VIDEO_DRIVER_X11` (`linux_window.cpp:9,21,62`) compiles to `return false` — Linux fullscreen (`engine.cpp:2491`) and the `-nofbo` 640×480 pin (`engine.cpp:2417`) die | No error, no warning, no link failure. `-nofbo` is exercised by nothing in the whole test tree | `foreign_ifdef` **with `LinuxBuild` as a fifth base** (row 45); `#else #error`; smoke.sh `-nofbo` pin; smoke.sh fullscreen geometry made mandatory | 1, 2 |
| 2 | `uint startTicks` (`cf_rewind.h:35`) stops being a modular delta once `SDL_GetTicks` is `Uint64` — 934 of 1750 frames wrong at 2³² ms | A garbled REWIND arrow, once every 49.7 days | **`enum class Ticks : Uint64` — a compile error at every narrowing site**, not a check; `tick_width` only as a cast backstop; `sdl_contract.cpp`'s two `static_assert`s | 2 |
| 3 | Half-widening `font.h:76` and leaving `font.cpp:208`'s `uint minTime = ~0;` narrow → the LRU loop assigns nothing and erases a **default-constructed iterator** (`font.cpp:220-221`) | A crash, not a degradation, and only past 49.7 days | **The declaration widened to `Ticks minTime = TICKS_NEVER;`** — writing `~0ULL` into a `uint` is a no-op and does not fix this. Plus `oldestEntry = stringCache.begin()`, which closes the class rather than the instance | 2 |
| 4 | GLU vanishes on **Windows only** — 13 call sites in six files | Linux and the browser build clean; only `syntax.sh` or a real MSVC sees it | `pch.h` include made unconditional; three `glu*` calls in **`Blocks5/src/sdl_contract.cpp`** — in the vcxproj, so MSVC compiles it too — behind an `getenv` guard so the linker must resolve them | 2 |
| 5 | `if(SDL_Init(…))` inverts — SDL3 returns **true** on success, so the game refuses to start on every successful init | `gcc`, `mingw`, `-Wall -Wextra -Wconversion`: **all silent** | `sdlwrap.cpp` funnel + `sdl_funnel` check (a grep for "must be negated" cannot be green on SDL 1.2) | 2 |
| 6 | `SDL_WaitSemaphoreTimeout` inverts — encoder free-runs at ~100 Hz, **2.2× the frames**, every duplicate clamped to a full frame's duration, plus **29 measured `glReadPixels` races in 5 s** | An MP4 that plays and looks nearly right | Same funnel, over **both** call sites — `videorecorder.cpp:199` and `streamedsound.cpp:339`; smoke.sh F12 **frame-count-vs-wall-time** assertion (an "is there an MP4?" check passes on the broken build) | 2 |
| 7 | NULL semaphore returns **true** under SDL3, so the corrected wait falls straight through and the encoder runs unpaced | Only when `SDL_CreateSemaphore` fails | `if(!p_semaphore \|\| !p_thread)` at `videorecorder.cpp:432`; the same F12 assertion | 2 |
| 8 | `SDL_HINT_MAIN_CALLBACK_RATE` alone → 101 iterations, **15 logic ticks** where 100 were wanted | The game at one seventh speed, with nothing in any diff to point at | smoke.sh tick-rate assertion (480–520 ticks in 10 s), passing since before anything changed | 1, 4, 8 |
| 9 | Under `SDL_MAIN_USE_CALLBACKS` the `__try` at `main.cpp:614` covers nothing that runs during play | Crash reporting stops; a game that has quietly stopped reporting looks like a game that has stopped crashing | **Conditional, not answered.** The entry mechanism keeps the `__try` *if* E-MSVC-7 passes; if it does not, `SetUnhandledExceptionFilter` becomes the primary reporter and §6 item 5's crash test becomes mandatory | 0, 4, 8, §6 |
| 10 | `/Yu` discards a `#define` above `#include "pch.h"`, and SDL tests the macro with `#ifdef` so even `… 0` selects callbacks | No diagnostic; the entry point is simply not the one you wrote | No macro is ever written — plus `main_entry` and the E-MSVC-6 probe as belt and braces | 0, 3, 8 |
| 11 | `flushInput` hollow for the key grab under callbacks (`flushInput-swallowed=0` measured) while still real for `transfer.cpp:495` | Escape closes the dialog *and* reaches the GUI; no error | Split into `discardPendingInput()` + a gate in the event dispatch; new smoke.sh key-grab step (open a grab, tap Escape, assert the dialog still open and the binding cleared) | 1, 4 |
| 12 | Five `SDL_PushEvent` sites push a partly uninitialised `SDL_Event` | Harmless under SDL 1.2; SDL3 reads `common.timestamp`, runs the filter and two watch lists, and consults `ShouldDispatchImmediately` | `SDL_zero(e)` at all five | 2 |
| 13 | `verify.py`'s `check_version` regex `[\d.]+` silently drops `main.cpp` on a `-beta` suffix; the other seven then agree and the check reports clean | A check that has stopped checking | The regex repair; plain numeric versions | 1, 9 |
| 14 | Xvfb autorepeat is 660 ms / 40 ms against `b5_hold`'s 400 ms, so both held-key guards pass **without ever generating a repeat** | Two tests that cannot fail | `XkbSetAutoRepeatRate(dpy, XkbUseCoreKbd, 100, 40)` in `b5_start`, plus an assertion that a repeat occurred | 1 |
| 15 | **`gs_menu.cpp`'s demo playback writes `keyData` from outside `engine.cpp`.** `translateRecordedKey`'s fall-through (`:47`) is `return recorded;` — a raw SDL 1.2 number used directly as a slot index — and its table (`:34-37`) is keycode-valued | **The path runs at every single start.** No error; the title demo simply stops moving, or moves the wrong thing | Table retargeted to the frozen scancodes; fall-through becomes **ignore**, provably lossless because `demo1.dat`'s distinct key numbers are exactly the eight in the table; `clearAllKeyData()` replaces the `SDLK_LAST` loop; **`KeySlot` makes the whole family a compile error**; rule T3; the title-demo smoke assertion | 3, 6 |

### B. Live through the flip (Stages 6–8)

| # | Failure | Why it is silent | Caught by | Armed |
|---|---|---|---|---|
| 16 | `SDL_PIXELFORMAT_RGBA8888` instead of `RGBA32` — red and blue swapped in every texture and every diamond-machine spark | Compiles, runs, looks like a stylistic choice | `sdl_traps` rule **T1**; **S1**, the per-channel-mean comparison against a red/blue-swapped baseline — animation-invariant, unlike the "screenshot diff" this replaces | 1 |
| 17 | `SDLK_LMETA`/`SDLK_RMETA` exist in SDL3 as `0x2000xxxx` extended keycodes for a *different* physical key, and any `k & ~SDLK_SCANCODE_MASK` mangles them | Compiles cleanly; **no compiler and no other check can see it** | `sdl_traps` rule **T4**, scoped to `keytable.h` where the fault is (tree-wide it is red at five legitimate SDL 1.2 sites today); widened tree-wide in Stage 9 | 3, 9 |
| 18 | `keyData`/`keyHeld` change index space (keycode → scancode) without changing type | Compiles and **runs wrong** — the expensive kind | **`enum class KeySlot : int`, minted only by `keySlotFor` and the frozen table — a compile error at every site that passes a keycode.** Rule T3 as the backstop for subscripts and loop bounds. The first edition's "no direct indexing outside `engine.cpp`" rule is deleted: measured, every such site is already inside `engine.cpp` and the only two hits elsewhere are an unrelated `unordered_map` | 3 |
| 19 | `gs_menu.cpp:154-162`'s `p_keyStates[SDLK_c]` / `[SDLK_LSHIFT]` — a **~1 GB out-of-bounds read** if only the `#ifdef` is removed | It is a cheat code; nobody exercises it | Moved in **Stage 3**, not Stage 6, alongside the demo playback in the same function; `KeySlot`; rule T3 | 3 |
| 20 | `SDL_StartTextInput(window)` never called → **the level editor's title field, the campaign editor and the hint editor accept nothing at all** | No error, no compiler complaint; the editors look fine | **smoke.sh types into the editor title field and asserts its contents through the hook.** Nothing in this tree has ever typed into an editor | 1 |
| 21 | The GLImmediate fix written as one statement — `initted=true`, the reassuring warning printed, and the **identical TypeError at `_emscripten_glLoadIdentity` on the first frame** | No compile error, no link error | Both statements in the proven order; smoke.js's frame-rendered assertion; E-GL-WEB before any of it | 0, 1 |
| 22 | `SDL_SetWindowFullscreen` on the web promotes the **canvas**, so the on-screen pad — its sibling — vanishes | The pad reports a full-size `getBoundingClientRect` **while invisible**, so the existing test sees nothing | `sdl_traps` rule **T2, in its allow-only-inside-`#ifndef __EMSCRIPTEN__` form** — the first edition's "must not appear in `WebBuild/` or in an `__EMSCRIPTEN__` branch" passes on the realistic bug, which is an *unguarded* call in `engine.cpp`; `mobile.js` moved to `document.elementFromPoint` on the pad's centre while fullscreen | 1 |
| 23 | Two owners of the canvas backing store → SDL scales pointer coordinates by `window->w / css_w` → **systematic click offset** | Every click lands off by a constant ratio; a screenshot looks perfect | One owner (`pre.js`), re-fitting after `SDL_CreateWindow`; **S4**, the click-offset assertion that names *which element* a known coordinate hit | 1, 7 |
| 24 | Mouse-wheel sign: `gui.cpp:301` maps WHEELUP to −1, `integer_y` is positive away from the user | Every list scrolls the wrong way — obvious to a human, invisible to every check | Named; `wheel = -integer_y`; smoke.js list-scroll step | 6 |
| 25 | `flushInput` peeping only 0x300..0x301 — SDL 1.2 carried the typed character *inside* the key event, SDL3 does not | Typed characters leak past a key grab into whatever edit box has focus | Keyboard range widened to 0x300..0x303 (`SDL_EVENT_TEXT_INPUT`); the key-grab smoke step | 4, 6 |
| 26 | **The mouse half of the same range.** SDL 1.2's `SDL_EVENTMASK(SDL_MOUSEBUTTONDOWN)` *did* flush the wheel (buttons 4/5); `SDL_FlushEvents(MOUSE_MOTION, MOUSE_BUTTON_UP)` stops at 0x402 and leaves `SDL_EVENT_MOUSE_WHEEL` (0x403) **queued** | A wheel tick survives a modal file dialog and arrives afterwards. Tiny — but "matching today's behaviour" was stated backwards | **A recorded decision** (Stage 3): extend to 0x403 and flush it, matching today | 3, 6 |
| 27 | `NUM_BUTTON_SLOTS = 6` — X11 delivers SDL button 6, 7 for side buttons, which today land in a 323-slot array and satisfy `wasAnyButtonPressed` (leave-the-pause) | A mouse feature quietly stops working on one platform | Size 8–16, and never write "SDL3 can only ever deliver 1..5" into a comment | 6 |
| 28 | Dropping `platform_stubs.cpp` gives the browser a **real system cursor** beside the one the game draws | Neither `smoke.js` nor `mobile.js` would catch it | Explicit `SDL_HideCursor()` in the web build | 6 |
| 29 | `Emscripten_ShowCursor` is a no-op while mouse focus is NULL, against `updateCursorSize`'s early return on an unchanged scale — a 1×→2× change during a fullscreen toggle is applied to nothing and never retried | The cursor is simply the wrong size, sometimes | Drop the early return in the browser or reapply on mouse-enter | 7 |
| 30 | `SDL_GL_FRAMEBUFFER_SRGB_CAPABLE` set before `SDL_Init` is wiped by `SDL_GL_ResetAttributes` inside `SDL_VideoInit` | The window simply fails to create — on llvmpipe under Xvfb, i.e. `smoke.sh`'s own machine | Placed with the seven at `engine.cpp:461-467` and nowhere earlier, with a comment saying why | 6 |
| 31 | `displaySize` driven only off `RESIZED`, which is suppressed when the logical size is unchanged | Pixel-size changes are missed where logical and pixel size diverge | Also drive off `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` | 7 |
| 32 | `SDL_GL_ALPHA_SIZE 8` makes SDL3 give the WebGL canvas an alpha channel; the page composites through it wherever the game leaves alpha < 1 | SDL 1.2's JS made this choice differently; nothing tests page compositing | Set alpha 0 in the web build or make `shell.html`'s canvas background opaque | 6 |
| 33 | Browser Shift+1..5 palette shortcuts stop firing (SDL3 delivers `'!'`, not `SDLK_1`) | A level-editor shortcut quietly gone in one build | Match on `event.key.scancode`, or take the loss **in the changelog** | 6 |

### C. Player data and the long tail (Stages 3, 7)

| # | Failure | Why it is silent | Caught by | Armed |
|---|---|---|---|---|
| 34 | Minting ids from `SDL_GetKeyFromScancode` makes them **keyboard-layout dependent** — `key:CARET` on a German Windows where a US one mints `key:BACKQUOTE` | The file format survives; only its *portability* dies | The frozen `keytable.h`, never SDL; E-KEY-TABLE proves the property | 0, 3 |
| 35 | 19 named scancodes mint the same id `key:UNKNOWN`; `getVKFromId` returns the **first match** | A binding on such a key resolves to a different physical key | `key:sc#<n>`; the `vk_ids` check, whose fault is *the default outcome of the naive design* and therefore certainly reachable | 3 |
| 36 | Deleting `engine.cpp:3879-3883` — a 1.1.2 player's bare-integer bindings are all lost on first start and **written back empty** | No error, and a downgrade does not restore them | Translated, not deleted; the migration corpus test; the unresolved-binding toast and log line | 3, 7 |
| 37 | `key:#N` ids — `SDLK_WORLD_0..95` is where SDL 1.2 on Windows puts the accented keys, so a German player's umlaut binding is `key:#246` and means nothing under SDL3 | The one place non-US keyboards live | Covered by the same translation table and by a corpus case | 3 |
| 38 | Joystick ids resolve and mean a **different physical button** — SDL3 has no winmm driver and HIDAPI deliberately remaps known pads | The id resolves. There is no error and no compile break | Clear + toast + changelog; and the toast is the *only* signal, because `repairLostBindings` returns at the first action that still has a key | 3, 7 |
| 39 | A latched XInput trigger resting at −32768 leaves a bound action **permanently down** (measured: unchanged after ten update cycles) | The game plays as if a key were held | The same joystick clear defuses it | 7 |
| 40 | Windows Per-Monitor-V2 DPI by default → `getDefaultWindowSize` jumps 1×→2× and a restored `<Window sizeX="1280">` is physically smaller | Nobody's config changed; the window did | `SDL_HINT_WINDOWS_DPI_AWARENESS "unaware"`, or a deliberate re-derivation of the 120px margin | 7 |
| 41 | `SDL_GetKeyName` returns multi-byte UTF-8 above 0x7F into a Latin-1 font | Two garbage glyphs on a key button; `verify.py`'s `encoding` check reads source files, not runtime strings | Filter the fallback to ASCII, else fall back to the frozen id | 7 |
| 42 | **Key repeat now runs at the OS rate**, feeding seven `GUI::isKeyRepeat()` consumers | Not silent to a player — but it was silent in the *plan*, appearing only in a documentation list | A changelog line and, if the owner wants the old feel, an explicit tick-timed repeat in `GUI` | 3, 7, 9 |

### D. Build, checks and harness

| # | Failure | Why it is silent | Caught by | Armed |
|---|---|---|---|---|
| 43 | cmake **skips the configure** once the build directory exists, so a changed flag set is silently ignored | `--build` is 0.042 s and reports success | A stamp file over the exact flag string in all three build scripts | 5 |
| 44 | `-Wl,--wrap=SDL_CreateRGBSurface` left pointing at a symbol that no longer exists, in **`build_asan.sh:84`** as well as `build.sh:140` | Named twice in the survey, tested never | Both removed together with `platform_stubs.cpp` from `build.sh:40` **and `build_asan.sh:33`** | 6 |
| 45 | **`foreign_ifdef` cannot see the file it exists for.** `verify.py`'s `source_files()` (`:53`) walks four bases and `LinuxBuild` is not one of them, so the natural implementation returns all-clear on a tree with landmine 1 fully armed | The check reports an all-clear about the wrong thing — worse than no check | Its **own** five-base list, `LinuxBuild` included, written down with a comment saying why it is not `source_files()` | 1 |
| 46 | **Stage 5 disarms `foreign_ifdef` on the exact macro it was written for.** After vendoring, `SDL_VIDEO_DRIVER_X11` *is* defined in the tree — `libs/SDL3-3.4.2/include/build_config/SDL_build_config.h.cmake:427`, `SDL_build_config_macos.h:191`, and a real `#define` in the generated `obj/sdl3/include-config-release/build_config/SDL_build_config.h:427` | "Grep the tree for a `#define`" is the obvious implementation and the wrong one; the check goes quiet at the stage that makes the landmine live | The definition search pinned to the same five bases — never `libs`, never `obj`. **A selftest case that plants a `#define` under `Blocks5/libs/` and requires the finding to still fire** | 1 |
| 47 | `foreign_ifdef` reports thirteen findings on day one and its allowlist gets stuffed | An allowlist with thirteen unexplained entries stops meaning anything | A `//`-commented `#define` counts as a definition site — `engine.cpp:842` `// #define RECORD`, `:1572` `// #define PROFILE_ENGINE_UPDATE` and five more are this tree's idiom for a hand-thrown switch | 1 |
| 48 | **`b5_stale` is blind to the SDL3 archive.** `harness.sh:46-49` compares the binary against `Blocks5/src` and `data.zip` against `Blocks5/data`, and after the migration there is a third input | Change a `-DSDL_X11_*`, rebuild, run the harness, and it certifies a binary linked against the previous library | A third `b5_stale` against the archive and the Stage 5 stamp file | 1, 5 |
| 49 | **The SDL3 objects go through both md5-keyed object caches** and get built twice, once for `build/` and once for `build-test/` | Nothing fails; the build is simply twice as slow, in two scripts, for identical output | Shared archive location outside `$OUT` in **`LinuxBuild/build.sh:79` and `WebBuild/build.sh:57` alike** — one finding, two files | 5 |
| 50 | **The vendored `SDL_build_config.h` belongs to the emsdk, not to SDL**, and it defines `SDL_THREADS_DISABLED` and `SDL_THREAD_PTHREAD` together | An emsdk upgrade, or one `-pthread`, silently turns `SDL_CreateThread` from "returns NULL" into "starts a real thread" against a main-thread-only Emscripten OpenAL | `PROVENANCE.txt` names emsdk **6.0.8** by version and states the `-pthread` consequence; `WebBuild/README.md` repeats it | 5 |
| 51 | **An IDE build produces no SDL3.** After Stage 5 there is no fourth project, no project reference and no build ordering, so `Blocks5.sln` opened in Visual Studio fails to link until `Build.bat` has run once | CLAUDE.md documents the IDE path and nothing in the build tells you it has stopped working | A decision (Decision 2's last row), a `Build.bat /?` line and a CLAUDE.md passage — or the fourth mechanism, which keeps the path | 5, 9 |
| 52 | The public `SDL_AddEventWatch` list is **second in line** behind SDL's two internal window-event lists and stops firing the moment anything disables the event | A live-resize repaint that just stops happening | Recorded beside the live-resize refusal; `hookWindowProc` is not deleted | 8 |
| 53 | `xdotool click 1` delivers **two** SDL3 button pairs on Xvfb where the harness's own shape delivers one | A "simplification" of `b5_click` that makes every click a double-click | `b5_click`'s shape kept, comment rewritten with the measurement | 1 |
| 54 | `libwayland-client` prints `error: XDG_RUNTIME_DIR is invalid or not set` on every headless run, and SDL3's own log handler prefixes `ERROR: ` | `smoke.sh:275`'s case-sensitive grep changes meaning under the migration | `XDG_RUNTIME_DIR` / `SDL_VIDEO_DRIVER=x11` exported in `harness.sh` | 1 |
| 55 | **The screenshot diff the plan leaned on three times is impossible.** The menu runs the animated title demo and the global `MTRand mt` (`util.cpp:11`) is seeded from `/dev/urandom` or `hash(time(NULL), clock())` (`MersenneTwister.h:262`), so no two frames match; and a scale change cannot appear in a 640×480 FBO readback at all | A check that is flaky from the first run gets switched off within a week, taking the channel-swap guard with it | **S1–S4** (§4 Stage 1): channel means against a swapped baseline; one pixel-exact frame from the *level editor*, which runs no logic — deleted rather than loosened if Stage 1 finds it unstable; the present rect through the hook; the click-offset assertion | 1 |

---

## 6. What only the owner can do

**A Windows machine with MSVC v145 that can be used repeatedly — not once.** Twelve items below need it and several are re-runs after a change. A borrowed hour settles Stage 0's probes and leaves Stages 5, 7 and 8 unverifiable.

1. **Stage 0, before Stage 5 merges.** Run `Tools/msvc_probe/`: E-MSVC-1 (cmake 4.2 32-bit + real MSVC, both configurations from one configure, `-G` omitted, **`-A Win32` given**, and record the configure wall time, the `SDL3-static.lib` path and whether `cmcldeps.exe` was wanted); E-MSVC-2 (`dumpbin /directives` → **LIBCMT**, never MSVCRT — the only proof of the `/MT` mechanism); E-MSVC-3 (`dumpbin /dependents` → only system DLLs + `OpenAL32.dll`, no `MSVCR*`/`VCRUNTIME*`); E-MSVC-4 (one link with `dxguid.lib` removed); E-MSVC-5 (MSBuild's reaction to the `pch.obj` name collision — the names were proved to collide, the reaction was not); E-MSVC-6 (the `/Yu` rule); **E-MSVC-7 (the entry point — own `main()` + `__try` + `SDL_EnterAppMainCallbacks`, both configurations, crash-tested from inside an `SDL_AppIterate`)**.
2. **Stage 0.** Archive today's `Release/blocks5.exe`, its `dumpbin /dependents` output, and the `config.xml` a fresh first start writes. That file is the golden binding baseline and it must come from real SDL 1.2 — this box runs sdl12-compat.
3. **E-WIN-RESIZE, and it is now five questions rather than one.** A ~60-line drag-and-log program under `SDL_EnterAppMainCallbacks`, **with a window procedure chained in front of SDL's exactly as `Engine::hookWindowProc` does it**. Drag the border, hold still, open a window menu, and report each of the five separately (§3): the `WM_TIMER` re-entry depth; our `SDL_PumpEvents` inside the modal loop; **how many key-ups arrive from `SDL_ResetKeyboard()` with keys held**; **what `WM_ENTERMENULOOP` does**; and whether SDL's 10 ms timer and our 15 ms one coexist on one HWND. Items 3 and 4 are **unanswered behaviour changes** that keeping the hook does not address — the first edition listed them as though the decision defused them, and `engine.cpp:2190-2196` `break`s and chains, so SDL's case runs regardless. This decides whether `hookWindowProc` can *ever* be deleted; until it says yes it stays, and that retreat is permanent if need be.
4. **After Stage 5.** `Build.bat`, `Build.bat /rebuild` and `Build.bat /clean` with the cmake step in place: **first-build wall time** (nobody has an MSVC configure number — every timing in the survey is Linux or mingw with Ninja), incremental wall time, the disk footprint of both configurations, and that `/clean` leaves nothing behind. Also confirm the banner prints the MSBuild toolset and the cmake generator and that they agree.
5. **After Stage 4, and again after Stage 8.** Crash a Release build deliberately and confirm `log.txt` carries a StackWalker trace. Once more with the `__try` temporarily removed, to prove the `SetUnhandledExceptionFilter` half independently. **This is the single most important thing the callbacks work can get wrong without anyone noticing** — and if E-MSVC-7 came back red, it stops being confirmatory and becomes the only proof that crash reporting still exists.
6. **After Stage 6 and again after Stage 9.** `dumpbin /dependents` on the built exe — the one-DLL rule verified on the artefact rather than on the link line.
7. **After Stage 7 — window placement by hand.** Off-centre, maximize, quit, restart; then the same with a second monitor **to the left** of the first, which is where the negative coordinates live. Three of the four `<Window>` attributes cannot be tested from here at all — and this is also the only way to confirm that Windows sets `SDL_WINDOW_MAXIMIZED` before the accompanying MOVED/RESIZED, which was measured on X11 and is unverified on Windows.
8. **After Stage 7 — the DPI decision**, measured on a real 150%-scaled display. The recommendation is `SDL_HINT_WINDOWS_DPI_AWARENESS "unaware"`; going DPI-aware means re-deriving CLAUDE.md's 120px-margin argument.
9. **After Stage 7 — a real controller.** Play a level with it. The joystick remap has no test on any platform and cannot get one from here.
10. **After Stage 7 — a real phone.** Fullscreen on first touch with the on-screen pad still visible; the landscape lock; a tap reaching the GUI; a reload with the network off. `mobile.js`'s CDP touch was proven only as far as a bare canvas receiving trusted pointer events.
11. **Four decisions.** (a) **Has 1.2.0 actually shipped?** `git tag` is empty in this checkout. If it has not, the bare-integer-config population is *every existing player* and the numeric fallback is load-bearing for all of them rather than for a tail. (b) Accept or reject the browser Shift+1..5 palette loss. (c) **Accept the OS key-repeat rate, or pay for a tick-timed repeat in `GUI`** — the rate is no longer the game's to set and every list, edit box and the level editor change feel. (d) **The IDE build path**: document "run `Build.bat` once first", or take the fourth mechanism, which keeps `Blocks5.sln` self-sufficient.
12. **One sign-off, up front.** The entry mechanism in §2 — keeping `main()` and calling `SDL_EnterAppMainCallbacks` rather than defining `SDL_MAIN_USE_CALLBACKS`. It delivers all four callbacks and the appstate pointer, and it is a departure from the literal wording of a decision the owner made. Note that it is a **variant** of the shape the survey exercised, not that shape: `exp/web/cb2.cpp` used `SDL_MAIN_HANDLED` and therefore took the opposite branch through both of `SDL_main.h`'s guards. Stage 0's E-ENTRY-LOCAL proves the real spelling on three builds before the owner spends a minute on it.

---

## 7. Documentation

`grep -ci sdl CLAUDE.md` → **59 lines across roughly fifteen passages.** CLAUDE.md is the file this project is steered by; leaving it describing SDL 1.2 is worse than leaving a compile error, because the compile error gets found. Treat the rewrite as the deliverable of Stage 9 and the version bump as the formality, not the other way round.

**CLAUDE.md — the passages, by name:**

1. **The MultiByte section.** Both its stated facts die with SDL 1.2 — the 67 sources no longer compile inside `Blocks5.vcxproj`, so `CharacterSet` becomes a free choice rather than a rule. Keep `MultiByte` (the game's own code still calls `MessageBoxA`/`ShellExecuteA`), lose the justification. **And add the mirror-image rule:** SDL3's own sources use `TEXT()`/`TCHAR` in at least nine Windows files and SDL's own project sets **no** `CharacterSet`, so a vendored SDL3 project must be left exactly as SDL ships it, never given the game's rule.
2. **"all 67 files of the Win32 subset out of `libs/SDL-1.2.15/src`"** — gone.
3. **Four rows of the vendored-patch table** — the two SDL 1.2 patches go, one SDL3 `dynapi` patch and one emsdk-owned `SDL_build_config.h` arrive.
4. **The `DECLSPEC=` paragraph** — gone; SDL3 spells it `SDL_DECLSPEC` and defaults it to nothing.
5. **`DIB_SetVideoMode`'s fast path and the whole "SDL's video flags must never change" argument** — that constraint does not exist in SDL3. Verified against a real 1024×512 FBO with a packed depth-stencil, not a 1×1 texture: the context and the FBO's contents survive `SetWindowFullscreen` in both directions, `SetWindowSize` and `MaximizeWindow` (`fbo=1 tex=1 rb=1 status=COMPLETE readback=26,230,76,255 glerr=0`, `ctxSame=1`, all five probes). Note that `engine.h:133-136` is a *comment* attached to `overrideFullScreen()`, which implements `-windowed`/`-fullscreen` and stays — only the four comment lines go.
6. **The `hookWindowProc` / `WM_ENTERSIZEMOVE` passage** — rewritten to say what is kept and why, **and to name the two hazards that keeping it does not answer**: `SDL_ResetKeyboard()` on entering the modal loop and `WM_ENTERMENULOOP` reaching the same case, both because our `WM_ENTERSIZEMOVE` handler `break`s and chains. Plus the `reentryDepth` guard, the two timers, and the event-watch ordering.
7. **`SDL_EnableKeyRepeat(140, 60)`** and the whole `b5_key`/`b5_hold` timing argument that hangs off it — SDL3 delivers repeats at the OS rate with a `repeat` flag and **the rate is no longer the game's to set**, which reaches seven `isKeyRepeat()` consumers and is a changelog line, not only a doc fix.
8. **`SDL_GetKeyState`** and the two-layer input description — the state array is now `const bool*` indexed by scancode, and `KeySlot` is the type that says so.
9. **The four-checks section** — now six checks longer: **23 checks, 29 selftest cases**, plus `Blocks5/src/sdl_contract.cpp` as a fifth thing that runs (a translation unit every compiler in the loop sees, including MSVC). The count must match `Tools/README.md`'s table, which gains six rows: `sdl_traps` (four rules), `foreign_ifdef`, `tick_width`, `sdl_funnel`, `vk_ids`, `main_entry`.
10. **The window/placement passage** — `applyWindowStyle`, the `SDL_VIDEORESIZE` route and `fixWindowSize` all change mechanism; the placement block becomes portable.
11. **New:** the frozen scancode → id vocabulary and why it is not SDL-derived.
12. **New:** the vendoring-location rule and why `verify.py` cannot see `Blocks5/libs/` — **and the corollary that `foreign_ifdef`'s definition search must not either**, which is the one place that rule has a sharp edge.
13. **New:** the shipped `Tools/cmake` — a **directory of 1,715 files**, the ≥ 4.2 floor, the 32-bit choice, `-A Win32`; the SDL3 build step in each of the three build systems; `libxi-dev`; and the IDE-build sentence.
14. **New:** the browser's two-statement GLImmediate fix, who owns the canvas size, **and the payload cost** — about +700 KiB of wasm and +130 KiB of JS, roughly +65%, against a 49 KB drop in startup allocation.
15. **New:** the licence paragraph — LGPL 2.1 statically linked → zlib.
16. **The "no destructor ever runs in the browser" passage** — the `simulate_infinite_loop` argument is retired by Stage 8; `SDL_AppQuit` runs on the Quit button and `config.xml` is finally written there.

**Elsewhere:** `README.md` (10 SDL lines) · `LinuxBuild/README.md` (11, plus the new cmake / generator / libxi-dev prerequisites **and the new `smoke.sh` wall time**) · `WebBuild/README.md` (8, including **`:85-86`** "it cannot read the synthesised SDL_RWops" and **`:313`** "SDL_BlitSurface is implemented on a 2D canvas", both now false, plus the emsdk-version note on the vendored build config) · `Tools/README.md`'s check table (six new rows) · **`img_load.h`'s 12-line header comment**, which ends "always 32 bit RGBA, SDL_SWSURFACE" — a flag that does not exist in SDL3 · `streamedsound.h:61-66` (SDL3's `SDL_mutex.h` carries no such warning) and `streamedsound.cpp:332-333` (states `SDL_SemWaitTimeout`'s old polarity in words) · `WebBuild/videorecorder_stub.cpp:5-7` (the stub stays; only the comment is wrong) · `testhooks.cpp:189` · `harness.sh:152`, `smoke.sh:211`, `smoke.sh:267` (stale prose that names no SDL symbol and would survive a rename sweep) · `Build.bat`'s header (`/sdl3gen:`, `/nosdl`, the disk cost, the IDE sentence) · the three build scripts' headers.

`ROADMAP.md` (36) and `FINDINGS.md` (41) are historical logs and stand.

**Write `Blocks5/libs/SDL3-3.4.2/PROVENANCE.txt`** in the house style, naming the **zlib** licence explicitly — `Blocks5/libs/SDL-1.2.15/PROVENANCE.txt` is one of only three of the twelve that never mentions a licence, and it is the LGPL one.

---

## 8. Abort criteria and the retreat

**Stop conditions, in the order they can occur.**

1. **E-GL-WEB does not render a correct frame** — display lists, `glBegin`/`glEnd`, texture matrix, an FBO through `SDL_GL_GetProcAddress`, `glScissor`, a stencil pass — in Chromium with the two-statement fix in the proven order. Then nothing is spent: no vendoring, no git history, no restructure. This is deliberately the first and cheapest kill switch on the whole browser story.
2. **Stage 6's Gate 0 cannot reach `GS_Menu` in the real game.** Stage 5 does **not** merge, the 30 MB stays out of git history, and the web target is reconsidered separately from the desktop one. Stages 5 and 6 share a branch for exactly this reason.
3. **E-MSVC-2 shows `MSVCRT` rather than `LIBCMT`, or `/MT` cannot be made to hold.** Take the recorded retreat: **the fourth mechanism** — vendor SDL's own `VisualC/SDL/SDL.vcxproj` plus its `Directory.Build.props` under `Blocks5/libs/SDL3-3.4.2/`, patch `ConfigurationType` from `DynamicLibrary` to `StaticLibrary`, drop `DLL_EXPORT`, add `SDL_STATIC_LIB`, and add it to `Blocks5.sln` as a fourth project with a project reference. It already uses `/MT` and `/MTd`, sets no `PlatformToolset` (so it inherits `$(DefaultPlatformToolset)` for free), needs no cmake, no build-ordering step and no contamination of the game's compiler settings — **and it keeps the Visual Studio IDE build working**, which the cmake route does not. **Exclude SDL's `pch.c` and `pch_cpp.cpp`** — they collide with `Blocks5/src/pch.cpp` on the object name, and a vcxproj gets one PCH per configuration. **Leave it at whatever `CharacterSet` MSBuild defaults to, never the game's MultiByte rule.** This is a retreat, not a re-opening of the owner's decision. Never a second shipped DLL.
4. **Option (a) — folding SDL's 235 sources into `Blocks5.vcxproj` — is refused outright**, and stays refused: the `pch.obj` collision above, plus 235 SDL sources inheriting `/fp:fast`, `BufferSecurityCheck=false`, `Optimization=Full` and `_SECURE_SCL=0`, plus SDL's `src/` and `src/core/windows` landing on the game's project-wide include path, plus ~940 lines of churn. SDL does not test under `/fp:fast`.
5. **Any stage that would need a second DLL beside the executables.** Not a trade-off to weigh — a stop.
6. **Stage 6 has not passed all three gates inside its time box.** Do not add scope; land a smaller flip with the pad, the recorder or the joystick in a named degraded state. A running SDL3 game with two features degraded beats an un-runnable tree.
7. **S1, S2, S3 or S4 fails against the Stage 1 baseline** — the channel-mean comparison, the pixel-exact editor frame, the present rect, or the click-offset assertion. Do not proceed. Those are the failures that look right in a running game and are exactly what the golden artefacts exist for. **Note what this criterion is not any more:** it is not "a screenshot diff", because the menu animates and its particles are seeded from `/dev/urandom`, so that comparison could never have been run. A criterion whose instrument does not work is worse than no criterion.
8. **Stage 8 cannot hold 50 Hz, cannot write `config.xml` on the browser Quit button, or breaks the live resize.** Revert Stage 8 alone (see the retreat below).
9. **Stage 3's frozen table cannot resolve all four legacy vocabularies without ambiguity, or cannot reproduce the fourteen golden id strings from the archived SDL 1.2 `config.xml`.** Do not ship a partial vocabulary: a binding that fails to resolve is lost silently and then written back as an empty string, so downgrading the executable does not restore it. **Better to stay on SDL 1.2 than to ship a build that quietly empties players' key bindings.**
10. **`Tools/selftest.py` cannot be made to fire every case, or cannot restore a file byte-for-byte and mtime-for-mtime.** The mtime half is not pedantry: `b5_start` refuses to run a binary older than its sources, so a selftest that moves an mtime forces a full rebuild — and one that cannot restore has been editing the tree the checks are judging.
11. **Any stage where a check has to be weakened, narrowed or exempted to pass.** Moving `BASELINE` forward, adding a file to the vendored exemption to dodge `encoding`, or relaxing `foreign_ifdef`'s allowlist to swallow a real finding are each a stop-and-re-plan. **The three self-retiring exemptions this plan does grant are not covered by this criterion and must not become the precedent for one that is:** each names an exact file, gives a reason, and **fails the check if the excused site stops matching**, so it cannot outlive the code it excuses. An exemption that is not itself checked is a hole.
12. **Any stage that reports success on the strength of a check that has not itself been proven to fire.** That is the operating rule, not a contingency, and it is why Stage 1 comes before Stage 2. **It applies to this plan's own checks**, which is the finding that produced this edition: three of the first edition's four new rules were green by construction and one was red for a reason unrelated to SDL, and the criterion that would have caught them was written down and then not turned inward. Every rule in §4 Stage 1 now states its fault, its reachability and its colour on today's tree, and any rule added later must do the same before it is registered.
13. **If executing this plan requires contradicting something the survey REFUTED**, write it down as such, with its own evidence, before acting. Silently planning around a refuted claim is worse than being wrong, because the next reader will not know it was refuted.

**The retreat.**

> A classic `main()` with `emscripten_set_main_loop_arg` has been **proven** to work against SDL3 in the browser: 45 frames, 7 events, `driver=emscripten`, WebGL 1.0, `simulate_infinite_loop` correctly not returning. The only SDL3 coupling to callbacks anywhere is `SDL_GL_SetSwapInterval` (`SDL_emscriptenopengles.c:56-63`), which this game never calls.

So the callbacks restructure is a **preference** and SDL3 is the **goal**. Stage 6 delivers a working SDL3 game on a classic `main`; Stage 8 is one revert away from it; and the value of the migration — the licence, the real C in the browser, the dead library gone — is already banked at Stage 6, before the riskiest work begins.

---

## Estimate

In focused engineer-days, not calendar time.

| Stage | Days | Owner time |
|---|---|---|
| 0 — Prove first (now including E-ENTRY-LOCAL) | 3–4 here | ~3 h Windows |
| 1 — Arm the checks | 3–4 | — |
| 2 — Defuse the landmines (now including the strong `Ticks` type) | 3–4 | — |
| 3 — Seams, vocabulary, `KeySlot`, the `gs_menu` demo port | 5–7 | — |
| 4 — Lifetime and loop | 4–5 | ~30 min (crash test) |
| 5 — Vendor and build | 4–6 | ~1 h |
| 6 — **The flip** | 8–12 | ~1 h (MSVC gate) |
| 7 — Restore behaviour | 4–6 | ~2 h (DPI, monitors, pad, phone) |
| 8 — Callbacks | 3–4 | ~1 h (drag, crash test) |
| 9 — Delete and document | 2–3 | ~30 min (clean-clone build) |
| **Total** | **39–55** | **~9 h, spread across at least four sessions** |

Three stages moved from the first edition's numbers, and each moved for a named reason: **Stage 0** gains E-ENTRY-LOCAL, which is half a day and removes the cheap ways for E-MSVC-7 to fail; **Stage 2** gains the strong `Ticks` type, its operator set and the `printf`-format sweep that follows from it, which is what turns the whole widening class from a check into a compile error; and **Stage 3** gains the `KeySlot` type, `Engine::getKeyboardVK`'s 22 call sites and the whole `gs_menu.cpp` demo-playback port, which the first edition used as evidence for a different change and never scheduled.
