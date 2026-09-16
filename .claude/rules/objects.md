---
paths:
  - "Blocks5/src/{level,object,stdobject,presets,electronics,pin,particlesystem,player,gamestate,crossfade}.{cpp,h}"
  - "Blocks5/src/{e_,gs_,cf_}*.{cpp,h}"
  - "Blocks5/src/{activatorblock,arrow,barrage,barrage2,barrage2panel,barrageswitch,bomb,cannon,cannonpanel,cannonswitch,conveyorbelt,damage,diamondmachine,electricitypanel,electricityswitch,elevator,enemy,exit,eye,fire,hint,hotel,laser,lava,lightbarriersender,lightning,lightpanel,lightswitch,magnet,mirror,panel,projectile,rail,teleporter,toxicgas,toxicwaste}.{cpp,h}"
  - "Blocks5/data/cat*.xml"
  - "Blocks5/data/level_*.xml"
  - "Blocks5/levels/*.xml"
---

# Game states, levels and objects

**Game states** are a stack. Each derives from `GameState` (`gs_*.cpp`: Loading, Menu, SelectLevel, Game,
LevelEditor, CampaignEditor, Credits) and is registered by constructing it — the base constructor calls
`Engine::registerGameState`. Transitions go through `setGameState`/`pushGameState`/`popGameState` by string
name with an optional `ParameterBlock` context, applied at a safe point by `processGameStateChanges()`, not
immediately.

**Level and objects.** `Level` (`level.cpp`, ~61k) holds two tile layers plus a vector of `Object*` and a
spatial hash (`hashObject`/`getAllObjectsAt`). `Object` (`object.h`) is the base for everything dynamic;
behaviour is driven by an `OF_*` flag bitmask (`OF_MASSIVE`, `OF_GRAVITY`, `OF_DEADLY`, `OF_ELECTRONICS`, …)
plus virtual `onUpdate`, `onRender`, `onCollision`, `move`, `reflectLaser`, …. `StdObject` covers the plain
sprite cases (blocks, diamonds, grass), so most simple types need no new class. `Level::update()` is the tick
order: remove/add pending objects → `frameBegin()` on all → `update()` on all → `Electronics::updateAll()` →
particle systems → AI-trace decay → exit check.

**Drawing from an `onRender` obeys the sprite batch**, and `.claude/rules/rendering.md` has the whole of it.
The short form: a sprite drawn with `Engine::renderSprite` is queued, not drawn, so anything that draws raw
geometry or moves the state the queued quads will be drawn under calls `flushSprites()` first; texture
state goes through `GL::` (`bindTexture`, `setTexturing`, `deleteTexture`), never raw; a subclass adds to
`renderLayers` with `|=` and never assigns, or it wipes the bit an ancestor set, and the mask may name a
layer the object does not draw this frame but may never omit one it does; a strip is `GL_TRIANGLE_STRIP`,
never `GL_QUAD_STRIP`, because WebGL has no such primitive; and nothing in a render path calls `random()`.
`verify.py`'s `sprite_batch`, `gl_state`, `layer_bits` and `render_layers` checks catch the first three.

