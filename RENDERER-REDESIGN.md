# One renderer: the plan

The game draws through two mechanisms today - a sprite batch that covers one
loop, and fixed-function immediate mode everywhere else - and keeps them in
order by a convention that three regex checks and a runtime read-back police.
This document is the plan for replacing both with one renderer: every draw goes
through it, it batches by itself, and it flushes only where the state a quad is
drawn under actually changes. ROADMAP item 54 tracks the work; the numbers and
the reasoning are here so that each stage can be checked against what was
promised.

Status: **planned, not started.** Everything `.claude/rules/rendering.md`
describes is what the code does until the stages below land.

## 1. What is wrong, precisely

Counted on the tree at `c606c4f`:

- **118 `glBegin` blocks in 39 files.** The sprite batch covers
  `Level::renderObjects` and nothing else; the GUI (every widget), the game
  states, the weather, the backgrounds, tooltips, toasts, the crossfades, the
  credits, the hint mesh, the toxic grid, the lava passes and the points and
  lines of five objects are immediate mode.
- **Correctness is a convention.** A queued quad is drawn under whatever GL
  state stands at the flush, so every raw draw and every state change has to
  remember to `flushSprites()` first. `GLState` tracks three things - binding,
  enable, texel scale - and blend, scissor, colour mask, stencil, alpha test
  and matrix mode are tracked nowhere and flush ad hoc (`setBlendFunc`,
  `popTexturing`, `beginRenderToTexture`). `verify.py`'s `sprite_batch`,
  `gl_state` and `gl_doors` exist only to police this.
- **The transform lives in GL.** `queueSprite` reads the modelview back with
  `glGetFloatv` per sprite; the texture matrix is the mechanism for texel-to-uv
  scaling *and* for the weather's scrolling. Both are fixed-function state the
  browser has to emulate.
- **The browser pays for all of it.** `-sLEGACY_GL_EMULATION` rebuilds a vertex
  buffer per `glBegin` block, synthesises a shader per draw, loops over texture
  units (the `?texunits` knob exists for that), quantises colours to bytes
  inside blocks; `WebBuild/gl_immediate.cpp` and most of `gl_compat.cpp` exist
  to keep the emulator fed. The one number that sizes the lever: batching the
  *sprites alone* took a phone from 57 ms a frame to 9 ms on a full level
  (`.claude/rules/rendering.md`). The rest of the frame still goes through the
  emulator.
- **WebGL has none of the primitives the code uses**: `GL_QUADS` (emulated
  through an index table with the "must start at vertex 0" trap `quadarray.h`
  describes), wide lines, `GL_LINE_STIPPLE` (already a stub), sized
  `GL_POINTS`, the alpha test.

## 2. What was verified before writing this

Each of these is a fact the design leans on, and each was checked rather than
assumed.

1. **Our own shader draws can coexist with the emulation** for the interim
   stages. Emscripten 6.0.8, `src/lib/libglemu.js`: `glDrawElements` passes
   straight to WebGL when no client attribute is enabled, the mode is at most
   `GL_TRIANGLE_FAN` (6) and an element array buffer is bound - which is
   exactly the renderer's draw; `glUseProgram` is wrapped so that a program of
   ours makes the emulation rebind its own on its next emulated draw. The
   present filters already draw this way (`PresentProgram::drawQuad`) and
   work in the browser today.
2. **The GL 2.0 entry points are resolved** (`glextensions.h`): shaders,
   programs, uniforms, attribute pointers, buffers. Missing for this plan:
   `glUniformMatrix4fv`, to be added beside them. `glDrawElements`,
   `glColorMask`, `glScissor`, `glStencilFunc/Op` are GL 1.1 and need no
   loading; `glBlendFuncSeparate` is already resolved.
3. **A `vec4` position attribute fed with two components is well-defined**:
   GL fills the missing components with 0, 0, 1 (the vertex array section of
   the ES 2.0 and GL 2.0 specifications alike).
   That is what lets a 2D vertex and a 3D vertex share one program.
