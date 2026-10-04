---
paths:
  - "LinuxBuild/test/**"
  - "WebBuild/test/**"
  - "Blocks5/src/testhooks.*"
  - "WebBuild/test_hooks.cpp"
  - "Tools/testlevels/**"
---

# Driving the game: natively, in a browser and on a phone

## Natively

`LinuxBuild/test/smoke.sh` runs the built game under Xvfb with openbox, clicks through menu, options and
manager, toggles fullscreen, screenshots with F11, quits with Escape. It clicks by element name, not
coordinate: `Blocks5/src/testhooks.cpp` — the same hook the browser uses — reports the GUI tree, and
since there is no JavaScript here the request goes through a file (`$B5_TEST_DIR/request`, answered once
per logic tick). Every request carries a serial and the answer repeats it on its first line, so a late
answer to an ask that had given up is never taken for the current one — which used to read as a click
finding a whole dump "on top" of its button. That catches what a screenshot cannot: on a first start `Menu.CrtPane` covers
everything, so a click on the middle of `Menu.Options` lands on the pane.

**The two input layers want opposite treatment — the trap that costs the most time.** A key the GUI reads
is an SDL *event* and must be tapped, not held: `SDL_EnableKeyRepeat(140, 60)` turns an Escape held for
400 ms into six, the first closing the dialog and the second quitting. A key bound to a named *action*
must be held, not tapped, because `Engine::updateVKs` reads `SDL_GetKeyState`, a snapshot taken once per
20 ms tick, so a press and release in the same millisecond is never seen. The same sampling rule governs
mouse and touchscreen, which is why `page.mouse.click()` and `page.touchscreen.tap()` are equally
useless: move, settle, hold, release. Alt+Enter misleads, hanging off `SDL_KEYDOWN`, and events queue.

**A tapped key is not an instantaneous one.** `b5_key` holds 60 ms rather than calling `xdotool key`,
which presses and releases in about twelve. `SDL_PollEvent` runs once per rendered frame, and under
llvmpipe a frame is a fifth of a second — so a run landing inside those twelve milliseconds sees the
press and not the release, and at the *next* run SDL's repeat, 140 ms overdue, posts a second key-down
before the release is read. Measured, every fifth press arrived twice; at 60 ms none did, eight times out
of eight. A real machine renders at 60 Hz and a real finger holds far longer, so this is the harness
lying, not the game.

`xdotool windowclose` calls `XDestroyWindow` and SDL trips over a window it still believes is its own.
Quit the way a player does — Escape in the menu — or `Engine::exit()` never runs and `config.xml` is
never written.

**Two harnesses on one display take each other down**, and the wreckage reads as a rendering fault rather
than a collision: `b5_clearDisplay` clears the Xvfb an aborted run left behind, and on the shared default
`:99` it cannot tell that server from a live one. It refuses where a `blocks5` is still attached and
names `B5_DISPLAY`, which is what separates two runs. A frame taken while the server was going down is not
evidence of anything.

**The select screen's campaign list keeps the keyboard focus after a click**, deliberately —
`GUI_ListBox` handles the same four keys — and is a trap for a test that picks a campaign then presses End
for the last level: the press goes to the list, which selects its own last entry and therefore *another
campaign* at level 0, with nothing saying so. There is no list of levels to click instead, so take the
focus off it by clicking any of the six buttons, or drive by element name.

**Two windows open themselves over the menu, and `b5_start` writes both markers away.** The CRT offer
appears on a first start and the donation window once enough time has been played — which a machine that
has run the tests often enough reaches on its own. Both are one-shot, so no test touching the menu is
repeatable while they may appear; `.crt_offered` and `.donation_asked` are written exactly as the game
writes them.

**A request reaches the hook by rename, never by redirection.** The hook polls `request` once a logic tick
and `echo > request` creates the file empty before it writes, so a poll in that gap reads an empty line,
answers with a dump under no serial and deletes the file with the real request in it; `b5_ask` then
waits its five seconds and the scene fails as "could not be written". `b5_ask` writes `request.tmp` and
renames it, as the hook itself publishes `response`, so a poll sees the whole line or no file.

**The harness drives `build-test/`; `LinuxBuild/build.sh` without `hooks` writes `build/`.** Building one
and testing the other is an afternoon's worth of a change that appears to do nothing, so `b5_start`
compares the binary against `Blocks5/src` and LinuxBuild's own sources and `build.sh`, and `data.zip`
against `Blocks5/data`, and refuses to run on either out of date. `Tools/selftest.py` puts each file's
mtime back along with its bytes, or every run would trip that check.

**The game runs under `LC_ALL=C`, and with the update check pointed at nothing.** With no `<Language>` in
`config.xml` the game takes the system's, and a check that reads a caption must not hang on the
developer's locale. And `b5_start` hands every start a `B5_UPDATE_URL` on a port nothing listens on,
unless a script sets its own: a run that switches the check on, or a home whose `config.xml` has it on,
must never ask the website.

## The frame oracle

`LinuxBuild/test/drag.sh` is the one test that reads the *level* rather than the GUI. The mouse gestures
steer a character, so no widget can be asked whether they worked; the hook's `state` therefore reports
`player`, the cell the active character stands on (`[-1, -1]` with no level running), `nightVision`,
which is what a light switch does — of the eight switches the one whose effect is a single bit rather
than something to be recognised in a picture — and `note`, whether a hint note stands open on that cell,
which is drawn and no widget. It exists because the feature shipped once without working
at all — the bindings were registered before `Engine::init` had built the virtual-key table — and
because the obvious check does not work: it rains in level 1, so two frames differ by a million pixels
whether or not anybody walked.

