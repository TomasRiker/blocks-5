---
paths:
  - "Blocks5/src/{level,texture,tileset,sprite,linedrawer,particlesystem,lightning,lava,glextensions,glstate,quadarray,engine}.{cpp,h}"
  - "Blocks5/src/renderlayer.h"
  - "Blocks5/src/fatalerror.{cpp,h}"
  - "Blocks5/src/util.h"
  - "WebBuild/gl_compat.cpp"
  - "WebBuild/gl_immediate.cpp"
  - "WebBuild/compat.h"
---

# Rendering: the GL floor, the tile grid, the sprite batch and GL state

**The floor is GL 2.0 with framebuffer objects, and it is a floor, not a hope.** Buffers are core in GL
1.5 (2003), shaders in GL 2.0 (2004), framebuffer objects an EXT from 2004; both software rasterizers
tested against, llvmpipe and SwiftShader, carry all three, and in WebGL 1 they are core — so
`GLExtensions::init` resolves all three and stops where one is missing, and `createFrameBuffer` and
`createUpscalerGL` add the two failures a resolved entry point can still produce, a framebuffer that
will not complete and a shader that will not link. The message is written for a new machine in a
particular state, not an old one: Windows with no graphics driver in play — fresh installation, safe
mode, a VM, an RDP session — hands out `opengl32.dll`'s GDI Generic renderer, OpenGL 1.1, so the box
names the missing group with `GL_VERSION`, `GL_RENDERER` and `GL_VENDOR` and says to install the
driver, which turns a support mail into a self-fix. English, because `Engine::init` runs before
`main()` loads `languages.txt`.

**`fatalError()` (`fatalerror.h`) is the one way the game gives up**, written once per platform because
that is the whole of what differs: `MessageBoxA` under Windows, zenity or kdialog under Linux (the pair
the file dialogs reach for), a DOM overlay in the browser. The Linux half uses `fork`/`execlp` rather
than `system()`: the message carries strings the driver wrote, and an argument handed straight to the
program needs no quoting and can carry no command. `execlp` returns only where the program is missing,
so the child's `_exit(127)` is how the parent knows to try the other.

**The tile grid is a vertex array, built once and drawn three times.** `Level::renderTiles` writes the layer
into a `std::vector<QuadVertex>` whenever `layerDirty` says it changed and hands that to one
`glDrawArrays(GL_QUADS)` out of client memory. The vertices carry a position and a texture coordinate and
**no colour**, which is the point: `Level::render` makes three passes over a layer — two shadow samples and
the picture — differing in nothing but a `glColor` and a translate, so one built array serves all three. Six
places set `layerDirty`, covering every way a tile or the picture it is cut from can move; a tile id alone
would not, since the texture coordinates come from the `TileSet` and a skin change moves every tile without
moving an id. Measured on the title demo: the median frame's render half **2.1 → 1.7 ms**, spread 0.1 ms.

**A whole render pass of sprites is one draw call.** `Level::renderObjects` opens a batch around its object
loop (`Engine::beginSpriteBatch`); while one is open `Engine::renderSprite` appends four `ColorQuadVertex` —
position, texture coordinate, and a colour of its own — instead of drawing, and `flushSprites` puts the lot
up with one `glDrawArrays(GL_QUADS)`. The colour must be per vertex and not in `glColor`: every object
brings its own tint, death countdown and conversion ghost, and a shared colour would flush at every object.
Measured in the browser, `?nobatch=1` against the default: **276 draw calls a frame → 35** in a played
level, median frame **3.90 → 2.40 ms**, render half **2.40 → 1.10**, same vertex count — and those numbers
understate it, since they were taken while redundant state calls still broke the batch at every
`renderSprite(Texture*)`. `LinuxBuild/test/frames.sh` reports sprite-batch draws per frame and quads per
draw for five scenes on every run; `WebBuild/test/perf.js` reports real GL draw calls beside its
milliseconds.

**On a real phone the batch is worth six times as much as any desktop number says**: `?perf=1` on a level
with a full tile map and grass objects over the whole of it gave **57 ms a frame with `?nobatch=1` and 9 ms
without**. Against a 20 ms tick those are two different games. The gap belongs to the platform, not the
scene — swiftshader is capped by rasterizing, where a phone's main thread *is* the limit and each draw call
drags Emscripten's GL emulation through a stretch of JavaScript — so **read every browser number here as a
lower bound on what a phone gets**. That scene is the best case: `-nobatch` does not touch the tile map, so
the whole 48 ms is the *grass* — `StdObject`, therefore `renderSprite`, hundreds of quads off one sprite
sheet with nothing between them to break the batch.

