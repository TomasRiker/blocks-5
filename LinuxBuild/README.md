# The Linux build

Blocks 5 runs natively under Linux. It is the same source as under Windows and
in the browser - the difference sits in `#ifdef` branches in eight of the
game's `.cpp` files and in this one translation unit here.

## Building

    sudo apt install build-essential libsdl1.2-dev libopenal-dev \
                     libglu1-mesa-dev libgl1-mesa-dev

    LinuxBuild/build.sh              incremental
    LinuxBuild/build.sh clean        from scratch
    LinuxBuild/build.sh run          build and start

Everything else - zlib, minizip, libogg, libvorbis, TinyXML, stb, minih264,
shine, minimp4 - comes out of `Blocks5/libs`, exactly as in the Windows and the
browser build. All three therefore compile the same code.

The game has to run out of `Blocks5/`, because it opens `data.zip` relative to
the working directory. `build.sh run` does that; by hand:

    cd Blocks5 && ../LinuxBuild/build/blocks5 -windowed

`data.zip` and `levels/skins/*.zip` are build products and are not in Git;
without them the game does not start. `Blocks5/pack.sh` builds them -
zip_data.bat and zip_skins.bat in one script, with the distribution's 7za and
optipng instead of the ones in `tools\`. 7za and not the more obvious `zip`:
Info-ZIP writes an encrypted entry differently - bit 3 of the general purpose
flags, the data descriptor, and the time of day rather than the CRC for the
check byte of the encryption header - and although the game reads either form,
the archive that comes out here should be the one the Windows build produces:

    sudo apt install p7zip-full optipng
    Blocks5/pack.sh                 everything
    Blocks5/pack.sh data            data.zip only
    Blocks5/pack.sh --no-optipng    without the slow step

**SDL 1.2 today is sdl12-compat**: Debian, Ubuntu and Fedora ship the
reimplementation of the 1.2 interface on top of SDL 2 under `libsdl1.2-dev`.
That is exactly what a player gets, and that is what this is tested against.
The real SDL 1.2.15 does lie in `Blocks5/libs`, but only with the Win32 part
that the Visual Studio project compiles.

## What is in here

    build.sh            the build
    linux_window.cpp    the fullscreen switch and the fixed window size
    linux_window.h      their interface, with no Xlib in it
    test/harness.sh     start the game and drive it by element names
    test/smoke.sh       one round through the GUI

`linux_window.cpp` is a file of its own for one reason: `<X11/Xlib.h>` makes
`Font`, `Window`, `Screen` and `Cursor` type names of its own, and the game has
classes called the same. Included in `engine.cpp`, the next line with a `Font*`
in it no longer compiles. Everything Xlib-specific therefore stays in here.

## What is different from Windows

- **The user directory** is `$XDG_DATA_HOME/blocks5/`, failing that
  `~/.local/share/blocks5/`, instead of `My Documents\Blocks 5\`.

- **Fullscreen** goes through the window manager. Under Windows the game sets
  the window style to `WS_POPUP` and the size to the screen; under X11 a
  program does not put its own window into fullscreen - it tells the window
  manager that it wants one, with a `_NET_WM_STATE` message (EWMH). The
  manager decides on size and position, sends a ConfigureNotify, and SDL turns
  that into an `SDL_VIDEORESIZE` that `handleResize()` picks up - the same way
  as dragging the window border. SDL's flags are touched as little as under
  Windows: an `SDL_FULLSCREEN` would have `X11_SetVideoMode` rebuild the window
  and take the GL context with it.

- **The file dialog** is `zenity` or `kdialog`, whichever is installed. That
  saves GTK and Qt as a dependency. With neither there the Import button does
  nothing and one line in the log says why.

- **The import runs alongside**: `popen()` gives a pipe that `pollImport()`
  reads tick by tick, keeping the window drawing while the dialog is open. The
  export cannot do that - `doExport()` delivers its result at once, which is
  how `transfer.h` declares it - and therefore stops the game like the modal
  dialog under Windows.

- **The update check** calls `curl` or `wget` instead of bringing an HTTPS
  client of its own. It is off as shipped (`.update_checker` in the user
  directory) and, when it finds something, says so only in the log: the engine
  is not running yet at that point, and there is therefore neither a toast bar
  nor a dialog.

- **No crash handler.** The one under Windows is SEH, and that does not exist
  here.

- **The audio capture for videos** goes through PulseAudio instead of WASAPI:
  `@DEFAULT_MONITOR@` is the source that listens in on what the default sink is
  putting out. PipeWire does just as well with `pipewire-pulse`. libpulse is
  loaded at runtime rather than linked against - the build needs no
  libpulse-dev, and where no PulseAudio runs the videos stay silent as before.

## Upper and lower case

The game looks files up by name, and under Linux the case has to match - inside
`data.zip` as much as outside it. The archive lookup hands `unzLocateFile` a
case sensitivity of 0, which means whatever the operating system does: `strcmp`
here, a case-insensitive compare under Windows. Measured against the shipped
`data.zip`, `buttons.png` is found and `Buttons.png` is not. And the loose
files - the player's own levels, skins, and the development mode with
`fs.pushCurrentDir("data")` - lie on a filesystem that tells `Sprites.png` and
`sprites.png` apart, where Windows does not. Anyone building under WSL should
do it out of the ext4 filesystem (`~`) and not from `/mnt/c`: DrvFs is
case-insensitive by default and hides exactly these mistakes.

## Testing

    LinuxBuild/build.sh hooks && LinuxBuild/test/smoke.sh

Starts Xvfb and openbox, runs the game inside them, clicks through menu,
options, the CRT sliders and the Manager, starts a level and checks Escape
there, switches into fullscreen and back, triggers a screenshot and quits
through Escape.

`B5_SHOTS` says where the images go (default `/tmp/blocks5-smoke`). **That
directory is deleted and created afresh at the start** - so name one of your
own, and not a directory with anything else in it.

Clicks go to element names and not to coordinates. The test hook from
`Blocks5/src/testhooks.cpp` - the same one the browser uses - puts the GUI tree
with every element's window coordinates down as JSON and answers who would get
a click on a point. Because the browser has JavaScript for that and there is no
such channel here, the request goes in a file: the test writes
`$B5_TEST_DIR/request`, the game reads it once per logic tick and puts the
answer down beside it.

What that is worth shows in the test's first click: on the very first start
`Menu.CrtPane` covers everything, and a click on the middle of `Menu.Options`
lands on the pane. A screenshot does not show that.

**`xwininfo` says where the cursor goes, not `xdotool`.** Under openbox the
game's window sits in a frame, and `xdotool getwindowgeometry` reports that
frame's corner; the title bar therefore shifts every click down by its own
height. On the finger-sized buttons of the main menu that never shows -
measured ten game pixels, and the buttons are eighty high. On the 18-pixel-high
*CRT settings ...* every click missed, with no error message: the hit query
reckons in game coordinates and was satisfied, only the mouse stood somewhere
else. `xwininfo -id` names the absolute corner of the content itself, and after
that it is right to a pixel.

And two things about keys that want the exact opposite of each other: what the
GUI reads (Escape, Alt+Return) comes as an SDL event and has to be tapped -
`SDL_EnableKeyRepeat(140, 60)` turns a held Escape into six. What is bound to a
named action (F11 and the rest) is read by `Engine::updateVKs` with
`SDL_GetKeyState`, a snapshot taken once per logic tick, and has to be held.
`b5_key` and `b5_hold` in `harness.sh`.