It plays **a level of its own**, written by the script for the same reason `frames.sh` writes its scenes:
the geometry *is* the test. A wall five rows tall and open above and below, so that a leg walking west is
stopped by it while one walking north is not — which is what proves a blocked leg hands over to the
other axis rather than leaning on the wall; a switch beside where the character starts and another three
cells off; and a panel under its feet, which must be walked onto and not clicked. In a shipped level all
of that would be whatever happened to lie near the start, and an assertion about it would be a statement
about level 1. The private `XDG_DATA_HOME` is frames.sh's arrangement exactly: it puts the level first in
the single-levels list and keeps the developer's own levels and progress out of it.

**A second level of its own is the pause and the hint note** (`input.md`): a character one step from a note,
with a switch beside the note and a second character further off, so that a click from the note's field has
something it would work and somebody it would wake, reached with NextLevel after the first. A drag across the
note and a key held across it run past without stopping. Paused - `focusblip` pauses as a return from another
window does - a key, a click on the field, Escape and a press on the character each only end the pause, F11
and Alt leave it, and the Menu button opens the menu; with the note open, Return, Escape, Space and a click
on the field only put it away and the next press acts, an arrow walks off, and the Menu button opens the menu
with the note still behind it. A drag going on walks on when Space or its own second button puts the note
away. Against the code before, fifteen of its checks failed: a click on the other character with the note
open woke it, a click worked the switch under the note and a press on the character dragged it away, Space
did nothing, the key that ended the pause walked off the field as well, F11 and Alt ended the pause, and
Escape opened the menu as it ended it. The character is moved onto and off the note's field by drag rather
than by key, since a key held past the repeat delay by a slow frame takes a second step; the pause's own
checks start off the field, where no open note can be what kept the menu shut; and a drag meant to carry on
over the note sets off from beside it with nothing open, because a press on the character with the note open
only puts the note away.

`LinuxBuild/test/undo.sh` reads the level editor's `undo` and `redo` depths off the dump, the only
place they show: an undo step that changed nothing looks like any other until Ctrl+Z visibly does
nothing, and by then it has cleared the redo list. The tools, keys and dialogs are worked, most of them
once changing the level and once not; which of the two it was, `GS_LevelEditor::endChange` decides from
the level's XML.

**`LinuxBuild/test/update.sh` never asks the website.** A hooks build takes the update check's address
from `B5_UPDATE_URL` — `updatecheck.cpp` is one of the five files `build.sh hooks` compiles with the define —
and the script answers there from a server of its own: the same version, a newer one, garbage, a newer
version made too long by spaces after it, a newer version under an error status, the same version behind
a byte order mark, or nothing until the script says. Each failure is built so that only what it is named
after can refuse it: spaces the parser would skip, a body that would read as a version. So every state of
the menu's version button comes on demand, in both languages, and what it asserts is the dump's:
`updateCheck` (`UpdateCheck::State` by number), a button's `title`, `titleSize` and `flashing`, a toggle's
`checked` and any element's `toolTip`. The versions are the game's own, read out of `main.cpp`, and one
past it. PATH is the other lever: directories of links to everything but curl, wget or both stand for a
machine without them - the one that gets the plain label `Menu.Version` where `Menu.VersionButton` would
be and no box in the options, told apart by the dump's `shown` - a `curl` that execs `sleep` for a check
that never answers, and an `xdg-open` that writes its argument down for the download page being opened.
So curl and wget both have to be installed, and the script says so before it starts. `CURL_HOME` hands
curl a `.curlrc` of the script's that would put the headers in front of every answer - the game's `-q` is
what keeps it out, and it keeps the developer's own out of the run too. The installation's default is a
`.update_checker` beside the game, and a hooks build reads it where `B5_UPDATE_DEFAULT` says, so the
script keeps it in its own folder rather than in the working tree every other harness starts the game
from. A `config.xml` that cannot be written is a symlink into a folder that is not there: not even root
writes through it, and it reads as missing, where a folder of that name would have TinyXML ask `ftell` how
long a directory is. One that takes the write and loses it is a symlink to `/dev/full`, which takes the
open and fails the write at the close, after TinyXML has asked for errors: the old switch must stay there
too. And a switch beside a `config.xml` that says is left over, deleted without being taken in.

Three traps cost a run each. The server is started from a subshell, because `b5_stop` ends in a bare
`wait`, which waits for every job of the shell — a server started with `&` among them, for ever. The menu
comes in through a transition, so the button is photographed only once the dump's `crossfade` is -1;
before that the control shots of a button standing still differed, being shots of the transition. And
the click on the button while it asks raced a server that slept four seconds, on a busy machine landing
after the answer; the server now holds the answer until the script has clicked.