**A sprite is drawn at the size it was given, odd numbers included.** `halfSize` and `otherHalf` split an
odd size rather than halving it, which spanned `size` texels over `size - 1` pixels and resampled; and
mirroring swaps the two `u` coordinates rather than applying `glScaled(-1, 1, 1)`, which on an asymmetric
quad shifts it a pixel. The only odd sizes in the tree are the 39x39 level-status stamp in
`gs_selectlevel.cpp` and `Menu.Donate` at 100x43 in `menu.xml`; neither runs inside a batch.

**The transform is baked into the vertices, and that is what the batch costs.** A sprite is drawn under
whatever matrix its caller pushed — `Object::render`'s translate to the cell, the squash of a teleporting
object, the unbalanced `glTranslated` `Enemy` does inside its own `onRender`, the half pixel `Level::render`
puts under the wires — and sprites from different objects cannot share a draw call while that lives in the
matrix stack. So `queueSprite` reads it back with `glGetFloatv(GL_MODELVIEW_MATRIX)` and multiplies the four
corners itself: one GL call in place of the fourteen to sixteen the immediate path made per sprite, and in
the browser that read is a copy of sixteen floats out of a JavaScript array, not a pipeline stall.

**The flush must therefore draw under `glLoadIdentity`**, and getting that wrong is invisible almost
everywhere: a level renders under an identity modelview except for the camera shake and the half pixel
under the wires, and either applied twice passes for the effect itself. The level editor is what shows it:
its object palette is drawn under `glTranslated(245, 428, 0)`, and a flush that leaves the matrix applied
puts every sprite in it off the right of the screen.

**A queued quad is drawn with the state at the flush, not at the call.** No depth buffer in this 2D path, so
painter's order is the only order: anything that draws, or moves state the queued quads will be drawn under,
must flush first. Today that is the texture binding, texture matrix, blend function and framebuffer; nothing
the batch reaches touches the scissor box, colour mask, stencil or alpha test, and neither check would
notice if something started to. `Engine::setBlendFunc`, `beginRenderToTexture`, `endRenderToTexture` and
`acquireOffscreenTexture` flush themselves, and the object sources reach texture state through **`GL::`**
(`glstate.h`, pulled in everywhere by `pch.h`) — `setTexturing`, `bindTexture`, `deleteTexture`,
`pushTexturing`/`popTexturing` — so the rule lives in one file instead of at a dozen call sites.

**Only `GL::bindTexture` still flushes, and only when the binding moves**, since what is queued was queued
against the binding about to be replaced. `setTexturing` does not: `flushSprites` declares texturing for
its own draw (`GL::beginBatchDraw`) and restores the game's wish afterwards, taking the enable out of the
batch's state altogether. The texture matrix has no entry point of its own, because **every absolute
matrix this tree sets is a function of the binding** — a `Texture`'s own `1/w, 1/h`, or a screen copy's
`1/pow2` with flipped y — so `bindTexture` takes those two numbers as its second argument, there is no way
to bind without saying how the picture is sampled, and the matrix is not independent state at all. The
weather wants more than a scale, and composes its scroll on top of the bound picture's own inside a
balanced push and pop.

**So the ordering is said where the drawing is.** The seven `onRender`s that draw raw geometry — the laser's
and the light barrier's beam points, the lava's flow arrow, the censor bar, the projectile's point, the
teleporter's target line and the speech balloon — each call `flushSprites()` themselves, plus the two lava
passes, which draw raw quads under a texture bound from outside and so move no state anything would flush
for. `drawQuadArray`'s two array forms and `LineDrawer::draw` flush at their own definition instead, because
a built array is reached as a member or local through layers of call no static check can follow.
`verify.py`'s `sprite_batch` check counts nothing else as a flush; its `gl_state` check bans the raw forms,
scoped to what the batch can reach — the sources defining an `Object::onRender`, plus `texture.cpp` and
`linedrawer.cpp` which they all draw through, plus `Level::renderShine` and `Font::drawText` by name. The
crossfades, GUI and credits are deliberately left alone, which keeps the ban checkable by a reader.

