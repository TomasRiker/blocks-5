 ____  _            _          _____
|  _ \| |          | |        | ____|
| |_) | | ___   ___| | _____  | |__
|  _ <| |/ _ \ / __| |/ / __| |___ \
| |_) | | (_) | (__|   <\__ \  ___) |
|____/|_|\___/ \___|_|\_\___/ |____/

== Bob's Amazing Adventures == v1.2.0

    by David Scherfgen
        Website ... http://www.scherfgen-software.net/blocks5/
        E-Mail .... d.scherfgen@googlemail.com


Where is the Help?
==================
The actual game help is in the game itself.
In the main menu, you'll find a "Help" button. You can also access
the help from the in-game menu. Probably, you won't need the help
if you just play the game, because you learn all important things
from the hints that can be found in many levels.
However, if you want to make your own levels, having a look at the
help is probably a good idea.


Changelog
=========
1.2.0 ... - Blocks 5 can now be played in a web browser, with no
            installation at all. Your progress, your own levels and
            your campaigns are stored by the browser, so they
            survive closing the tab. Recording videos and taking
            screenshots are the only things the browser version
            cannot do. Look on the website for the link.

          - Video recording now records the game's own sound.
            Until now it recorded whatever Windows had selected as
            the recording device, which on most machines is the
            microphone. The "Stereo Mix" / "What you hear" setup
            described under version 1.1.0 below is no longer needed.

          - Recorded videos are now MP4 files, with H.264 video and
            MP3 sound, instead of AVI. They play in Windows Media
            Player, in the Photos app and in any browser without
            installing a codec pack.

          - The installer no longer has to install the Visual C++
            runtime or OpenAL. The game brings everything it needs,
            so the download is smaller and there are fewer steps
            that can go wrong.

          - The game now always uses its own copy of OpenAL Soft
            rather than whatever OpenAL happens to be installed on
            the computer. This should fix sound problems on systems
            with an old OpenAL installation.

          - Under the hood: every third-party library is now built
            from source with a current compiler, and twelve DLLs
            have left the game folder.

1.1.2 ... - Joystick hats can now be used to play the game.

          - Recompiled all libraries with Visual C++ 2013.

1.1.1 ... - The default key for screenshots is now F11.

          - Added default secondary keys for some actions.

          - Added a "Donate" button in the main menu.

1.1.0 ... - The game now stores levels and user content in the
            user's "My Documents" directory. Therefore, the game can
            be run without administrator privileges.

          - In-game videos can now be recorded with the F12 key.
            Press it again to stop recording. The videos are recorded
            to the new "videos" directory. You find it inside the
            new directory mentioned above.
            Important: first choose the correct recording source in the
            Windows sound settings as the default recording source
            (should be named "Stereo Mix" or "What you hear").

          - Keys for mute, screenshot and video recording can now be
            configured in the options dialog.

          - Improved drawing performances of lasers and light barriers.
            Due to some strange ATI driver bug, this could get very slow
            before.

          - Added a "Quit" button to the in-game menu and the editor
            menu for better usability.

          - Improved the GUI behavior for better usability.
            For example, dragging scrollbars is now easier because
            the cursor is now allowed to leave the scrollbar while
            dragging.

          - Added numpad keys as default secondary keys for movement.

          - Fixed a bug where the game would crash when one character
            dies and another one is standing on a hint object.

          - Fixed a bug with screenshots not working on some machines.

          - Changed the naming of screenshots to include the current
            date and time.

          - General performance optimizations.

          - Re-colored the title screen.

          - Changed installer to ask for automatic updates.

          - Changed installer to ask before installing
            Visual C++ runtime and OpenAL.

          - Updated all libraries to their latest versions.

1.0.7 ... - Automatic update checking can now be disabled.

          - Changed level 3, which was too difficult.

1.0.6 ... - The game should now run smoothly on an ATI graphics card
            under Windows Vista.

          - Overall performance optimizations.

1.0.5 ... - Key presses are now buffered, which makes controlling
            the player easier.

          - Fixed a bug where hint texts would sometimes disappear
            or be displayed incorrectly.

          - Fixed a potential crash.

          - The -audioDeviceID command line argument now works
            correctly.

          - Some performance improvements.

          - Added this file.

1.0.4 ... - ???

1.0.3 ... - Made the controls configurable (primary and secondary
            keys for all actions).

          - Joysticks and gamepads can be used now.

          - Changed the way the player moves.

          - Added a hotel in level 9.

          - Changed some key shortcuts due to the new input system.

          - Empty campaigns can't be saved any more.

1.0.2 ... - Added a text in the level selection screen that tells
            the player which levels can be played next.

          - Changed the application item.

1.0.1 ... - Fixed a bug where bombs would disappear.

          - The game now checks for new versions at startup.

1.0 ..... - Initial release.