**`LinuxBuild/test/dialogs.sh` runs the Manager's file dialogs through a zenity of its own**, first on
PATH: as the import's dialog it writes down its descriptors and waits until the script hands it a path, as
the export's it writes down its descriptors and arguments and answers with a path or cancels. The import's
dialog runs alongside the game, so an export started while it is open puts a second program in between,
and each must be handed nothing of the game's beyond stdin, stdout and stderr — before the import's pipe
was closed on exec, the export's dialog held its read end. One trap in writing descriptors down: dash
redirects a command's output in the shell itself and keeps the stdout it replaced above 10, so `ls -l
/proc/$$/fd > file` lists the file and that copy as the shell's own; a subshell lists them from outside.
The second start sets `B5_UTF8_NAMES`, which has a hooks build convert file names as Windows does under
its UTF-8 code page (`filesystem.md`; `filesystem.cpp` is the fifth file the define reaches), in a user
directory under a folder with an umlaut: a level named with one is listed by the game's name for it, read
off the dump's `selectedText`, exported into a folder with an umlaut and imported back from there. Against
the build before, the export's dialog held the import's pipe, the list showed *BÃ¤r.xml* and the import
installed *B__r.xml*; with only the dialogs' conversions taken out of `transfer.cpp`, the dialog was
offered a Latin-1 byte, nothing was exported and nothing imported. Both starts also answer the Manager's
questions, read off the dump's `drawnText`: the first imports its level a second time, which asks before
replacing it, and then deletes it, and the second asks to delete *Bär.xml* and answers No. Against the
build before the questions named the file, both deletes asked "Really delete this file?".

**`LinuxBuild/test/record.sh` brings a sound server of its own**: a PulseAudio the script daemonizes with a
null sink as its one output, reached through `PULSE_SERVER` by the game, by `pactl` and by a tone the script
plays into it for the whole run - 440 Hz at a quarter of full scale, beside the menu's music - so that any
moment of a recording has something to hear and a gap would show. `B5_ALSOFT_DRIVERS=pulse` has OpenAL play
into it, where every other script keeps the null output. It counts the game's capture streams with `pactl`
before, during and after two recordings made with F12 (`audio-video.md`), and reads each video back with
`ffmpeg`: the sound has to begin within 150 ms - the MP3 codec alone puts it 20 ms in - run on without a 5 ms
window falling silent, and be as long as the picture. Against the capture opened at every start, the stream
stood open with no video being made and after each one, and four checks failed. Two traps: the tone is played
at a low latency, because a null sink renders in blocks as long as the latency its players ask for, and a
capture opened in the middle of a two-second block waits for the next one; and the server is daemonized
rather than started with `&`, for the reason `update.sh` starts its own from a subshell. Needs pulseaudio,
pactl, pacat and ffmpeg, and says so before it starts.

**`LinuxBuild/test/volume.sh` reads the music's gain off the dump** (`audio-video.md`): `music` names the
track, whether it is the menu's, its fade and the gain OpenAL has for it, which must be the fade times the
slider of its kind - read once the fade-in has ended, so that it is the slider's value itself. It starts from
a `config.xml` as 1.2.0 wrote it, with one music volume, which the menu must take over; then each music slider
has to turn its own kind of track as soon as it moves and leave the other alone, OK has to write both and
Cancel take a moved one back, the level selection and the level editor have to play the menu's music at the
menu's volume and the first level its own at the game's, the mute key has to silence the menu's as well, a
second start has to read both back, and a third, with no `config.xml` at all, has to begin both at 100. A
slider is set by pressing its track, which centres the knob there, and its value is read back off the dump
rather than worked out. One trap: a press on the knob moves nothing, and the knob is a sixth of the track
wide, so a press at a quarter of the track with the knob at 30 leaves it at the 30 the old `config.xml` set,
and a game slider that did nothing would pass; the script notes it when two values it means to tell apart come
out equal. Against six builds that each get one part wrong, every one fails: nine checks with the stream deaf
to its kind, two without the migration, one with the mute key not reaching the menu's volume, eight with the
key not written, ten with the slider not wired, and the third start's two with the menu's default not the
game's.

**`LinuxBuild/test/touch.sh` lends the game a finger**, since Xvfb has none: a hooks build reads
`B5_FINGER`, which makes every press a finger's and gives the game pixels per reference pixel - 1, a reach
of 16 game pixels and a margin of 4 whatever the window's size. The hook answers `touch x y` with what a
finger there would press, at which point and whether that is a move, and `touchsweep x0 y0 x1 y1 step`
with a line a point - what a mouse would hit, what the finger presses and at which point, and what a mouse
would hit at that point - in one tick, where thousands of single asks would cost a tick each. The sweeps
hold every answer against rules that must never break: a hit on a control stays on it, a press on the
level stays on the level, a moved press lands on what it picked, within reach and never behind the pane in
front, and every point of the grid is answered. **A press moved within one element is a move too** - off a
window's body onto its title bar, off the editor's toolbar onto its level - so the rules check the point
as well as the element, and so does `touch`.

What no rule can see is a press left where it should have moved, which breaks none of them, so the real
presses after the sweeps name their cases one by one: a near miss, a wobble, a slide away; a moved press
within an element, under the options' title bar and on the toolbar under the editor's level, the second
landing right across from the finger; a near miss of a greyed-out button, which does nothing rather than
press the one beside it; a slider dragged beside its bar against one dragged on it; a finger landing under
a title bar, which must leave the window where it stood; a tap between two of the Manager's kinds, a third
picked first so that a press moved onto either would show; a press on the editor's level over the undo
button; a stroke started under the level, which must go on along its bottom row - read off the game's own
frame (`shot`), since nothing else reports a tile; a held button that a pane then covers, which must not
fire; and, in a level written by the script, a press under a character on the bottom row, which must take
hold of it. **The first start ends beside the picture**: the window made 1280x720, black bars stand
either side, and a tap in the right one presses nothing, nor does a press on the editor's menu button
slid off into it. The tap is made level with a row where a finger on the picture's last column would press
the button - the picker is asked for one, since on most rows the level above or the palette below is as
near and such a finger presses nothing anyway. A last start without `B5_FINGER` shows the mouse as exact
as before, and then that it was there and pressing: a press of it on the button does open the options.
Which radio button is checked comes from the dump's `checked`, which a radio button reports as a checkbox
does, and a slider's value from its `scroll`.

**A finger lands with its press**, and `pressAt`, like every harness click, moves first and presses after
a rest - so its press tick has no jump in it, and nothing a finger's jump upsets can show there. `landAt`
sends the move and the press in one `xdotool` call, back to back, so the game drains the two together as
it does a touch's. And a key pressed while a mouse button is held must not go through `b5_key`: its
`--clearmodifiers` lets go of held buttons for the length of the key and presses them again after, a
release and a fresh press the game takes for the hand's own, so a button held through the key fires - for
a mouse as much as for a finger.

**A third start drags a list** (`gui-text.md`): the campaign editor's list of levels, made long with 160
copies of a shipped level written into the private home, so that a glide has room to run before it reaches
an end. The dump reports a list's `scroll` in pixels, its `selection`, its `items` and its `lineHeight`,
from which the script works out which item a tap at a point means, and its `changes`, how often the
selection changed: a gesture that selected on the press and put the selection back would end where it
began and show only there. Every drag is checked to the pixel - 100 up scrolls 92, the slop not counted;
past the top and 30 back is 30 - and a tap, a wobble within the slop, a slide sideways, a drag on past the
list's edge, a glide, a finger stopping one and a double tap each by what it selects and where it leaves
the list. **The flick runs in `lockstep`**, and through `xdotool` alone with the release on the last move:
in a frame that runs several ticks only the first sees where the finger is, and a long frame holding the
whole flick would make it one step. The finger that catches the glide presses a tenth of a second after the release,
not after a `b5_mouseAt` and its dump: this screen renders at about eighty frames a second, and a glide was
over before a press that took that long arrived; and the list it stops has to stand short of where the same
flick came to rest uncaught, by half that glide at least, or nothing showed the glide was still going. The
mouse's start shows the other half: its press selects while still held, and dragging it scrolls nothing.
Against the list before it panned, nine of the first twelve checks fail; the tap, the wobble and the double
tap pass, as they should.

Two checks there stand for bugs a review found in the gesture, and failed against it before they were
fixed: a double tap whose first lift and second touch are sent in one `xdotool` call, so that they arrive
in one tick, the lift first, as on a slow frame - it added nothing; and a tap held on the list while
Escape's question whether to quit opens over it - it selected behind the pane. `mobile.js` has the third: a
touch the system cancels never lifts as far as Emscripten's SDL tells the game, and the next touch,
elsewhere, took the list along.

**A frame can be made long by stopping the game.** `kill -STOP` on its process while `xdotool` sends
events, and `-CONT` after: X keeps them, and the next frame's first tick drains them all. So a lift, a
touch and a lift arrive in one tick, as two quick taps on a slow frame, and must be two taps - before the
release-first rule keyed on the button being down before them, the first tap was lost; and a slow drag's
end and its lift arrive in one tick after two seconds, and must fling nothing - measured in ticks, that
was a step of 60 pixels in one and a glide of 262. `gamePid` finds the game itself, which `harness.sh`
starts from a subshell.

**A release that never comes is made by the hook**: `focusblip` takes the focus away and gives it back
while a button is held, which clears the button with no release, as SDL 1.2 under Windows leaves it after
a release in another window; the release X delivers afterwards is the one the game must not act on. The
mouse's start presses the sound volume's track and moves on with the button up for the game - the slider
must not follow - and presses Cancel - which must not fire on the release after. Both failed before the
GUI let go of such a press; the list run's tap whose release went that way selects nothing, and did
before too.

The campaign's description is the multi-line edit box of that run, given twelve lines of seven
characters, so that line *k* begins at character 7*k*: dragged up 100 it scrolls 92 and leaves caret and
selection alone, and a tap at a line's left edge puts the caret at its start. The dump reports such a box's
`scroll` as a pair, its `caret` and its `selected` as character indices, and its `lineHeight`. Two traps
in filling it: `xdotool type` types a newline in its text as nothing the game takes for Return, so the
lines are typed one by one with Return pressed between them; and the box's bottom 16 rows are its
horizontal scroll bar, which takes a press there for itself. With the mouse a drag over the lines still
selects them.

**The touch keyboard is asked for in the dump**, as `touchKeyboard`: what the GUI wants, reported on
every platform, though only Windows has a keyboard to show (`input.md`). The list run taps the list, the
campaign's title, the list again, the title's label and the description, drags the description and
presses its scroll bar, and reads it after each - wanted after a tap on a field or its label, each from
an asserted no, kept on the field's scroll bar, gone after a tap on the list, not asked for by a drag
that took the focus into a field. The finger's run presses a note in the level editor with the modify
tool, which opens the note's editor with the focus in its text: no keyboard, and none until the text is
tapped. The mouse's start shows that its click asks for none. Whether Windows then shows a keyboard is
what no harness here can see.

**The browser's question is asked natively too**: `texttap x0 y0 x1 y1` answers what
`GUI::textFieldTapped` says to a finger that pressed at one point and went no further than the other.
The run asks it about a wobble and a drag on the title, its label and the list, and then about the
description, doubled twice over with the clipboard so that a flick has room to glide: while it glides,
while a finger holds it caught, when the catching finger lands in the tick the flick lifts, and while
the GUI pans it under a held finger the answer is no sheet, and at rest it is the description. The
glide is read around the question - one scroll reading before it and one after, both past the 48 the
finger left it at and growing, or a note says there was no glide to ask about; read straight after the
lift, the first reading is often from before it. The note in the level editor is asked about while the finger holds it:
the editor's text box has opened under the finger, and the answer is still no sheet, the press having
gone to the level. Built without the rules in `textFieldTapped` that consult the gesture and the press,
and with the keyboard kept only while the focused element itself takes text, the glide's, the catch's,
the pan's and the scroll bar's checks fail; built without the release spent on the gesture before, the
one-tick catch's does, and so does the list's check that a finger landing in the tick the last one lifts
drags it - that finger was tapped at once, item 39 selected, and the list did not follow it; and built without
the press deciding, the note's: keyboard wanted, and the sheet opened for the text box under the finger.

`LinuxBuild/test/frames.sh` renders twenty named scenes as 640x480 PNGs meant to be byte-identical
between two runs of one binary, and between two binaries when nothing should have moved. It is what
every rendering change is checked against, so what makes a frame reproducible is worth
knowing before adding one. Three things do it, and every scene needs all three: `B5_SEED` seeds the
generator per rendered frame and per tick, keyed on the scene's clock (`Engine::render`, `Engine::update`,
`seedForLoad`); `freeze <tick>` stops the logic at a named tick of that clock, checked before the tick
runs; `shot <path>` writes what the game read out of its own framebuffer. **The path has to be absolute**,
and the hook refuses any other: the game resolves a relative one through its own file system, against the
mounted `data.zip`, so the picture became a member of the archive - a build product the browser build
ships - while the hook answered ok. `frames.sh` makes its `<outdir>` absolute, and `smoke.sh` asks for a
relative shot and counts the archive's members. **The scene's clock is `Engine::sceneTick`**, which a
level, the credits and the logo screen each set from their own count — the engine's `getTime()` counts
from program start and stands wherever the harness's timing put it, which is exactly what a named tick
must not depend on.

**The frozen frame is rendered once more, with `getTime()` pinned to zero.** The caret's pulse, the
editor's marching ants, the contamination's throb and the "Pause" text read the engine clock, so a frame
that merely stopped would carry the harness's timing in those pixels. `TestHooks::frozenFrameDue()` is the
one-shot that asks for that render; the request is answered while frozen, since the harness still has to
take its picture and quit.

**A crossfade's clock moves once per loop iteration and the screen under it once per tick**, and how
many ticks an iteration bunches is the machine's business — a level load alone is a backlog of several.
So a frame in a transition needs two things: `freeze fade <ms>` stops at the first tick at which the running
crossfade has reached that many milliseconds, and `lockstep 1` makes every iteration exactly one tick
(the main loop throws the backlog away), which pins the fade to the screen behind it. The
`cube` and `star` scenes are that; measured without lockstep the cube froze at level tick 540 on one run
and 500 on the next. The credits need lockstep for a different reason: they draw their own last frame back
into the next one, so their picture depends on how many frames were rendered, not only on the tick.
`state <name>` switches game state by name, which is how the credits and the logo screen are reached, and
one word after the name is a boolean parameter set to true in the context the state is entered with.

**The credits are two scenes, because they are two screens.** `GS_Credits::onEnter` runs the ending only
where the shipped campaign has been finished, and a home written fresh per run has no progress in it — so
`credits` says `state GS_Credits full` and gets the ending, and `credits-plain` says nothing and gets
what a player who has not won sees. Seeding a `ProgressDB` with the campaign's 42 levels would be the
other way to reach the first, and the parameter is a line against a fixture.

The two are different pictures and not one with a switch: `credits` is the gradient, the star field and
the motion-blur buffer under `$C_THANKS_FOR_PLAYING` at an animated `charScaling`, which is the one text
in the game nothing caches; `credits-plain` is the *programming* credit on flat black at a scaling held
at 1. Only the first needs `lockstep`, since only it draws its own last frame back into the next one —
which is also why the second costs seconds where the first takes a minute. Running second,
`credits-plain` is a re-entry into a state the run has already left once, which is what proves `onEnter`
starts from nothing.

**Their ticks are not comparable**, 6000 against 4000, and that is the versions and not the scenes: the
plain one's clock starts at 0 where the ending's starts at -2000, since it has no star field to fade up
and nothing to establish, so `sceneTick` (`time + 2000`) begins at 2000 for it and at 0 for the ending.
Both land four fifths of the way through a block's fade-in, clear of the half-way point the fade turns
round on — a frame sitting on that branch would flip between two pictures for a change of a millisecond.

**What neither can see is which key asked for which**, so `smoke.sh` drives the two chords and reads the
answer off the one behaviour that separates the versions: a click or Escape leaves the plain credits,
where the ending takes neither as an exit. Ctrl+Shift+F2 is the plain one and Ctrl+Shift+F3 the ending;
the *modifiers* are held across the key, because `GS_Menu::onUpdate` reads those with `SDL_GetKeyState` —
see the two-input-layers trap above — while the function key itself it reads with `wasKeyPressed()`, the
edge an `SDL_KEYDOWN` sets, so a short press inside the hold is seen however long a frame is taking.

**A transition's own clock is in the dump**, as `crossfade`: milliseconds into a running one, negative
through its lead-in and -1 where there is none — the same number `freeze fade` stops on. It is what makes
the length of a transition measurable rather than something counted under the breath, and `smoke.sh` uses
it for the one bug it was written for: five quick presses of F5 must run **one** transition through, so
the check is not how long that takes — a timing assertion under whatever load the machine is under — but
whether the clock ever runs *backwards* afterwards. It can only do that if a second transition began,
which is the bug exactly. Proved against the code that had it: "the restart transition went back from
679ms to 80ms".

The other two of that family are read the same way, off state the game already reports: `smoke.sh` holds
the pause key for a second and a half and asserts the dump's `paused`, and `drag.sh` — which has a second
character parked in a corner for it — holds Tab and asserts the active cell does not move again, then taps
it three times and asserts it did switch three times. The second half of that one matters as much as the
first: it is what would catch the removal of `GS_Game`'s throttle having made tapping slower. Both were
proved against the actions that repeated. **Neither compares against a cell written down in the file** —
the first character has walked a long way by the time `drag.sh` gets there, so an assertion naming its
starting cell passes whatever Tab does.

**A static text reports how big it is**, and that is what `smoke.sh` asks the help with. The frame around
a help page is a fixed 580x370; what goes in it is written by hand in `languages.txt`, wrapped at display
time, and carries `%BINDING` markers that expand to whatever the player rebound them to — so "it fits" is
not a property anybody can read off the file, and when it stopped being true the only symptom was a row
sliced in half by the frame's bottom edge. The dump carries each `GUI_StaticText`'s laid-out size, measured
through the element's own font by the same `measureDrawnText` its `onRender` and `containsPoint` use, so
the harness compares the game's own answer against the box rather than reimplementing the wrap in shell.
Beside the size stands what it says, as `drawnText`, for a text the code sets - a question naming a file.
The walk is done twice, the second time with the language switched to German from the options dialog,
since German is the longer of the two everywhere else. It was proved against the geometry that was wrong:
360 px of text in 345 px of box, both languages.

**Wrapping is asked of the font itself**, at every width at once: the hook's `wrap <font> <italic> <from>
<to> <text>` answers a line a width, each holding the lines `adjustText` broke the text into and how wide
each is drawn - hundreds of layouts in one tick, where a request a width would cost a tick each. A line is
measured behind one `<h>` for each heading open where it begins, since a heading broken across lines is
slanted from the first letter of the second. `smoke.sh` lays out three texts full of keycaps whose names
hold a space, glued to words and punctuation and paired by half spaces, in both fonts the GUI wraps with,
upright and slanted, from 1 to 600 pixels. No line may stop inside `<k>…</k>`: against `adjustText` without
its keycap guard, 1450 of the 7200 answers broke inside one. And none may be wider than its width unless
all it holds is one keycap or one character, which no width makes narrower: against `adjustText` counting
no slant, 624 answers held one. A row of the help table follows, tabs and all, from 160 pixels, where its
two stops fit; 319 of its 1764 answers ran over against an `adjustText` that carried a tab moved down by its
glyph's width and counted no slant.

**Draw calls are counted at the link, natively.** `LinuxBuild/build.sh hooks` links with
`--wrap=glDrawArrays,--wrap=glDrawElements`, so every one of those from the game's own
objects passes through the wrappers at the foot of `testhooks.cpp`; no header carries the define and no
other translation unit needs it, which is what the `hooks_layout` check protects. The dump reports them
as `draws.calls` over `draws.frames`, and `frames.sh` prints the ratio per scene beside the renderer's
own draws and a histogram of what ended each batch (`batch.byReason`: a texture change, a blend change, a
scope, a full stream, an explicit flush before a copy, a clear, a 3D draw or a target switch, the end of
the frame, and a `Renderer::DirectGL` bracket opening for raw GL - `perf.md` reads the same keys).
In the browser the same key comes from `WebBuild/test/harness.js`, which counts `drawArrays` and
`drawElements` on the WebGL context's prototype — what reaches WebGL, the number a phone pays: the
renderer's draws and the present's, one to one, with no emulation in between — and `resetStats()` starts
both counters in one evaluate so no frame falls between them.

**The CRT settings button switches the filter on there and then**, and only the dialog's Cancel takes it
back, through `loadConfig()`: every setting starts from its default and then reads `config.xml`, so on a
harness run's fresh home, which has none, Cancel lands on the default filter and not necessarily on the
one that was on. A filter left on with its curvature warps every later click off its element, and what
that looks like is a scene three steps later failing on a button that "is not visible". The `crt` scene
therefore clicks the previous filter's own radio button before Cancel, under the name the dump reports as
`filter`, and does not rest on the default being it.

Scenes are driven by element name where there is one and by game coordinate where there is not:
`b5_clickAt` and `b5_mouseAt` take a 640x480 coordinate and map it through the present rectangle, which is
how the editor's palette and the pins of its parts are reached; `b5_drag` presses at one point and releases
at another; `b5_type` and `b5_chord` type into whatever has the focus. Three traps found building the
scenes: **Escape closes an open hint note before it opens the game menu**, so `quitLevel` looks whether
the menu came up and presses again if not; the editor's quit asks a yes/no question once the level is
modified, and the scenes that modify it answer it by name; and a scene without a clock of its own — the
editor's level does not tick — asks for `now`, the next tick whatever it is, rather than a tick it could
miss. The select screen is not such a scene: its preview is a level and the level ticks, with the night
vision's noise and the fire's particles in it, so `select` freezes on a tick and the cube crossfade out
of it starts from a frozen one. The script traps its own exit and stops the game and
the X server it started, because a `FAILED` from inside a function otherwise leaves both attached to
`:88`, which the next run refuses to start over.

**Two binaries are compared by running the oracle twice, each in a home of its own.** A worktree of the
other commit is built with `Blocks5/pack.sh data && LinuxBuild/build.sh hooks`, and its run
gets `B5_DISPLAY`, `B5_SHOTS` and `B5_FRAMES_XDG` of its own, because two runs cannot share a display, a
shots directory or a home; then `cmp` over the twenty PNGs says which scenes moved, and a pixel diff of
one says where. The tag `render-baseline` marks the last immediate-mode binary, the one the renderer
redesign was measured against, so that comparison can still be made.

**Two clocks that come apart, and the order of the scenes follows from them.** The select screen, the
editors and a played level are *pushed* on top of the menu, and the menu's own clock — the one the title
demo's recording and the clouds run on — carries on across the visit while the restored title level's
starts again from zero; after a pop the two stand apart by whatever the harness's timing made of the
visit, and the clouds at a level tick are a different picture on every run. So the menu and its dialogs
are photographed on the first visit, when both start together, and both crossfades — the `star` into the
editor, the `cube` into a level — are started by the hook's `click <element>` request while the screen
they leave stands frozen at a named tick (`b5_transition`): a click that lands on a tick, which no real
click can. The other clock is the level's own: `Level::clear`
resets `sceneTick`, because `Engine::update` seeds a tick on the clock as it stands, and before that a
new level's first tick was seeded on the previous level's last — a gas cloud's first particles then took
different slots from one run to the next, and alpha-blended particles are drawn in slot order. And one
table was built from the shared generator on the first frame that needed it: the toxic effect's noise,
which therefore depended on how many frames the process had rendered by then — it now comes from a
generator of its own with a fixed seed (`Level::renderToxicEffect`), the one place a render path still
drew from `random()`. The last one hid behind the audio clock: `Sound::createInstance` drops a one-shot
that follows the same sound within ten milliseconds of *wall* time, and `Engine::playSound` drew its
random pitch only when the instance came — so two ticks the machine bunched into one iteration consumed
one draw fewer than two it ran apart, and the gas cloud spread differently from one run to the next.
The pitch is now drawn before the lockout is asked. And the order the objects are walked in was the
render's: `Level::render` sorts the object vector by depth and shown position for painting, so a tick
that followed a rendered frame walked a sorted vector and a tick that followed another tick walked the
spawns in the order they were appended — the same random draws went to different gas cells. The particle
dump of the hook (`particles`) is what showed it: the same particles, sixteen pixels apart.
`Level::update` now sorts before it walks — and the sort's last word, the UID, is unique now: spawned
objects were numbered from the *new* last object's UID, which was still zero, so every spawn reused the
load's numbers and two gas cells of one row had no order at all.

## In a browser

`WebBuild/build.sh hooks` builds to `build-test/` with `-DBLOCKS5_TEST_HOOKS`, turning on
`WebBuild/test_hooks.cpp`. The shipped build has none of it — the whole translation unit is inside the
`#ifdef`, and `blocks5_testDump` does not appear in `build/blocks5.js`. `./build.sh` without `hooks`
writes `build/` and leaves `build-test/` as it was, so `harness.js` refuses a `build-test/` older than
`Blocks5/src` or than what `build.sh` reads from `WebBuild`, as `b5_start` does natively: a smoke or
perf run against the previous build passes for a game that no longer exists.

