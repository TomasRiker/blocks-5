Blocks 5 - Bob's Amazing Adventures
===================================

A 2D tile-based puzzle/action game in C++, on SDL 1.2, OpenGL and OpenAL Soft.


Building on Windows
-------------------
Run `Build.bat` from the repository root. It locates MSBuild, builds all three projects for
`Win32`, and then packs `data.zip` and `levels/skins/*.zip` — build products that are not in
Git and that the game cannot start without.

    Build.bat                     Release, default toolset (v143)
    Build.bat Debug
    Build.bat /toolset:v120       build with the original Visual Studio 2013 toolset
    Build.bat /stage              also assemble a redistributable tree in Blocks5\stage
    Build.bat /clean              delete every build product again, and exit
    Build.bat /?                  all options

For a compiler-only setup, "Build Tools for Visual Studio 2022" is enough; no IDE is needed.
The toolset notes at the top of `Build.bat` explain what each version requires.

The game must run with `Blocks5\` as its working directory, because it opens `data.zip`
relative to the current directory:

    cd Blocks5
    ..\Release\blocks5.exe -windowed

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

The game needs no Visual C++ redistributable, no system-wide OpenAL and no codec pack: the
three executables link the CRT statically, SDL 1.2.15 is compiled in from
`Blocks5/libs/SDL-1.2.15/src`, video recording writes H.264 and MP3 into an MP4 that Windows
plays out of the box, and the only DLL that ships is `Blocks5/OpenAL32.dll` — OpenAL Soft,
whose only CRT import is `msvcrt.dll`, part of Windows, rather than a versioned runtime that
would need a redistributable. Every vendored library under `Blocks5/libs` has a
`PROVENANCE.txt` saying where it came from, what was compared against upstream, which of its
files are compiled, what was changed locally and why, and how to update it.

The Inno Setup installer script is `Blocks5/setup/Blocks 5.iss`. Its version number has to
stay in sync with `p_localVersion` in `Blocks5/src/main.cpp` and with `Blocks5/readme.txt`.

Saves, progress, custom levels and screenshots live under `My Documents\Blocks 5\`, never
next to the executable.
