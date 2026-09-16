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
