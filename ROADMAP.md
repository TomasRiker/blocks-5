Blocks 5 - Roadmap
==================

Planned work, roughly in the order it was proposed. An open entry records what is
actually in the way, with the functions and files involved, so the next person
does not have to rediscover it. Nothing here is scheduled.

A finished entry is kept short: what was decided, and the parts that must not be
undone by accident. How things work now is in `CLAUDE.md` and `.claude/rules/`,
and the reasoning about one file's internals lives in that file. The numbers are
stable, because sources and rule files cite them.

Open: 6 (the campaign half), 13, 19, 22, 27, 28, 29, 30, 31 (the slider), 33, 35,
36, 37, 38, 40, 41, 46, 47, 48, 51 and 58. Everything else is done.


1. Auto-detect the user's language on first start  - **DONE**
------------------------------------------------------------
`Engine::detectSystemLanguage` asks the OS (or `navigator.languages`) and answers
only `de` or `en`: of the 349 IDs in `data/languages.txt` exactly one has a `§fr:`
body and one a `§es:`, so detecting `fr` would give an English game with a French
label. It runs only when `config.xml` has no `<Language>`, and nothing ships a
template `config.xml` any more - the one that pinned English before the detection
could run is gone from the tree, the webroot and the installer alike.


2. Replace HQ2X with something that ships as source  - **DONE**
----------------------------------------------------------------
hq2x is gone with its `__asm` probe and the `-hq2x` switch; in its place an FBO
and `sharp-fit`, a twenty-line shader of our own (`u_sharpfit.cpp` carries its
derivation and its verification). The measurement worth keeping: every
edge-directed filter fails on this art. hq2x changed more than 8/255 on 4.85% of a
real frame's pixels for 7.6 ms a frame, because 95% of the picture is airbrushed
rather than flat-shaded and no 3x3 pattern matches; xBR-lv2 flickered text, since
its `step()` thresholds sit exactly where a dialog's semi-transparency nudges the
level behind it by 0-3 levels. Scale2x and a GLSL hq2x share the premise. A
nostalgic look is item 11.


3. Compile every dependency from source  - **DONE**
---------------------------------------------------
Every library is vendored source with a `PROVENANCE.txt`; what is left as a
Windows binary is `libs/bin/OpenAL32.lib`, the import library of the one DLL that
must stay a DLL. SDL_image became `src/img_load.cpp` over stb_image, which also
retired codec DLLs it `LoadLibrary`d and never shipped. ffmpeg 0.8 became
minih264 + shine + minimp4 - H.264 Baseline and MP3 in an MP4, the one
combination native on Windows and Linux (Theora never shipped in any Windows and
Chrome dropped it; AAC has no usable small encoder). Two traps live in those
libraries' `PROVENANCE.txt`: minimp4 could not mux MP3 without the
header-included-twice hook, and minih264 and minimp4 both define a `bs_t`, hence
the two one-line `*_impl.c` files.


4. Build with the newest MSVC  - **DONE**
------------------------------------------
Built and run on v143 and v145. The `.vcxproj` files ask for
`$(DefaultPlatformToolset)` and `Build.bat` passes none unless `/toolset:vNNN`
names one, so a newer Visual Studio needs no change. v120 and v140 have not been
built since the prebuilt libraries went; the `/toolset:` plumbing is the starting
point for whoever tries.


5. Enable a Linux build  - **DONE**
-----------------------------------
`LinuxBuild/build.sh` builds a native binary that plays and passes
`LinuxBuild/test/smoke.sh`. Four answers of its own, each still the shape to
keep: fullscreen asks the window manager with a `_NET_WM_STATE` message and takes
the answer as an ordinary resize (`LinuxBuild/linux_window.cpp`, its own
translation unit because Xlib claims `Font`, `Window`, `Screen` and `Cursor` as
type names); the file dialog is `zenity` or `kdialog` through `popen()`; the
update check is `curl` or `wget` the same way; and `equalsNoCase()` stays
hand-rolled because `strcasecmp` follows the locale and Turkish has two i's.
Audio capture reads the PulseAudio monitor through a `dlopen`'d libpulse, so the
game starts where PulseAudio is absent. Case sensitivity is a `verify.py` check;
if this is ever tested under WSL, put the tree on ext4, because `/mnt/c` hides
exactly those bugs.


6. Skins in the browser  - **DONE**, and skins that travel with campaigns
-------------------------------------------------------------------------
A custom skin drew black in WebGL until `Texture::applyWrapMode` switched
non-power-of-two textures to `CLAMP_TO_EDGE` - the one set on which `GL_REPEAT`
could never have worked - and only those, since rain, snow and clouds scroll
without bound and need the repeat. It runs on every platform, so a skin does not
tile for its author and clamp for everybody else (`rendering.md`, texture
wrapping).

**Still open: skins in campaigns.** A campaign archive carries its levels and
music but not the skins they reference, so a campaign built on a custom skin
still needs the skin sent separately. `Campaign::save` is built around a
`LevelRef` that knows whether its source is loose or inside an archive; skins
would follow the same shape. The music half was solved differently - a level says
`musicFilename="blocks:music2.ogg"` and borrows a track from the shipped campaign
instead of carrying a copy.


7. Translate all source comments to English  - **DONE**
--------------------------------------------------------
One sweep over 4493 comment lines in 286 files plus the tooling's output and the
READMEs; what German remains is data (`languages.txt`, the inline `"\xA7" "de:…"`
strings, the two word lists in `verify.py`'s `comments` check and the fault
`selftest.py` injects into it). Every source was pure ASCII already, so no
encoding decision was involved. The sweep was proved by reducing every file to
its code tokens before and after: not one byte of code moved, and the web build
stamped the same payload hashes.


8. Rendering performance  - **DONE**, by item 54
------------------------------------------------
This entry named the immediate-mode renderer and the browser's GL emulation as
the cost, and proposed batching the sprites first and a programmable pipeline
after. All of it happened: the sprite batch, then item 54's renderer under
everything, then item 51's atlas under that. What is left of the list is one
unmeasured knob: `WebBuild/build.sh` passes no `-msimd128`, so the shipped wasm
holds no vector instructions at all.


9. Stop needing the Visual C++ redistributable  - **DONE**
-----------------------------------------------------------
All three projects build with `/MT`, SDL is compiled in, and `vcredist_x86.exe`
with its installer task is gone. `OpenAL32.dll` is the only DLL beside the
executables and imports only `msvcrt.dll`, which is part of Windows; nothing
allocates on one side of that boundary and frees on the other, because the game
calls only core AL/ALC entry points.