The hook only reads. It puts the GUI tree into `Module["b5_test"]` as JSON — every element with its window
rectangle, whether visible and enabled, plus game state, language and filter — and `blocks5_testHitAt(x,
y)` says which element a click would reach. `WebBuild/test/harness.js` turns that into `clickPath(page,
'Menu.Options')`, and the click stays an ordinary mouse click travelling through SDL, Engine and GUI.

Do not go back to reading coordinates off a screenshot: the buttons are eighteen pixels high, the window
is scaled, and a pane drawn on top looks like a missed click.

Four things about this environment, each of which cost real time:

- **A wasm trap looks like a hang, not an exception.** `page.evaluate` never settles and there is no error
  anywhere. `computePresentRect` divided by a zero `screenSize` before `main()` had run and the
  NaN-to-int cast trapped; `GUI_Element::getFullName()` walked past a null parent. Guard anything the
  hook calls before the engine is up, and bisect a hang by adding stages.
- **`Module.calledRun` never appears** in this Emscripten. Wait for the page-level `runtimeInitialized`
  *and* a dump with a game state and a non-empty element list; the runtime is up well before `main()` has
  built anything.
- **Under swiftshader the game needs about half a minute to reach the menu**, and a frame takes a fifth of
  a second. Never sleep a guessed interval; wait on the reported state.
