# Blocks 5 — WebAssembly port (spike)

An Emscripten build of the game. **Status: it renders and plays.** The menu,
level select and gameplay all draw correctly in a browser, with no GL errors.
Everything here is additive: the Visual Studio build is untouched, and every
change to `Blocks5/src` sits behind `#ifdef __EMSCRIPTEN__` or is a
standards-conformance or bug fix that MSVC also accepts.

## Building

Needs the Emscripten SDK and four dependencies that this repo only vendors as
headers (their prebuilt `.lib` files in `libs/bin` are Windows binaries):

```sh
git clone https://github.com/emscripten-core/emsdk && emsdk/emsdk install latest && emsdk/emsdk activate latest

mkdir -p ~/deps && cd ~/deps
git clone --depth 1 https://github.com/madler/zlib.git zlib
git clone --depth 1 https://github.com/xiph/ogg.git ogg
git clone --depth 1 https://github.com/xiph/vorbis.git vorbis
git clone --depth 1 https://github.com/jslee02/tinyxml.git tinyxml1   # TinyXML 1 (2.6.2), not tinyxml2
git clone --depth 1 https://github.com/nothings/stb.git stb
printf '#ifndef __CONFIG_TYPES_H__\n#define __CONFIG_TYPES_H__\n#include <stdint.h>\ntypedef int16_t ogg_int16_t;\ntypedef uint16_t ogg_uint16_t;\ntypedef int32_t ogg_int32_t;\ntypedef uint32_t ogg_uint32_t;\ntypedef int64_t ogg_int64_t;\ntypedef uint64_t ogg_uint64_t;\n#endif\n' > ogg/include/ogg/config_types.h
```

Then build `data.zip` and the skin archives (the `zip_*.bat` equivalents), and run
`./build.sh` (`BLOCKS5_DEPS` overrides the dependency root). Serve `build/` over
HTTP — `file://` will not work.

## What this build does and doesn't do

Working: boot, config, the user directory (on IDBFS, so saves persist), SDL
video, OpenGL, OpenAL, texture loading straight out of the encrypted `data.zip`,
the fixed-timestep main loop, mouse and keyboard input, and rendering — tile
layers, sprites, fonts, the GUI, particles and weather.

Amputated: video capture (`videorecorder_stub.cpp`), screenshots
(`Engine::screenshot` returns early), the hq2x upscaler
(hand-written x86 assembly), the SEH crash handler, and the update checker. The
$A_CAPTURE_SCREENSHOT and $A_TOGGLE_CAPTURE_VIDEO actions are not registered
under `__EMSCRIPTEN__`, so F11/F12 no longer appear in Options -> Controls.

Not yet done: audio starts muted until the first click, because browsers block
`AudioContext` without a user gesture.

No display lists remain. All four sites re-emit their geometry directly: the
tilemap and the glyph cache under `#ifdef __EMSCRIPTEN__` (the Windows build
keeps its compiled lists), the thunderstorm bolt likewise, and the star wipe on
both toolchains - `CF_Star` lost its GLU tessellator as well, since the star is
a fixed shape a triangle fan covers exactly.

## The pieces

| file | what it does |
|---|---|
| `build.sh` | the whole build; also stages the runtime tree, mirroring `stage.bat` |
| `compat.h` | force-included; `stdext::hash_map` → `std::unordered_map`, MSVC CRT spellings, and the `random()` clash with POSIX |
| `gl_immediate.cpp` | intercepts immediate mode and re-emits every attribute per vertex (see below) |
| `gl_compat.cpp` | the GL entry points Emscripten declares but never implements |
| `img_load.cpp` | replaces SDL_image with stb_image (see below) |
| `platform_stubs.cpp` | SDL cursors, SDL surface locking, hq2x |
| `videorecorder_stub.cpp` | an inert VideoRecorder, so `engine.cpp` needs no edits |
| `pre.js` | mounts IDBFS at `/blocks5_home` and flushes it periodically |

Two of those deserve explanation.

**`gl_immediate.cpp`.** Emscripten's GL emulation computes a block's vertex count
as `4 * floatsWritten / bytesPerVertex` and asserts the result is whole — which
only holds if every vertex carries every attribute. Like most fixed-function code,
this game sets a colour once and then emits four vertices, and 95 of its 119
`glBegin` blocks are shaped that way. Rather than rewrite them all, this file
buffers each block and replays it with the current colour and texcoord attached to
every vertex.

**`img_load.cpp`.** Every texture is read out of a password-protected zip through
the game's own virtual filesystem, which hands SDL_image a synthesised
`SDL_RWops`. Emscripten's `IMG_Load_RW` decodes via the browser and only accepts
names of preloaded files, so it cannot do that at all. This build therefore does
not link Emscripten's SDL_image; it supplies `IMG_Load_RW` itself and decodes with
stb_image. `texture.cpp` and `engine.cpp` are untouched.

## What was actually wrong

Worth recording, because none of it was predictable from reading the code.

1. **`glPushAttrib`/`glPopAttrib` as no-ops turned the screen black.**
   `Texture::bind()` brackets a `glMatrixMode(GL_TEXTURE)` edit with them, so the
   matrix mode stayed `GL_TEXTURE` after the first texture bind and every
   `glPushMatrix`/`glTranslated` in the game transformed texture coordinates
   instead of geometry. Nothing errored.
2. **`SDL_BlitSurface` is implemented on a 2D canvas.** It `drawImage`s from a
   source canvas, which only exists for surfaces Emscripten's own SDL created
   from an image. Every surface this game blits is written directly in memory, so
   the blit copied nothing and every texture uploaded fully transparent.
3. **Emscripten numbers keysyms SDL2-style** (`scancode | 1<<10`), so `SDLK_F7`
   is 1088 and `SDLK_LSHIFT` 1249, against `Engine`'s 512-entry key tables. The
   overflow read back as Shift+F7, which is the unlock-all-levels cheat.
4. **`GL_INT` is not a valid vertex-attribute type in WebGL**, and
   **`GL_UNPACK_ROW_LENGTH` does not exist** — both silently ignored after
   raising `INVALID_ENUM`.

Emscripten's own legacy-GL texturing, texture matrices and immediate mode were
all fine; each was ruled out with a standalone 40-line test program before
suspicion moved on. That is the technique worth reusing: when the whole port
misbehaves, isolate the platform feature in a program small enough to be
obviously correct.
