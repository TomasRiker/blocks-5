Blocks 5 - Bob's Amazing Adventures
===================================

A 2D tile-based puzzle/action game in C++, on SDL 1.2, OpenGL and OpenAL Soft.


Building on Windows
-------------------
Run `Build.bat` from the repository root. It locates MSBuild, builds all three projects for
`Win32`, and then packs `data.zip`, `levels/skins/*.zip` and `levels/campaigns/blocks.zip` —
build products that are not in Git and that the game cannot start without.

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

Two of these are there to double-click from Explorer, so that building and playing needs no
command prompt at all:

    Build and run.bat             Build.bat /run
    Build and run -nosplash.bat   Build.bat /run -nosplash

The second one skips the logo and the jingle, which is what you want when you are starting the
game for the twentieth time that afternoon.

To build from the IDE instead, open `Blocks5.sln`, build all three projects, and run
`zip_data.bat`, `zip_skins.bat` and `zip_campaign.bat` in `Blocks5\` once.


Building on Linux
-----------------
`LinuxBuild/build.sh` builds a native Linux binary. It needs a compiler, SDL 1.2, OpenAL,
OpenGL and GLU; everything else comes from `Blocks5/libs`, the same vendored sources the
other two builds use.

    sudo apt install build-essential libsdl1.2-dev libopenal-dev \
                     libglu1-mesa-dev libgl1-mesa-dev

    LinuxBuild/build.sh              incremental
    LinuxBuild/build.sh clean        from scratch
    LinuxBuild/build.sh run          build, then run from Blocks5/

`libsdl1.2-dev` is sdl12-compat on every current distribution — the 1.2 API reimplemented on
SDL 2 — which is what a player gets and what this is tested against.

`data.zip`, `levels/skins/*.zip` and `levels/campaigns/blocks.zip` are build products that
are not in Git, and the game will not start without them. `Blocks5/pack.sh` builds them here —
it is `zip_data.bat`, `zip_skins.bat` and `zip_campaign.bat` in one script, with the
distribution's `7za` and `optipng` (`7za` and not `zip`, so that the same tool writes the
same archive as under Windows):

    sudo apt install p7zip-full optipng
    Blocks5/pack.sh                 everything
    Blocks5/pack.sh --optipng       and squeeze the PNGs first

The port is the same source as the Windows build: eight `#ifdef` branches, plus one
translation unit for the fullscreen switch, which goes through the window manager
(`_NET_WM_STATE`) rather than through SDL. `LinuxBuild/README.md` describes what differs —
the user directory, the file dialog, the update check, video recording without sound — and
`LinuxBuild/test/smoke.sh` drives the built game under Xvfb.


Building for the browser
------------------------
`WebBuild/` holds an Emscripten port that runs the game in a browser. `WebBuild/build.sh`
builds it from a clean clone with nothing but the Emscripten SDK; `WebBuild/README.md`
describes what works, what is left out, and the shims it needs.


Checks
------
There is no unit test suite, but two things do run on every change.

`python3 Tools/verify.py` is a set of static checks over the tree: element paths that no
dialog XML knows, `$IDs` missing from `languages.txt`, XML attributes written and never
read, a source file missing from the Visual Studio project, the version number drifting
apart across the four places it lives, encoding and indentation, and more.
`python3 Tools/selftest.py` proves those checks still bite by injecting each fault in turn.
`sh Tools/syntax.sh` compiles every source with mingw's `-fsyntax-only`, which is the only
way to put a compiler over the Windows code from a Linux machine. `Tools/README.md` lists
them one by one.

`WebBuild/build.sh hooks` builds the browser port with a small read-only introspection hook
(`WebBuild/test_hooks.cpp`, compiled out of the shipped build), which lets
`WebBuild/test/smoke.js` drive the real game through its menus by element name rather than
by guessed pixel coordinates. See `WebBuild/test/README.md`.

`LinuxBuild/test/smoke.sh` does the same for the native build, under Xvfb and openbox, with
the same hook (`Blocks5/src/testhooks.cpp`, compiled in by `LinuxBuild/build.sh hooks` and
answering through a file, since there is no JavaScript to call it): the menus, the options
dialog and the manager open, a played level starts and its menu opens, Alt+Return reaches
fullscreen and comes back, F11 writes a screenshot, and quitting writes `config.xml`.
`LinuxBuild/test/frames.sh` renders twenty named scenes as byte-reproducible PNGs, which is the
oracle a rendering change is checked against.


Layout
------
    CLAUDE.md       how the tree is built, checked and put together; the orientation
    .claude/rules/  the detail, one file per area, loaded when a file of that area is read
    ROADMAP.md      planned work and what stands in the way of each item
    FINDINGS.md     the review findings of the 1.2.0 overhaul and what became of each
    SDL3-MIGRATION.md  a plan for moving off SDL 1.2, surveyed and deliberately not started
    Tools/          the static checks (verify.py, selftest.py, syntax.sh) and the generators
    Blocks5/        the game: sources in src/, assets in data/, levels and skins in levels/
    PWEncrypt/      CLI that encrypts an archive password into the bracket form used in paths
    ShowUserDir/    opens the user data folder in Explorer
    WebBuild/       the Emscripten port and its glue
    LinuxBuild/     the native Linux build and the one file that needs Xlib

The window is resizable, keeps its 4:3 shape with black bars, and toggles borderless
fullscreen with Alt+Return — none of which loses the GL context, because SDL's video flags
never change and fullscreen goes behind SDL's back: a Win32 style flip on Windows, an EWMH
request to the window manager on Linux. The scaling filter
(`SharpFit`, `Sharp`, `Smooth`, `Crt`) is an in-game option under Options → Scaling, saved
alongside the window size and fullscreen state in `config.xml`.

The game needs no Visual C++ redistributable, no system-wide OpenAL and no codec pack: the
three executables link the CRT statically, SDL 1.2.15 is compiled in from
`Blocks5/libs/sdl-1.2.15/src`, video recording writes H.264 and MP3 into an MP4 that Windows
plays out of the box, and the only DLL that ships is `Blocks5/OpenAL32.dll` — OpenAL Soft,
whose only CRT import is `msvcrt.dll`, part of Windows, rather than a versioned runtime that
would need a redistributable. Every vendored library under `Blocks5/libs` has a
`PROVENANCE.txt` saying where it came from, what was compared against upstream, which of its
files are compiled, what was changed locally and why, and how to update it.

The Inno Setup installer script is `Blocks5/setup/Blocks 5.iss`. Its version number has to
stay in sync with `p_localVersion` in `Blocks5/src/main.cpp`, with `Blocks5/readme.txt`, and
with `FILEVERSION`/`PRODUCTVERSION` in `Blocks5/src/resources.rc`.

Saves, progress, custom levels and screenshots live under `My Documents\Blocks 5\` — on Linux
under `$XDG_DATA_HOME/blocks5/`, or `~/.local/share/blocks5/` — never next to the executable.
