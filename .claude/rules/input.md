---
paths:
  - "Blocks5/src/engine.{cpp,h}"
  - "Blocks5/src/options.{cpp,h}"
  - "Blocks5/src/main.cpp"
  - "Blocks5/src/gs_game.{cpp,h}"
  - "WebBuild/touch_controls.js"
---

# Input: virtual keys, actions and the key grab

**Input** is two-layered. Physical keys / joystick axes / hats map to *virtual keys* (`VirtualKey`), and
named *actions* (`"$A_LEFT"`, `"$A_PLANT_BOMB"`, …) bind a primary and secondary VK. Gameplay queries
`wasActionPressed(name)` / `isActionDown(name)`; bindings are registered in `main.cpp` and remappable in the
options dialog, where *Reset selected* and *Reset all* work off `Action`'s `defaultPrimary` and
`defaultSecondary` and grey out without a selection.

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
`Player::canMove`, and a leg that has walked into something is over as surely as one that has run out, so
the other axis takes over — which is what walks the character round an obstacle. Held against the wall
instead, the key would keep that leg alive for as long as the button was down, and the axis that could
still move would never get its turn. `canMove` answers about the geometry and not about this tick —
whether the character has already moved or is sliding decides when a step lands, not whether the way is
open, and a key dropped for one tick would restart the repeat that paces the walk. And it stops one cell
deep: whether a pushable block really gives way depends on the whole chain behind it, and leaning on a
block that will not move is what the keyboard does too.

**A click works what the character is standing next to.** With blocked directions no longer commanded, a
switch or a magnet would otherwise be out of a mouse player's reach: both are solid and fixed and do their
whole job in `onTouchedByPlayer`, which is reached by walking into them. `GS_Game::bumpCell` takes the
press that did *not* land on a character, and has two guards and no more — orthogonally adjacent, which is
what "walk into it" means, and only where the character cannot go there, so a click is never a step and
never a push. What happens then is left to `Player::move`, and that is the point: a click reaches exactly
what a walk that way reaches, whatever the case. A panel falls out of it for nothing, being walked *onto*
rather than into, so `canMove` says yes and the click works nothing — which is what a panel is for. So
does a switch standing on a solid tile, which `move` refuses to touch through.

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

**Any key and any click leave the pause**, not only the pause key — `wasAnyKeyPressed` and
`wasAnyButtonPressed` read the same per-tick bits. Coming back from another window is what makes it worth
having, since `onAppLoseFocus` pauses and the click that returns is then the one that resumes. The press is
*spent* on resuming, and that ordering is the trick: the resume sits in front of the action chain as its
`if`, so the pause key cannot switch back on in the same tick what it just switched off.

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