- **`GS_Loading` waits for a real gesture**, because the AudioContext is suspended until one arrives — so
  a test must click *before* waiting for `GS_Menu`, or it waits forever.

## On a phone

`WebBuild/test/mobile.js` is the same idea in Chromium's mobile emulation, loading `index.html` rather
than `blocks5.html` — that is the file that ships and the only one registering the service worker. It
checks the page around the game: layout viewport the device width and not the ~980px default, nothing
scrolls or zooms, the canvas covers the viewport, the manifest says what an install needs, the worker's
cache holds the payload, and a reload with the network off still boots. **`isMobile: true` in the context
is what makes any of it mean something** — without it Chromium lays out at the window width and the
viewport meta has nothing to do.

The checks about the game rather than the page use a real touch: `touchStart`, a wait, `touchEnd`,
through `Input.dispatchTouchEvent` over CDP. That found the click-ordering bug in `GUI::update()`; the
dump reports `cursor` and `mouseDown` so a tap that does not arrive can be told from a button that does
not react. The same touch at the corner of the options button's cell - outside the square, inset 8 in
it, that the button is hit on - has to open the options, and a real mouse there must not, though the
mouse was there and a press of it on the button does open them; `blocks5_testTouchAt` puts what the
picker says into `Module["b5_touch"]`, as `touch x y` answers it natively. And a finger dragged up the
options' list of actions in five `touchMove`s, resting before it lifts, has to scroll it by the drag less
the slop and select nothing, where a tap on it then selects the item under it; and one dragged down it
and cancelled (`touchCancel`) must leave it where it stood when the next touch lands on the dialog's
Cancel, and that touch must still press it. Run against the page without its cancel handler - a capture
listener added by `addInitScript` swallows the event first - both fail: the list followed the next finger
and Cancel was never pressed.