**`GLState` skips a call that sets what is already set, and what that is worth is the draw calls, not the
calls.** 28 redundant state calls of 4833 GL entry points a frame is noise; what matters is that each of
them *flushed the sprite batch*. A bind and a switch back off sit around every
`Engine::renderSprite(Texture*)`, which is what `Level::renderShine` is, so a level full of shines queued
one quad and drew it, over and over. Measured with `frames.sh`, quads per draw call: a night-vision level
**1.1 → 19.0**, the level select **1.1 → 19.0**; the title demo stays at 50.2, a plain level at 4.0, the
editor at 21.5, none of which has a shine. 11% to 25% of calls into `GL::` do nothing, depending on the
scene.

The routing is complete, which is what the `gl_doors` check says: every raw `glBindTexture`, `GL_TEXTURE_2D`
enable, `glDeleteTextures` and absolute texture matrix comes through `GL::` — a delete included, because GL
reverts the binding to 0 when the bound texture is deleted. `presentFrame` keeps its raw calls inside
`glPushAttrib(GL_ALL_ATTRIB_BITS)` and calls `GL::invalidate()` afterwards, since what the pop restores
differs between desktop and browser and a record nobody can work out is better dropped than guessed. The
failure mode — a wrong picture rather than a slow one — is checked rather than reasoned about: a **native**
test-hooks build reads the real binding, matrix and enable back on **every** call and reports a record that
disagrees, and `frames.sh` fails on any such line. Not in the browser, where that is a WebGL `getParameter`
per call and would swamp `perf.js`; what it looks for is the tree's own code, the same on both platforms.
The `glPushAttrib(GL_TRANSFORM_BIT)` inside `GL::bindTexture` stays: replacing it is safe as the tree stands
but would leave the call with a silent precondition, and it saves nothing in the browser, where
`glPushAttrib` issues no GL call at all.

**The flush says which matrix stack it means.** It draws under `glLoadIdentity` because the vertices already
carry their modelview — but a flush happens wherever state moves, `Texture::bind` included, and
`Level::render` binds the snow and clouds with `GL_TEXTURE` current. An unqualified `glPushMatrix` there
would push, wipe and pop the *texture* matrix and leave the sprites under whatever modelview stood; the
batch is empty at that call today, the only reason it never showed. The bracket costs three calls a flush
on the desktop and two in the browser, and is the cheap half of a belt and braces: a batch left *open*
across the weather block would still be drawn under the texture matrix the weather scrolls, and what keeps
that safe is that `endSpriteBatch` runs long before it.

**`flushSprites` deliberately does not restore the current `glColor`.** A flush happens wherever state moves,
including the middle of somebody else's drawing: `Font::renderText` sets its shadow colour and calls
`drawText`, whose first act is a bind — a restore there repaints every text shadow in the last sprite's
colour. The other direction is worth knowing before the next renderer moves: the spec leaves the current
colour **indeterminate** after a draw with `GL_COLOR_ARRAY` enabled, so a strict reading has `renderText`'s
first shadow pass drawing in whatever the batch left. Measured, both targets keep it — llvmpipe answers
`GL_CURRENT_COLOR` unchanged, Emscripten writes `GLImmediate.clientColor` only from a `glColor*`.

**Browser colour has two quirks, and one of them is not cosmetic.** Emscripten truncates a `glColor*`
issued inside `glBegin`/`glEnd` to a byte, where the same call outside a block becomes a constant
`vertexAttrib4fv` at full float, and a float colour array is not truncated at all. So the batched shadow
pass's alpha of 0.35 arrives as 0.35 where the immediate path's arrived as 89/255 — measured on the level
editor, 3230 of 512000 pixels differ by exactly one, all inside a tile shadow. It follows that **`-nobatch`
is not a byte-exact oracle in the browser**, though it is one on the desktop; and that every colour the
remaining `glBegin` blocks set is still truncated down to the next 1/255.

The second quirk is the **clamp**. The game hands GL colours above 1 deliberately — `Level::renderShine`
takes `deathCountDown * 5.0` from an exploding bomb, the teleport swirl ramps its red to 2.1, and the three
spark bursts add half a level of red a tick until the particle has shrunk away, landing between 5.5 and 25.5
— and relies on the hardware to cut them off. Desktop GL clamps a primitive colour *before* multiplying the
texel; Emscripten does not (its generated vertex shader is `v_color = a_color;`, and the `clamp` it can emit
sits behind `GL_LIGHTING`, never switched on here). So the browser computes `clamp(colour · texel)` where
the desktop computes `clamp(colour) · texel` — at a red of 2.0 every texel above 0.5 saturates and a soft
glow comes out a hard-edged blob. `clampColor()` in `util.h` puts it back in the two places a colour reaches
GL uncut — `Engine::queueSprite` and `ParticleSystem::render`, both colour arrays — and **only in the browser
build**. A colour array is the only unprotected path: every `glColor*` spelling funnels into one `glColor4f`
that clamps on the way in, inside a `glBegin` block and outside alike, and a vertex attribute goes nowhere
near it. ROADMAP item 42 is how to stop paying for it on the CPU.

