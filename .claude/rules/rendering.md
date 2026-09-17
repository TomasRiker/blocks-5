---
paths:
  - "Blocks5/src/{renderer,renderstate,level,texture,tileset,sprite,particlesystem,lightning,lava,glextensions,glstate,engine}.{cpp,h}"
  - "Blocks5/src/renderlayer.h"
  - "Blocks5/src/fatalerror.{cpp,h}"
  - "Blocks5/src/util.h"
  - "WebBuild/gl_compat.cpp"
  - "WebBuild/gl_immediate.cpp"
  - "WebBuild/compat.h"
---

# Rendering: the GL floor, the renderer and its two modes, GL state

**The renderer is being put under the whole game in stages, and `RENDERER-REDESIGN.md` (ROADMAP 54)
is the plan.** Stage 1 has landed: everything the *level* draws goes through `Renderer` (`renderer.h`),
and so does every sprite, string and shine anywhere; the GUI, the game states, the crossfades, the weather,
the toxic grid and the hint's note mesh still draw fixed-function immediate mode, and the two coexist
through the brackets described below. Stage 2 converts those, deletes `GL::` and the brackets, and the
browser stops needing the GL emulation in stage 3. Until then everything below is what the code does.

**The floor is GL 2.0 with framebuffer objects, and it is a floor, not a hope.** Buffers are core in GL
1.5 (2003), shaders in GL 2.0 (2004), framebuffer objects an EXT from 2004; both software rasterizers
tested against, llvmpipe and SwiftShader, carry all three, and in WebGL 1 they are core — so
`GLExtensions::init` resolves all three and stops where one is missing, and `Renderer::init`,
`createFrameBuffer` and `createUpscalerGL` add the failures a resolved entry point can still produce, a
program that will not link and a framebuffer that will not complete. The message is written for a new
machine in a particular state, not an old one: Windows with no graphics driver in play — fresh
installation, safe mode, a VM, an RDP session — hands out `opengl32.dll`'s GDI Generic renderer, OpenGL
1.1, so the box names the missing group with `GL_VERSION`, `GL_RENDERER` and `GL_VENDOR` and says to
install the driver, which turns a support mail into a self-fix. English, because `Engine::init` runs
before `main()` loads `languages.txt`.

**`fatalError()` (`fatalerror.h`) is the one way the game gives up**, written once per platform because
that is the whole of what differs: `MessageBoxA` under Windows, zenity or kdialog under Linux (the pair
the file dialogs reach for), a DOM overlay in the browser. The Linux half uses `fork`/`execlp` rather
than `system()`: the message carries strings the driver wrote, and an argument handed straight to the
program needs no quoting and can carry no command. `execlp` returns only where the program is missing,
so the child's `_exit(127)` is how the parent knows to try the other.

## The renderer

**A quad handed to `Renderer` is baked and queued, and one `glDrawElements` puts the queue up at the next
flush.** Baked means the transform is applied to the four corners on the CPU, the uv is multiplied by the
texture's `1/w, 1/h`, and the colour rides along per vertex — 32 bytes a vertex, 2D. The queue is a stream
VBO and a static index buffer of six indices a quad, `GL_TRIANGLES` because WebGL has nothing else, 16384
quads a draw at most. A flush happens where the next quad's *state* differs from the stream's — the state
is two things, the texture and the blend mode (`renderstate.h`), because those are what alternate from
quad to quad — where a scope begins or ends, where the stream is full, where the frame ends, and where
something raw needs the screen in order. The test hook reports every one of those by reason
(`batch.byReason`), which is the instrument every later tuning decision is made with. One GL 2.0 program
draws everything: `texture2D * colour`, the colour clamped to 0..1 in the vertex stage on every platform,
and a `discard` of alpha 0 behind a uniform for the one pass that needs an alpha test.

**Flat geometry is textured too.** The renderer owns a 32x32 texture with a white block and a soft disc.
A rectangle, a line, a keycap frame or a beam samples the centre of a texel inside the block — the centre,
so that linear filtering has one texel to weigh and the sample is exactly white — and a point stretches
the disc over its own size, which is what `GL_POINT_SMOOTH` made of `GL_POINTS`. "Texturing off" is
therefore a texture like any other, the flat things share one draw among themselves, and the GL enable is
no longer state the batch is drawn under. Lines are the quads `LineDrawer` used to build — one per
segment, a wedge on the outside of a turn — so a laser, a wire or a shot looks as it did, with
`Renderer::polyline`, `line`, `point`, `rect` and `rectOutline` in place of a `glBegin`.

**The rare state is a scope, never a field.** Colour mask, stencil write, stencil test and the alpha
discard are RAII objects (`ColorMaskScope`, `StencilWriteScope`, `StencilTestScope`,
`DiscardTransparentScope`): each flushes on construction, applies through the renderer at the next
flush, flushes on destruction and puts the previous value back, so a restore cannot be forgotten and
nesting is the C stack. The lava is the whole use today: the edge pass writes the stencil under a colour
mask of nothing and the discard, and the two lava passes draw where it wrote nothing. Drawing inside a
scope is ordinary batched drawing.