4. **Exactly eight blend quadruples exist** in the tree (`setBlendFunc`
   call sites): normal (17), additive (6), multiply `DST_COLOR, ZERO` (2),
   and one each of `ZERO, ZERO`, `ONE, ONE`, `ONE_MINUS_DST_ALPHA, DST_ALPHA`,
   the bake `SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ONE, ONE_MINUS_SRC_ALPHA` and the
   premultiplied `ONE, ONE_MINUS_SRC_ALPHA, ONE, ONE_MINUS_SRC_ALPHA`.
5. **Every rare state is used as a contiguous bracket** - set, draw a group,
   reset - never alternating per quad inside a group:

   | state | where |
   | --- | --- |
   | colour mask | night vision (alpha-only clear, the whole `RL_LIGHT` pass, the darken quad), the lava-edge stencil write, the credits (whole screen) |
   | stencil | lava edges (write), lava back and front (test), `cf_star` (write, then test) |
   | alpha test | the lava-edge stencil write; rain, snow and the menu clouds, where it is a no-op (see 6) |
   | scissor | `GUI_Window`, list boxes, edit boxes - a subtree of children each, nested |
   | render target | the hint bake |
   | projection | `cf_camera`, `cf_cube`, `cf_slices`, `cf_zoom`, the credits' sprites |
   | cull face | `cf_cube`, `cf_slices` |

   What *does* alternate at the quad level is the texture (objects on one
   sheet, shines on another, text on a third) and the blend (the flash and the
   projectile glow go additive mid-pass and back).
6. **The alpha test on rain, snow and the menu clouds changes no pixel.** They
   blend with `SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ONE, ONE`; a texel of alpha 0
   contributes `0 * src + 1 * dst` to every channel, which is what discarding
   it leaves. Only the lava-edge pass needs the test, because it writes the
   stencil for every fragment that passes.
7. **Points are round today**: `Engine::init` enables `GL_POINT_SMOOTH`. A
   replacement has to be a disc, not a square.
8. **Two draw sites set the modelview to the identity** rather than pushing
   onto it - `Presets::renderPreset` and the loading screen - so the transform
   stack needs `loadIdentity()`.
9. **The frame oracle drives scenes by element name** (`frames.sh`: `b5_click
   Menu.StartGame`, `b5_key End`, `b5_frame night 6000`), so adding scenes is
   scripting, not new machinery. Its five scenes never show a dialog, the help
   page, a hint note, lava, the toxic effect, the credits or a crossfade.
10. **uint16 indices allow 16384 quads per draw** (65536 vertices at four a
    quad), the same ceiling `BATCH_MAX_QUADS` has today.

## 3. The design

### 3.1 State is what alternates per quad; everything else is a scope

```cpp
struct TextureRef  { uint id; Vec2f texelScale; };   // id 0 = the renderer's own white texel
enum class BlendMode : uchar { NORMAL, ADDITIVE, MULTIPLY, ZERO, ADD_ALL, DARKEN_UNLIT, BAKE, PREMULTIPLIED };
struct RenderState { TextureRef texture; BlendMode blend; };   // 16 bytes
```

The per-draw check is one 64-bit compare of `id << 8 | blend`; the texel scale
is a function of the id and rides along for the uv bake. No hash, no
interning: the two fields that vary are small enough that comparing them is
cheaper than baking one vertex.

The rare states are RAII scopes on the renderer. Each flushes in its
constructor, applies through the renderer, flushes in its destructor and puts
the **previous** value back, so nesting is a stack on the C stack and a restore
cannot be forgotten:

```cpp
Renderer::ScissorScope            s(rect);            // frame pixels, top-left origin
Renderer::ColorMaskScope          m(r, g, b, a);
Renderer::StencilWriteScope       w(ref);             // ALWAYS / REPLACE, colour untouched (cf_star draws in colour)
Renderer::StencilTestScope        t(ref);             // EQUAL / KEEP
Renderer::DiscardTransparentScope d;                  // the lava-edge write, and nothing else
Renderer::RenderTargetScope       o(textureId, size); // FBO, viewport and ortho in one; suspends the scissor
Renderer::ProjectionScope         p(mat4);            // the 3D users
Renderer::DirectGL                g;                  // see 3.6
```

Drawing inside a scope is ordinary batched drawing: the night-vision light
pass with its dozens of shines is one draw under its mask. Because the settings
go through the renderer, its record is complete and the test-hooks read-back
(3.7) can check all of it.

