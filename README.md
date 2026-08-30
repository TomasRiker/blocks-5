Blocks 5 - Bob's Amazing Adventures
===================================

A 2D tile-based puzzle/action game in C++, on SDL 1.2, OpenGL and OpenAL Soft.


Building on Windows
-------------------
Run `Build.bat` from the repository root. It locates MSBuild, builds all three projects for
`Win32`, and then packs `data.zip` and `levels/skins/*.zip` — build products that are not in
Git and that the game cannot start without.

    Build.bat                     Release, newest toolset this Visual Studio has
    Build.bat Debug
    Build.bat /toolset:v143       pin one toolset instead of taking the newest
    Build.bat /stage              also assemble a redistributable tree in Blocks5\stage
    Build.bat /clean              delete every build product again, and exit
    Build.bat /run -windowed      build, then run the game with these arguments
    Build.bat /?                  all options

For a compiler-only setup, "Build Tools for Visual Studio" — 2022 or any later year — is
enough; no IDE is needed. Tested with **v143 and v145**; the toolset notes at the top of
`Build.bat` say what that does and does not mean for older ones.

The game must run with `Blocks5\` as its working directory, because it opens `data.zip`
relative to the current directory:

    cd Blocks5
    ..\Release\blocks5.exe -windowed

`Build.bat /run` does exactly that for you. It has to come last on the command line, because
everything after it is handed to `blocks5.exe` untouched — that is what keeps the game's own
switches from colliding with Build.bat's:

    Build.bat /run -windowed
    Build.bat Debug /rebuild /run -fullscreen

To build from the IDE instead, open `Blocks5.sln`, build all three projects, and run
`zip_data.bat` and `zip_skins.bat` in `Blocks5\` once.


Building for the browser
------------------------
`WebBuild/` holds an Emscripten port that runs the game in a browser. `WebBuild/build.sh`
builds it from a clean clone with nothing but the Emscripten SDK; `WebBuild/README.md`
describes what works, what is left out, and the shims it needs.


Layout
------
    ROADMAP.md      planned work and what stands in the way of each item
    Blocks5/        the game: sources in src/, assets in data/, levels and skins in levels/
    PWEncrypt/      CLI that encrypts an archive password into the bracket form used in paths
    ShowUserDir/    opens the user data folder in Explorer
    WebBuild/       the Emscripten port and its glue

The window is resizable, keeps its 4:3 shape with black bars, and toggles borderless
fullscreen with Alt+Return — none of which loses the GL context, because SDL's video flags
never change and fullscreen is a Win32 style flip behind its back. The scaling filter
(`sharp-fit`, `nearest`, `bilinear`) is an in-game option under Options → Scaling, saved
alongside the window size and fullscreen state in `config.xml`.

The game needs no Visual C++ redistributable, no system-wide OpenAL and no codec pack: the
three executables link the CRT statically, SDL 1.2.15 is compiled in from
`Blocks5/libs/SDL-1.2.15/src`, video recording writes H.264 and MP3 into an MP4 that Windows
plays out of the box, and the only DLL that ships is `Blocks5/OpenAL32.dll` — OpenAL Soft,
whose only CRT import is `msvcrt.dll`, part of Windows, rather than a versioned runtime that
would need a redistributable. Every vendored library under `Blocks5/libs` has a
`PROVENANCE.txt` saying where it came from, what was compared against upstream, which of its
files are compiled, what was changed locally and why, and how to update it.

The Inno Setup installer script is `Blocks5/setup/Blocks 5.iss`. Its version number has to
stay in sync with `p_localVersion` in `Blocks5/src/main.cpp`, with `Blocks5/readme.txt`, and
with `FILEVERSION`/`PRODUCTVERSION` in `Blocks5/src/resources.rc`.

Saves, progress, custom levels and screenshots live under `My Documents\Blocks 5\`, never
next to the executable.