**A render layer is a pass, and it has a name.** `renderlayer.h` holds the twelve `RL_*` that `Level::render`
walks in order, each a single bit, so an object's set is the OR of the ones it draws on.
`Object::getRenderLayers()` is a plain member behind an inline getter and deliberately **not** virtual:
asking costs a load, and — the real reason — a subclass cannot then answer differently from the `onRender`
it inherits. The mask may name a layer the object is not drawing this frame and may never omit one it is, so
`say()` and `flash()`, which draw from `Object::render` rather than `onRender`, add their bit and never
remove it.

`Level::renderObjects` skips an object whose bit is clear, which is most of them on most passes. **That is
worth almost nothing in milliseconds and was measured before it was built** — 27,720 no-op matrix
operations a frame cost 1.0 ms, so the 2,772 the old unconditional bracket spent were worth 0.1 ms. It earns
its place as names, and did so immediately by making a dead pass visible: `renderObjects(735, …)` walked
all 84 objects with a matrix bracket and a virtual call each, and no `onRender` had ever handled 735.

**Two `verify.py` checks guard the layers, each for a bug that compiled and ran.** `render_layers`: when the
values moved, every surviving magic number — `layer == 939`, `layer != 18` — became the wrong layer, and C++
compares an enum to an int without a word, so five such lines built on all three platforms and were never
true: the sprite texture stopped being bound for the lava passes, the wires lost their offset, the speech
balloons stopped appearing. `layer_bits`: `Electronics` sets `RL_WIRE` in its own constructor and draws
every connection on that pass, and all thirteen parts then ran `renderLayers = RL_MAIN` in theirs, which
runs after the base and wipes the bit — **not one wire was drawn anywhere in the game** for months, and the
only symptom is a picture with something missing. The check knows which ancestor put bits in `renderLayers`
and reports a subclass that replaces rather than adds.

**The five palette levels are the oracle for the first kind and blind to the second.** `cat0`..`cat4` hold
an instance of 60 of the 65 types `instancePreset` knows, so walking them draws all but two of the
`onRender`s in the tree — `Damage` and `Projectile`, which the game spawns during play and no palette can
place — and four of the five are byte-identical across such a change; the fifth, `cat1`, has two
ConveyorBelts that start their band at `random(0, 6)` in the constructor, so `frames.sh` seeds it
(`B5_SEED`) and opens its editor scene on that tab. But `cat4` holds the parts and nothing in it is
*connected*, so the `plain` oracle level carries a clock wired to a light bulb — the one thing that makes
the wire pass visible to a byte-exact comparison.

**There are no display lists anywhere, and `verify.py` keeps it that way.** They were a second way of keeping
geometry beside these arrays, and one WebGL does not have — so every place that used one carried a browser
path under `#ifdef __EMSCRIPTEN__` and a stub in `gl_compat.cpp`. The `display_lists` check reports
`glNewList` and its six relations in either build: added back, one would compile on Windows, link on Linux
and misbehave only in the browser, the build nobody runs first. The three that used them are the tile grid,
the font and `Lightning`, whose two passes are built when the bolt is generated and drawn unchanged for the
forty frames it takes to fade — only colour and alpha move. `quadarray.h` is where the three meet:
`QuadVertex`, and one `drawQuadArray` so the client-state dance is written once rather than three times.

**Anything that reads the rendered frame must bind the FBO itself**, and `Engine::encodeFrame` is the second
half of that rule: it binds the frame buffer before `glReadPixels` rather than reading
`GL_COLOR_ATTACHMENT0` of whatever stands bound. Its two callers inside the main loop — the screenshot key
and the video recorder, both in the `frameRendered` block above the `unbindFrameBuffer()` that precedes the
present — have it bound already, but the test hook asks from the *frozen* branch, where nothing has rendered
and the last present left the window's own viewport standing. What came back there was the frame rasterized
at 2x under a 1280x960 viewport, of which a 640x480 read takes one quarter — and a doubled quarter is
perfectly reproducible, so **the frame oracle compared it for as long as it existed**. `frames.sh` fails any
capture whose every row *and* column pair is a copy, the shape of an integer upscale whatever caused it.