### 3.2 The vertex is 2D and 32 bytes

```cpp
struct Vertex  { Vec2f position; Vec2f uv; Vec4f color; };   // 32 bytes, the whole game
struct Vertex3 { Vec3f position; Vec2f uv; Vec4f color; };   // 36 bytes, the five 3D users
```

The program declares `attribute vec4 a_position`; the 2D path sets the
attribute pointer with size 2 and GL fills z = 0, w = 1; the 3D path sets size
3 from the 36-byte layout. Same program, same VBO, same fragment stage. The 3D
users - four crossfades and the credits' flying sprites - get their own entry
point, `draw3D(state, modelview, vertices, count, cullBackFaces)`, which
flushes the 2D batch, bakes their 4x4 on the CPU and draws with the
`ProjectionScope`'s matrix; interpolation stays perspective-correct because
the GPU still does the divide. Nothing drawn per frame in play pays for z.

Colours stay floats. Bytes would make the vertex 20 bytes and shift desktop
pixels by one level wherever alpha is blended (0.35 becomes 89/255), for a
saving that does not matter.

### 3.3 The transform is baked, on the CPU

A stack of 2D affine transforms (`push`, `pop`, `translate`, `scale`,
`rotate`, `loadIdentity`), applied to the four corners at submission - six
multiply-adds per vertex, two adds where the stack is a pure translation,
which a flag on the stack tracks (the tile grid and every string are drawn
under one). A change of transform therefore never breaks a batch, which is
the whole point of baking it. Rotation by a multiple of 90 degrees uses exact
0 and 1 rather than `sin`/`cos`, so the scrollbar arrows land where GL's
matrix put them.

uv is written in texels by every caller, as today, and normalised at
submission by `float(u) * texelScale.x` - the multiply the fixed-function
texture matrix performs in the vertex stage, done in `float` and not by a
`double` division, so that a non-power-of-two texture samples the same texel
it does now.

### 3.4 GPU side: GL 2.0 and nothing else

- One program. Vertex stage: `gl_Position = u_projection * a_position`,
  `v_color = clamp(a_color, 0.0, 1.0)` - the desktop's fixed-function clamp,
  now on every platform, which closes ROADMAP 42 and deletes `clampColor()`.
  Fragment stage: `texture2D(u_tex, v_uv) * v_color`, with a uniform switch
  for the `discard` the `DiscardTransparentScope` asks for.
- **A built-in 32x32 texture** holds a white block and a soft disc. Flat
  colour draws sample the block, points draw the disc at their size - so
  rectangles, frames, lines, marching ants and points all share one binding
  and never break a batch between themselves, and "texturing on or off"
  stops being state at all.
- Vertices stream into one VBO per flush (`glBufferData`, orphaning); indices
  come from a static element buffer built once, six per quad; the batch splits
  at 16384 quads. **`GL_TRIANGLES` everywhere.** WebGL forbids client-side
  arrays, so this is the only shape the browser accepts anyway.
- Lines, line loops, the editor's stippled rectangle and points become quads
  (the `LineDrawer` already builds lines that way).
- The scissor rect, colour mask, stencil, viewport and target are applied at
  flush time from the current scope, through one place that compares against
  what was last applied.

### 3.5 What happens to the caches

Both caches keep their shape. The tile grid and the font cache `QuadVertex`
(position and uv, 16 bytes, no colour) precisely so one built array can be
drawn three times under a different colour and offset. They are expanded on
the way into the stream buffer:

```cpp
void drawQuads(const RenderState& s, const QuadVertex* v, uint quads, const Vec4f& color);
```

- the copy is what a streaming renderer does with every vertex anyway. The
grid is 40x25 tiles, layer 1 drawn three times and layer 0 once, about 16 000
vertices a frame; text is a few hundred quads. uv is normalised once when an
entry is built. `QUAD_BUDGET` is untouched because the cached vertex stays 16
bytes. What the caches gain: the three tile passes and the object passes
between them differ only in colour and offset and so stop breaking the batch;
the remaining break between tiles and objects is the texture, which is
ROADMAP 51, and with uv baked an atlas is a per-`Texture` origin.