**The transform is baked in `float`, in GL's own arithmetic, and that is what makes a frame byte-exact.**
`Renderer::push`, `pop`, `translate`, `scale`, `rotate` and `loadIdentity` keep a 2D affine stack of
`float` entries, as GL's matrix stack was; a translate composes as Mesa's `glTranslated` does — the
products first, the old translation last — and a rotate takes its sine and cosine as Mesa does, the angle
to `float`, the radians in `double`, `sinf` and `cosf`, which is why a right angle has a cosine of
-4.4e-8 and not 0. A corner is then the `float` entries promoted to `double`, summed, and rounded to
`float` once — the arithmetic the old sprite batch did with the matrix it read back from GL. The
projection is the one matrix left to the GPU, as a uniform; `Mat4::ortho` (`vec.h`) builds the same
numbers `gluOrtho2D` did. `Engine::beginRenderToTexture` pushes both the projection and an identity
transform, so a bake starts from the texture's origin whichever transform the caller stood under — the
hint's note was the frame that showed it.

**The index buffer splits every quad from its first corner to its third, and the choice shows.** A quad
whose attributes are not affine across it — the lava's four alphas, the lightning's trapezoids — is two
different pictures along the two diagonals. Mesa uses this diagonal for a `glBegin(GL_QUADS)` quad and
the other for a quad out of an array, so the old tree drew both, and the renderer can draw only one. It
keeps the immediate-mode one because those quads are the many; the lightning is the one array-drawn
non-affine quad in the game, and `RENDERER-REDESIGN.md` section 6 carries it as an accepted difference.

**`-flushall` is the bisecting tool.** It makes the renderer flush after every quad, so every quad is
drawn under the state that stood when it was handed in; the picture must be byte-identical to the
batched one, and where it is not, the difference bisects to the draw that was queued under the wrong
state. `?flushall=1` is the same switch in the browser.

## Two modes, until the last `glBegin` goes

**Outside a `Renderer::DirectGL` bracket the renderer batches; inside one, the code around is raw GL and
the renderer draws at once.** The bracket's constructor flushes what is queued and puts the fixed-function
pipeline where raw code expects it — no program, no arrays of ours, the current transform loaded into the
modelview on top of a push of its own, the current texture bound and enabled with the texture matrix
holding its texel scale, the current blend set — and its destructor pops that modelview and makes the
renderer forget what GL is holding, so that the next flush applies everything again. "Restore" means
invalidate, never reconstruct. Inside the bracket a draw call is the unit: a sprite, a string, a
polyline bakes under GL's own modelview, read once per call, and goes up before the call returns, and
the state calls — `setTexture`, `setTexturing`, `setBlend`, the transform — reach GL as well as the
record, so that raw `glBegin` blocks and renderer draws interleave in order. That is what every screen
outside the level costs today (`batch.byReason.direct`), and what stage 2 removes.

**`Renderer::Batched` is the bracket the other way round.** `Level::render` opens one: inside a screen
that is still raw, it reads GL's modelview as the raw code left it — the menu's `glTranslated(0, 65)`,
the select screen's half scale, the editor's palette offset — as the base of the transform stack, batches
everything the level draws, and puts the fixed function back at its end. Inside the level the three
places that still draw raw — the weather, the toxic grid, the hint's note mesh — open a `DirectGL` of
their own, which is where the outer push of the modelview matters: the weather's bracket starts from the
level's camera shake and must not leave it behind for the screen that drew the level.

**GL's texture binding is the bound texture at all times except inside a flush.** `Renderer::setTexture`
binds for real whatever the mode, so that a `glTexImage2D` or a `glCopyTexSubImage2D` right after a bind
lands in that texture — the crossfade's capture, the toxic effect's copy, a texture upload — and a flush
that bound the stream's texture binds it back afterwards. The record beside it — binding, texel scale,
texturing enable, blend — survives a raw section, because every door to those in the tree comes through
the renderer, which is what the `gl_doors` check says: `GL::` (`glstate.h`) is four forwarding calls kept
for the sources not yet converted, and `Texture::bind()` is `setTexturing(true)` plus `setTexture`.
`presentFrame` keeps its raw calls inside `glPushAttrib(GL_ALL_ATTRIB_BITS)` and calls `GL::invalidate()`
afterwards, since what the pop restores differs between desktop and browser and a record nobody can work
out is better dropped than guessed.

**A native test-hooks build reads GL back after every draw.** `Renderer::checkRecord` compares the
binding, the four blend factors, the program, both buffer bindings and, outside a bracket, the colour
mask, the stencil enable and the alpha test against what the renderer applied, and `frames.sh` fails on
any line it prints. A wrong record is a wrong picture rather than a slow one, and a message in a log
nobody reads is not a check. Not in the browser, where that is a WebGL `getParameter` per draw and would
swamp `perf.js`; what it looks for is the tree's own code, the same on both platforms.

