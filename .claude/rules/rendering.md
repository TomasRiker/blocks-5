---
paths:
  - "Blocks5/src/{renderer,renderstate,level,texture,textureatlas,tileset,sprite,particlesystem,lightning,lava,glextensions,engine,crossfade,hint}.{cpp,h}"
  - "Blocks5/src/cf_*.{cpp,h}"
  - "Blocks5/src/vec.h"
  - "Blocks5/src/renderlayer.h"
  - "Blocks5/src/fatalerror.{cpp,h}"
  - "Blocks5/src/util.h"
---

# Rendering: the GL floor, the renderer, its bracket, the files that own raw GL

**The renderer is under the whole game** (ROADMAP 54 is the redesign that put it there). Everything the
game draws — the level, the GUI, the game states, the crossfades, the
credits, the weather, the toxic grid, the hint's note — goes through `Renderer` (`renderer.h`), the present
draws through a `PresentProgram` of the filter's, and raw GL survives only in the files that own it, which
`verify.py`'s `RAW_GL_FILES` lists with a reason each: the renderer itself, the texture upload, the engine's
framebuffer, render target, readback and present, the present filters, and the entry-point loader. Nothing
fixed-function is left on any platform, and the browser build links against WebGL alone, with no emulation
in between.

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
no longer state the batch is drawn under. A line is a quad per segment, half the width to either side,
with a wedge on the outside of a turn so a laser, a wire or a shot has no notch where it bends —
`Renderer::polyline`, `line`, `point` and `rect` in place of a `glBegin`.