The alternative - a static VBO per cache entry drawn with a uniform colour and
transform, no per-frame copy - is rejected: a uniform colour makes each cached
entry its own draw call, so the help page's forty strings become 120 draws,
which is the anti-goal. If the flush histogram ever shows the copy mattering,
static buffers for the tile grid alone are a contained addition later.

### 3.6 The exceptions, and how they restore

Direct GL survives in `renderer.cpp`, texture upload (`texture.cpp`), FBO
creation, `presentFrame` and the readback (`engine.cpp`), the upscalers and
`glextensions.cpp`. Each raw section is bracketed by `Renderer::DirectGL`,
whose constructor flushes and whose destructor tells the renderer to forget
its GL cache and re-apply everything on the next draw. "Restore afterwards"
therefore means *invalidate*, never *reconstruct* - the same conclusion
`GL::invalidate()` reached, applied to the whole state. A `DirectGL` asserts
that no scope of 3.1 is open. Screen copies become `Renderer::copyFrame(id,
rect)` and clears `Renderer::clear(color)` / `clearStencil()`, both of which
flush first because they read or replace what was drawn.

### 3.7 Measuring and checking itself

- The test hook reports draws, quads and a **flush-reason histogram**:
  texture, blend, scope begin, scope end, target, buffer full, explicit,
  frame end, direct GL. `frames.sh` and `perf.js` print it. This is the
  instrument for every tuning decision after the redesign.
- A native test-hooks build reads the real binding, blend factors, colour
  mask, scissor, stencil, program and buffer bindings back after every flush
  and reports a record that disagrees, as `checkRecord` does for three fields
  today; `frames.sh` fails on any such line.
- `-flushall` replaces `-nobatch`: the same path with a flush after every
  draw call, and the output must be byte-identical. That is the bisecting
  tool for any ordering bug.

### 3.8 What an object author writes

Today, the laser's beam:

```cpp
Engine::inst().flushSprites();
glPushMatrix(); glTranslated(-sp.x, -sp.y, 0.0);
GL::setTexturing(false);
line.setWidth(6.5f); line.setColor(color); line.draw();
glPointSize(7.0f); glBegin(GL_POINTS); glColor4dv(color); glVertex2dv(p); glEnd();
GL::setTexturing(true);
glPopMatrix();
```

After:

```cpp
Renderer& r = Renderer::inst();
r.push(); r.translate(-sp.x, -sp.y);
r.polyline(beamPoints, 6.5, color);
r.point(p, 7.0, color);
r.pop();
```

No flush, no texturing toggle, no primitive that WebGL lacks. `Engine::
renderSprite` and `renderSprites` keep their signatures and draw with the pass
state `Level::renderObjects` set; an object that wants another blend passes
`r.state().with(BlendMode::ADDITIVE)`. `GUI_Element::render` opens the
`ScissorScope` around the children of an element that clips, retiring
`onRenderEnd`'s `glDisable`.

## 4. Why this is the sweet spot

Alternatives weighed, and why each lost:

- **Static geometry per entity with uniform colour and transform**: fewer CPU
  bytes, many more draw calls; on the phone the draw call is the cost.
- **Sorting draws by state to reduce flushes**: painter's order forbids
  reordering; the histogram will show what a flush costs and the atlas removes
  the texture flushes at the source.
- **z in the common vertex**: 12% bigger vertices and a 3D bake for five call
  sites that a stride switch serves.
- **Byte colours**: desktop pixels move for nothing.
- **A hashed or interned state**: moot once the per-quad state is two fields;
  interning would also need a lifetime policy for states that embed a
  per-frame scissor rect.
- **A separate program for flat and for textured draws**: a program switch is a
  flush; the white texel makes the distinction disappear.
- **Keeping fixed function on the desktop**: two paths again, and the browser
  as the build nobody runs first again. GL 2.0 is already required (ROADMAP 50).
- **Keeping `glBegin` behind a shim that batches**: the callers would still
  carry GL state and matrix calls, which is what the emulation is slow at and
  what the checks exist to police.
- **Rare state as fields of the render state** (an earlier draft of this
  plan): correct, but 64 bytes to compare and hash for states that never
  alternate per quad. The scopes are cheaper to compare, cheaper to read, and
  impossible to leave set.

