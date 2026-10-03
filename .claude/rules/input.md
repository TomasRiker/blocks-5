---
paths:
  - "Blocks5/src/engine.{cpp,h}"
  - "Blocks5/src/options.{cpp,h}"
  - "Blocks5/src/main.cpp"
  - "Blocks5/src/gs_game.{cpp,h}"
  - "Blocks5/src/touchkeyboard.{cpp,h}"
  - "WebBuild/touch_controls.js"
---

# Input: virtual keys, actions and the key grab

**Input** is two-layered. Physical keys / joystick axes / hats map to *virtual keys* (`VirtualKey`), and
named *actions* (`"$A_LEFT"`, `"$A_PLANT_BOMB"`, …) bind a primary and secondary VK. Gameplay queries
`wasActionPressed(name)` / `isActionDown(name)`; bindings are registered in `main.cpp` and remappable in the
options dialog, where *Reset selected* and *Reset all* work off `Action`'s `defaultPrimary` and
`defaultSecondary`; *Reset selected* greys out without a selection, *Reset all* needs none.

**The mouse drag is a device, not a special case in the game.** `Engine::updateMouseDrag` is a recogniser
of the same kind as the joystick hat: it sets six virtual keys (`Mouse DragW`, `DragE`, `DragN`, `DragS`,
`DragB2`, `DragB12`), and `main.cpp` binds those to `$A_LEFT` and the five other actions the drag feeds.
`Player` asks for the action and never learns a mouse exists.

The six sit **directly behind the keyboard block** in `virtualKeys`, so their base is `SDLK_LAST` whatever
joysticks turn up. That is not tidiness: `main.cpp` registers its actions *before* `Engine::init` builds
the table, so a binding has to be nameable before the table exists. A keyboard key manages it because its
index is its own key code; these manage it by having a base that is a constant. Getting that wrong is
silent — the drag simply does nothing, because the indices it lands on are key codes that are never down.

**The drag names a cell, not a direction.** The character walks to whatever the cursor is over and keeps
going while a button is held. The cells come from the game state, not from `Engine`, which has no idea
what a level is: `GameState::getMouseDragCells` is asked for the active character's cell and the cursor's,
and only `GS_Game` answers — with a level running, nobody paused, no menu over it, the press having
landed on the field rather than on a widget (the GUI remembers what it went to, and for the play area that
is the `GameGUI` itself), and the press having landed **on a character**. A drag is a command to the one
the player took hold of, so dragging from empty ground steers nobody; `GameGUI::onMouseDown` is the one
place that knows a press landed on a character, being already there to wake it up — the same press does
both — and what it sets lives until the last button comes up rather than until the next press, because
the second button of a two-button grip lands wherever the cursor has got to by then. It is asked from
`updateVKs()` and not computed in `onUpdate()`, which runs *after* `updateActions()`: a step decided there
would reach the character a tick late at both ends, and the late one at the end is a step taken after the
player let go.

**Three rules keep it honest.** The keys are *held* and never pulsed — a press that lands while an action's
repeat is counting down goes into that action's buffer and is played out later, so a pulsed key would
stack steps up and walk on after the button came up. **One axis moves at a time**, committed until it
runs out, so the path is two straight legs and never a staircase. A staircase is no faster than the legs
— the same number of steps either way — but it is a path nobody would walk by hand on the keyboard, so
allowing it would be an advantage for nothing. The leg that begins takes whichever axis is further off.

And **a direction the character cannot go is never commanded**: `GameState::canMouseDragStep` asks
`Player::move(dir, false, true)`, and a leg that has walked into something is over as surely as one that
has run out, so the other axis takes over — which is what walks the character round an obstacle. Held
against the wall instead, the key would keep that leg alive for as long as the button was down, and the
axis that could still move would never get its turn.

**That question is `move()` itself in `simulate` mode**, not a second opinion beside it. Every path
returns at the point where it would otherwise commit, and the push recurses in simulate mode too, so a
chain of pushable blocks against a wall answers no at whatever depth the wall stands — while a single
block with room behind it still answers yes, because a drag that could not push would be worse than one
that leans. Nothing else would stay true: the decisions are 200 lines in `Object::move` and another 119
in `Player::move`, and a predicate written beside them starts agreeing and ends disagreeing.

