---
paths:
  - "Blocks5/src/engine.{cpp,h}"
  - "LinuxBuild/linux_window.{cpp,h}"
  - "WebBuild/pre.js"
  - "WebBuild/shell.html"
  - "WebBuild/web_bluescreen.{cpp,h}"
  - "Blocks5/libs/sdl-1.2.15/src/video/windib/**"
---

# The window: placement, fullscreen, focus and the cursor

**The window.** Resizable, aspect kept, black bars. **SDL's video flags are `SDL_OPENGL |
SDL_RESIZABLE` for the whole life of the process and must stay that way** — `DIB_SetVideoMode` keeps the
GL context only on its fast path, which requires flags and bpp unchanged and `SDL_FULLSCREEN` clear.
Setting `SDL_FULLSCREEN` or `SDL_NOFRAME` runs `WIN_GL_ShutDown` instead and takes every texture and the
FBO with it. So fullscreen is *not* an SDL flag here: `applyWindowStyle` sets the Win32 style to
`WS_POPUP` and the size to the desktop directly, SDL notices through `WM_WINDOWPOSCHANGED` and posts an
ordinary `SDL_VIDEORESIZE`, and `handleResize` — the one place that owns `displaySize` — picks it up.
Dragging the border and Alt+Enter run the same code, and nothing is ever destroyed. Alt+Enter is swallowed
so the game never sees a bare Return.

**A window that stops presenting loses control of what it shows.** While the app is inactive the main loop
skips logic and rendering but must still put the last frame up — `showLastFrame()` does that every 50 ms
(unbind, `presentFrame`, swap). A bare `SDL_GL_SwapBuffers` without drawing is not enough: it flips to the
other buffer and shows the frame before the last. And a full-screen popup is exactly the shape Windows may
hand a direct scanout path, after which the compositor's own copy stops being updated — with the Start menu
open over one, the game showed a frame from seconds earlier.

**Drawing while the border is dragged** needs a window procedure of the game's own in front of SDL's
(`Engine::hookWindowProc`), because `DefWindowProc` runs its own modal message loop and the main loop sits
in `SDL_PollEvent` until the mouse comes up. `engine.cpp` carries the mechanism and its traps — what may not
be called during a drag, and what has to be put back afterwards.

**The window's placement is saved on exit.** One `<Window positionX= positionY= sizeX= sizeY= maximized=
fullscreen=>` is written by `Engine::exit`; the position is the only part that can be absent, because on a
first start there is none and a 0,0 would be a claim rather than a fact.

**`GetWindowPlacement` and `SetWindowPlacement`, and never either paired with `SetWindowPos`.**
`GetWindowRect` on a maximized window gives the maximized frame, whose corners are negative because the
invisible grab handles count, so what is saved is `rcNormalPosition` — the rectangle "restore" goes back to —
with `showCmd` as the `maximized` flag. And `rcNormalPosition` is in **workspace** coordinates: the work
area, with the taskbar and any docked toolbar taken out, where `SetWindowPos` takes **screen** coordinates.
The two agree exactly while the work area begins at the monitor's top left corner, which is what a taskbar
along the bottom or right gives — and that is why putting one back with the other was invisible to almost
everybody. With the taskbar at the top or left, every save and restore shifts the window by its size, in the
same direction each time, until it has walked into the corner. `SetWindowPlacement` closes the round trip,
replays `showCmd` itself, and moves a window that would land on no screen back onto one.
`rememberWindowPlacement` logs both rectangles, the one place the
two systems meet and the only way to read a machine's work-area offset off a log.

**The same coordinate system has to reach the fullscreen path**, which is why `setFullScreen` asks
`rememberWindowPlacement` *before* it flips the flag rather than letting `Engine::exit` ask afterwards: in
fullscreen the window is the screen-sized popup with no windowed placement left to read. `applyWindowStyle`
therefore keeps only the style.

`handleResize` skips updating `windowedSize` while `IsZoomed`, so the remembered size is always the windowed
one — and the maximized state is replayed on both paths, at startup and on the way out of fullscreen. What
makes the second work is a line in the vendored SDL: `DIB_ResizeWindow` does its entire body inside `if (
!SDL_windowid && !IsZoomed(SDL_Window) )`, so the `SDL_SetVideoMode` that `handleResize` performs afterwards
moves and sizes **nothing** while the window is maximized. The one thing `applyWindowStyle` owes it is the
size the window actually became, read back with `GetClientRect`, instead of the `windowedSize` that
`setFullScreen` passed down: a window that has just come back maximized is the size of the work area, and
passing the windowed size on would resize the maximize away in the same breath as restoring it.

On first run, or when the stored size no longer fits, `getDefaultWindowSize` picks the largest integer
multiple of 640x480 leaving a 120px margin in *both* directions, so "sharp" starts with no black bars. 120 is
derived, not felt: the largest margin under which 1920x1080 still gets 2x (2*480 = 960 = 1080-120, nothing to
spare). The same value goes horizontally, where it is pure slack, because a taskbar is not always at the
bottom. `-windowed`/`-fullscreen` set the state for that start rather than overriding it for one run, since
`Engine::exit` always saves. In the browser the canvas fills the page (`WebBuild/pre.js`), Alt+Enter goes
through the Fullscreen API from a real DOM keydown — the main loop's own events do not count as a user
gesture — and the main loop reads the canvas size once a frame.

**The mouse cursor follows the scale; the framebuffer has nothing to do with it.** The arrow is drawn once
at 16x16 — the size it was designed as, and the size the video recorder and `screenshot()` stamp into the
640x480 frame whatever the window does. `createCursor(factor)` builds the two the system can draw, 16 and
32; `updateCursorSize` picks from the width of the rect `presentFrame` fills, not from the window, because
`Sharp` snaps to whole steps and between scale 1.5 and 2 shows the picture unscaled where other filters
nearly double it. `render` asks once a frame rather than hanging off events: the answer moves on a resize,
fullscreen toggle, filter change and the browser's canvas alike, and asking costs two divisions and a
comparison. Two sizes exist, so the choice is which of 16 and 32 lands closer to 16·s: `|32 − 16s| < |16 −
16s|` from **s = 1.5**. At exactly 1 and 2, where `getDefaultWindowSize` puts almost everyone, the chosen
one is pixel-exact. Measured at 1.40/1.50/1.60: 16, 32, 32.

**The first gesture takes the fullscreen, on every device, and never again on its own.** `pre.js` listens in
the capture phase for the events that carry a *transient user activation*, which the Fullscreen API demands:
a mouse button going down, a finger or a pen lifting, a key other than Escape. The loading screen already
stops for a gesture, so nobody pays an extra one, and the listeners stay armed until a request was actually
made. After that a swipe or a long Escape out of it is meant: Alt+Enter or the pad's button bring it back,
through `Module.b5_setFullscreen`, which asks `navigator.userActivation.isActive` first and stays quiet
when there is none. There is no device test in any of it. The one that stood here, a coarse pointer and no fine one, called a Galaxy with no pen and no
mouse a notebook and left it under the address bar for good.

**That same first gesture resumes the AudioContext**, and not only `GS_Loading`. Going fullscreen turns the
phone to landscape, and the rotation makes the browser cancel the touch in flight — SDL never sees the
press, so `GS_Loading` does not know a gesture happened and waits for a second tap the player should not
have to give.

**Escape stays with the game while fullscreen, where the browser allows it.** The menu, the note and the
dialogs all hang off that key, and a browser's own exit would take the first press. `Module.b5_lockEscape`
asks the Keyboard Lock API for Escape on every entry and unlocks on the way out; Chromium alone has it,
asks for a long Escape to leave instead and says so in its own bubble. Firefox and Safari keep their exit,
and the game gets its Escape on the next press. The pad's Esc is a synthetic key, which no browser treats
as the exit, so a phone player keeps the fullscreen.

**The fullscreen goes on the root element, never on the canvas** (`Module.b5_setFullscreen`, not
`emscripten_request_fullscreen_strategy("#canvas")`). A browser paints only the fullscreen element and its
descendants, so with the canvas promoted the on-screen pad — its sibling — disappears the moment the game goes
fullscreen. It still reports a full-size `getBoundingClientRect` while invisible, which is why a test that
measured it saw nothing wrong. From `<html>` both are inside, and the canvas is 100%/100% of the page anyway.

**The engine asks the browser whether it is fullscreen, never its own flag.** `Engine::isFullScreen` reads
the document's state in the browser build and `setFullScreen` refreshes the member from it first: the page
takes the fullscreen on its own first gesture and loses it to a swipe or a long Escape, and neither tells the
engine, so Alt+Enter would otherwise need two presses the first time.

**The pad's button** (`touch_controls.js`) sits in the slot beside Esc and calls `Module.b5_toggleFullscreen`
on pointer-up, which carries the activation. It is left out where there is nothing to toggle: iPhone Safari
has no element-level Fullscreen API, and a page installed to the home screen is fullscreen already. The
loading screen's Alt+Enter hint follows the pad the other way round, shown only where the pad is hidden
(`Engine::isPadShown`), since that is the one signal there is for playing without a keyboard.

**The landscape lock hangs off `fullscreenchange`, not the request.** `screen.orientation.lock` is refused
unless the document is already fullscreen, so the other order simply rejects; `Module.b5_lockOrientation`
waits for the event and unlocks on the way out. It is attempted on every entry: it rejects on a desktop, and
on a tablet held upright it turns the picture the right way like on a phone, so every path swallows the
failure. The manifest asks for landscape too, but only an installed app gets that.