The main loop binds it only on an iteration that ran a logic tick. Natively there is no other kind, because
the `SDL_Delay` at the foot stretches every iteration to at least one tick; in the browser
`requestAnimationFrame` sets the pace, so at 16.7 ms against a 20 ms tick most iterations render nothing and
the *screen* is bound, left from the previous present. That is what made every screen transition start from
black: the crossfade's one-shot capture of the old image is the only `glCopyTexSubImage2D` not already
inside a `frameRendered` block, so it read the default framebuffer, which WebGL clears before every frame.
It calls `bindFrameBuffer()` first, right on either platform: the FBO holds the last frame that *was*
rendered, which is exactly the screen being faded out.

**A scrolling texture offset is reduced to one period, and that is a phone bug.** `wrapTextureOffset`
(`util.h`) is called on all five scrollers — the menu's title clouds, the level's rain, snow and clouds,
and the lava — because each scrolls by an offset that has been growing since the level began. A texture
coordinate reaches the fragment shader as a *varying*, and the shader Emscripten's GL emulation builds
opens with `precision mediump float;` with the texcoord varyings under it: ten mantissa bits, which a
desktop GPU implements as fp32 and a phone actually honours. The step it quantizes to is
**offset/2048 texels**, so the clouds — moving one texel a tick — drift smoothly for about forty seconds
and then go visibly steppy, and the rain, at twenty texels a tick, crosses the same line in two seconds.
Nothing is wrong on any desktop, which is what makes it hard to see.

Subtracting whole periods is **exact** under `GL_REPEAT`: it moves the finished coordinate by a whole
number and samples the same texel. Verified against the real matrix order — bind's `1/w,1/h`, the scale,
the translate and the rotate — for the four weather scrollers, deviation 0.000e+00 at offsets up to
900000. Two things to keep right: the wrap goes **after** the `sin` that reads the same offset, whose
phase has to follow the unwrapped value, and the period is the *texture's* own size, since a skin brings
its own art.

**The lava is the one whose period is not the texture**, and getting it wrong is a jump of half a tile.
Its four cousins hand GL a texture matrix; `Lava::onRender` writes the texels into `glTexCoord2d` itself,
on a 16x16 sub-texture cut out of the skin's sprite sheet by `createSubTexture` — a real 16x16 texture of
its own, so `GL_REPEAT` wraps at 16. But the front pass halves the *whole* coordinate (`t /= 2.0`) before
it draws, so a jump of 16 moves that pass by eight texels and only 32 moves it by a period.
`SCROLL_PERIOD` is therefore twice the tile, also exact for the back pass at two periods. The `shift`
beside it is `2·sin(0.1·anim)` and `3·cos(0.05·anim)`, and `anim` stays unwrapped for it: neither period
divides 32, so wrapping what feeds them would jog the wobble every time it came round. Measured over
`anim` 0..900000, both signs and both axes, the sampled fraction agrees to 4e-12 of a texel; the same
wrap at 16 puts the front pass out by exactly 0.5.

**The angle those sines are given needs no such care.** They are `double` throughout, so one ULP at
argument *A* is `A/2^52`: the snow's argument grows at 0.2 rad/s and its sine is scaled by 500 pixels, so
half a pixel of error needs 1.7e-3 rad and arrives in about **700 000 years**; after 25 days of rain — the
fastest — one ULP is 9.6e-9 rad. What runs out first is the millisecond counter feeding it: `Level::time`
is `int` and undefined after **24.9 days** in one level, `GS_Menu::time` and `Engine::time` are `uint` and
wrap at 49.7; all three reset on entering a level or the menu. In `float` the same rain argument would have
a ULP of 4 radians — the `mediump` distinction above, two steps further along.

An imported skin also needs `Texture::applyWrapMode`: WebGL 1 samples a non-power-of-two texture as pure
black unless its wrap mode is `GL_CLAMP_TO_EDGE`, silently and with no GL error, and the default is
`GL_REPEAT` — which rain, snow and clouds genuinely need, since `level.cpp` scrolls the texture matrix
without bound to tile them. So the wrap mode is switched for NPOT textures only, precisely the set where
`GL_REPEAT` could never have worked. The game's own art is all power-of-two; this exists for imported
skins alone.