**Why ask at all, rather than hold the key and see whether anything moved?** Because trying is not free.
A walk into a switch or a magnet *works* it — that is what `onTouchedByPlayer` is for — so a drag
that routed around an obstacle by bumping it would flip whatever it brushed, which is exactly what the
click below is meant to be the only way of doing. A failed push downward can burst what it lands on. And
while a bomb button is held the character does not move at all (`Player::onUpdate` takes the bomb branch),
so there would be no answer to read.

Two of `move()`'s early-outs are skipped when asking, and that is deliberate. Whether the character has
already moved this tick, and whether it is sliding, decide *when* a step lands rather than whether the way
is open; the drag holds its key across ticks, and an answer that flickered with the tick would hand its leg
to the other axis and back. `updateVKs()` also runs before `Level::update` clears `moved`, so in simulate
mode that flag is always the previous tick's. The one place the answer is generous is a push made from
ice: the pushed object goes, but the character standing on the ice stays where it is, so the real `move()`
reports false while the way is in fact opening.

**A click works what the character is standing next to.** With blocked directions no longer commanded, a
switch or a magnet would otherwise be out of a mouse player's reach: both are solid and fixed and do their
whole job in `onTouchedByPlayer`, which is reached by walking into them. `GS_Game::bumpCell` takes the
press that did *not* land on a character, and has two guards and no more — orthogonally adjacent, which is
what "walk into it" means, and only where the character cannot go there, so a click is never a step and
never a push. What happens then is left to `Player::move`, and that is the point: a click reaches exactly
what a walk that way reaches, whatever the case. A panel falls out of it for nothing, being walked *onto*
rather than into, so the simulated move says yes and the click works nothing — which is what a panel is
for. So does a switch standing on a solid tile, which `move` refuses to touch through.

**Which buttons the drag carries is latched when it sets off**, not read per tick, and that is the one
thing here that would be a bug the other way: on the way into a two-button grip there is a tick with only
the right button down, and that is the gesture for a *lit* bomb — the player would get one where they
asked for a bomb put down safely.

The drag is bound as an action's **third** source. Both real slots are taken on all six actions and
worth keeping (`$A_LEFT` is Left and KP4, `$A_PLANT_BOMB` is either Shift), and a gesture is its own
binding: `tertiary` is set from `main.cpp`, never offered by the options dialog and never written to
`config.xml`, so none of the binding machinery above has to know about it.

**Two things drop a drag.** The game menu opening calls `Engine::cancelMouseDrag` — through
`Game.ShowMenu`, the one funnel Escape and the on-screen button both take — because a drag is a command
to a character and the menu is not; it blocks until every button is released, or the character would walk
on the moment the menu closed. And losing focus clears the held *buttons* as it already cleared the held
keys, since no release arrives for either: a button let go of in another window would otherwise still be
steering on the way back.

`LinuxBuild/test/drag.sh` is what proves any of it (`testing.md`).

**A finger's press is known as one where the platform says so**, and `Engine::wasFingerPress` reports it
for the tick, cleared with the other press edges; the GUI gives such a press a reach (`gui-text.md`). In
the browser Emscripten's SDL queues an `SDL_FINGERDOWN` with the `SDL_MOUSEBUTTONDOWN` it makes of every
touch, in one handler, so the two are drained in one tick - **and one with every mouse press too**, ahead
of it, from the device `SDL_TOUCH_MOUSEID`, which is no finger: taken for one, every mouse press in a
desktop browser would get a reach, and `mobile.js`'s mouse beside the options button is what notices. The
id arrives as -1 in a 64-bit field and is compared on its low 32 bits, which is the only way it equals
the `Uint32` constant. A stylus the page is handed as touches is a finger there; Emscripten's SDL says
nothing of `touchType`.

