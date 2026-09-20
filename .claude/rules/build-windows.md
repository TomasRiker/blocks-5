---
paths:
  - "Build.bat"
  - "Blocks5.sln"
  - "Blocks5/Blocks5.vcxproj*"
  - "Blocks5/setup/**"
  - "Blocks5/src/resources.rc"
  - "Blocks5/src/stackwalker.*"
  - "Blocks5/src/main.cpp"
  - "Blocks5/libs/**"
  - "Blocks5/readme.txt"
  - "PWEncrypt/**"
  - "ShowUserDir/**"
---

# The Windows build, the installer and the vendored libraries

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

**Three compile errors had to be fixed to get there**, all in vendored libraries, each written up in
the relevant `libs/*/PROVENANCE.txt`: shine's `__attribute__((unused))`, which MSVC rejects; `misc.c`
in the libvorbis file lists, a pthreads debug allocator upstream never compiles; and `windows.h`
inside SDL's `#pragma pack(push,4)`, which makes every `C_ASSERT` in a modern `winnt.h` fail.

**The fourth fix is a rule: build MultiByte, never Unicode.** SDL 1.2 is an ANSI codebase — `char*`
throughout, calling `RegisterClass`, `LoadLibrary`, `GetLocaleInfo` unsuffixed. It was a DLL before,
built ANSI by SDL's own project, so `CharacterSet` never mattered; now that its 67 sources compile
*inside* `Blocks5.vcxproj`, `Unicode` resolves those to the `...W` variants and MSVC merely warns
(C4133). The first such build died in `SDL_RegisterApp`: `GetCodePage` passes `char buff[8]` and
`sizeof(buff)` to `GetLocaleInfo`, whose last parameter counts *characters*, so `GetLocaleInfoW` wrote
16 bytes into 8 — and forty other C4133 warnings were the same bug waiting to happen. The game's own
code never depended on Unicode: `MessageBoxA`, `ShellExecuteA` explicitly, no `TCHAR`, `TEXT()` or
`wchar_t` outside vendored `stackwalker.cpp`. `SDL_win32_main.c` keeps `#undef UNICODE` as a guard.

**What nothing here can check is a runtime contract in the Microsoft CRT, and on the startup path that
failure is silent.** MSVC validates arguments glibc accepts and hands a violation to the invalid
parameter handler, which *ends the process* rather than returning an error — so the console window
opens and shuts with nothing in it, nothing in `stdout.txt` or `stderr.txt` and nothing in `log.txt`,
because the statement that would have written a line is the one that died. The measured case is
`setvbuf(stdout, 0, _IOLBF, 0)` at the head of `runTheGame()`: glibc reads the 0 as "choose a size for
me", MSVC documents 2..INT_MAX for `_IOLBF` and rejects it. `Tools/syntax.sh` compiles `main.cpp` and
the Windows-only sources, which is the most any check here can do; a contract is not a compile error.
The same call carries the other trap on that path — SDL 1.2 points stdout at `stdout.txt` before
`main()` is reached, so anything documented as "before any I/O on this stream" has already missed its
window. A libc call added to the startup path is Windows-risky by default, and worth saying so about
where it merges rather than after.

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

Installer: `setup\Blocks 5.iss` (Inno Setup). The version number lives in **four** places that must
stay in sync — `p_localVersion` in `src/main.cpp`, `AppVersion`/`OutputBaseFilename` in the `.iss`, the
banner and changelog in `readme.txt`, and `FILEVERSION`/`PRODUCTVERSION` plus the two string values in
`src/resources.rc`, which is what Explorer shows and a crash log reports. The `.rc` had been missed
before and sat at 1.1.1 through the whole of 1.1.2.

**OpenAL is OpenAL Soft**, vendored in `libs/openal-soft-1.25.2` (headers, public domain) with its import
library in `libs/bin` and `Blocks5/OpenAL32.dll` — `soft_oal.dll` renamed, how that distribution is meant to
be used without the router. Because the app directory beats `system32` in the DLL search order, the game
always gets this implementation and never whatever Creative's 2009 installer left. The game calls only core
AL/ALC 1.1 (23 functions, no extensions, no `alGetProcAddress`), so the switch needed no source change. The
DLL is LGPL v2 and must stay dynamically linked.

**Deployment.** All three projects link the CRT statically (`/MT`, `/MTd` for Debug), so nothing needs a
Visual C++ redistributable — the installer has no runtime task at all any more. Exactly one DLL ships beside
the executables, `OpenAL32.dll`, and the only CRT it imports is `msvcrt.dll`, part of Windows — not a
versioned `MSVCR*`/`VCRUNTIME*`. Its other imports are all core Windows: `KERNEL32`, `USER32`, `SHELL32`,
`ole32`, `WINMM`, `AVRT`. Keep it that way: a new dependency needing a redistributable, or a second DLL,
undoes the whole arrangement.

## Every local change to a vendored library

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