The result is the batching model of every mature 2D renderer - stream
vertices with baked transform and per-vertex colour, break on texture or blend
- with two simplifications this tree earns: rare state as brackets, and a 2D
vertex.

## 5. Stages - one PR each, oracle-green at every step

**Stage 0 - widen the oracle before touching anything.** Scenes to add to
`frames.sh`, by element name: the options dialog with the CRT settings pane, the
Manager with a list, an edit box with a selection and caret, the help page, the
level editor in connection mode with two pins and a stippled rectangle, a
level with lava and the toxic effect, a hint note unrolled (`Tools/testlevels/
keycaps.xml`), the loading screen, the credits at a fixed time and one 3D
crossfade at a fixed `t` - the last two need a `state <name>` request in the
test hook and a frozen crossfade clock. Add the flush histogram and a real draw
count to the hook. Record baselines: desktop bytes for every scene, browser
timings from `perf.js`. Tag the commit so a worktree can rebuild the old binary.

**Stage 1 - the renderer, under the level.** New `renderstate.h`,
`renderer.h/.cpp`, `mat4` in `vec.h`, `glUniformMatrix4fv` in `glextensions`.
Route `Engine::renderSprite`, `drawQuadArray`, `LineDrawer`, the tile grid,
`Font::drawText`, `ParticleSystem`, `Lightning` and the level's own quads
through it; the immediate-mode leftovers still work, bracketed by `DirectGL`
where they interleave. Desktop frames byte-identical except the causes in
section 6. `-flushall` in place of `-nobatch`.

**Stage 2 - every remaining `glBegin`.** The 118 blocks: GUI widgets, game
states, weather, hint mesh (250 quads), toxic grid (2560 quads with per-vertex
colour and uv), crossfades and credits through `draw3D`. Delete `GL::`,
`GLState`, `quadarray.*`, the sprite batch in `Engine`, `pushTexturing`,
`Texture::bind`, the particle system's `#ifdef` twins. `verify.py`:
`sprite_batch`, `gl_state`, `gl_doors` and `display_lists` are replaced by
**`raw_gl`** (no `gl*` call outside the whitelist of 3.6) and
**`direct_gl_scope`** (every raw section in a whitelisted file sits inside a
`DirectGL`), each with a `selftest.py` fault. `.claude/rules/rendering.md` is
rewritten, and `CLAUDE.md`'s rules list replaces the flush and `GL::` rules with
one: all drawing goes through `Renderer`, raw GL only inside a `DirectGL` in a
whitelisted file.

**Stage 3 - cut the emulator.** Drop `-sLEGACY_GL_EMULATION`; delete
`gl_immediate.cpp`, the attribute stack and the `glu*` shims in
`gl_compat.cpp`, the `?texunits` knob and its `INCOMING_MODULE_JS_API` entry;
retire the `GL_UNPACK_ROW_LENGTH` shim by asserting at upload that an SDL
32-bit surface's pitch is `4 * w` (it is: `SDL_CalculatePitch` pads to four
bytes, which four bytes a pixel already are). Browser smoke and mobile tests; an
interleaved `perf.js` run of the old build against the new one; a phone
measurement by the author (`?perf=1`, the same level as before).

**Afterwards, separate items:** the atlas (ROADMAP 51); the blurred shadow
pass (ROADMAP 41), which now has the shader path it lacked.

## 6. Where pixels will legitimately move

The oracle lists every scene that differs; a difference is accepted only with a
cause from this list, and any other difference is a bug:

1. Lines drawn with `GL_LINE_SMOOTH` on - the editor's tile highlight and
   selection frames - lose the anti-aliased fringe when they become 1-pixel
   quads on whole coordinates; the unsmoothed frames are identical by
   construction.
2. Points become discs from the built-in texture; the rasterizer's smoothed
   point was driver-defined anyway.
3. Browser only: colours arrive unquantised and clamped like the desktop -
   both are fixes, and the browser was never a byte-exact oracle.
4. A rotation or scale computed on the CPU in `double` rather than by GL's
   `float` matrix stack may move an edge pixel of a rotated sprite; sprites in
   a level already go through the CPU bake today, so the exposure is the
   handful of rotated sprites outside a batch.