**The text sheet is driven in the campaign editor** (`web.md`), whose title, description and file name
are the three kinds of field. A tap on each opens the sheet with that field's caption and text, its field
focused; text goes in through CDP's `Input.insertText`, an edit of the field with no key events at all,
which is what an Android keyboard produces; and the dump reports every edit box's text as `value`,
Latin-1 escaped as `\u00XX`, so what OK handed back is read off the game's own field. Typed keys are
`page.keyboard`, trusted as a phone's are, and two checks hang off them: the game's title behind the
sheet must not take them, and the pad must stay up. With the sheet's `stopPropagation` and the pad's
field test patched out of the built page, both fail - the keys went into the game's title at its caret
and none into the sheet's field - and Escape reached the editor, which asked whether to save. Three more
hang off real keys: Enter on Cancel, reached with Shift+Tab, must cancel; with the field's focus taken -
a tap on the dimmed page must leave it, so the check takes it with `blur()` - typed keys must still not
reach the title, the game's focus asserted on it, and Escape must cancel; and Shift held over the tap
that opens the sheet and let go of inside it must leave the dump's `actionsDown`, since Shift plants a
bomb. With the three fixes behind them patched out of the built page, the first two fail - Enter on
Cancel kept "Not this", and "Leak" went into the game's title with Escape raising the editor's question
- and with only the release's patched out, the Shift check alone fails. More of the same kind: an Escape
held as it cancels, its repeats sent as `page.keyboard` sends them, a second `down` without an `up`,
must not quit the editor; Space on a focused Cancel must cancel and take the focus out of the sheet; and
the sheet's `CloseWatcher`, which an init script records as the page makes it, asked to close as Back
asks, must cancel. Each of those, and the tap on the dimmed page, fails alone with its own fix patched
out of the built page and the rest pass: the held Escape raised the editor's question, Space left the
sheet open with the focus in it, the tap took the focus off the field, and Back asked a watcher nothing
listened to. A question a failed check leaves up would otherwise fail every check after it, so
`tidy()` cancels a sheet left open and answers No after each check that can leave one. The description's
field is measured as it takes the focus and 150 ms later, from a `focus` listener the check adds: two
lines high both times, and grown well past that once the moment a keyboard has to come up is over -
against a page that sized it at once, it read 16 and 266. A drag on the description must open no sheet
but take the game's focus, a mouse on the title must open none, and the description's lines come back
into the sheet the next time. The section has a `try` of its own, so that
a step that throws leaves the service worker's checks to run, and the context's locale is English, the
language the captions are compared in.