10. A window that behaves like a window  - **DONE**
---------------------------------------------------
Resizable with a kept aspect ratio and black bars, fullscreen by Alt+Return, the
filter switchable in the options. The FBO from item 2 is what makes all three
cheap: the game always renders 640x480 offscreen, so every hardcoded coordinate
stays valid and only the final blit's rectangle changes. Two rules must not be
undone, both argued in `window.md`: SDL's video flags stay `SDL_OPENGL |
SDL_RESIZABLE` for the life of the process, because any other flag runs
`WIN_GL_ShutDown` and takes every texture and the FBO with it, so fullscreen is a
Win32 style flip behind SDL's back; and video and screenshots read
`GL_COLOR_ATTACHMENT0` at 640x480, never the window. Drawing while the border is
dragged is `Engine::hookWindowProc`, a procedure in front of SDL's - the same
subclassing SDL itself does for `SDL_WINDOWID`.


11. A CRT effect  - **DONE**
-----------------------------
`src/u_crt.cpp`, a fourth entry in Options -> Scaling with six sliders behind
*CRT settings ...*, and the right kind of nostalgic filter: it adds a
presentation on top of the art as drawn instead of reconstructing detail the art
never had, with no thresholds anywhere, so a one-in-255 nudge moves the output by
about one. The file carries its own reasoning - which monitor `SCANLINE_PERIOD`
imitates, the mask in output pixels, halation averaged in linear light, the
distortion going through the mouse in `warpToSource`, convergence as three beams
and not a lens - and its measurements; `upscalers.md` has the sliders and the
one-shot offer. Left for later: anisotropic curvature, a shadow-mask triad,
halation as a second pass.

**A VCR rewind when a level restarts** - also done, `CF_Rewind`, chosen over
`CF_Slices` only while the CRT filter is in effect: on `sharp` the game does not
claim to be a tube. Two images cut into strips and interleaved is what a tape in
search really puts out, and everything else follows from the physics
(`cf_rewind.cpp`). The `REWIND` and its two arrows in the corner must not move
with any of it: they come from the recorder's own character generator, and that
one steady thing is what makes the mess read as a machine. `ROLL_SCREENS` is a whole number so the
roll lands back on zero exactly when the crossfade ends.


12. Tell the player about the hardcoded keys  - **DONE**, it already did
------------------------------------------------------------------------
`$H_HELP_PAGE1` has listed Alt+Return all along, at the foot of the *Controls
(standard)* table. Worth keeping is the list of keys that bypass the action
system and so never appear in the options dialog:

| key | what it does | where |
| --- | --- | --- |
| Alt+Return, Alt+Enter | window / full screen (`isReturnKey` takes both keys) | `engine.cpp` |
| Shift+C | the plain credits | `gs_menu.cpp` |
| Ctrl+Shift+C | the ending's credits | `gs_menu.cpp` |
| Shift+D | turns the donation prompt off for good | `gs_menu.cpp` |

The help is six pages, `$H_HELP_PAGE1` ... `$H_HELP_PAGE6`, built by page number
in `help.cpp` and capped at 6 there; each has a `§en:` and a `§de:` body.


13. The particle system's container
----------------------------------
`ParticleSystem` keeps its particles in a `std::list`, one 80-byte `Particle` per
heap node, walked by pointer every tick and every frame. The manual `_mm_prefetch`
pair is there because no hardware prefetcher follows a pointer chain; it earns
3-9%.

**Nothing here is a bottleneck.** At a heavy 5000 particles - the biggest single
burst in the tree is 500 - `update()` costs about 0.05 ms of a 20 ms tick and
filling the render buffer about 0.17 ms of a 16.7 ms frame. This entry exists so
the next person does not re-derive the options and, above all, does not
re-discover the two traps.

The struct is already single precision throughout; doubles would cost 16-33%.

Measured, ns per particle-tick:

| container | order | native 5k | native 20k | wasm 5k |
| --- | --- | --- | --- | --- |
| `std::list` + malloc (today) | preserved | 7.9 | 23.0 | 10.3 |
| `std::list` + pool allocator (LIFO) | preserved | 6.2 | 22.0 | 8.9 |
| `std::vector` + swap-and-pop | **unstable** | 4.2 | 4.6 | 6.7 |
| `std::vector` + stable compaction | preserved | 10.3 | 11.4 | 14.7 |
| `std::vector` + tombstones, compact every 16 | preserved | 3.8 | 5.3 | - |
| ring of buckets keyed by death tick | preserved | 4.2 | 4.9 | 5.9 |

**Trap one: swap-and-pop breaks the picture.** Two of the three systems are alpha
blended and therefore order dependent; only the fire system is additive. What
swap-and-pop produces is not a wrong order but an *unstable* one - a particle
jumps from the end of the array into the middle the instant a neighbour dies, so
the layering of overlapping sprites changes between frames, which reads as
flicker. Over 800 ticks at 5000 particles the survivors' relative order was
unchanged in 800 frames for the list and in **19** for swap-and-pop.

**Trap two: a pool allocator buys much less than it looks like it should.** It
fixes where the nodes are, not the order they are visited in. After 300 ticks of
churn, forward-and-within-128-bytes steps collapse from 100% to 3.0% for malloc
and 0.0% for the pool. Worth about 1.3x at 5000 and 1.02x at 20000, less in the
browser, where Emscripten's dlmalloc already packs same-size nodes densely. Still
the cheapest thing on this list: about thirty lines, never touches draw order, and
gives a fixed memory ceiling. A **FIFO** free list looks better (98.2% forward,
1.64x) and is an illusion - that only holds while the pool is far larger than the
live set. Size it to the maximum, which is the point of having one, and
wrap-around shuffles the free list back to 0.0% forward and 1.16x.

**The one that works: a ring of buckets keyed by the absolute death tick.**
`bucket = deathTick & (WHEEL-1)`, so nothing ever moves. Each tick sweeps buckets
`now` through `now + WHEEL-1` and then `clear()`s bucket `now` - death is O(1) for
the whole cohort and the inner loop has no branch at all. That is why it beats
even swap-and-pop in the browser.

Its order is stable, and the reason is worth writing down because getting it
backwards is easy and silent: **sweep `now` first, not last.** A live particle's
position in the sweep is exactly its remaining lifetime, and every lifetime falls
by one each tick, so no two can swap. Sweeping `now+1 ... now+WHEEL` instead puts
the about-to-die bucket at the end and every particle jumps to the back on its
final frame - measured at 19/800 stable, indistinguishable from swap-and-pop.

What it would cost:

- **Memory**: 13.1x the live count in capacity, 5.0 MB at 5000 particles against
  a vector's 0.8 MB, because each of the 512 buckets keeps its own historical peak.
- **`WHEEL` must exceed the longest lifetime.** The longest in the tree is
  `random(150, 300)`, so 512 has room - but anything longer would silently alias
  into a bucket that dies early. Wants an assert in `addParticle`.
- **The `p.size <= 0` early death** becomes a death tick computed at insertion,
  `min(lifetime, ceil(size / -deltaSize))`. Nine emitters have a negative
  `deltaSize`, and float accumulation could put it one tick off.
- **Order becomes remaining-lifetime order rather than creation order.** Stable,
  but different: a burst sharing one lifetime is unchanged, `random(20, 50)` is
  not.

`getNewParticle()` has no callers anywhere; it returns `&particles.back()`, which
was the only thing requiring stable addresses, so nothing blocks a vector.

One smaller note from the same measurements: in `render()`, `sinf`/`cosf` per
particle is 54-61% of the fill loop, and since `rotation` advances by a constant
every tick the pair could be advanced by a fixed rotation instead of recomputed.


14. An import overwrites, unless the name is one the game ships  - **DONE**
-----------------------------------------------------------------------------
An import that names a file the player already has replaces it, and one that
names a shipped file is refused - a skin's filename *is* its identity
(`skin0="space"` looks for `space.zip`), and the weaker form holds for the rest.
Re-importing an updated campaign keeps its progress, since `ProgressDB` is keyed
by the campaign filename; the toast says **Replaced** rather than **imported**,
which is the only sign the player gets. The target name and the shipped check
come before `isImportableArchive`, so a damaged archive carrying a good one's name
leaves the good one alone.


15. Let the Export dialog delete what it lists  - **DONE**, in the Manager
---------------------------------------------------------------------------
Done as part of item 17, with a confirmation pane and the shipped-content check
gating it. A deleted campaign leaves its `ProgressDB` rows behind, which is right
if it is ever imported again; a deleted skin breaks the levels naming it,
visibly, through `Level::loadSkin`'s toast.


16. Reset one control instead of all of them  - **DONE**
----------------------------------------------------------
`resetAction(name)` off `Action`'s `defaultPrimary` / `defaultSecondary`, as two
stacked buttons as wide as the list, because a `Reset: selected all` row could
not be sized for both languages. Found on the way: key rebinding did not work in
the browser at all, because the grab was a `while` loop around `SDL_PumpEvents`
and nothing reaches the event queue while C holds the thread. It is a state
machine now, `beginKeyGrab()` / `pollKeyGrab()`, advanced by the ordinary main
loop (`input.md`).


17. One Manager button instead of Import and Export  - **DONE**
------------------------------------------------------------------
One 80x80 *Manager* button and one dialog that imports, exports and deletes:
Import had no dialog and Export had nothing but a list, and together each fixes
what the other lacked. `pollImport` runs every tick because the browser's dialog
cannot be modal, so a completed import switches the kind radio, re-reads the list
and selects the new entry. Escape belongs to the topmost pane - confirmation, then
Manager, then quit - since an Escape with the export pane open once quit the game.


18. A switch should flash when it is thrown  - **DONE**
---------------------------------------------
Brightening a sprite's colour cannot work - there is no brighter than white in a
colour clamped to [0, 1] - so `Object::render()` draws the sprites a second time
additively while `flashAmount` decays (`FLASH_STRENGTH` / `FLASH_DECAY` in
`object.cpp`, decayed per tick in `frameBegin()`). Three things not to undo: the
pass is gated on `layer == flashLayer`, 1 in `Object` and 0 in `Panel`, because
the objects lying on the ground draw on layer 0; the activator block flashes too
and lost its old darkening wobble; and `flash()` is not called from
`Object::onTouchedByPlayer`, which fires for every block the player bumps into.
`WebBuild/test/burst.js` checks it in the browser with `FLASH_DECAY` raised.


19. Playable on a phone
-----------------------
`WebBuild/touch_controls.js` is the pad: a four-way d-pad low in the left
letterbox bar, Swap above it, Bomb and Put in the right bar, Esc, F5, F10 and a
fullscreen toggle high on the right where a mis-hit costs nothing. It sends fixed
keys as ordinary `keydown` / `keyup` on the document - the one route into the
action layer, since `Engine::setKeyData` writes the raw layer the GUI reads and
the named actions never see - held for as long as the finger is down, because
`updateVKs` samples once per tick; and it shows itself by a guess from the pointer
corrected by behaviour: a real key hides it, a touch brings it back, `?pad=on|off`
overrides and is remembered. The page takes the fullscreen on the first gesture
and asks for landscape (`window.md`). Measured on an emulated Pixel 7: every
control reaches its action and clears on release.

Still missing:

1. **Text fields.** `GUI_EditBox` reads `event.keysym.unicode` out of SDL key
   events, and a phone only shows its keyboard for a focused DOM element - the
   canvas is not one. So a real `<input>`, positioned over the field and focused
   when the field is, with what it receives fed back as key events. The most edge
   cases (autocorrect, IME, the keyboard covering the field) and the least payoff:
   it is only needed for naming a level or a campaign.

2. **Hitting things.** See item 22 - the buttons are the problem the pad does not
   solve.

3. **Haptic feedback on the pad's buttons.** A glass button gives a finger nothing
   back. `navigator.vibrate` is the whole mechanism, a few milliseconds on
   `pointerdown`, guarded so it is silently nothing where the API is absent. Only
   on the state change, never on the 80 ms repeat - `setDirection` already fires
   once per change; switchable off, through `localStorage` like the `b5pad` key,
   since the pad knows nothing of `config.xml`; and Android only, since Safari on
   iOS has no `navigator.vibrate`. A reason to keep it small, not to skip it.

4. **A place to switch the pad inside the game.** The pad remembers `?pad=` and
   its own behaviour in `localStorage`, and the options dialog knows nothing of
   it: a checkbox there needs the C++-to-page channel `publishLanguage()` opened,
   carrying one more value.


20. A freshly built data.zip breaks the browser  - **DONE**, and it was never the zip
--------------------------------------------------------------------------------------
`WebBuild/build.sh` piped `em++` through `tail` and tested `tail`'s status, then
asked whether `blocks5.wasm` existed - which it did, from the run before. `em++`
writes `blocks5.data` before `wasm-ld` runs and the byte offsets into it live in
`blocks5.js`, so a failed link leaves a fresh data bundle beside stale offsets and
every preloaded file is sliced in the wrong place. Fixed with `${PIPESTATUS[0]}`;
the lesson is in `CLAUDE.md`: a check that can pass on a previous run's artifact
is worse than no check, and this one cost a day of looking at zip files.


21. The browser build on a phone  - **DONE** for the page, item 19 owes the controls
--------------------------------------------------------------------------------------
`shell.html` replaces Emscripten's desktop page: the viewport meta,
`touch-action: none`, `overscroll-behavior: none`; `-sINITIAL_MEMORY` down from
256 MiB to a measured 48; `manifest.json` and `sw.js` for an installable, offline,
cached app (`web.md` has the stamp-and-cache story); `navigator.storage.persist()`
for the saves. Two bugs in the game came out of the touch test, and either fix
alone changes nothing: `GUI::update()` recomputed `p_elementAtCursor` at the
bottom, so a click went to whatever had been under the cursor a tick earlier, and
`Engine` only took the cursor position from `SDL_MOUSEMOTION`, which a touch never
produces.


22. A finger is not a point: hit testing with a tap radius
----------------------------------------------------------
Item 21 got taps to land where the finger is. What it did not do is make the
targets big enough for a finger: `GUI::getElementAt` tests a single point, which
is right for a mouse and wrong for a fingertip whose contact patch is eight to ten
millimetres across. The accessibility guidance everybody uses is a minimum target
of about 44 CSS px.

**The numbers say how far off it is.** The GUI is laid out in the game's 640x480
space - buttons are eighteen pixels high, the Manager's bottom row is four 92x20
buttons - and that space is letterboxed into the canvas. On an emulated Pixel 7 in
landscape the present rect is 549x412, a scale of 0.858, so an eighteen-pixel
button is **15 CSS px** on the glass: about a third of the recommended minimum. On
a narrower phone it is worse, because the scale is `min(w/640, h/480)` and the
height usually binds.

**What unit the radius lives in.** It is a property of a finger, so it belongs in
CSS pixels, and the conversion into game coordinates is the inverse of the present
transform:

    scale      = pw / 640.0        // from Engine::computePresentRect
    radiusGame = radiusCss / scale

That is the "the smaller the game renders, the bigger the circle has to be"
intuition, falling straight out of the transform rather than needing a second
rule. It works because `b5_fitCanvas` sizes the drawing buffer in CSS pixels, so
canvas pixels and CSS pixels are the same thing here - **if that ever becomes a
device-pixel-ratio-sized buffer, this formula changes with it.** With the barrel
distortion on the radius is not constant across the picture; near the edge it
should be divided by the local derivative of `warpToSource`, or simply left alone,
since the CRT filter is a desktop indulgence.

**The sampling idea**: lay a fixed grid over a disc of that radius around the tap,
run the ordinary `getElementAt` at each sample, and count the votes. It inherits
everything the point test knows - z-order, and `containsPoint` being virtual so a
checkbox is hit on its caption too - and a 5x5 or 7x7 grid clipped to the disc is
21 to 37 lookups once per tap, which is nothing.

Four things to get right, three of which "highest count wins" gets wrong:

- **A large element must not outvote a small one it surrounds.** A pane behind a
  button wins on area every time. Two composable fixes: count only elements that
  are active and really visible, and weight each sample by its distance from the
  centre.
- **The exact hit still wins.** If the centre sample lands on an active element,
  take it and do not vote. That makes this a *fallback* for a near miss rather
  than a reinterpretation of every tap, and it cannot make an accurate tap worse.
- **Only for touch.** A mouse is exact and must stay exact. SDL 1.2 has no flag,
  but Emscripten's SDL pushes an `SDL_FINGERDOWN` alongside the synthetic mouse
  event, so the information is there; failing that, `pre.js` can set one.
- **The cursor itself must not move.** Only the element receiving the click is
  chosen by the vote; `cursorPosition` stays where the finger landed, or the level
  editor would place tiles somewhere other than where you touched.

**The cheaper alternative worth measuring against it**: grow each candidate's hit
area by the radius, keep those that then contain the point, and pick the one whose
true distance is smallest. O(elements), no sampling, exactly "the nearest target
within a finger's reach", and no grid resolution to tune; the sampling version is
easier to trust where `containsPoint` is overridden into a non-rectangular shape.
Both hang off the one line in `GUI::update()` that computes `p_elementAtCursor`.

`WebBuild/test/mobile.js` is where this gets its test: tap a few pixels *outside*
a small button and expect it to fire.


23. The hint note should be a sheet of paper  - **DONE**
--------------------------------------------------------
The text is baked into the paper: sheet and text render together into one
512x512 texture, borrowed from a pool the Engine owns (it falls with the FBO, and
an `Object` dies long after), and from then on there is one thing on screen,
which can be bent - the top and bottom wound onto a cylinder that unrolls, 48
bands each. `ROLL_TURNS` is at most half a turn, because the pass has no depth
buffer and only up to a half turn does back-to-front painting stay correct.
Whether a note rolls is a property of the picture, so `hintscroll.txt` beside the
`hint.png` that is really loaded says so - a marker, not a tileset attribute,
because every skin slot is chosen on its own. `objects.md` has the rest. Left
undone deliberately: no mipmaps, which the cube crossfade wants too.


24. The diamond machine's conversion deserves a real effect  - **DONE**
------------------------------------------------------------------------
The block is taken apart and put back together: sparks in its own colours,
sampled texel by texel through `Sprites::sample()`, fly out and come back as the
colour of the diamond, while the block fades to `CONVERSION_GHOST`. Out-and-back
beat a spiral, plain dust beat glowing sparks (the machine is handed rock and
grass as readily as metal), and the block owns `conversionProgress` and clears it
in its own `frameBegin()` - not once it is dying, or it snaps back over the new
diamond. `Particle` gained `uint id` after all, because an abandoned conversion
runs its sparks backwards and the machine has to find its own. Test level:
`Tools/testlevels/diamondmachine.xml`.


25. The diamond machine's sound handle is never set  - **DONE**
----------------------------------------------------------------
`p_soundInst` takes `playSound()`'s return now, and an abandoned conversion
slides the sound out, volume and pitch at `SOUND_FADE_SPEED`, instead of letting
a one-shot play to the end; `Sound::isLiveInstance` guards the handle, since a
one-shot may have finished on its own.


26. The shipped content should not be copied into the user's folder  - **DONE**
--------------------------------------------------------------------
Two roots, and the game folder is asked first: `FileSystem::resolveContentPath`
is the one place that knows, `isShippedContent` ("it exists in the game folder")
replaced the hand-written list in `Transfer::isBuiltIn`, and the one-time copy in
`main.cpp` is gone. Seven files belong to the player and reverse the order - the
two example levels and the five `readme.txt`, `FileSystem::getPlayerFiles`. Both
editors refuse to save under a shipped name, since the game folder would answer
first for ever after; `retireShadowingCopies` renames old copies to `.bak` rather
than deleting them. The trap that was not in this item: `ProgressDB` keyed on the
campaign's full path and would have silently reset everyone's 42 levels; it keys
on the bare filename now. `filesystem.md` has the rest.


27. A Credits button in the main menu - **DONE** but for the line itself
-------------------------------------------------------------------------
The credits are reachable by finishing the shipped campaign or from the main
menu. The visible entry - a `Credits` line at the foot of `menu.xml` in the style
of the `Website` link - is the author's to add, and it needs nothing of this:
`setGameState("GS_Credits")` is the whole of it, and every way in takes that
path. What this item was really about is the second presentation, and
`GS_Credits` now has two versions of itself:

- **Finished the shipped campaign** - the whole sequence, and byte-for-byte the
  one that ran after the last level before this item: proved by forcing the flag
  on and comparing the oracle's `credits` frame against the previous binary's.
- **Not finished** - the names on flat black. No `$C_THANKS_FOR_PLAYING` and no
  `$C_STAY_TUNED`, which address someone who has just won and give away that
  there is an ending to reach; no star field, no gradient and no motion-blur
  buffer, so nothing flies and nothing is loaded to make it fly - not the
  `title.xml` the stars are cut from, not the frame-copy texture; no
  `credits.ogg`; and no per-character zoom, so the blocks only fade in and out.
  It keeps the mouse cursor, and any click or any of the four keys the ending
  fast-forwards on - Return, Num Enter, Escape, Space - leaves it at once,
  because it is a screen offered from the menu rather than an ending being
  watched. Not scrolling text, which would have been a second layout for
  nothing.

**Who decides, and where.** `Campaign::isBuiltInCompleted()` answers it in
`onEnter`, unless the caller said so outright in the `ParameterBlock`. Nothing a
player takes has to say it - a `Credits` entry in the menu leaves it, and the way
in from the last level is right for free, because the level just finished is
already in the database. Two callers do say it: **Shift+C** in the menu is the
plain version and **Ctrl+Shift+C** the ending, so that which one the author is
looking at does not depend on what their own save file holds; and the frame
oracle's `credits` scene asks for the ending through the same parameter on the
`state` hook. The bar is not simply "all levels": where the campaign
has a bonus level it is `getLevels().size() - 1`, the count that unlocks that
level in `GS_Game::loadLevel` and `GS_SelectLevel::getLevelStatus` alike. The
bonus is extra rather than the end of the run, so beating the other forty-one
counts - and a player who did reach the credits by playing is past the bar either
way, since the level just finished is written to the database before `GS_Game`
hands over. Every answer but yes is false, and the ways to get one - no archive,
an archive that will not parse, an empty campaign, no progress at all - are all
the same to the caller.

**The timeline is laid out rather than written down.** The table of eight blocks
left `onRender` for the file scope and grew two fields, `lineSpacing` (which was
`i == 5 || i == 6`) and `ending`, which marks the two blocks the short version
drops. `onEnter` then computes three numbers from whatever is left: `shift`, what
the first block shown is moved to and everything behind it with it; `fadeAt`,
when the last block shown has gone; and `endAt`, the fade plus five seconds in
the ending, which is what the three `character*.ogg` goodbyes need. Full gives
0 / 53 / 58, which is exactly what was hardcoded; plain gives 5.5 / 35.5 / 36.5.

**A lead-in needs something to lead in to.** The ending opens on two seconds of
star field while the screen fades up from black and two more before the thanks,
which is an establishing shot. The same four seconds in the plain version are
black fading up from black and then black, which nobody can tell from a game
that has hung - so its clock starts at 0 rather than at -2000, and the lead-in
it keeps is half a second: the star wipe it arrives behind takes 0.85 s, and a
name fading up under a wipe that is still running reads as one thing rather than
two. The tail goes the same way: the fade to black over a screen that is already
black is a fade from black to black, so it gets one second as a beat before the
wipe out, where the ending gets five. 39 seconds of which 6 were nothing, to
36.5 of which the first half second is the wipe coming in.

**Both ways in and out of the menu take the star wipe**, the one the menu goes
behind everywhere else (`CF_Star`, 0.85 s). Out is both exits, the clock running
out and the player saying enough: the second is the one that would otherwise cut,
and a screen that can be left at any moment is exactly where a cut shows. The way
in from the last level of the campaign keeps its own `CF_ColorBlend`.

**The music is the other half of "minimalistic".** The short version does not play
`credits.ogg`: it is run from the menu, the menu's own track is playing, and
swapping it would announce an ending the player has not reached - and stop the
menu music dead on the way back, since `playMusic` resumes a track it never left.

**What the checks cover.** `frames.sh` renders both: `credits` asks for the
ending and `credits-plain` takes what the private home's empty progress gives.
Neither can see which *key* asked for which, so `smoke.sh` drives the two chords
and reads the answer off the one behaviour that separates them - a click or
Escape leaves the plain version, where the ending takes neither as an exit. Both
are ignored for the tick the screen is entered in: a menu entry answers a click
or a Return, `GUI::update()` dispatches it, `processGameStateChanges()` runs
`onEnter` and `onUpdate` follows, all inside the tick whose press bits are
cleared only at its foot.


28. Video recording in the browser
----------------------------------
Screenshots work there (`img_save.cpp` writes the PNG, `WebTransfer::
downloadBytes` delivers it), and `$A_TOGGLE_CAPTURE_VIDEO` is the one action
`main.cpp` still withholds from the web build. Four things stand in the way, and
only two of them are real.

- **The action is not registered.** One `#ifndef __EMSCRIPTEN__`.
- **The recorder is stubbed.** `WebBuild/build.sh` filters `videorecorder.cpp`
  out and links `videorecorder_stub.cpp`, whose `getError()` answers `true`;
  minih264, minimp4 and shine are not in that build either.