**In the browser the two modes share one WebGL context with Emscripten's GL emulation.** The renderer's
`glDrawElements` passes through to WebGL because no client-state array is enabled, the mode is
`GL_TRIANGLES` and an element buffer is bound; `glUseProgram` resets the emulation's own program; the
attribute pointers are re-specified at every direct-mode draw because the emulation's immediate-mode
renderer sets its own. Each switch costs the emulation a lookup, which stage 3 deletes with the emulation.

## The tile grid, the sprites, the passes

**The tile grid is built once and drawn three times.** `Level::renderTiles` writes the layer into a
`std::vector<QuadVertex>` — position and uv in texels, 16 bytes, no colour — whenever `layerDirty` says
it changed, and hands it to `Renderer::quads` under the pass's colour and offset. No colour in the cache is
the point: `Level::render` makes three passes over a layer, two shadow samples and the picture, differing
in nothing but a colour and a translate, so one built array serves all three, and the renderer expands it
on the way into the stream. Six places set `layerDirty`, covering every way a tile or the picture it is
cut from can move; a tile id alone would not, since the texture coordinates come from the `TileSet` and a
skin change moves every tile without moving a single id. The font keeps its strings the same way.

**A level frame is a few draws, and the passes merge.** `Level::renderObjects` walks the objects of one
layer and each hands its sprites to `Engine::renderSprite`, which is `Renderer::sprite`; nothing between
two passes flushes unless the texture or the blend changes, so the tile grid's three passes, the objects
between them and the particles run together. Measured with the oracle: a night-vision level draws 33
calls a frame where it drew 55, the lava level 37 where it drew 86, the level select 58 where it drew 80.
The screens around the level are unchanged at 209 and 253 (options, manager) because they still draw
raw. On a real phone batching is worth six times what any desktop number says: the sprite batch that
preceded the renderer took a full level from **57 ms a frame to 9** under `?nobatch=1` against the
default, and that measurement is the reason the redesign exists; read every browser number as a lower
bound on what a phone gets.

**A sprite is drawn at the size it was given, odd numbers included.** `halfSize` and `otherHalf` split an
odd size rather than halving it, which spanned `size` texels over `size - 1` pixels and resampled; and
mirroring swaps the two `u` coordinates rather than applying a scale of -1, which on an asymmetric quad
shifts it a pixel. The only odd sizes in the tree are the 39x39 level-status stamp in
`gs_selectlevel.cpp` and `Menu.Donate` at 100x43 in `menu.xml`.

**Browser colour has one quirk left, and it is confined to the raw draws.** Emscripten truncates a
`glColor*` issued inside `glBegin`/`glEnd` to a byte, where the same call outside a block becomes a
constant `vertexAttrib4fv` at full float — so a colour the remaining `glBegin` blocks set is still
rounded down to the next 1/255 in the browser, and the browser is not a byte-exact oracle for them. The
renderer's own draws carry their colours as float attributes and clamp them in the vertex stage, the
desktop's fixed-function clamp on every platform: `Level::renderShine` hands in `deathCountDown * 5.0`
from an exploding bomb and the spark bursts run a particle's red past 5, and both come out as the desktop
draws them, where the emulation computed `clamp(colour * texel)` and made a soft glow a hard-edged blob.
That closes ROADMAP 42, and `clampColor()` is gone with its two callers.

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

**Three more police the renderer's convention, scoped to what the level reaches.** `direct_gl`: a
`glBegin`, `glDraw*` or `glRect*` in a source defining an `onRender` — plus `texture.cpp`, which every
object binds through, and `Level::renderShine` and `Font::drawText` by name — has to stand in a block
that declared a `Renderer::DirectGL` before it, read with the block, chain and preprocessor rules its
docstring lists, because a raw draw outside the bracket lands underneath everything queued before it and
the object that broke the rule looks right while its neighbours do not. `gl_state`: the same sources
change texture state through `GL::` or `Texture::bind()` and never raw. `gl_doors`: the whole tree does,
because the record is only as good as its coverage. The crossfades, GUI and credits are left to the
first two deliberately, which keeps the ban checkable by a reader; `Hint::renderNoteMesh` is the one
exemption, called from inside the bracket `Hint::onRender` opens for it.

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
forty frames it takes to fade — only colour and alpha move. `QuadVertex` in `renderer.h` is where the three
meet.

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
Nothing is wrong on any desktop, which is what makes it hard to see. The renderer's own fragment shader
asks for `highp` where the browser has it, which is what the lava, the one scroller through it, gets.

Subtracting whole periods is **exact** under `GL_REPEAT`: it moves the finished coordinate by a whole
number and samples the same texel. Verified against the real matrix order — bind's `1/w,1/h`, the scale,
the translate and the rotate — for the four weather scrollers, deviation 0.000e+00 at offsets up to
900000. Two things to keep right: the wrap goes **after** the `sin` that reads the same offset, whose
phase has to follow the unwrapped value, and the period is the *texture's* own size, since a skin brings
its own art.

**The lava is the one whose period is not the texture**, and getting it wrong is a jump of half a tile.
Its four cousins hand GL a texture matrix; `Lava::onRender` writes the texels into its quad's uv itself,
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