Every quad the game draws is expected byte-identical on the desktop, and a
stage is not done until it is.

## 7. Risks and what answers them

- **Interim mixed mode in the browser** (stages 1 and 2): verified in section 2,
  item 1; each switch between our program and the emulation costs the
  emulation a renderer lookup, which is gone in stage 3.
- **WebGL context loss**: the renderer owns every GL object it creates, so the
  page's `webglcontextlost` handling does not change.
- **The 16384-quad ceiling**: the batch splits, as today.
- **Ordering bugs during conversion**: `-flushall` must reproduce the batched
  output byte for byte; a difference bisects to the draw that was queued under
  the wrong state.
- **What it is worth on a phone** cannot be measured here; stage 3 ends with
  the author's number, against the 9 ms the sprite batch alone reached.

## 8. Decisions taken

1. Rare state is scopes, not fields (section 3.1).
2. The vertex is 2D and 32 bytes; the 3D users use a 36-byte layout on the
   same program (3.2).
3. Colours are floats (3.2).
4. No fixed-function path survives on any platform (4).
5. `-nobatch` becomes `-flushall` (3.7).
6. Pixel differences are accepted only against the list in section 6.
7. The atlas is not part of this work (5).

## 9. Stage 0 - the baseline

Done on `claude/render-stage0`; the tag `render-baseline` marks the commit whose
binary every later stage is compared against.

**The oracle.** `LinuxBuild/test/frames.sh` renders nineteen scenes, every one
of them frozen on a tick of the scene's own clock, photographed out of the
game's own framebuffer, and proved reproducible both ways: two runs at one
seed give nineteen byte-identical frames, and a run at another seed moves
fifteen of them and leaves four - `editor-connect`, `hint`, `loading`,
`plain` - because nothing in those four is random. The four scenes the old
binary rendered on the same ticks (`menu`, `night`, `plain`, `editor`) are
byte-identical between the tagged binary and its predecessor f2efb8b, so
none of the fixes below moved a pixel there. `.claude/rules/testing.md`
("The frame oracle") has the mechanics: the frozen frame with the engine
clock pinned, `freeze fade` and `lockstep` for the crossfades and the
credits, `state` and `click` for screens and transitions no real input can
reach on a named tick, and the two clocks that come apart.

**The numbers a change is measured against.** Draw calls are what reached
GL per rendered frame - a `glBegin` block, a `glDrawArrays`, a
`glDrawElements`, counted at the link (`--wrap`) - and the batch's own
draws are the sprite batch's flushes among them, with what caused each
flush. Today the batch flushes almost only at its own edges
(`beginSpriteBatch`/`endSpriteBatch` around each render pass); the `crt`
scene adds a few blend changes. Everything else a frame draws is immediate
mode, one draw call per `glBegin`, which is what the options dialog's 209,
the manager's 256 and the credits' 403 are made of.

| scene | tick | draw calls / frame | batch draws / frame | quads / batch draw | what flushed the batch | md5 (first 12) |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `menu` | 4000 | 47.0 | 4.0 | 50.2 | edge 340 | `5a1923d007c7` |
| `options` | 10000 | 209.0 | 4.0 | 49.5 | edge 344 | `6bf311aa9cfc` |
| `crt` | 16000 | 290.7 | 4.7 | 40.9 | blend 26, edge 148 | `7368e41e18bd` |
| `manager` | 30000 | 256.0 | 4.0 | 46.9 | edge 232 | `77e1b2fd4128` |
| `star` | 0 | 57.6 | 3.8 | 38.7 | edge 485 | `6614780948da` |
| `editor` | 0 | 92.0 | 8.0 | 21.5 | edge 88 | `592f92665c27` |
| `help` | 0 | 109.0 | 8.0 | 21.5 | edge 72 | `2423aa2035b6` |
| `editbox` | 0 | 217.0 | 8.0 | 21.5 | edge 64 | `12135896399c` |
| `editor-select` | 0 | 87.0 | 8.0 | 21.5 | edge 88 | `078076f8af16` |
| `editor-connect` | 0 | 85.0 | 7.0 | 21.4 | edge 77 | `3a6a1d2ad654` |
| `select` | 4000 | 80.0 | 5.0 | 19.0 | edge 545 | `8db8e3d97b2a` |
| `cube` | 440 | 74.6 | 5.0 | 19.0 | edge 535 | `a28958b2b1f6` |
| `night` | 6000 | 55.0 | 5.0 | 19.0 | edge 1020 | `9581c2c5fb5f` |
| `plain` | 3000 | 25.4 | 3.0 | 7.0 | edge 180 | `0c1f93ff4d53` |
| `lava` | 15300 | 86.3 | 5.0 | 7.2 | edge 2375 | `b8e827239518` |
| `toxic` | 3000 | 29.0 | 3.0 | 4.0 | edge 150 | `3ebd84025c4e` |
| `hint` | 3000 | 28.0 | 3.0 | 3.0 | edge 183 | `938241ff7321` |
| `loading` | 2920 | 1.1 | 0.0 | 0.0 | none | `b2dda9398d02` |
| `credits` | 3000 | 402.6 | 0.0 | 0.0 | none | `21699b37863a` |