- **The encoder runs on its own thread.** `videorecorder.cpp` calls
  `SDL_CreateThread` and blocks on `SDL_SemWaitTimeout`, and `streamedsound.cpp`
  already writes down what that does here: `SDL_CreateThread` aborts and
  Emscripten's SDL has no semaphores at all. The build passes no `-pthread`.
- **Nothing captures the audio.** `audiocapture.cpp`'s browser branch reports
  silence - and its old reason was wrong, since a page can hear its own output:
  every OpenAL source connects to `AL.currentCtx.gain`, and one extra connection
  from that node to a `createMediaStreamDestination()` is the finished mix, the
  same thing WASAPI loopback gives under Windows.

**The route to take: let the browser encode, off an offscreen 2D canvas.**
`glReadPixels` at 640x480 stays as it is - so does the cursor `Engine` draws into
that buffer - and the frame goes into a 2D canvas nothing displays;
`canvas.captureStream()` on that canvas plus the audio track above is a
`MediaStream`, and `MediaRecorder` turns it into a file. No encoder in the wasm,
no thread, and the chunks are Blob parts the browser may spill to disk rather
than 22 MB of resident memory per minute. Capturing the game's canvas directly
would be simpler and is the wrong trade: it sees the composited, upscaled,
letterboxed picture, and recordings are deliberately the clean 640x480.