**File names beyond ASCII are saved, listed and loaded in the same editor** (`filesystem.md`): a level
added, the campaign saved through the sheet as *Bär.zip* and as *Bör.zip*, `FS.readdir` asked whether
both stand under those names, the editor's own list read back an entry at a time - its Select puts the
entry into the file name, which the dump reports - and *Bär.zip* loaded by the name the list gave, into
an editor emptied with New first. Against the page before the conversion all three failed: the folder
held one file named `B`, U+FFFD, `r.zip`, the second save asked to overwrite it, and the list read
`Bï¿½r.zip`, the three bytes of U+FFFD as Latin-1. A copy planted under a name with a U+FFFD in it, as a
player's IndexedDB can hold one, must be listed as that character's three bytes and load by them; with
`platformName`'s pass-through for such names taken out, that check alone fails, the file missing from the
list.

**The dump also lists `actionsDown`**, the only window onto the *action* layer from outside:
`Engine::updateVKs` reads `SDL_GetKeyState` and not `keyData`, so whether a key reached the named actions
cannot be inferred from anything else. It established that an on-screen pad can drive the game with an
ordinary DOM `keydown`/`keyup` on the document — a synthetic `ArrowLeft` with `isTrusted === false` shows
up as `["$A_LEFT"]` and clears on `keyup`, where `Engine::setKeyData` would not have worked at all
(ROADMAP item 19).