Under Windows the mouse message Windows makes of a touch carries a signature in `GetMessageExtraInfo()` -
`0xFF515700` under the mask `0xFFFFFF00`, bit 7 set for a finger and clear for a pen, which is as precise
as a mouse and stays one - and `engineWindowProc` reads it off every left and right press. **It cannot
simply mark the tick**: SDL pumps the window's messages inside a tick too (`updateVKs`, `flushInput`), and
the press such a pump queues is only drained by the main loop's next poll, a tick later - a mark set on
the message and cleared with that tick would be gone before its press arrived, and the press would be a
mouse's. So `noteButtonMessage` leaves the answer for the press itself, which takes it when it is drained;
every press is answered, a mouse's as no, so that nothing a dropped press left behind reaches the next,
and `flushInput` throws the answer away with the press. Under X11 SDL 1.2 cannot tell, and a finger stays
a mouse. The first finger's press is logged once, the one sign apart from how taps land that a machine
marks them.

**A drag with one finger has to reach the game as a mouse drag**, and under Windows it does not by
default: Windows takes a finger dragged up or down for its own pan gesture, which a window that handles no
gestures gets as scroll messages, and the list that should follow the finger (`gui-text.md`), the slider and
the editor's stroke would stutter or stop. `engineWindowProc` therefore answers every `WM_GESTURENOTIFY`,
sent as a gesture is about to begin, by turning off the pan with one finger in either direction
(`SetGestureConfig`); asked every time, the setting holds for whatever window SDL has. Two fingers still
pan, which the game never asks for. **A finger or a pen held still is the game's as well**: by default
Windows makes a right click of it at the lift, holding the left press back until it can tell, so a finger
resting on a button would never press it and one resting on a cell of the level editor would rub it out;
a pen's flicks are gestures of Windows' own the same way. `hookWindowProc` turns both off with the window
property `MicrosoftTabletPenServiceProperty`, and `engineWindowProc` says the same when Windows asks by
`WM_TABLET_QUERYSYSTEMGESTURESTATUS`. Not yet tried on a device: ROADMAP 22 has the steps. In the
browser the page's `touch-action: none` is the same answer (`web.md`).

**A finger's tap on a text field brings up Windows' touch keyboard** (`TouchKeyboard`), which Windows
opens by itself only for a text control of its own, and the game's window is none. `GUI::fingerFocused`
asks for it whenever a finger's press goes to a one-line field or its tap to a multi-line one, or a pen's
press to either (`gui-text.md`) - every time, so one dismissed by hand comes back at the next tap - and
`GUI::update` sends it away once the focus has left the text fields, a field's own scroll bars counting as
inside it. A finger's drag on a multi-line box asks for none, and neither does a mouse; a pen does, being
as precise as a mouse but having no keyboard either, and `engineWindowProc` tells it from both by the same
signature that marks a finger. `touchkeyboard.cpp` shows it only on a tablet in tablet posture, as Windows
does for its own fields - over a keyboard that is there to type on it would only cover the game - and logs
what happened whenever that changes (*Touch keyboard: shown.*, or why not), the one place a tablet says
why no keyboard came. A pane whose `TryShow` fails is let go of, and the next tap asks for a new one: the
keyboard's host may have gone and come back.

**What the touch keyboard types may come as `VK_PACKET`**: a character rather than a key, which SDL 1.2
turns into nothing, since it makes its characters with `ToUnicode` and never asks `TranslateMessage` for
the `WM_CHAR` that holds it. `engineWindowProc` does that for such a key itself, takes the `WM_CHAR`
straight back off the queue as Unicode and hands the game a key event of no key carrying it, which a text
field types like any other - as a system key too, which SDL would take for the key whose scan code it reads
off it. A `WM_CHAR` already queued means one translation has been made: SDL's `WIN_FlushMessageQueue`
translates every message it dispatches as a resize sets the video mode, so such a key is not translated
twice, and of what is queued only the last character, this key's, is typed. Which characters the keyboard
sends that way, if any, no machine here can show. **All of it was written without a Windows tablet and has
never run on one**: ROADMAP 22 step 10 is the try it needs, and says what to change for each thing the log
might report. The browser opens its text sheet for the same tap instead (`web.md`).

