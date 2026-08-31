 ____  _            _          _____
|  _ \| |          | |        | ____|
| |_) | | ___   ___| | _____  | |__
|  _ <| |/ _ \ / __| |/ / __| |___ \
| |_) | | (_) | (__|   <\__ \  ___) |
|____/|_|\___/ \___|_|\_\___/ |____/

== Bob's Amazing Adventures == v1.2.0

    by David Scherfgen
        Website ... https://www.david-scherfgen.de/meine-spiele/blocks-5/
        E-Mail .... d.scherfgen@googlemail.com


Where is the Help?
==================
The actual game help is in the game itself. In the main menu, you'll find a
"Help" button. You can also access the help from the in-game menu. Probably, you
won't need the help if you just play the game, because you learn all important
things from the hints that can be found in many levels. However, if you want to
make your own levels, having a look at the help is probably a good idea.


Command Line Options
====================
Blocks 5 understands three options. Upper and lower case do not matter.

    -windowed      Start in a window.
    -fullscreen    Start in full screen.
    -nosplash      Skip the logo and the jingle and go straight to the main
                   menu.

The first two do the same as picking the window or full screen inside the game,
and the choice is remembered: the next start uses it again, with or without the
option. "windowed.bat" next to the game is a ready-made shortcut for the first
one.

While playing you can switch between the two at any time with Alt+Return.


Changelog
=========
1.2.0 ... - Blocks 5 can now be played in a web browser, with no installation at
            all. Your progress, your own levels and your campaigns are stored by
            the browser, so they survive closing the tab. Recording videos and
            taking screenshots are the only things the browser version cannot
            do. Look on the website for the link.

          - New in the main menu: an Import button and an Export button, on both
            the browser version and this one.

            Import takes one file and works out for itself what it is - a level,
            a campaign, a piece of music or a skin - and puts it where it
            belongs. Export asks what kind of thing you want, lists what you
            have of it, and hands you a copy.

            Music can be brought in this way for the first time. A level can
            also borrow one of the game's own pieces by writing "blocks:" in
            front of the name, as in blocks:music2.ogg. A campaign built that
            way stays small, because that music is already installed.

          - The game window can be resized now. Drag its edge to any size you
            like; the picture keeps its shape and gets black bars where the
            window does not match. It keeps drawing while you drag, where it
            used to freeze until you let go, and it can no longer be dragged
            smaller than the 640x480 the game draws.

            Alt+Return switches between the window and full screen at any time.
            The game remembers which of the two you left it in, where the window
            was and how big - and whether it was maximized, which it used to
            forget, coming back half off the screen.

            A fresh installation no longer starts at a tiny 640x480. The window
            opens at the largest whole multiple of that which still leaves room
            for the taskbar - twice the size on a Full HD screen, four times on
            a 4K one.

            Full screen no longer changes the screen resolution. It is a
            borderless window the size of the desktop, which is what most games
            do these days: Alt+Tab is instant and does not rearrange your other
            windows.

            The mouse pointer is twice as big. It used to be drawn by Windows at
            a fixed size while everything around it grew with the window, which
            left it looking tiny.

            Screenshots and recorded videos are unaffected by any of this. They
            are always the clean 640x480 picture, without the scaling and
            without the bars.

          - The HQ2X start menu entry is gone, and with it the HQ2X mode. It
            scaled the picture on the processor, cost about half of the time
            available for a frame, and changed less than 5% of the pixels. In
            its place the options now have a "Scaling" setting with four
            choices:

              Sharp, fitted  crisp pixels at any window size. This is the new
                             default
              Sharp          every pixel exactly the same size, so the picture
                             only grows in whole steps and does not fill the
                             window
              Smooth         plain stretching, blurry
              CRT monitor    the sort of screen the game was written for: a
                             curved glass tube with a phosphor mask, a glow
                             around bright things and scan lines

            "CRT settings ..." beside the list has five sliders: the scan lines,
            the curvature of the screen, the glow around bright areas, and two
            kinds of flicker: an unsteady brightness, and the scan lines
            drifting slowly down the picture the way they never quite stood
            still on a real set. Each can be turned all the way down.

            "Sharp, fitted" and "CRT monitor" need a graphics card that can run
            shaders, which means anything made since about 2005. If yours
            cannot, they are not offered and the game uses "Sharp".

          - Video recording now records the game's own sound. Until now it
            recorded whatever Windows had selected as the recording device,
            which on most machines is the microphone. The "Stereo Mix" / "What
            you hear" setup described under version 1.1.0 below is no longer
            needed.

            The recordings are MP4 files now, with H.264 video and MP3 sound,
            instead of AVI. They play in Windows Media Player, in the Photos app
            and in any browser without installing a codec pack.

          - The game now starts in the language your system is set to, instead
            of always English. You can still change it in the options; that
            choice always wins.

          - Escape in the main menu quits the game. It also closes the level
            editor's menu and its settings, the options and the help, and Return
            confirms the settings and the options - the same way clicking OK or
            Cancel does.

          - Alt+F4 now closes the game, stopping a running video recording
            properly on the way out.

          - Clicking the text next to a checkbox or a radio button now works the
            same as clicking the box itself - and so does clicking either of the
            two language flags in the options. Clicking the caption of a text
            field puts the cursor into it.

          - Fixed: a hint note could be seen for a fraction of a second at the
            wrong place before unfolding, usually when stepping onto the same
            note a second time.

          - Fixed: a cannon that was turning when you saved came back pointing
            the wrong way after loading.

          - Fixed: in the level editor, switching the electricity on or off
            could not be undone, and undoing it threw away everything that could
            be redone.

          - The installer no longer has to install the Visual C++ runtime or
            OpenAL. The game brings everything it needs, so the download is
            smaller and there are fewer steps that can go wrong. It also always
            uses its own copy of OpenAL Soft rather than whatever OpenAL happens
            to be installed on the computer, which should fix sound problems on
            machines with an old OpenAL installation.

          - Under the hood: every third-party library is now built from source
            with a current compiler, and twelve DLLs have left the game folder.

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