Three details that decide the work: `VideoRecorder`'s interface survives
unchanged (`isReadyForNextFrame()` / `getInputFrameBuffer()` /
`encodeNextFrame(timecode)` map onto "hand JS a heap buffer, then push it into
the canvas"), so `engine.cpp` needs no edit beyond the missing action and a
`WebBuild/videorecorder_web.cpp` replaces the stub; the frame arrives upside
down, so the flip happens while filling the `ImageData` or through
`createImageBitmap(..., { imageOrientation: "flipY" })`; and the container is the
browser's choice - WebM/VP8 everywhere, MP4/H.264 where `isTypeSupported` agrees,
a step down from the desktop's MP4 that a browser which recorded the file can at
least play back. The alternative, porting the existing encoder onto the logic
tick as `StreamedSound` gave up its decoder thread, keeps one code path and pays
with a full H.264 frame encoded thirty times a second inside the frame budget. A
backgrounded tab gets no frames either way; the recording simply stops there.


29. Eight new levels for 1.2.0, and a skin to put them in
---------------------------------------------------------
The shipped campaign has **42** levels, so eight more make it 50.

The list starts from what the campaign does not use. Three presets are placed in
no shipped level and are not spawned by anything either - `TeleporterNoPlayer`,
`ShieldedActivatorBlock` and `E_Multiplexer`. (`ToxicGas` is placed in none, but
that means nothing: `ToxicWaste` makes it when a barrel is destroyed, and nine
levels hold 39 barrels between them. What no level does is start with gas
already there.) The electronics family is thin everywhere:
`E_PulseSwitch` and `E_PulsePanel` live in one level between them, `E_HexDigit`
in two, `LightSwitch` in exactly one level with exactly one piece. `Syringe`
appears in three levels and `Eye`, `Spike` and `ShieldedBlock` in three each.

1. **Stock up before you go in.** Collect enough syringes first, then survive
   long enough inside the toxic gas to reach what is on the other side. The
   syringes are a supply, not a cure: `contamination` is allowed to go negative
   for exactly this, and `gs_game.cpp` only crackles and spreads toxin above
   zero.

   A `Hint` before the gas has to say so, in the shape of *"you will need enough
   protection"* - the mechanic is invisible otherwise, since nothing on screen
   counts the syringes and a player who walks in with two instead of five simply
   dies. Hint text is a `$ID` in a `<Text><![CDATA[...]]></Text>` child, resolved
   through `data/languages.txt`, so it needs an entry there with `§en:` and
   `§de:` bodies, named like the existing `$HINT_BLOCKS_NN_MM`.

   Placing the gas in the level file rather than bursting a barrel for it is the
   first time the campaign does that.
2. **The mask is worth more than the mask.** One mask, two gassed corridors, and
   the mask has to be dropped and fetched again - `inventory[2]` holds only one.
3. **A door that only blocks you.** `TeleporterNoPlayer` sends blocks somewhere
   the player cannot follow, so the way through has to be built remotely.
4. **Counting.** `E_HexDigit` as the visible goal: feed it a number with
   `E_BlockDetector` and `E_Gate`, and the exit opens on the right one.
5. **One switch, four places.** `E_Multiplexer` steering a single pulse train to
   one of several barrages, so the order of the throws is the puzzle.
6. **Light and mirrors.** `LightBarrierSender` and the receiver, with `Mirror`
   redirecting the beam and blocks casting the gaps.
7. **Everything on rails.** `Elevator` and `Rail` carrying blocks past `Spike`
   rows on a timing the player sets with `E_Clock`.
8. **The eye in the dark.** `Eye` plus `nightVision`, where what you cannot see
   is watching, and `LightSwitch` decides which of you is blind.

**A skin for them.** The four that ship are `blocks_01/02/03` - earth, brick and
grass - and `space`. Both themes that would fit these levels are indoors, which
is what neither existing family offers:

- **Laboratory or chemical plant.** Tiled walls, pipework, warning stripes.
  It covers the most of the list above at once - gas, syringe, mask, and the
  diamond machine reads as a centrifuge rather than as magic. The hint would be
  a clipboard on the wall, so no `hintscroll.txt` and no roll.
- **Inside the machine.** Circuit board green, gold traces, solder pads; the
  natural home for the `E_*` family, which is the thinnest part of the campaign.
  The hint would be a small display, again unrolled.

Of the two, the laboratory earns its keep across more levels; the circuit board
is closer to a single level's gimmick. Other themes that were considered and are
weaker for this set: ice cavern, volcano, temple ruins, sewers, greenhouse.

A skin needs `tileset.xml`, `sprites.png` and its own `hint.png`; see
`Level::loadSkin` and the packing rules in `Blocks5/pack.sh`.


30. Let a skin override the sound effects too
---------------------------------------------
A skin replaces everything a level *looks* like and nothing it *sounds* like. The
laboratory of item 29 would want its own door, its own machine, its own alarm,
and a skin somebody else writes has no way to bring them.

The two halves of the game meet nowhere at the moment, and that is the whole of
the work. Pictures go through `Level::getSkinFilename(SKIN_*)`, which walks the
loose folder, then `default_<name>`, then the archive, and answers with the
*final* path - that is what makes `blocks_02` reach `blocks_01`'s paper through
`default_hint.png`. Sounds go through `Engine::playSound(filename)` straight into
`Manager<Sound>::inst().request(filename)`, which resolves against the asset root
mounted in `main.cpp` and therefore always lands inside `data.zip`. Nothing in
that path knows a level is loaded, let alone which skin it wears.

The shape that fits the tree: keep `playSound` taking a bare filename, and give
the resolution a hook - the level, when it has a skin, answers "this name comes
from here instead". `p_skinFilenames` is a fixed table of eleven entries, one per
`SKIN_*` slot, so sounds cannot join it as they are: there are fifty-odd effects
and a skin would override two or three. A per-skin `sounds.xml` listing only what
it replaces is the smaller answer, and it can share the file `data/sounds.xml`
already uses for playback gains rather than inventing a second format.

Four things will need deciding, and each is a trap:

- **Which sounds may be overridden.** A skin taking over `screenshot.ogg` or the
  menu jingle is nobody's idea of a skin. The set that belongs to the *level* -
  blocks, machines, doors, weather - is not currently marked as such anywhere.
- **`gs_loading.cpp` preloads every sound by name**, and `verify.py`'s `sounds`
  check enforces that a `playSound()` name is preloaded, or the first play is
  silent while the file is read. A skin's sounds are known only once a level is
  loaded, so they need loading at `Level::loadSkin` time, not at startup.
- **The cache is keyed by filename.** `Manager<Sound>` hands out one `Sound` per
  name; two skins overriding `push.ogg` differently would collide unless the key
  becomes the resolved path, which is what `getSkinFilename` already returns for
  pictures.
- **`Sound` looks up its playback gain once at construction** out of
  `data/sounds.xml` (`audio-video.md` has the mix). A skin's own file needs the
  same treatment, or an imported effect plays at whatever level it was exported
  at while the shipped ones sit 6 dB down.

The export side is free: `Transfer` copies a skin archive as it stands, so an
`.ogg` inside it travels with everything else.


31. Menu music that picks up where it left off, with a slider of its own
-------------------------------------------------------------------------
The first half is done: `Engine::playMusic` remembers where a track was stopped
(`musicStoppedAt`, filled by `stopMusic()` from the stream's read cursor) and
resumes there, so switching from the editor to a level and back no longer
restarts the piece from zero - which, with a track long enough to sit with,
turned into a nag.

Open: **a separate volume slider for the menu and the editors**, in case somebody
gets tired of the music that follows them around. `options.xml` has the one
`MusicVolume` scrollbar; this is a second beside it and a second key in
`config.xml`, since `<MusicVolume>` is taken. Its default is copied from the
existing music volume, which is renamed in the GUI to *in-game music* -
`$O_VOLUME_MUSIC` is the string; the config key and `Engine::musicVolume` need
not follow the label.


32. A sound when a hint note opens  - **DONE**
-----------------------------------
`hint.ogg` in the tick the note opens and `hintscroll.ogg` in the tick the paper
sets off, unrolling or rolling up, under `Level::isHintScroll()`; a motion cut
short fades its rustle out (`hint.cpp`). Both recordings went through the stock's
treatment (`audio-video.md`), and `hintscroll.wav` lost a 135 ms noise floor
ahead of the rustle that would have put the sound that far behind the paper.
Item 30 would give a skin's own panel its own sound.


33. Draw the keycap frames behind the text  - **DONE**
--------------------------------------------------------
A frame was drawn over the letters it surrounds, and where the text sits a row
high in its box - which a small font cannot always avoid, see the keycap notes in
`gui-text.md` - the top edge crossed the capitals. `Font::drawText` now puts the
frame quads up before the glyph quads, so the same overlap passes behind the
letters and cannot be seen.

It came to the two `renderer.quads` calls swapping places, as the entry
predicted: `buildText` had already collected the frames into the cached entry's
`keyBoxes` in the same walk that lays out the glyphs. The `setTexture` moved down
with the glyphs, since the frames carry no texture of their own and were only
replacing what it had just bound - which is where the editor scene's one draw
call went.

Measured against the frame oracle: of the nineteen scenes exactly the three with
a keycap in them moved, and by 37 pixels (`help`), 210 (`hint`) and 3 (`editor`).
The hint note is where it shows, its font being the one whose letters are taller
than their line: the frame's top edge ran through the ascenders of `Left Shift`
and now runs behind them. Draw calls a frame are unchanged in `help` and `hint`,
and one lower in `editor`.


34. Switch the language inside the hint editor  - **DONE**, without a switch
-----------------------------------------------------------------------------
The preview follows the caret: `languageAtCursor` looks for the last `§xx:`
marker before it, and `Hint::setPreviewLanguage` bakes the preview in that
language, falling back to the game's own where the caret stands in text every
language shares. So checking that a note fits its paper in both languages is
putting the caret in the other section. Nothing touches `Engine::setLanguage`,
so the player's setting and the editor's captions stay put, which answered both
open questions of this entry at once; `Engine::localizeString(text, language)`
is the overload it needed.


35. Close the hint note with a click, and spend the input that does it
-----------------------------------------------------------------------
Return and Escape put an open note away (`GameGUI::onKeyEvent` through
`Level::dismissDisplay`); a click does not, although a click is what a player
reaches for after the note has covered the play area they were looking at. The
pause already takes any key *and* any button - `wasAnyKeyPressed() ||
wasAnyButtonPressed()` in `GS_Game::onUpdate` - and the note should read the
same way.

**One input must do one thing, and today it does two.** The press that leaves
the pause also closes the note, and it moves the player besides: `GUI::update()`
runs before `p_gs->onUpdate()`, so `dismissDisplay()` has already run by the time
the resume is decided; and `Player::onUpdate` reads `wasActionPressed("$A_LEFT")`
and the rest - an edge, not a held state - in the same tick the resume clears
`paused`, so the key that resumes takes a step as well. The `else
if(!menuVisible)` chain around the resume protects only the three actions inside
it, and movement is not one of them.

The shape that fits: one notion of "this input has been spent this tick",
consulted by the GUI's key handler, by the dismissal and by the action layer -
not a third guard beside the two that already disagree. The key grab is the
precedent: it already says "the keyboard belongs to something else this tick",
and `Engine::update` acts on it by skipping `updateActions()`. Both gestures want
it, and in the same order: resume, then dismiss, then act. Closing the note must
not step either.


36. Let the details setting reach the text shadows
---------------------------------------------------
Every string is drawn three times: `Font::renderText` lays down two offset copies
in black before the text itself, and `Engine::getDetails()` is not asked about
it. Level rendering, the weather and the lightning all consult it; the font does
not, so the one thing drawn on every screen in the game ignores the setting
meant for exactly this.

`Font::Options::shadows` is where it would go, and its name is the first thing
to fix: it reads as a count and is an offset style. Both non-zero values draw
**two** samples - 2 gives (2,1) and (1,2), 1 gives (1,0) and (0,1) - and the
alpha is divided by the number of them, so dropping one is a matter of changing
`0.7 / numSamples`, not of leaving a hole. 0 already means none, which is what
the credits use.

Where it is worth most is the browser, though less than when this was written:
both builds lay a string out once and draw the cached arrays three times, so a
dropped sample saves a draw call per string per frame rather than a whole walk
over the text, which on a phone is still worth having and is no longer the
headline.

Two things to decide:

- **Whether the setting picks the sample count or the whole style.** One sample
  at an offset of (1,1) is cheaper than two and still reads as a shadow; two
  exist to soften the corner.
- **Who wins where a caller already asked.** `gui.cpp`, `hint.cpp` and the
  credits all set `shadows` themselves, so the details setting has to be a
  ceiling over what they ask for rather than a replacement - the credits' 0 must
  stay 0 at any detail level.


37. SDL 1.2 -> SDL 3, planned and deliberately not done
--------------------------------------------------------
Surveyed in depth and written up in `SDL3-MIGRATION.md` at the repository root.
**The decision was not to do it**, and the plan exists so that decision can be
revisited from evidence rather than from memory.

Why it is worth doing eventually: Emscripten's SDL 1.2 is a 134 KB hand-written
*JavaScript* reimplementation, and `WebBuild/platform_stubs.cpp` exists only to
patch its gaps; SDL 1.2 has had no upstream since 2013; 2.4 MB, 170 tracked
files, 67 `.vcxproj` entries and two vendored patches would leave the tree; and
`rememberWindowPlacement` and its neighbours, wholly inside `#ifdef _WIN32`
today, become portable and therefore testable.

Why not now: seven of the ten surveyed areas came back **hard** after adversarial
review, the estimate is 39-55 engineer-days plus about nine hours on a Windows
machine, and **not one MSVC compile happened anywhere in the survey** - the
largest cost centre is the least verified part, on the platform the game
actually ships. The failure mode is the expensive kind: SDL3 does not break this
tree loudly. The sharpest of the proven landmines is that
`LinuxBuild/linux_window.cpp` wraps its whole implementation in `#ifdef
SDL_VIDEO_DRIVER_X11`, a macro SDL3 does not publish - after a header rename both
functions compile to `return false` with no error and no warning, and Linux
fullscreen is simply gone.

**Item 54 made the browser half smaller since the plan was written.** The plan's
first kill switch was whether Emscripten's legacy GL emulation could be brought
up under a context SDL3 created, and one of its three landmines was GLU vanishing
from SDL3's Windows headers. There is no emulation and no GLU in the tree any
more: the browser build links plain WebGL, so what is left to prove is that the
context SDL3 creates is the one Emscripten's WebGL library serves. The plan says
so where each of those stood.

**Not a reason to do it, contrary to a first reading:** the licence. SDL 1.2 is
LGPL 2.1 and is statically linked, but the game is GPL v3 with its complete
source published, which more than satisfies LGPL 2.1 section 6 - see the LICENCE
section of `Blocks5/libs/sdl-1.2.15/PROVENANCE.txt`. `shine` is in the same
position. Nothing has to change.

Two things settled while planning, kept because they are cheap to lose and
expensive to rediscover:

- **Main callbacks do not require `SDL_MAIN_USE_CALLBACKS`.**
  `SDL_EnterAppMainCallbacks` is declared unconditionally, so `main.cpp` keeps
  its own `main()` and its SEH crash handler, and MSVC's `/Yu` rule - which
  silently discards anything written above `#include "pch.h"` - never gets a
  chance to bite.
- **A classic `main()` with `emscripten_set_main_loop_arg` works against SDL3 in
  the browser**, measured: 45 frames, 7 events, `driver=emscripten`, WebGL 1.0.
  So the callbacks restructure is a preference and SDL3 is the goal, which is
  what makes the retreat in the plan real.


38. A finger cannot scroll a list
----------------------------------
`GUI_ListBox` scrolls two ways and a phone has neither. `onMouseWheel` needs a
wheel, and the `GUI_ScrollBar` the constructor puts down the right-hand edge is
**16 pixels wide** - a quarter of the 48-pixel target a finger wants, and at a 2x
window still only 32 device pixels. A touch on the list body goes to
`onMouseDown`, which selects the item under it and nothing else. So on a phone
the entries past the bottom of the box are reachable only by hitting a 16-pixel
bar.

Three lists carry real content: the campaign list in the select screen, the
action list in the options, and the Manager's file list, which is as long as the
player's folder.

What is missing is a *gesture* layer, and that is the item rather than the list
box alone. The GUI knows down, up, move and wheel; a touch surface wants at
least drag-to-scroll with the press held back until the finger has moved less
than a threshold - otherwise every scroll also selects whatever it started on -
and probably a fling with friction after it, since the whole point is to cross a
long list quickly. Both belong above `GUI_ListBox`: a multi-line edit box and
the level editor's own field want the same distinction between a tap and a drag.

Two things to settle first:

- **Where the threshold lives.** `GUI::update()` already has the answer to "did
  the pointer move" and holds `p_elementAtCursor`; a drag that has passed the
  threshold has to reach the element as something other than a click, which
  means a new event and not a flag on the old one.
- **What a scrollbar is for afterwards.** Once the body scrolls, the bar is a
  position indicator that could stop being a control - which frees its 16
  pixels, and would be the first widget in the tree drawn for a phone rather
  than for a mouse.


39. The level editor paints a line to wherever the finger last was  - **DONE**
-------------------------------------------------------------------------------
`LevelEditorGUI::onMouseMove` interpolates from `oldCursor` so a fast drag leaves
a continuous stroke, and `oldCursor` was never cleared on release - invisible
with a mouse, a line between two strokes with a finger. `onMouseDown` sets it
now, on the press and to the press position, and only when `realDown` (the
interpolation calls `onMouseDown` for every cell with the flag false).
`WebBuild/test/editorstroke.js` is the check, and it has to be a browser, since
CDP can press without a move before it and a mouse cannot.


40. A pad button says which key it sends; it does not say what that does
------------------------------------------------------------------------
The on-screen pad's key buttons are labelled `Shift`, `Ctrl`, `Tab`, `F5`, `F10`
and `Esc` - the keys they dispatch - and not `Bomb`, `Put`, `Swap`, `Retry`,
`Hotel` and `Menu`. The reason is that the label has to be true, and only one of
the two always is: the pad sends a **fixed** key (`KEYS` in
`WebBuild/touch_controls.js`) and the game's action layer maps it, so a rebinding
in the options dialog moves the meaning and leaves the key alone. A button
reading `Bomb` after the player has bound something else to Shift is simply
wrong, and nothing would ever correct it. The other half of the argument is that
the game goes on naming keys by name - in the hints, in the help table, in the
options dialog - and a thumb should be able to find the button that sentence is
talking about. The words are the game's own, shortened from
`$VK_KEYBOARD_LCTRL` and its neighbours in `data/languages.txt`, and they follow
the language the game settled on rather than the browser's, through
`Engine::publishLanguage()`.

What is missing is the *other* information: what the key is for. The answer is a
symbol beside the name rather than instead of it - a fused bomb on the key bound
to `$A_PLANT_BOMB`, an inert one on `$A_PUT_DOWN_BOMB`, three figures with arrows
between them on `$A_SWITCH_CHARACTER`. A picture needs no translating, which is
the same reason the four arrows of the d-pad and the fullscreen button's corners
are drawn and not written (it keeps that file plain ASCII).

Two things to settle first, and the second is the real work:

- **Where the art comes from.** The pad is DOM and not GL, so it cannot sample
  the sprite sheet the game draws from - and the sheet is inside `data.zip`
  behind its password anyway, which no page script can open. So these are new
  drawings, inline in the page as SVG or as a data URI, in the idiom
  `WebBuild/make_icon.py` and `make_text.py` already use for the icons and the
  boot line: generated at build time from something committed, never fetched.

- **Which button gets which symbol.** That is a question about the *current*
  bindings, and the pad does not know them - by design, since not knowing is
  exactly what lets a rebinding work for free. Baking the symbol to the default
  binding would reintroduce the lie the key names were chosen to avoid, only in
  a form that cannot be read off the screen. So the pad needs the bindings
  pushed to it, which is the channel `publishLanguage()` has already opened: the
  same idea, carrying `$A_*` names per key instead of a language, republished
  whenever the options dialog changes one. Until that exists the symbols would
  be decoration that is right by luck.

Item 22 (tap radius) is the neighbour: both are about a button a finger has to
find, one by size and this one by what it says.


41. Blurred shadows in one pass, with a shader
-----------------------------------------------
Every shadow in the game is the same geometry drawn again in black at an offset:
the tile grid and the objects three times at (2,1), (1,2) and (2,2)
(`Level::render`), every string twice at the first two of those
(`Font::renderText`), each sample at `0.7 / numSamples` so that the copies sum to
one shadow. Two offset copies of a hard-edged glyph are not a blur, they are a
double image; what softness there is comes from the corner where the two overlap.
Draw the shadow **once** instead, in a fragment shader, with a real falloff.

It is worth doing twice over: one draw in place of two or three on everything the
game puts on the screen, and it retires the sample count that item 36 was going
to hang a detail setting on.

Three things are in the way. The fourth this entry once named - that there was no
shader path for ordinary drawing - went with item 54: everything draws through
the renderer's own program, so a shadow stage is an addition to it and not a new
kind of pipeline.

- **The atlases have no margin to blur into.** A shadow computed from the same
  texture fetch needs taps around the sample point, and the glyph rectangles in
  `data/font.xml` sit about five pixels apart with six pairs touching outright
  (measured over all 256 entries), so a tap would pick up the neighbouring
  letter. `sprites.png` and the tile sets are packed the same way. Either every
  atlas gains a margin - which moves every rectangle in every `*.xml`, skins
  other people made included - or the shadow comes from a silhouette rendered
  into a texture of its own, which is the second pass this item set out to
  avoid. Item 51 would repack the small textures anyway, and a margin is a
  question to settle in the same breath.

- **The shadow is not one silhouette.** `Level::render` draws the tile shadow and
  the object shadow from separate passes at the same offsets, objects opt out of
  it with `OF_NO_SHADOW`, and the text shadow has to cover the keycap frames,
  which carry no texture at all. A shader handed "the frame" would shadow
  everything on the screen, the GUI over it included.

- **What it should look like is a taste decision, not an optimisation.** The
  shadow today is two pixels down and to the right and hard-edged; a soft one
  changes the look of every screen in the game. `u_crt.cpp`'s halation is the
  precedent for both halves of that - two rings of four taps thresholded in
  linear light, and a slider rather than an imposition.


42. Clamp the vertex colour where it is free, not on the CPU  - **DONE**
------------------------------------------------------------------------
The game hands the renderer colours above 1 on purpose - the exploding bomb's
shine, the teleport swirl's red, the spark bursts - and relies on the clamp that
desktop GL applied before the texel multiply. The browser's emulation clamped
`colour * texel` instead, which ate every falloff, and a CPU clamp under `#ifdef
__EMSCRIPTEN__` stood in for it. The renderer's own vertex shader does it now on
every platform, `v_color = clamp(a_color, 0.0, 1.0)`, and `clampColor()` is gone
with the colour arrays it guarded (`rendering.md`).


43. One quad budget for all fonts, and a way to say "do not cache this"  - **DONE**
-----------------------------------------------------------------------------------
`QUAD_BUDGET` (8192 quads, 512 KB) is shared across every live font, evicting the
oldest entry wherever it lives, and a `cache` parameter on `renderText` that never
reaches the key exempts the credits, whose animated `charScaling` ran 0% of 244
lookups hit with 212 evictions; every other screen already hit 100%, so this is a
correct ceiling rather than a saving. The `SDL_GetTicks()` eviction stamp became
a counter, since the clock wraps at 49.7 days. The bigger half was `measureText`,
which walked the same strings and kept nothing; it is cached in two tiers now, so
a string that is drawn is measured for free and the help page went from 2448
walks to 66. `gui-text.md` has the caches.


44. Cache the GL state, or decide once and for all not to  - **DONE**, then superseded
--------------------------------------------------------------------------------------
A `GL::` state layer stood between the sprite batch and the renderer for a while,
and what it taught is worth keeping: the saving was never the redundant state
calls (28 of nearly five thousand a frame) but the batch flushes they caused - a
bind of the texture already bound broke the sprite batch around every shine, and
routing it through a cache took a night-vision level from 1.1 to 19 quads per
draw. Item 54's renderer keeps the record itself, compares the texture and the
blend per quad and flushes only on a real change, so the layer, its read-back
check and its `verify.py` checks all went with it.


45. The window creeps where the taskbar is not at the bottom  - **DONE**, unverified
-------------------------------------------------------------------------------------
`GetWindowPlacement`'s `rcNormalPosition` is in workspace coordinates and
`SetWindowPos` takes screen ones, so with the taskbar at the top or the left
every save and restore shifted the window by its height. `SetWindowPlacement`
takes the rectangle in the coordinates it came from, replays `showCmd` and puts
an off-screen window back on a screen. Two more came out of the same search: the
placement is remembered before a fullscreen switch, while the window is still
the one `config.xml` is about, and a maximize survives Alt+Enter twice because
`DIB_ResizeWindow` does nothing to a maximized window. All of it is inside
`#ifdef _WIN32`, so `Tools/syntax.sh` is the only check that has seen it;
`rememberWindowPlacement` logs both rectangles so that one line of `log.txt` says
whether a machine's work area starts at (0,0).


46. Open the loopback capture when a recording starts, not at every start
--------------------------------------------------------------------------
`Engine::init()` opens `AudioCapture` unconditionally and nothing closes it
before `Engine::exit()`. `startCapture()` and `stopCapture()` only move a
`capturing` flag; the device stays open and the thread keeps reading either
way. Under Linux that is a `pa_simple_read` on the monitor of the default sink
for the whole session, and under Windows a WASAPI loopback client for the whole
session - so a desktop that shows a recording indicator shows one the entire
time the game is running, whether or not anything is being recorded.

It is deliberate as it stands, and the reason is written where the loop reads:
*"Reading has to continue even while nothing is being recorded: otherwise the
server's buffer overflows and the next recording begins with music seconds
old."* Opening lazily therefore cannot be a matter of moving the `open()` call
- it has to answer that, either by accepting that the first recording starts
with whatever latency `pa_simple_new` costs, or by opening on the keypress and
throwing the first buffers away. Both are a restructuring of the ring and its
clock-based padding rather than a fix.


47. tellStream() reads the decoder thread's position without a lock
---------------------------------------------------------------------
`Engine::stopMusic()` calls `p_currentMusic->tellStream()`, which is
`ov_pcm_tell(&vorbisFile)`, on a `StreamedSound` whose decoder thread is still
running - the volume slide that ends it happens afterwards. That thread is
inside `ov_read` and `ov_pcm_seek` on the same `OggVorbis_File`, so the main
thread reads a field the decoder thread writes with nothing between them.

`ov_pcm_tell` is `return vf->pcm_offset;` on an aligned 64-bit field, so on
every platform this ships to the load is atomic in practice and the worst real
outcome is a resume position a fraction of a second stale - which the caller's
own comment already allows for (*"more or less, this just asks the audio
stream's read cursor"*). It is still a data race by the language's definition,
and `threadProc` right beside it already takes the trouble to ask OpenAL for
the pitch rather than read the member, *"which belongs to the main thread"* -
so the ownership rule is stated in the file and this is the one place that
breaks it. The cheap answer is a `pcmOffset` the decoder thread publishes under
the mutex the ring already has; the honest one is to say in the file that the
read is deliberate and why it is safe here. Item 31's resume, which now reads
that position, makes the answer worth having.


48. adjustText can break a line inside a keycap whose key name has a space
----------------------------------------------------------------------------
A keycap is an atom on the way **in**: `adjustText` finds the `</k>` that
closes a run, measures the whole run and moves it to the next line as one
piece. The backward search that picks the break point does not know that. When
a later word overruns, the walk goes back through `out` looking for the last
break character, skipping *elements* through `tagEndingAt` - and a space inside
a keycap run it has already appended is not an element, it is a space. It
breaks there.

`<k>Num Enter</k>` is exactly such a run, and `%BINDING{$A_SAVE_IN_HOTEL}`
expands to one. The result is `<k>Num` at the end of one line and `Enter</k>`
at the start of the next, with `buildText` opening the frame on the first line
and closing it on the second - a box drawn across a line break, which is the
one thing the atom rule exists to prevent.

Nothing in the shipped text hits it today: it needs the overflow to land with
no other break candidate between the keycap and the end of the line. A hint
note somebody writes, a longer translation or a rebound key is all it takes.
The fix is for the backward walk to know where a `<k>` run begins - skip back
over the whole run the way it skips back over a tag - rather than to forbid
spaces in a key name.


49. CLAUDE.md is 194 KB, and every session reads all of it  - **DONE**
-----------------------------------------------------------------------
The first pass took 16% off by cutting retellings and moving the reasoning about
one file into that file; item 53 then split the rest into path-scoped rule files.
Verified by extracting every identifier, filename and number from the old file
and diffing against the new one.


50. OpenGL 2.0 is a requirement now, not a hope  - **DONE**
------------------------------------------------------------
Framebuffer objects, GL 2.0 shaders and vertex buffer objects are what the game
is built on; `GLExtensions::init`, `createFrameBuffer` and `createUpscalerGL`
end the program with a message where one is missing. Vertex buffers are core in
GL 1.5 (2003), shaders in 2.0 (2004), FBOs an EXT from 2004, and both software
rasterizers the tree is tested against carry all three. Gone with the fallbacks:
`-nofbo`, `-noshader`, `useFrameBuffer` and its twenty branches, the 640x480
window pin, `Hint::renderNoteFlat`, `Upscaler::isAvailable` and the options
dialog's reflow. The message is the part worth getting right: it names the
missing group and the `GL_VERSION` / `GL_RENDERER` / `GL_VENDOR` strings,
because the case it is written for is Windows with no GPU driver in play - a
fresh installation, safe mode, a VM, an RDP session - falling back to the GDI
Generic renderer, OpenGL 1.1, and seeing "GDI Generic" in that box turns a
support mail into a self-fix. `fatalError()` (`fatalerror.h`) is the one way the
game gives up, written once per platform, in English because it runs before
`languages.txt` is loaded.


51. Throw every small texture into one atlas so a bind stops breaking the batch  - **DONE**
--------------------------------------------------------------------------------------------
The renderer batches until the texture or the blend changes, and the texture was
what changed: 18 of a level frame's 24 draws, 26 of the menu's 29. `TextureAtlas`
puts the pictures that can share into pages of 2048 square, capped by
`GL_MAX_TEXTURE_SIZE` and added on demand up to four, and the draws a frame fell
to this:

    menu    29.0 -> 6.0      night   24.0 -> 14.0
    plain   16.4 -> 7.4      lava    24.2 -> 14.2
    toxic   21.0 -> 10.0

**The page size is not the largest the machine would give**, and that was
measured rather than assumed. The resident set - `data/` and one skin - is 32
pictures and 8.7 Mtexel, of which 6.2 can be packed; two 2048 pages hold that,
where one 4096 page would allocate 64 MB to keep 25 MB of pictures. llvmpipe
reports 16384 and SwiftShader 8192, so two pages is what every machine tested
gets; the GL 2.0 spec guarantees only 64, which is why it is asked at startup
(`GLExtensions::maxTextureSize`) instead of assumed.

**A picture says at its request whether it tiles**, because that is the one
thing which decides whether it can share. `Manager<T>::request` carries the
resource type's options, and `Texture::WrapMode` is two:

    WM_WRAP    every quad's uv stays inside the picture, either by itself or
               because Renderer::tiledQuad cut the quad at its edges; packs
    WM_REPEAT  GL wraps it, so it needs a texture of its own

Only the weather is `WM_REPEAT` - rain, snow and the two clouds, whose uv is
rotated with the scroll, so the cuts a split would need are not axis-aligned in
screen space and the pieces would not be quads. The lava's two 16x16 tiles sit
in a page with the sprite sheet they were cut from: they span exactly one copy
at an offset, so `tiledQuad` cuts each into two or four pieces that sample one
copy each.

**The gutter is exact, not a fudge, and it carries the opposite edge.** Linear
filtering reaches one texel past the coordinate it was given and there are no
mipmaps anywhere in this game, so that one texel is the whole of what a wrap
mode decides - and `GL_REPEAT` is the only mode this game has ever had.
`GL_TEXTURE_WRAP_S` and `GL_TEXTURE_WRAP_T` are set nowhere in its history: the
2014 import has eight `glTexParameteri` calls and all eight are the min and mag
filters, and `95660bb`, the last commit before this work, has none either. So
every texture ran at GL's default. A gutter copying the picture's own edge would
return what `GL_CLAMP_TO_EDGE` returned, a mode nothing here ever sampled with;
one copying the opposite edge returns what `GL_REPEAT` did.

**And the sampling is bit for bit what it was**, because a page's edge is a
power of two: `px/pageEdge` and `origin/pageEdge` are both exact in float and
their sum is exactly `(px + origin)/pageEdge`. All twenty oracle scenes are
byte-identical with the atlas live, which is what proves the gutter and the
arithmetic together rather than arguing them.

**Nothing had to be told a picture had moved**, and that is the property the
whole design rests on: uv is written in the picture's own texels everywhere in
the tree and turned into the page's in `Renderer::pushQuad`, the one line every
quad passes through. So the tile grid's cache, the font's and the lightning's
stay valid across a repack, and no `layerDirty` is set.

**Fragmentation is repacked, and only when the room is needed.** A rectangle
given back is joined to any neighbour it makes a rectangle with; where a
reservation still cannot be met from the pieces, `Engine::update` repacks at the
top of the next tick - a point where the renderer holds nothing - by copying
each rectangle with its gutter to its new page with `glCopyTexSubImage2D`, both
ends in GL's own coordinates so nothing is flipped, and telling each picture
where it now is. Without the joining a smoke run repacked seven times; with it,
once, settling at two pages.

**A test-hooks build checks the rule on every quad** (`Renderer::checkTiling`):
a quad may sample outside [0,1] only from a texture declared `WM_REPEAT`. It
found one the twenty oracle scenes do not - `Crossfade` kept the frame copy's
texel scale and rebuilt a bare ref from it, dropping the flag that says that
ref's negative y wraps on purpose.

What is left of the texture changes is the four weather pictures and the frame
copies. Cropping the art would shrink the pages further, but not safely as a
blanket pass: a free crop moves the origin of 11 of the 28 packable pictures,
seven of them sheets addressed by hardcoded source coordinates, and trimming
only the right and bottom recovers 40 percentage points of the 41.

52. Documentation that describes one file belongs in that file  - **DONE**
-----------------------------------------------------------------------------
The rule stands in `CLAUDE.md`: orientation and what spans files there and in
the rule files, the reasoning about one file's internals in that file.
`u_crt.cpp`, `u_sharpfit.cpp` and `audiocapture.cpp` were the moves; the survey
found that most of what looked like duplication was already in the code, so the
rest was deletion. The ceiling to work against is `verify.py`'s `comments`
check, 50% of `//` lines to code lines on files of at least 100 code lines; a
`/* */` block counts as code there.


53. The detail in CLAUDE.md belongs in path-scoped rule files  - **DONE**
--------------------------------------------------------------------------------
`CLAUDE.md` is the orientation and `.claude/rules/*.md` the detail, one file per
area, each loading itself for the globs in its front matter. The rules every
source obeys stay in `CLAUDE.md` as one-liners, since a path-scoped rule cannot
help before its file is opened, and `objects.md` repeats the drawing rules in
short form because every object source loads it and none of them loads
`rendering.md`.


54. One renderer in place of the sprite batch and immediate mode  - **DONE**
-----------------------------------------------------------------
One `Renderer` that every draw goes through, batching by itself and flushing
only where the texture or the blend changes - the two things that alternate per
quad - with everything rarer (scissor, colour mask, stencil, an offscreen target,
a 3D projection) a scope that flushes at both ends and restores what it found.
The transform is baked on the CPU in GL's own float arithmetic, the vertex is 2D
and 32 bytes, one GL 2.0 program serves textured and flat drawing alike through
a built-in white texel, and painter's order holds by construction. Raw GL
survives only in the files that own it, inside a `Renderer::DirectGL` bracket,
and the browser build links plain WebGL with no emulation, so a fixed-function
call anywhere is an undefined symbol at that link. `rendering.md` has the design
and `perf.md` the numbers it bought.

Done in four stages, each proved on the frame oracle (`testing.md`): the
oracle's nineteen scenes first, then the renderer under the level, then every
remaining `glBegin`, then the emulation cut out of the browser build. Where a
pixel moved, the cause is one of four, and each is a fact about the renderer
now: points became discs from the built-in texture; the lightning's trapezoids
split along the other diagonal than an array-drawn quad did; the editor's
smoothed lines became one-pixel hairlines; and a corner or an interpolated texel
on a rounding boundary lands one level over where the vertex stage's float order
differs from the fixed function's. Items 42 and 44 went with it, and item 51's
atlas came after it on the ground it laid; the tag `render-baseline` marks the
last immediate-mode binary.


55. The laser beam leaves the emitter half a pixel beside its ruby  - **DONE**
------------------------------------------------------------------------------
A parity problem rather than a rounding one. Both emitters trace from
`getShownPositionInPixels() + 7.5`, the centre of pixel 7, while a 16-pixel cell
has its centre on the boundary between pixels 7 and 8 - and that boundary is the
line the art is drawn about: the laser's ruby has its two strong columns at 7 and
8, and the light barrier's lens is symmetric to the pixel about it in all four
shipped skins. A quad lights a pixel whose centre falls inside it, so the core
lit column 7 alone and the beam left the emitter beside its ruby.

The drawing adds `BEAM_DRAW_OFFSET` (`object.h`, the reasoning in `object.cpp`)
in `Laser::onRender` and `LightBarrierSender::onRender`, and every cross-width
there is now an even number of pixels, which at that centre is the only kind
whose edges land on pixel boundaries: an odd width puts its two edges on two
pixel centres and leaves the fill rule to decide, and a width under one pixel
passes between them and draws nothing. The laser's core is 2, its glow 6, its
caps 4 and 6; the light barrier's core is 2, its glow 4, its caps 2 and 4.
Measured on the oracle's `lava` and `night` scenes, for a beam pointing left, one
pointing right and one pointing down: the core lit cell pixel 7 alone before and
lights 7 and 8 equally now, with the glow symmetric about them.

What must not be undone: the traced points are untouched, because the hit test
reads the same list - `isFreeAt2`, `reflectLaser` and the `destroyTime`
countdown. `Level::renderBeamShines` needed nothing, since it already re-bases
the beam onto the cell's centre: it subtracts the 7.5 the trace added, and
`renderShine` centres its disc on the cell.

56. Drive the character with the mouse  - **DONE**
--------------------------------------------------
Drag a character to the tile it should go to and it walks there while the
button stays down. The drag begins on the character - the same press that
wakes one up takes hold of it, and a drag from empty ground steers nobody. A
drag carrying the right button plants a bomb in the direction of the step, one
carrying both puts a bomb down there - the two things `Player::onUpdate`
already did for a direction with Shift or Ctrl held. And a click on what the
character is standing next to works it, which is how a switch is reached with
the mouse.

It is a device rather than a special case: `Engine::updateMouseDrag` sets six
virtual keys the way a joystick hat's four are polled, and `Player` still only
ever asks for the action. What the engine cannot know is where anybody is, so
`GameState::getMouseDragCells` hands it the active character's cell and the
cursor's, and only `GS_Game` answers.

Four decisions the work turned on. The keys are **held, never pulsed**: a
press landing while an action's repeat counts down goes into that action's
buffer and is played out later, so pulsing would stack steps up and walk on
after the player let go. **One axis moves at a time**, committed until it runs
out, so the path is two straight legs - a staircase is no faster, but it is a
path nobody would walk by hand on the keyboard, so allowing it would be an
advantage for nothing. **A direction the character cannot go is never
commanded** (`canMouseDragStep`, and `move()` itself in `simulate` mode): a leg
that has walked into something is over as surely as one that has run out, and
the other axis takes over and walks round the obstacle - held against the wall
instead, the key would keep the leg alive and the axis that could still move
would never get its turn. `simulate` takes every decision `move()` takes and
performs none of them, the recursion into the pushed object included, so a
chain of blocks against a wall answers no at whatever depth the wall stands
while a single block with room behind it still answers yes. Asking beats trying
because trying is not free: a walk into a switch *works* it, so a drag that
felt its way round an obstacle would flip whatever it brushed. And the
**buttons are latched** when the drag sets off: on the way into a
two-button grip there is a tick with only the right button down, which is the
gesture for a *lit* bomb.

That last rule is what the click is for. A switch or a magnet is solid and
fixed and does its whole job in `onTouchedByPlayer`, which is reached by
walking into it, so refusing blocked directions would have put them out of a
mouse player's reach. `GS_Game::bumpCell` has two guards - orthogonally
adjacent, and only where the character *cannot* go there, so a click is never a
step and never a push - and leaves the rest to `Player::move`, which is the
point: a click reaches exactly what a walk that way reaches. A panel falls out
of it for nothing, being walked onto rather than into; so does a switch behind
a solid tile, which `move` refuses to touch through.

Two more that fell out of the plan rather than into it. The drag binds as an
action's **third** source, because both real slots are taken on all six actions
and worth keeping, and a gesture is its own binding - so `tertiary` is set from
`main.cpp` and neither the options dialog nor `config.xml` knows it exists. And
the six keys sit directly behind the keyboard block so that their base is a
constant: `main.cpp` registers its actions before `Engine::init` builds the
table, and a base discovered during init is still `-1` when the binding is
made. That one shipped broken once, silently, which is why
`LinuxBuild/test/drag.sh` exists - it reads the character's cell and the state
of the lights out of the test hook, because these gestures steer the level and
no widget can be asked whether they worked, and it plays a level of its own,
because the geometry a drag has to walk round is the test.

Two things drop a drag: the menu opening (`Game.ShowMenu`, which Escape and the
on-screen button both go through) and losing focus, which now clears the held
mouse buttons as it already cleared the held keys, since no release arrives for
either. `input.md` has the rest, and the in-game help lists both gestures.

57. One floating-point type  - **DONE**
---------------------------------------
One type for everything the game measures, and it is `float`. The values are
pixels (at most 640), cells (40 by 25), fractions of a tick and colour
components, and a float's 24-bit mantissa holds all of them exactly or to a part
in sixteen million. `Vec2d` and `Vec4d` are gone, their typedefs with them, and
so is the seam they sat on: 274 `Vec2d`, 360 `Vec4d` and 14 `static_cast<Vec4f>`
down to none, 54 `static_cast<double>` down to two.

**Two mentions of `double` are left in the whole of `Blocks5/src`**, and both are
the same thing: `EM_ASM_DOUBLE` and the lookahead beside it, where a JavaScript
number is an IEEE double and nothing else will do. The game's own arithmetic has
none.

Two changes got the last of them out. The wall clock became an **integer**:
`getExactTimeUS` counts microseconds since the first call, because every caller
reads the difference of two readings and a count cannot drift at all, where a
float of seconds steps by 244 us an hour in and a double only pushes that out
instead of removing it. Windows has `QueryPerformanceCounter`'s int64 and POSIX
`tv_sec`/`tv_nsec`, so two of the three platforms never see a floating-point
value; the browser converts once from `emscripten_get_now`, which is a JS number.

And every growing animation is formed from that integer: each is `rate * clock +
base`, a straight line in an exact counter, and `clockPhase`, `scrollOffset` and
`wrapTextureOffset` (`util.h`) are where that happens. All three are `float`,
which was measured rather than assumed: the clouds' one-texel-a-tick step comes
out exactly 1.0000 for as long as twelve hours in a single level, 0.5 to 1.5
after a day, a stutter after three, and `Level::time` restarts at every level.

Only the texture offsets reduce. A sine's argument does **not**: `sinf` and
`cosf` reduce against the real pi themselves and land 3e-08 from the true value
at any clock, where a `fmodf` by a float 2*pi first divides by a constant that is
1.7e-07 out and drifts with the turns discarded - 2.0e-03 after an hour in a
level. A texture offset is the opposite case: its period is exactly
representable, so the reduction is exact, and the wrap is what the shader needs
rather than the CPU. Nine call sites use the three: the five weather and title
scrollers, the lava's scroll and its two wobbles, and the CRT filter's flicker
and crawl.

The last class was invisible to a search for the word `double`. An unqualified
`sin(x)` on a float is the **C** function: `<cmath>` puts the float overloads in
`std::` and `<math.h>` puts only the double ones in the global namespace, so the
call widened to double, went through the double routine and narrowed back. 164
call sites across 33 files now read `sinf`, `cosf`, `floorf` and their cousins,
`Vec::length` takes `sqrtf`, and no object in the game refers to a double libm
function any more. `Tools/syntax.sh` gained the matching gate: a double handed
to a float now fails the run exactly as an integer handed to one does.

`Mat4` is `float` throughout. It still keeps the order of operations of the GL
and GLU calls it stands for, which costs nothing and makes the two readable
against each other, but not their precision - the only entries that differ are
`gluPerspective`'s depth row, `m[10]` and `m[14]`, while `m[0]` and `m[5]`,
which put a corner on the screen, read neither near nor far and are identical.

`static_cast<float>` went the other way, 201 to 225, and that is the gate
working rather than the change failing: an integer widening to a `double` is
silent in C++, but to a `float` it is MSVC's C4244 and GCC's `-Wconversion`, so
every int-to-float site the change created had to be written out. This is
`Tools/syntax.sh`'s gate reaching further, as the plan expected. TinyXML's 21
`QueryDoubleAttribute` became `QueryFloatAttribute`, the one edge where the
conversion could invert a test rather than round a number: it returns a status
code where `Attribute()` returns a pointer, and a site converted without
noticing that silently stopped reading `sounds.xml`.

Nine of the oracle's nineteen scenes moved, none by more than 8 channel levels
of 255, all of it speckle dominated by single levels. Three causes, and each is
now a fact about the game rather than a defect. `pointJitter`, the
`fract(sin(x) * large)` hash the beam shines take their per-point size from,
resolves 1/1024 in float against the double's 1/2^36, so it draws different but
equally uniform values - measured over 200 points of each of 2001 seeds: rms
0.5772 against the ideal 0.5774, deciles flat within 3%, neighbours correlated
at -0.002. That is `night`, `select`, and the `cube` that captures them. The
credits' stars round to a different subpixel. And a literal reading `0.1f` where
it read `0.1` moves a phase by a part in 10^8, which is the lava, the star wipe,
the toxic grid, the CRT filter and the options dialog, 8 to 40 pixels each.

The renderer's own corner baking stayed `float` on the same standard, measured
rather than assumed: against a sum promoted to `double` and rounded once, the
two differ for 41% of corners but by at most 0.000488 px, so 0.89% survive the
1/256 subpixel grid the rasterizer snaps to and no oracle scene moves a pixel.
It is four calls a quad in the renderer's hottest arithmetic, so the exactness
is not worth buying (`rendering.md`).


58. A collected item should go to the character, not just fade - **DONE**
--------------------------------------------------------------------------
A diamond, a bomb, a syringe or a gas mask vanished on the spot: `onCollect`
calls `disappear(0.2f)` and `Object::render` multiplies the alpha by
`deathCountDown`, so ten ticks later it was gone from where it lay. Nothing
carried the eye from the item to the character who now had it, and the two
counters at the bottom left flashed for a pickup the player never saw move.

It flies now, over those same ten ticks, turning and shrinking on the way out.
`Object::render` has a `collectFlight` branch beside the `falling` and
`teleporting` ones, and it hangs off the fade that was already there:
`1 - deathCountDown` is how far along the flight is, so nothing new is counted
and the two halves cannot come apart. What moves is the **shown position
alone** - the item was taken the moment `onCollect` returned, the cell is free,
and the order `Level::update` sorts and paints in reads the logical position,
so none of it is visible to anything that decides anything.

**Three numbers, all of them taste**, at the top of `object.cpp` with which way
to turn each: `COLLECT_EASE` (2.0) is the power the flight is eased with, 1
being a straight line and higher hanging longer where the item lay before
arriving faster; `COLLECT_SHRINK` (0.4) is how much of its size it has left at
the end; `COLLECT_SPIN` (180) is the turn it makes, in degrees, and 0 is none.
The turn is about the middle of the cell and not the corner the matrix stands
on, or the item swings away from the player it is being drawn into.

**Two things it had to get right**, both named in this entry before it was
done. The target is read every tick rather than snapshotted, because the player
walks on during the flight and an item aimed at where they were drifts off
behind them - and it is read by **UID** (`Level::getObjectByUID`, new and used
by this alone) rather than held as a pointer, since ten ticks is long enough
for a player to be blown up inside them and `removeOldObjects()` to delete it.
A collector that goes simply stops moving the target, and the item finishes on
the last place it was seen. Nothing in the flight draws a random number.

**Where it starts is the one thing this entry had wrong.** Not `onCollect`:
three classes override it and `StdObject`'s turns a second gas mask down,
leaving it lying, so a flight begun there would send an item that was never
taken. It begins where the collect is *noticed*, in `Object::update`, under
`!isAlive()` - `disappear()` being the one thing every accepting path does, the
bomb's own `onCollect` included.

No oracle scene catches it, as this entry said: a collected item is as
transient as `Damage` and `Projectile`, which no palette level can place. All
twenty scenes are byte-identical, which is the whole of what a check can say
here; the look went to the author.

59. The help screen's text runs out of its box - **DONE**, by one line
-----------------------------------------------------------------------
Page 1 did not fit. The frame around the text is `help.xml`'s deactivated
`EditBox`, and at 580x355 it held **23 lines** of the GUI font's 15 px, with the
text set 5 px inside it at either end. The page draws **24** - 23 hard lines in
`languages.txt` plus the *Goal* paragraph wrapping to two - so the last row,
"Window/fullscreen", was sliced in half by the bottom edge and its `Alt`+`Enter`
keycaps stood in the strip the OK button sits in. Nothing clips, so the text
simply spilled. Measured the same in **both languages**: German is the longer
one everywhere else in this file, but that paragraph happens to wrap to two
lines in each, and the German page is cut at the identical row.

The fix is the window, grown by exactly one line and nothing else: the frame
355 -> 370, everything below it down 15, the window 440 -> 455. The text keeps
its 5 px above the first line box and gets 5 px below the last, which is what
it always had at the top and never had at the bottom.

**That was the last line the screen can absorb**, and the arithmetic says so:
455 of 480 leaves 12 px above the window and 13 below (`<Center />` truncates an
odd half). Another line would be 470 in 480. So the entry the next added row
makes is not this one again - it is one of the two structural answers, kept
here for then:

- **Break the pages at the box.** Lay the text out, measure, and start a new
  page where the next line would cross the frame. The page count then follows
  the text instead of `Help::handleClick`'s hardcoded `page < 6`, and a section
  that fits stays exactly one page, so five of the six would not move. Worth
  knowing before doing it: the document is lopsided - page 1 carries 23 hard
  lines where pages 2 to 6 carry 8, 9, 7, 14 and 5 - so re-flowing the lot
  would fit it in about half the pages, at the cost of the authored breaks.
- **Clip the page element and give it a scrollbar.** Listed here once as the
  safety net under the first, and it is not one: with pagination by line a page
  cannot overflow, since a single line is never taller than the box. Two
  navigation models on one screen for a case that cannot arise.

**What guards it now**, since a fixed box means the next row overflows as
quietly as this one did: `smoke.sh` walks all six pages in both languages and
asserts each fits, and it is the *game's* answer it reads rather than a second
implementation of the wrap - the dump reports every `GUI_StaticText`'s laid-out
size, measured through the element's own font (`GUI_StaticText::measureDrawnText`,
which `onRender` and `containsPoint` now share). That is what makes it follow a
language switch and a rebound key, which no static check over `languages.txt`
could. Proved by running it against the old geometry: "English help page 1 is
360px of text in 345px of box - 15px past the frame", and the same for German.

How these connect
-----------------
   19 (controls) <--> 22 (tap radius): the pad answers the keys, 22 the buttons

   22 (tap radius) <--> 40 (pad labels): a button a finger has to find, by size
                      and by what it says; 40's symbols wait on the bindings
                      being pushed to the page, which publishLanguage() started

   36 (shadow detail) <--> 41 (one-pass shadows): 36 makes the sample count a
                      setting, 41 removes the count altogether, so 36 is worth
                      doing only while 41 is out of reach

   41 (one-pass shadows) <--> 51 (atlas): both repack the small textures, and a
                      blur margin is a question to settle in the same breath

   30 (skin sounds) <--> 32 (hint sound, done): the paper sound belongs to the
                      skin that brings the paper, so 32 is 30's first caller

    8 (performance) --> 54 (the renderer, done) --> 51 (atlas): what is left
                      of a frame is the texture changes, which 51 removes

   38 (gestures) <--> 39 (editor strokes, done): both are code that was correct
                      only because a mouse never teleports

   38 (gestures) <--> 56 (mouse drag): one recogniser for a drag, whether it
                      scrolls a list or steers Bob

   54 (the renderer, done) <--> 57 (one float type): the renderer computes in
                      floats already; 57 brings the game up to meet it

What left the tree along the way: `sdl.dll`, `sdl_image.dll`, `libpng15-15.dll`,
`zlib1.dll`, the four ffmpeg DLLs, `oalinst.exe`, `vcredist_x86.exe`,
`hq2x32.obj`, ten import libraries and the `msinttypes` shim; and with item 54
`gl_immediate.cpp`, `gl_compat.cpp`, `glstate.*`, `quadarray.*` and GLU. What
ships now is three executables, **one** DLL that needs nothing but Windows, and
the data.

60. Crop the art to the rectangles that are actually drawn  - **DONE**
----------------------------------------------------------------------
Item 51's pages were 6.2 Mtexel of pictures of which a good half was never
sampled: the sheets are powers of two with the art in one corner, and the
backdrops were 1024x512 for a 640x480 picture. Cropping the sources to what the
code addresses took the packable set from **5.30 M to 2.88 Mtexel, 46% off**,
and with it a session now runs on **one 2048 page and no repack at all**, where
it took two pages and a repack before. Twenty-eight files, 1.91 MB of PNG to
1.73 MB on disk.

**All twenty oracle scenes are byte-identical across the crop**, which is the
whole proof: a cut that had taken a texel something draws would have moved a
pixel. That includes the hint note, the one picture sampled at other than 1:1 -
it rolls and scales, so its edge texels are weighted rather than hit dead on.

**What "used" means here is what the code can address, not where the picture is
opaque**, and those differ in both directions: some buttons carry a deliberate
transparent margin, and `E_HexDigit` samples two rows that no skin has art in
(below). The sizes below come from the declarations that own them -
`tileset.xml`, the four font XMLs, the `u`/`v` on every `<Image>` in the dialog
XMLs - and from the source rectangles in the code, measured against an
instrumented run of all twenty oracle scenes, `smoke.sh` and `drag.sh` as a
cross-check.

**Every crop keeps the origin at (0, 0) and trims right and bottom only.**
Moving an origin would shift every hardcoded source coordinate in the tree, and
nothing would say so: seven of these are sheets addressed by literal
coordinates.

| `data/` | stored | used | fixed by |
| --- | --- | --- | --- |
| `buttons.png` | 512x1024 | 260x640 | menu/game/leveleditor/selectlevel.xml |
| `campaigneditor.png` | 1024x512 | 640x480 | backdrop over the screen |
| `credits_font.png` | 512x512 | 503x392 | `credits_font.xml` |
| `donate_button_de/en.png` | 128x64 | 100x43 | `menu.xml` button w/h |
| `donate_de/en.png` | 512x512 | 398x270 | `menu.xml` StaticImage |
| `font.png` | 512x256 | 509x186 | `font.xml` |
| `gui.png` | 256x256 | 192x240 | nine-slice frames, widget glyphs |
| `icons.png` | 256x128 | 220x100 | `help.xml`, `leveleditor.xml` |
| `languages.png` | 256x64 | 144x48 | `options.xml` |
| `lava_edges.png` | 128x64 | 80x48 | `lava.cpp`, twelve pieces |
| `lightning.png` | 256x16 | 248x16 | `lightning.cpp` (measured) |
| `logo.png` | 512x512 | whole | the splash, and `NEVER_PACK` |
| `menu.png`, `selectlevel.png` | 1024x512 | 640x480 | backdrops |
| `misc.png` | 256x256 | 218x176 | rewind OSD, 218x64 at (0,112) |
| `tooltip_font.png` | 512x128 | 507x105 | `tooltip_font.xml` |
| `window.png` | 32x32 | - | the window icon, not a texture |

| `levels/skins/*` | stored | used | fixed by |
| --- | --- | --- | --- |
| `background.png` | 1024x1024 | 640x**560** | 640x480 backdrop *plus* the 640x80 HUD strip at v 480..560 (`GS_Game::onRender`) |
| `hint.png` | 512x512 | 300x400 | `hint.cpp` NOTE_WIDTH/HEIGHT |
| `hintfont.png` | 512x256 | 508x247 | `hintfont.xml` |
| `particles.png` | 128x128 | 112x80 | measured only; check before cutting |
| `shine.png` | 128x128 | whole | |
| `sprites.png` | 256x1024 | 256x**720** | the object sheet, and exactly where its art ends |
| `tileset.png` | 128x128 | whole | `tileset.xml`, all four skins |
| `rain`, `snow`, `clouds`, `noise` | | **do not crop** | the first three tile, so the period is the size; the noise is sampled whole |

**Two things found on the way, each its own small item.**

`E_HexDigit` read each of its four inputs as a *number* rather than as a logic
level, and an `E_Value` or an `E_PulseSwitch` carries 0 to 7 - so the four
summed could reach 105, and `32*(value%8), 640 + 32*(value/8)` is then four
hundred rows below the sheet. Measured before the fix, the palette alone drove
it to rows 704 and **736**, where no skin has art. Reading each input as a level,
the way the gates' `&&` and `||` already do, bounds the sum at 15 by
construction; the deepest row any quad now reaches is **720**, which is exactly
where the art ends.

`title.png` was requested in `GS_Loading::loadGraphics()` and drawn nowhere -
no code path, no `<Image>`, no localized filename - and never released either.
It is the artwork `background.png` was built from rather than anything the game
shows; the picture the loading screen draws is `logo.png`. Both the file and the
request are gone, and half a megatexel with them.