**An action either repeats while the key is held or fires once per press, and the restarts are the
second kind.** `Action::repeats` decides, and with it a great deal more than auto-fire: a press that
arrives while a repeating action's `countDown` is running does not fire — it goes into a **buffer five
deep** and is played out one `interval` later, and again, until the buffer is empty. That is right for
walking, where a step pressed a moment early should still be taken. It is wrong for anything that is not
a step.

**Four actions were on the wrong side of that line**, and all four fire once per press now, as
`$A_TOGGLE_MUTE`, `$A_CAPTURE_SCREENSHOT` and `$A_TOGGLE_CAPTURE_VIDEO` already did:

- **`$A_RESTART_LEVEL` and `$A_RESTART_FROM_HOTEL`** had a `delay` and an `interval` of a second.
  Five quick presses of F5 restarted the level for seconds after the last one, each restart beginning
  the rewind again over the one still running — which is what "the transition stacks" was. Measured
  through the crossfade's own clock: **3.2 s of transition after five presses let go, against 1.6 s
  after one**, the clock resetting twice after the last press. Holding the key restarted once a second
  for as long as it was held.
- **`$A_PAUSE`** had 200 and 500, so holding the key toggled the pause every half second and a hold
  ended wherever the arithmetic landed — held a second and a half it fired at 0, 200, 700 and 1200 ms
  and came out switched off.
- **`$A_SWITCH_CHARACTER`** was on the defaults, 240 and then every 80, so holding Tab cycled the active
  character for as long as it was held — measured with two of them, about two and a half times a second
  once `GS_Game`'s own 400 ms throttle had had its say. That throttle is gone with the repeat, and with
  it a hardcoded `SDLK_TAB` in the key handler that cleared it on release: it read the *key* and not the
  action, so it did nothing at all for anybody who had rebound the switch — the one place in the input
  layer that had forgotten the rule the rest of this file is about.

Turning the repeat off removes the lockout with it, and that is deliberate rather than an oversight —
`updateActions` says so where it sets `countDown`: a lockout without a repeat would send a second press
inside the delay into a buffer that only the repeat ever empties, so it would not count at all. Once per
press means every press, which is why the throttle could go without tapping Tab getting slower.

**One press does one thing.** Paused, **any key or a click on the field ends the pause** and does nothing else
- coming back from another window is what makes that worth having, since `onAppLoseFocus` pauses and the click
that returns is then the one that resumes. With a hint note open on the active character's field, **Return,
Escape, Space or a click on the field puts the note away** and does nothing else: the click wakes no other
character, takes hold of none and works no switch. Every other key is the game's while the note is open, and
an arrow walks off the field, which closes the note on the way - so running past a note never stops, by key or
by drag, since neither a held key nor a held button is a fresh press. The Menu button is no part of the field
and opens the menu either way. Otherwise the press acts.

**A key is taken before anything else sees it.** `Engine::update` offers a tick's fresh key press to
`GameState::takeKeyPress` after `updateVKs()` and before `updateActions()` and `GUI::update()`, and only
`GS_Game` takes one. A key taken is *spent*: `updateActions(true)` fires nothing from it and buffers nothing,
and `spendKeyPresses()` takes it from `wasKeyPressed()` and from the GUI's key events - so the key that
resumes takes no step and Escape opens no menu. It is still held: the opposing actions are reset and the
countdown starts, so a key held on walks once the repeat delay is over, as after any first press. And only
that key: the mouse is let alone, so a drag going on when Space puts the note away walks on. It cannot be
dropped where it is used instead, because two layers read a key in one tick - the actions for `Player`,
`GameGUI::onKeyEvent` for Escape - and `GUI::update()` runs before `GS_Game::onUpdate()`, so a resume decided
there came after the GUI had acted and in the tick the level steps.

**A click is the GUI's.** Only the GUI knows where a press went - the Menu button is a widget of its own, and
a finger's press near it may be moved onto it - so the field's own `GameGUI::onMouseDown` is where a click
ends the pause or puts the note away, and returns before it wakes, takes hold of or works anything. A click
produces no action of its own; the drag needs the hold that handler takes, so a press it returns from drags
nobody. A drag's second button is a press there as well, and puts the note away while the character walks on.

