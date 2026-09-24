 ____  _            _          _____
|  _ \| |          | |        | ____|
| |_) | | ___   ___| | _____  | |__
|  _ <| |/ _ \ / __| |/ / __| |___ \
| |_) | | (_) | (__|   <\__ \  ___) |
|____/|_|\___/ \___|_|\_\___/ |____/

== Bob's Amazing Adventures == v1.2.0

    by David Scherfgen
        Website ... https://www.david-scherfgen.de/meine-spiele/blocks-5/
         E-Mail ... d.scherfgen@googlemail.com


Where is the Help?
==================
The actual game help is in the game itself. In the main menu, you'll find a
"Help" button. You can also access the help from the in-game menu. Probably, you
won't need the help if you just play the game, because you learn all important
things from the hints that can be found in many levels. However, if you want to
make your own levels, having a look at the help is probably a good idea.


Command Line Options
====================
Blocks 5 understands five options. Upper and lower case do not matter.

    -windowed      Start in a window.
    -fullscreen    Start in full screen.
    -nosplash      Skip the logo and the jingle and go straight to the main
                   menu.
    -perf          Show in the bottom corner what the last few hundred frames
                   cost, which is a diagnostic and not a feature. In a browser
                   the same thing is reached by adding ?perf=1 to the address.
                   Ctrl+Shift+F9 then writes the texture atlas pages into the
                   screenshots folder, one PNG each.
    -flushall      Draw every quad on its own instead of collecting a whole
                   render pass into one call. Slower, and only of interest if
                   a graphics driver draws the collected form wrongly. In a
                   browser: ?flushall=1.

While playing you can switch between full screen and windowed mode at any time
with Alt+Enter.


Licenses
========
Blocks 5 is free software: you can use it, study it, change it and pass it on,
under the terms of the GNU General Public License, version 3. The full text is
in LICENSE.txt. The complete source code - the game, every library it is built
from, and the scripts that turn one into the other - is at

    https://github.com/TomasRiker/blocks-5

Three of those libraries are covered by the GNU Lesser (or Library) General
Public License, and this is the notice that license asks for. You may modify
any of them and relink Blocks 5 against your own version; everything you need
in order to do that is at the address above, each library in its own folder
under Blocks5/libs with its own COPYING file.

    SDL 1.2.15     LGPL 2.1   compiled into blocks5.exe
    shine          LGPL 2     compiled into blocks5.exe
    OpenAL Soft    LGPL 2     shipped beside it, as OpenAL32.dll

The others are used under permissive licenses, and are named here with thanks
to the people who wrote them: zlib and TinyXML (zlib license), libogg and
libvorbis (BSD), the Mersenne Twister (BSD), and minih264, minimp4, stb_image
and sigslot (public domain).


Changelog
=========
1.2.0 ... - Blocks 5 now also runs in a web browser, with nothing to install -
            on a phone or tablet too, with an on-screen pad, and as an app on
            the home screen. Your progress and your own files are kept by the
            browser. A native Linux version can be built from the source code.

          - "Manage files" in the main menu imports, exports and deletes levels,
            campaigns, music, skins and your progress. Import works out by
            itself what a file is, and single levels can now be played straight
            from the level selection.

          - The window can be resized freely and keeps the picture's shape.
            Alt+Enter switches to full screen without changing the screen
            resolution, and the game remembers how you left it. The new
            "Scaling" option replaces HQ2X: sharp or smooth pixels, or a CRT
            monitor with curved glass, scan lines and glow, each to your taste.

          - Drawing is much faster, above all in the browser and on phones. The
            game now needs a graphics card that can run shaders - anything made
            since about 2005 - and says so at the start if there is none; on
            Windows that usually means a graphics driver is missing.

          - Drag a character with the mouse to walk it somewhere, and click
            something it stands next to, such as a switch, to work it. Collected
            items fly to whoever took them, switches light up when thrown, hint
            notes are sheets of paper that unroll with a rustle, and restarting
            a level under the CRT filter rewinds the tape.

          - The help and all messages name the keys you actually chose. In the
            level editor, Ctrl+Z now undoes and Ctrl+Y redoes on any keyboard.

          - Screenshots are PNG files, in the browser too. Videos are MP4 files
            and record the game's own sound; the Stereo Mix setup under 1.1.0 is
            no longer needed.

          - The game starts in the language of your system, and the credits can
            be watched from the main menu.

          - The installer needs neither the Visual C++ runtime nor OpenAL any
            more and works without administrator rights. The game brings its own
            OpenAL Soft, which should end the sound problems some machines had.

          - Well over a hundred fixes. Damaged or foreign levels, campaigns,
            skins and music no longer crash or freeze the game, and a missing
            skin or piece of music is reported instead of passing in silence.

1.1.2 ... - Joystick hats can now be used to play the game.

          - Recompiled all libraries with Visual C++ 2013.

1.1.1 ... - The default key for screenshots is now F11.

          - Added default secondary keys for some actions.

          - Added a "Donate" button in the main menu.

1.1.0 ... - The game now stores levels and user content in the user's "My
            Documents" directory. Therefore, the game can be run without
            administrator privileges.

          - In-game videos can now be recorded with the F12 key. Press it again
            to stop recording. The videos are recorded to the new "videos"
            directory. You find it inside the new directory mentioned above.
            Important: first choose the correct recording source in the Windows
            sound settings as the default recording source (should be named
            "Stereo Mix" or "What you hear").

          - Keys for mute, screenshot and video recording can now be configured
            in the options dialog.

          - Improved drawing performances of lasers and light barriers. Due to
            some strange ATI driver bug, this could get very slow before.

          - Added a "Quit" button to the in-game menu and the editor menu for
            better usability.

          - Improved the GUI behavior for better usability. For example,
            dragging scrollbars is now easier because the cursor is now allowed
            to leave the scrollbar while dragging.

          - Added numpad keys as default secondary keys for movement.

          - Fixed a bug where the game would crash when one character dies and
            another one is standing on a hint object.

          - Fixed a bug with screenshots not working on some machines.

          - Changed the naming of screenshots to include the current date and
            time.

          - General performance optimizations.

          - Re-colored the title screen.

          - Changed installer to ask for automatic updates.

          - Changed installer to ask before installing Visual C++ runtime and
            OpenAL.

          - Updated all libraries to their latest versions.

1.0.7 ... - Automatic update checking can now be disabled.

          - Changed level 3, which was too difficult.

1.0.6 ... - The game should now run smoothly on an ATI graphics card under
            Windows Vista.

          - Overall performance optimizations.

1.0.5 ... - Key presses are now buffered, which makes controlling the player
            easier.

          - Fixed a bug where hint texts would sometimes disappear or be
            displayed incorrectly.

          - Fixed a potential crash.

          - The -audioDeviceID command line argument now works correctly.

          - Some performance improvements.

          - Added this file.

1.0.4 ... - ???

1.0.3 ... - Made the controls configurable (primary and secondary keys for all
            actions).

          - Joysticks and gamepads can be used now.

          - Changed the way the player moves.

          - Added a hotel in level 9.

          - Changed some key shortcuts due to the new input system.

          - Empty campaigns can't be saved any more.

1.0.2 ... - Added a text in the level selection screen that tells the player
            which levels can be played next.

          - Changed the application item.

1.0.1 ... - Fixed a bug where bombs would disappear.

          - The game now checks for new versions at startup.

1.0 ..... - Initial release.