**Nothing in the render path draws a random number**, and the reason is not the one it looks like. **The loop
renders at most once per tick**: `timeProcessed` is zeroed at the top of each iteration and only raised inside
`while(timeToProcess >= logicRate)`, and the render is gated on it — so a machine that cannot keep up renders
*fewer* frames than it runs ticks, and one that flies cannot render more. What such a draw really costs is the
*shared generator*: a shipped build has no per-frame reseed (the one in `render()` is inside
`BLOCKS5_TEST_HOOKS`), so every draw the renderer makes shifts the sequence the logic then reads, by an amount
depending on how many frames the machine dropped and how much was on screen. `Object::glowJitter` is one
value in [-1, 1] redrawn in `frameBegin()`, and `noiseOffset1`/`noiseOffset2` (the night vision's noise) in
`Level::update()` — both once per tick, the same place and for the same reason the flash decays there.

**`Level::renderBeamShines` cannot take the object's `glowJitter` alone**: one value for the whole object makes every point of the beam breathe in unison, which reads as the beam
pulsing rather than light scattering along it. `pointJitter(seed, index)` hashes the per-tick value with the
point's index — `fract(sin(x) * 43758.5453)`, no state — so the jitter differs point to point as it always
did, and the beam draws nothing from the shared generator.

One jitter per object and not one per use: an object's own draws in a frame move together, which nothing can
see, while different objects stay independent, which is what reads as a field of lights. It is drawn for
*every* object rather than only the ones that glow, because a draw from the shared generator has to happen the
same number of times whatever is on screen, or a frame stops being reproducible from a seed. An object never
updated — an editor palette, a level select preview — keeps the 0 it was built with, exactly the brightness
the caller asked for.

**Three seeded streams, not two.** `Engine::render` and `Engine::update` each reseed from `testSeed()` and the
scene's tick — the odd half and the even half — which makes a frame reproducible however many renders fit
inside one 20 ms tick. A **level load** falls between ticks, where neither reaches, and a level's objects draw
from the generator in their constructors: a `Diamond` is `setAnimation(4, 5)` over an `anim` starting at
`random(0, 100000)`. Unseeded, the phase every object starts on continues a sequence whose length depends
on how many frames the machine managed on the way there, and the same diamond stands on a different
animation frame from run to run. `Engine::seedForLoad` is the third stream, called from the `Level::load` both overloads come through, offset
`0x40000000` clear of the tick's two. It is declared without a guard because `BLOCKS5_TEST_HOOKS` does not
reach `level.cpp`, and is an empty function in a normal build.

**All of it is night-vision-only**: `Level::render` walks `RL_LIGHT` inside `if(nightVision && !inEditor)`, so
a level without night vision draws no shines at all. The one place a per-frame random is still right is
`CF_Rewind`: a tape's snow, tracking jitter and seam shift belong to an analogue signal synchronised to
nothing — which is also why a rewind transition cannot be captured by a byte-exact oracle.

**Something that reacts lights up.** `Object::flash()` sets `flashAmount` to `FLASH_STRENGTH`; `frameBegin`
decays it by `FLASH_DECAY` per tick and `Object::render` draws the object's own sprites over themselves once
more, additively — about eight ticks, a sixth of a second. It is the acknowledgement a switch gives when
pressed, and the two counters at the bottom left give the same one when a diamond or bomb is collected:
`Player::addInventory` is the single funnel both go through, so it calls `Level::flashHudIcon` there, and
`GS_Game`'s HUD pass draws the preset a second time under the same additive blend. The two constants live in
`object.cpp` and are `extern` so the icons cannot drift away from the objects.

**The diamond machine** (`diamondmachine.cpp`) takes a block apart into sparks in its own colours, sampled
texel by texel through the debris mechanism, and brings more sparks back in to build the diamond; an aborted
conversion runs them backwards. The file carries the derivations — the travel formula the outward cloud and
the inward aim are both solved from, why `n` is the number of *moves* rather than the lifetime, and how the
abort (`DiamondMachine::abortConversion`) mirrors lifetime and damping. The block fades to
`CONVERSION_GHOST` over the conversion, and the outward sparks are dust rather than embers, so `OUT_BRIGHT`
and `OUT_END` are both 1 and only the opacity falls. Two things reach outside that file and break easily:
**`Particle::id`** is the one field that stays 0 everywhere else in the game, since the machine stamps its own
sparks per conversion and finds them again through `ParticleSystem::begin()`/`end()`; and `Particle` has a
constructor that zeroes every member, because the forty-nine callers of `addParticle` build one on the stack
and set only what they need, so a field none of them knows about would arrive as a random number. The sprite
must be the neutral white disc at (32,32) in `particles.png` — a particle is multiplied by its texture region,
and (32,0) is a pre-coloured orange that turned every cyan spark olive.

**The hint note** (`hint.cpp`) flies a 300x400 sheet of paper to the middle of the screen and unrolls it. The
geometry, shading and draw order are derived in that file, around `ROLL_TURNS` (0.50 — half a turn is a hard
painter's-order limit, not taste), `ROLL_BANDS` (48), `ROLL_LENGTH` (0.30), `PERSPECTIVE`, `VIEW_OFFSET_X`
(300), `SHADE_EDGE` (0.75) and `FADE_UNTIL` (0.5). What is worth knowing from outside:

- Paper and text are baked *together* into one texture (`Engine::acquireOffscreenTexture` +
  `beginRenderToTexture`) so the writing belongs to the sheet and flies, turns and rolls with it. It is
  composed with `glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)`,
  which leaves the colour premultiplied, so it is drawn again with `(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)`.
- **The texture belongs to the Engine, not the note**, and that is not tidiness: it falls with the framebuffer
  object, which `Engine::exit` destroys while the GL context still stands, whereas an `Object` is destroyed
  only after `main()` has returned. It is a **pool** because two notes overlap while one fades out, and
  sharing one texture meant a full render-to-texture, an FBO switch and a read-after-write stall per visible
  note per frame.
- **At rest the sheet is drawn at exactly 1:1 on whole pixels**, which is what `SNAP_RESIDUAL` is for: an
  exponential ease never arrives, so scale, angle and position are rounded once the residual is under half a
  pixel over the screen diagonal. Measured off a screenshot, 17% of outline pixels carried the font's own
  colour before, 58% after.
- **Whether it rolls at all belongs to the artwork**: a marker file `hintscroll.txt` beside the `hint.png`
  that is *actually loaded* — contents ignored, existence counts, resolved by `getSkinFilename` so it follows
  `default_hint.png` to wherever the picture really came from, and answered through `Level::isHintScroll()`.
  Beside the image and not an attribute in `tileset.xml`, because `<Level skin0=… skin10=…>` picks each slot
  separately and a flag in the tileset would describe a different file. It must be **named** in the packing
  scripts rather than swept up as `*.txt`, since `password.txt` is deliberately packed unencrypted in a second
  pass; in `pack.sh` the name is tested for first, because `packInto` drops only *patterns* that match nothing
  and a plain filename survives an empty glob, which broke the `space` archive once.
- **Return and Escape put the note away** without walking off the field. `Object::dismiss()` is a virtual
  answering false everywhere except a hint currently showing something, and `Level::dismissDisplay()` asks the
  objects on the player's own field. The key must be caught in `GameGUI::onKeyEvent` and not `Hint::onUpdate`,
  because `GUI::update()` runs before `p_gs->onUpdate()` and the game menu would already be open; Escape
  therefore asks `dismissDisplay()` first and falls through to the menu only when nothing took it.
- The mesh is a `GL_TRIANGLE_STRIP` and **not** a `GL_QUAD_STRIP`: WebGL has no such primitive, and
  `WebBuild/gl_immediate.cpp` hands the mode straight to it. That applies to any strip added anywhere.
- `Hint::onCollect` is deliberately empty, existing solely to stop `Object::onCollect` making the note
  disappear. And a bake that fails — out of texture memory, a lost context — draws nothing that frame rather
  than falling back to a flat sheet; `bakeNote` runs again on the next.

**Presets are the object factory.** `presets.cpp` maps a type-name string to a constructed `Object` in one
long `if/else if` chain (`instancePreset`), plus a `texCoords` table for the editor's sprite. Adding a type
means: write the class (if `StdObject` won't do), add sprite coords and a branch in `presets.cpp`, override
`saveAttributes` to round-trip its XML attributes, and place an instance in the right `data/cat<N>.xml` so it
appears in the editor palette (the palettes are themselves Levels, `p_cat[0..4]`).

**Electronics** (`electronics.cpp`, `pin.cpp`, `e_*.cpp`) is a small wire-level simulation layered on objects:
parts expose input/output `Pin`s, connections are saved separately from ordinary attributes
(`saveConnections`/`loadConnections`), and `Electronics::updateAll` propagates values each tick with an
undefined state for unconnected/unsettled inputs.

**Level file format.** A level is XML: `<Level>` attributes for size, skins, weather, light colour,
diamonds needed and music; one `<Layer>` per tile layer containing `<Row>` strings where each character's
raw code is the tile ID (space = 0); then a flat list of `<Object type="…" x="…" y="…" …/>`. Campaigns
(`campaign.cpp`) are an ordered list of level filenames, shipped zipped in `levels/campaigns/`.