md5 is of the PNG `frames.sh` writes; a run of the tagged binary reproduces
each of them exactly (seed 12345, the script's default).

**What had to be fixed for the frames to be reproducible**, each a real
behaviour of the game and not of the harness:

1. `Level::render` sorted the object vector for painting, so a tick that
   followed a rendered frame walked a sorted vector and a tick that followed
   another tick walked the spawns in the order they were appended - the same
   random draws went to different gas cells. `Level::update` now sorts before
   it walks, and spawned objects get UIDs no loaded object has
   (`addNewObjects` numbered them from the new last object's UID, still
   zero), which makes the sort's order total.
2. `Engine::playSound` drew its random pitch only when the instance came, and
   `Sound::createInstance` drops a one-shot within ten milliseconds of wall
   time of the last - two ticks bunched into one iteration consumed one
   draw fewer than two run apart. The pitch is drawn first now.
3. `Level::renderToxicEffect` built its noise table from the shared generator
   on the first frame that needed it. It comes from a fixed seed now.
4. `Level::clear` resets the scene clock, so a level's first tick is seeded
   the same on every run rather than on the previous level's last tick.
5. Ctrl+A, C, X and V in both edit boxes fell through into the character
   insert; under X11 the event's unicode is the letter, so Ctrl+A typed an
   "a" over the selection. Fixed, and `.claude/rules/gui-text.md` says why.

One thing found and left: on a fresh home the options dialog's Cancel cannot
undo the CRT settings preview, because it restores through `loadConfig()`
and there is no `config.xml` yet; the `crt` scene clicks the previous filter
back by hand.

**Browser.** `WebBuild/test/perf.js` on the title demo, the shipped build's
default arm, three interleaved runs of twenty seconds under swiftshader
(`B5_REPEATS=3 B5_WINDOW=20 node test/perf.js ""`); the draw calls are
what reaches WebGL after the emulation, counted on the context itself.

| | p50 of each run, median over the runs |
| --- | ---: |
| main-thread work per frame (`total`) | 1.70 ms |
| of which `render` / `update` / `present` | 1.00 / 0.20 / 0.10 ms |
| frame interval | 52.70 ms (swiftshader's rasterizing, not the game's) |
| WebGL draw calls per frame | 48.6 |
| sprite-batch draws per frame, quads per draw | 4.3, 45 |
| state calls skipped | 13% |

The native `menu` scene's 47.0 GL calls a frame and the browser's 48.6
WebGL calls are the same frame seen from the two ends of the emulation.

**Rebuilding the old binary** for a comparison:

```
git worktree add /tmp/b5-base render-baseline
cd /tmp/b5-base && Blocks5/pack.sh data --no-optipng && LinuxBuild/build.sh hooks
B5_DISPLAY=:89 B5_SHOTS=/tmp/blocks5-frames-base B5_FRAMES_XDG=/tmp/blocks5-frames-xdg-base \
  LinuxBuild/test/frames.sh /tmp/frames-base
```

Two runs cannot share a display, a shots directory or a home, which is what
the three variables separate.