**What is offered is the game's own keys** (`Engine::wasGameKeyPressed`): any key but two kinds. One bound to
the engine's own actions - `$A_TOGGLE_MUTE`, `$A_CAPTURE_SCREENSHOT`, `$A_TOGGLE_CAPTURE_VIDEO`, which work in
every state - so F11 photographs the pause and leaves it. And a modifier bound to nothing: Shift and Ctrl
count because they are bound to the bombs, while Alt and the Windows key belong to the system's chords, and
Alt+Return would otherwise end the pause on its way to the fullscreen. A joystick is not offered at all, its
buttons being no key, so a pause button bound to one still switches both ways. The second level of
`LinuxBuild/test/drag.sh` checks all of it (`testing.md`).

**Waiting for a key is a state, not a loop.** Clicking a key button sets its caption to `$O_PRESS_KEY` and
calls `Engine::beginKeyGrab()`; `Options::onUpdate` asks `pollKeyGrab()` each tick and applies the answer —
the pressed VK, `GRAB_NO_KEY` for Escape (which clears the binding and is the only way to leave an action
unbound), or `GRAB_TIMED_OUT` on the three-second deadline, which leaves it as it was. Waiting costs nothing
on purpose: that is what somebody does who opened the grab by accident, and it must not take the key they had.

A blocking loop around `SDL_PumpEvents` and `SDL_Delay` — the obvious shape, and what this was — **cannot
work in the browser**: the event queue is filled by DOM listeners on the JS thread, which only run when C
returns to the page, which is why `emscripten_set_main_loop_arg` calls `mainLoopIteration` once per frame. A
loop that never returns never sees a key, and a second main loop does not help either
(`emscripten_set_main_loop` either unwinds the wasm stack by throwing or returns at once).

**While a grab runs, the keyboard belongs to it.** `Engine::update` skips `updateActions()` and calls
`flushInput()` — otherwise binding F1 would toggle mute on the way past, and the cancelling Escape would
reach the GUI and close the dialog. The tick in which the key is *found* still counts as part of the grab
(hence the remembered flag, not the state after `updateKeyGrab()`), or the new binding would fire its own
action immediately. Nothing stale is left behind: the main loop clears every action's pressed/released bits
each tick regardless. One quirk in `flushInput()`: Emscripten's `SDL_PeepEvents` takes the SDL 2 argument
shape *and* asserts `requestedEventCount == 1`, so that branch fetches one event per call.

**A binding is stored in `config.xml` by name, not by number.** A VK is an index into `virtualKeys`, and that
index moves: the keyboard block is `SDLK_LAST` long, 323 under SDL 1.2 and 1536 with Emscripten's headers, so
every joystick entry after it sits somewhere else — and the joystick entries depend on what was plugged in at
startup. `VirtualKey::id` is the stable spelling written instead: `key:LEFT`, `key:KP_ENTER` from a table of
the 136 SDL 1.2 key names that resolve to whatever constant the current build means, and the
already-structural `Joystick1 B3` / `Joystick1 A2+` / `Joystick1 H1NE` for the rest. Reading tries the number
first, so a pre-1.2.0 config still loads and is rewritten by name on the next save. An id that resolves to
nothing — a joystick not connected — becomes "unassigned" rather than a wrong key.

**A letter shortcut goes by the label on the key, and the keysym cannot say that everywhere.** SDL 1.2 on
Windows translates keys through the US layout (`hLayoutUS` in `SDL_dibevents.c`), so its keysyms are
*positions*: on a German keyboard the key labelled Z arrives as `SDLK_y`. sdl12-compat under Linux and the
browser build report the layout's own letter. Every Ctrl+letter shortcut — undo and redo, cut, copy and paste,
select all, save — and the editor's plain `L` therefore ask `keyLetter()` (`util.h`), which takes the letter
the layout made of the key from `unicode` where SDL fills it in and from the keysym where it does not. The
actions need none of this: a binding is taken from the key the player pressed in the options dialog, so it
matches whatever SDL calls that key, and none of the defaults in `main.cpp` is a letter.