**The rare state is a scope, never a field.** Colour mask, stencil write, stencil test, the alpha discard
and the scissor are RAII objects (`ColorMaskScope`, `StencilWriteScope`, `StencilTestScope`,
`DiscardTransparentScope`, `ScissorScope`): each flushes on construction, applies through the renderer at
the next flush, flushes on destruction and puts the previous value back, so a restore cannot be forgotten
and nesting is the C stack — a scissor inside a scissor clips to their intersection. The lava writes the
stencil under a colour mask of nothing and the discard and draws where it wrote nothing; the star wipe
writes it and draws where it did; the night vision and the credits mask channels; a window, an edit box
and a list clip their contents, the level select its preview. Drawing inside a scope is ordinary batched
drawing, and a clear obeys the scopes as a draw does. A render target of another size
(`Renderer::beginTarget`, inside `Engine::beginRenderToTexture`'s framebuffer switch) pushes the
projection, an identity transform and no scissor, and `endTarget` puts all three back.

**The transform is baked in `float`, in GL's own arithmetic, and that is what makes a frame byte-exact.**
`Renderer::push`, `pop`, `translate`, `scale`, `rotate` and `loadIdentity` keep a 2D affine stack of
`float` entries, as GL's matrix stack was; a translate composes as Mesa's `glTranslated` does — the
products first, the old translation last — and a rotate takes its radians and then `sinf` and `cosf`,
which is why a right angle has a cosine of -4.4e-8 and not 0: the radians round to `float` before the
trigonometry, and forming them in `double` first moves a point at a 320-pixel radius by 0.000163 px. A corner is then summed in `float` and rounded at every step, the arithmetic GL's own
vertex stage does with a float matrix. Promoting the entries and rounding the sum once instead is the
exact way and buys nothing measurable: over 20 million rotated and scaled transforms the two differ for
41% of corners but by at most 0.000488 px, so 0.89% survive the 1/256 subpixel grid the rasterizer snaps
a vertex to and no oracle scene moves a single pixel — against four calls a quad in the renderer's
hottest arithmetic, paid for on every phone. The projection is the one matrix left to the GPU, as a
uniform; `Mat4::ortho` (`vec.h`) builds the same numbers `gluOrtho2D` did. The same `Mat4` stands in for
every other matrix the game once asked GL for — `gluPerspective`, `gluLookAt`, the in-place translate,
scale and rotate, the left-to-right sums in a product — keeping each call's *order* of operations, which
costs nothing and makes the two readable against each other, but not its precision: `Mat4` is `float`
throughout where GL and GLU were `double` in places. The one entry that reaches is `gluPerspective`'s
depth row, `m[10]` and `m[14]`; `m[0]` and `m[5]`, which put a corner on the screen, read neither near
nor far and come out bit for bit the same. The four 3D crossfades and the credits' stars hand
`Renderer::quads3D` a matrix built that way with the projection in it, and the rain, the snow and the
clouds hand `scrolledQuad` a texture matrix built from the picture's texel scale, which is applied to
the corners' uv in the order the fixed-function vertex stage summed it.

**`quads3D` takes one matrix per call, so what differs per piece decides the draw count.** A crossfade's
geometry moves under one matrix and is one call. The credits' four hundred stars each have a model
transform of their own, which made them four hundred calls of one quad — so `GS_Credits::renderStars`
puts the four corners through that transform itself (`Mat4::transformPoint3D`, the 3D twin of the texture
matrix's `transformPoint2D`) and hands the lot over under `projection * view`: 403 draw calls a frame to
4, the median render 3.39 ms to 2.44 under llvmpipe, and a floor rather than the figure, since a software
rasterizer pays for fill where a driver and a phone pay per call. It is the same bake the renderer does
to every 2D quad, one level up. Anything else that grows a per-piece matrix belongs here too; what it
costs is the last bit, since the corner is then rounded by the model matrix and again by the draw's,
where it used to be rounded once by the product — 197 of the oracle frame's 307200 pixels, by at most 3
of 255.

**Three shapes are not quads, and each is one with a rule.** A triangle is a quad whose fourth corner
repeats the third, so the second triangle of the split has no area — the hint's note mesh, the star
wipe, the scrollbar arrows. A one-pixel line is `Renderer::hairline`, which lights the pixels llvmpipe
lit for GL's own line, measured rather than derived: along the line a pixel whose centre lies in the
half-open span from the start point to the end, across it the pixel containing the coordinate, a
boundary going to the lower side in GL's coordinates — the column to the left, the row below on the
screen. `hairlineRect` is the four of a loop drawn separately, so that the corner two of them share is
lit twice, as GL lit it. The editor's marching ants are `dashes`.

**The index buffer splits every quad from its first corner to its third, and the choice shows.** A quad
whose attributes are not affine across it — the lava's four alphas, the lightning's trapezoids — is two
different pictures along the two diagonals. Mesa uses this diagonal for a `glBegin(GL_QUADS)` quad and
the other for a quad out of an array, so the oracle's frames hold both, and the renderer can draw only
one. It keeps the `GL_QUADS` diagonal because those quads are the many; the lightning is the one array-drawn
non-affine quad in the game, and its bolt moving by up to twelve levels inside is the accepted cost.

**`-flushall` is the bisecting tool.** It makes the renderer flush after every quad, so every quad is
drawn under the state that stood when it was handed in; the picture must be byte-identical to the
batched one, and where it is not, the difference bisects to the draw that was queued under the wrong
state. `?flushall=1` is the same switch in the browser.

## The bracket, and the files that own raw GL

**Raw GL stands inside a `Renderer::DirectGL` bracket wherever it runs while the renderer may hold quads.**
That is two files, `texture.cpp` and `engine.cpp`: a texture upload replaces what a queued quad samples, a
framebuffer switch moves where the queue lands, a copy or a read of the frame wants what is queued on it
first, and the present follows the overlays. The bracket's constructor flushes, so that what the raw code
reads or replaces is on the target; its destructor makes the renderer forget what GL holds, so that the
next flush applies everything again. "Restore" means invalidate, never reconstruct. `direct_gl_scope` is
the check, and it reads those two files with the block, chain and preprocessor rules a C++ object obeys;
`glGetError` and `glGetString` are queries and stand outside. The other owners need no bracket: the
renderer's own file is what the bracket exists for, the loader runs before the first quad, and the present
filters run inside `presentFrame`'s bracket.

**GL's texture binding is the current texture at all times except inside a flush.** `Renderer::setTexture`
binds for real, so that a `glTexImage2D` right after it lands in that texture, and a flush that bound the
stream's texture binds the current one back afterwards. The frame copies — `Engine::captureFrame` into a
texture from `createFrameCopyTexture`, drawn back through `getFrameCopyRef` in the game's own pixels —
bind for themselves inside `Renderer::copyFrame`, which flushes first because the copy reads the frame.
Every other GL texture the game makes comes from `Texture::createGLTexture`, so the upload stays in the
one file that owns it.

**A native test-hooks build reads GL back after every draw.** `Renderer::checkRecord` compares the
binding, the four blend factors and the blend enable, the program, both buffer bindings, the colour
mask, the stencil enable, the scissor enable and its box against what the renderer applied, and
`frames.sh` fails on any line it prints. A wrong record is a wrong picture rather than a slow one, and a
message in a log nobody reads is not a check. Not in the browser, where that is a WebGL `getParameter`
per draw and would swamp `perf.js`; what it looks for is the tree's own code, the same on both platforms.

**In the browser every draw reaches WebGL as it was issued.** The build links against Emscripten's plain
WebGL library — no `-sLEGACY_GL_EMULATION`, no shim of the tree's own — so the renderer's `glDrawElements`
and the present's `glDrawArrays` are the WebGL calls a phone pays for, one to one, and a fixed-function
call anywhere in the tree fails the browser link as an undefined symbol. `web.md` has the rest.

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
between them and the particles run together. Measured with the oracle: a night-vision level draws 24
calls a frame where it drew 55, the lava level 24 where it drew 86, the level select 28 where it drew 80,
the options dialog 71 where it drew 209 and the Manager 58 where it drew 253 - a dialog's strings and
frames now share a draw wherever they share the skin. On a real phone batching is worth six times what
any desktop number says: the sprite batch that
preceded the renderer took a full level from **57 ms a frame to 9** against the same build with the
batching off, and that measurement is the reason the redesign exists; read every browser number as a lower
bound on what a phone gets.

**A sprite is drawn at the size it was given, odd numbers included.** `halfSize` and `otherHalf` split an
odd size rather than halving it, which spanned `size` texels over `size - 1` pixels and resampled; and
mirroring swaps the two `u` coordinates rather than applying a scale of -1, which on an asymmetric quad
shifts it a pixel. The only odd sizes in the tree are the 39x39 level-status stamp in
`gs_selectlevel.cpp` and `Menu.Donate` at 100x43 in `menu.xml`.

**Browser colour is the desktop's.** Every colour reaches WebGL as a float attribute of the renderer's
and is clamped in the vertex stage, the desktop's fixed-function clamp on every platform:
`Level::renderShine` hands in `deathCountDown * 5.0` from an exploding bomb and the spark bursts run a
particle's red past 5, and both come out as the desktop draws them; the GL emulation the browser ran on
until stage 3 computed `clamp(colour * texel)` instead and made a soft glow a hard-edged blob, which was
ROADMAP 42, and `clampColor()` is gone with its two callers.

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

**Two more police the renderer's convention, over the whole tree.** `raw_gl`: every `gl*`, `glu*` or
`glExt*` call stands in a file that owns raw GL, and every owner still holds one, because a raw draw
anywhere else lands underneath everything queued before it — the object that broke the rule looks right
while its neighbours do not — and a raw state change fools a record that is only as good as its
coverage. `direct_gl_scope`: in the two owners whose raw GL runs mid-frame, the call stands in a
`Renderer::DirectGL` bracket. The first is also what keeps WebGL's missing primitives out: a display
list, a wide line, `GL_QUADS` or the alpha test is a `gl*` call, and there is nowhere outside the owners
to write one.

**The five palette levels are the oracle for the first kind and blind to the second.** `cat0`..`cat4` hold
an instance of 60 of the 65 types `instancePreset` knows, so walking them draws all but two of the
`onRender`s in the tree — `Damage` and `Projectile`, which the game spawns during play and no palette can
place — and four of the five are byte-identical across such a change; the fifth, `cat1`, has two
ConveyorBelts that start their band at `random(0, 6)` in the constructor, so `frames.sh` seeds it
(`B5_SEED`) and opens its editor scene on that tab. But `cat4` holds the parts and nothing in it is
*connected*, so the `plain` oracle level carries a clock wired to a light bulb — the one thing that makes
the wire pass visible to a byte-exact comparison.

**There are no display lists anywhere, and `raw_gl` keeps it that way.** They were a second way of keeping
geometry beside these arrays, and one WebGL does not have — so every place that used one carried a browser
path under `#ifdef __EMSCRIPTEN__`; added back, one would compile on Windows, link on Linux and fail the
browser link, where no emulation stands in for it. The three that
used them are the tile grid, the font and `Lightning`, whose two passes are built when the bolt is
generated and drawn unchanged for the forty frames it takes to fade — only colour and alpha move.
`QuadVertex` in `renderer.h` is where the three meet.

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
coordinate reaches the fragment shader as a *varying* at that shader's float precision, and the renderer's
fragment shader asks for `highp` only where `GL_FRAGMENT_PRECISION_HIGH` says the browser has it; a phone
without it gets `mediump`: ten mantissa bits, which a desktop GPU implements as fp32 and the phone actually
honours. The step it quantizes to is **offset/2048 texels**, so the clouds — moving one texel a tick —
drift smoothly for about forty seconds and then go visibly steppy, and the rain, at twenty texels a tick,
crosses the same line in two seconds. Nothing is wrong on any desktop, which is what makes it hard to see.

Subtracting whole periods is **exact** under `GL_REPEAT`: it moves the finished coordinate by a whole
number and samples the same texel. Verified against the real matrix order — bind's `1/w,1/h`, the scale,
the translate and the rotate — for the four weather scrollers, deviation 0.000e+00 at offsets up to
900000. The period is the *texture's* own size, since a skin brings its own art.

**The reduction runs off the clock, not off a value that has been kept.** Each scroller's offset is
`rate · clock + base`, a straight line in a counter that is an exact integer — `Level::time` and
`GS_Menu::time` in milliseconds, `Lava::anim` and `SDL_GetTicks` in ticks — so `scrollOffset` (`util.h`)
forms that line and reduces it in one step from the integer the caller still holds. A wobble bounded by
its own sine is added afterwards and the sum reduced again, which is exact for the same reason.

All of it is `float`, and the limit that puts on it is measured rather than assumed. The clouds scroll
one texel a tick, and that step comes out **exactly 1.0000 for as long as twelve hours in one level**;
after a day it is 0.5 to 1.5 and after three it collapses to a stutter of 0 to 2. `Level::time` starts
again at every level, so the float costs nothing a player can reach — this is a game, and a `double`
here would buy only a tidier number in a probe.

**The lava is the one whose period is not the texture**, and getting it wrong is a jump of half a tile.
Its four cousins scroll through the matrix they hand `scrolledQuad`; `Lava::onRender` writes the texels into its quad's uv itself,
on a 16x16 sub-texture cut out of the skin's sprite sheet by `createSubTexture` — a real 16x16 texture of
its own, so `GL_REPEAT` wraps at 16. But the front pass halves the *whole* coordinate (`t /= 2.0`) before
it draws, so a jump of 16 moves that pass by eight texels and only 32 moves it by a period.
`SCROLL_PERIOD` is therefore twice the tile, also exact for the back pass at two periods. The `shift`
beside it is `2·sin(0.1·anim)` and `3·cos(0.05·anim)`, and each of those reduces at a **turn** rather
than at the tile: neither wobble period divides 32, so reducing what feeds them by the tile would jog
the wobble every time it came round, while a turn cannot move a sine at all. Measured over `anim`
0..900000, both signs and both axes, the sampled fraction agrees to 4e-12 of a texel; a wrap at 16 puts
the front pass out by exactly 0.5.

**A phase is emphatically *not* reduced**, and `clockPhase` (`util.h`) exists to say so where a reader
would otherwise reach for the obvious. `sinf` and `cosf` reduce their own argument, against the real π
to as many bits as it takes, and land 3e-08 from the true sine at every clock value this game can
reach. Reducing by a `float` 2π first reduces against a constant that is itself 1.7e-07 out, and the
error grows with the turns thrown away: measured against the exact sine of the same float, 3.9e-05 one
minute into a level, **2.0e-03 after an hour**, 0.14 after three days. The library is better at this
than its caller. A texture offset is the opposite case and does reduce, because its period — 512
texels, or the CRT's eight seconds — is exactly representable, so `fmodf` divides by the right number
and is exact; and because the wrap is what the *shader* needs, not the CPU.

The counters are what runs out in the end: `Level::time` is `int` and undefined after **24.9 days** in
one level, `GS_Menu::time` and `Engine::time` are `uint` and wrap at 49.7; all three reset on entering a
level or the menu. Nine call sites go through the three helpers: the five scrollers, the lava's scroll
and its two wobbles, and the CRT filter's flicker and scan-line crawl (`upscalers.md`). With those in
place and the wall clock an integer, **`double` is gone from the game's own arithmetic** — the two left
in the tree are `EM_ASM_DOUBLE` and the lookahead beside it, where a JavaScript number is an IEEE double
and nothing else will do.

## The atlas

**Most pictures share a GL texture, because most draws were a texture change** — 18 of a level frame's 24,
26 of the menu's 29. `TextureAtlas` (`textureatlas.h`) packs them into pages of 2048 square, capped by
`GLExtensions::maxTextureSize()` and added on demand up to four; a guillotine packer places them biggest
edge first, and free rectangles are joined back together when they make one. Two pages hold the resident
set — `data/` and one skin, 8.7 Mtexel of which 6.2 can be packed — where one 4096 page would allocate
64 MB to keep 25 MB of pictures. Draws a frame: **menu 29 to 6, plain 16.4 to 7.4, toxic 21 to 10, night
and lava 24 to 14**.

**A picture says at its request whether it tiles**, because that is what decides whether it can share.
`Manager<T>::request` carries the resource type's options and `Texture::WrapMode` is three: `WM_CLAMP`,
nothing samples outside it; `WM_WRAP`, `Renderer::tiledQuad` cuts the quad at the picture's edges so that
every piece samples one copy, which packs; `WM_REPEAT`, GL wraps it at the *texture's* edge, so inside a
page it would read whatever was packed next door — a texture of its own. `Texture::NEVER_PACK` beside
the mode keeps a picture out for a reason that is not wrapping: the loading screen's `logo.png` and
`title.png` are drawn once and never again, and half a megatexel apiece is the wrong thing to hold a page
slot for — or to leave a hole in one when it goes. Only the weather is `WM_REPEAT`:
its uv is rotated with the scroll, so the cuts a split would need are not axis-aligned in screen space and
the pieces would not be quads. The lava's two 16x16 tiles are `WM_WRAP` and sit in a page with the sprite
sheet they were cut from.

**Every picture gets a texel of gutter, and it is exact rather than a fudge.** Linear filtering reaches one
texel past the coordinate it was given and there are no mipmaps anywhere in this game, so a copy of the
picture's own edge returns the same texel `GL_CLAMP_TO_EDGE` returned, and a copy of the opposite edge the
same texel `GL_REPEAT` returned. **The arithmetic is exact**, because a page's edge is a power of two:
`px/pageEdge` and `origin/pageEdge` are both exact in float and their sum is exactly
`(px + origin)/pageEdge` — measured, 0 mismatches over 200,000 random float32 cases, since scaling by a
power of two commutes with rounding. All twenty oracle scenes were byte-identical when the atlas went in.

**That is exactness of the formula and not of the picture, and the difference has bitten twice.** What the
formula does not give is invariance when the *origin changes*, and a repack changes it — so does adding a
picture, which shifts what the packer does with the rest. Putting the renderer's own 32x32 block and disc
into a page moved five scenes: one pixel each in `night`, `lava`, `toxic` and `star`, 28 in `credits`, by
one or two of 255. A bisect kept every other part of that change and marked the picture `NEVER_PACK`, and
all twenty scenes came back identical, so the page is where it comes from and not the code around it. The
evidence points at `Renderer::point`, which lays a 16x16 texel disc over a 2 to 6 pixel quad: the vertex uv
is exact either way, but in a page it is a small delta riding on a large origin and the rasteriser
interpolates *that* across the primitive. **The atlas is not sampling-neutral for soft, sub-texel
geometry**, and a byte-identical oracle after a packing change is a result to be pleased about rather than
one to expect.

**And the clamp gutter is right even though this game never clamped.** That reads backwards, so it is
written down: `GL_TEXTURE_WRAP_S` and `GL_TEXTURE_WRAP_T` are set nowhere in the game's history — the 2014
import has eight `glTexParameteri` calls and all eight are the min and mag filters, and `95660bb`, the last
commit before the renderer work, has none either. Every texture ran at GL's default of `GL_REPEAT`; the
clamping in `applyWrapMode` arrived with `a37ff2e`, for WebGL 1's non-power-of-two rule. So a gutter that
wraps looks like the faithful one, and it was tried.

It is not, and item 60 is why. `GL_REPEAT` was harmless in 2015 **because the sheets had transparent
margins to wrap into**, and the crop deleted exactly those margins:

    sprites.png  pre-crop   256x1024   last row 1023: max alpha   0
    sprites.png  post-crop  256x 720   last row  719: max alpha 231

A fragment at v = 0 of the credits star sheet's first cell blends `(0, 159, 0, 134)` — opaque green art —
where 2015 blended `(255, 255, 255, 0)`. The oracle moves eight of the twenty scenes on a wrapping gutter,
`credits.png` by 82 of 255 over 462 pixels, and `star`, `lava`, `toxic`, `night`, `manager`, `select` and
`cube` by two to nine. Rotated and scaled quads reach an outer edge routinely — the credits stars through
`quads3D`, the crossfades, the night vision's noise sampled whole — so this is not a corner nobody visits.

The clamp gutter therefore preserves the behaviour 2015 *had*, while wrapping would restore the mode it ran
in and a bleed it never suffered. Which is also why the three modes do not collapse into two: `WM_WRAP`
earns its place for pieces that genuinely tile, whose opposite edge is their own art.

**Nothing has to be told that a picture moved**, and that is what the whole design rests on: uv is written
in the picture's own texels everywhere in the tree and turned into the page's in `Renderer::pushQuad`, the
one line every quad passes through, from the `uvOrigin` its `TextureRef` carries. So the tile grid's cache,
the font's and the lightning's stay valid across a repack and no `layerDirty` is set. A rectangle given back
is joined to its neighbours; where a reservation still cannot be met from the pieces, `Engine::update`
repacks at the top of the next tick — a point where the renderer holds nothing — with `glCopyTexSubImage2D`
page to page, both ends in GL's own coordinates so nothing is flipped. Without the joining a smoke run
repacked seven times; with it, once.

**A test-hooks build checks the rule on every quad.** `Renderer::checkTiling` fails a quad that samples
outside its own picture from a texture not declared `WM_REPEAT`, and `frames.sh` fails on the line. It
found one the twenty oracle scenes do not: `Crossfade` kept the frame copy's texel scale and rebuilt a
bare `TextureRef` from it, dropping the flag that says that ref's negative y wraps on purpose.

**Against the picture and not against [0,1], which stopped being the same thing when the atlas arrived**:
a page is one texture holding thirty pictures, so a quad can run a long way outside its own without ever
leaving the page. That is what `TextureRef::uvExtent` is for, and written the page-relative way the check
was green while a credits star sampled row 752 of a 720-tall `sprites.png` — the bug ROADMAP 60 records,
which shipped. It runs on `Renderer::quads3D` as well as on `pushQuad`, because that star field is drawn
by the 3D path and the check covered only the 2D one. Proved by injection: with the old `random(0, 23)`
put back, the `credits` scene reports `a quad samples texel 192.0, 752.0 of a picture 256 x 720`.

`Texture::applyWrapMode` is where the three modes become GL's two, and WebGL 1 forces its hand for a
non-power-of-two picture: it is complete only sampled with `GL_CLAMP_TO_EDGE` and without mipmaps, and
otherwise every access returns pure black, silently and with no GL error. So a `WM_REPEAT` picture that is
not a power of two is clamped with a warning — deliberately under Windows too, where NPOT with `GL_REPEAT`
would work, because a 300x200 rain that tiled for its author and not for his players is the worse failure.
The game's own art is all power-of-two; this exists for imported skins alone, and only for the weather,
since nothing else needs `GL_REPEAT` any more.
