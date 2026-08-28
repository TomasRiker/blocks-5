# Blocks 5 — WebAssembly port (spike)

An Emscripten build of the game. **Status: it compiles, links, boots, and runs its
main loop in a browser — but the screen is still black.** This is a feasibility
spike, not a finished port. Everything here is additive: the Visual Studio build
is untouched, and every change to `Blocks5/src` sits behind `#ifdef __EMSCRIPTEN__`
or is a standards-conformance fix that MSVC also accepts.

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

Working: boot, config, the user directory (on IDBFS, so saves persist), SDL video,
OpenGL context, OpenAL, GUI init, texture loading straight out of the encrypted
`data.zip`, the fixed-timestep main loop running frame after frame.

Not working: **nothing renders yet.** A clear-colour probe confirms the canvas,
context and buffer swap are all live, so the remaining problem is in the draw
path, not the plumbing. Also amputated: video capture (`videorecorder_stub.cpp`),
the hq2x upscaler (hand-written x86 assembly), the SEH crash handler, and the
update checker.

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

## Next steps

1. **Find why nothing draws.** The pipeline is confirmed live, so suspect the
   immediate-mode shim's interaction with texture binding and the blend state.
2. **`glPushAttrib`/`glPopAttrib` are no-ops** (`gl_compat.cpp`), so state leaks
   across the 9 pairs that use them — 6 of which only need
   `glMatrixMode(GL_MODELVIEW)` on pop, and 2 only `glEnable(GL_TEXTURE_2D)`.
   This is a silent failure and a prime suspect for the black screen.
3. **`particlesystem.cpp:49`** passes `GL_INT` to `glTexCoordPointer`, which WebGL
   rejects; and `glDrawArrays(GL_QUADS, …)` (there and in `linedrawer.cpp`) is not
   a valid GLES2 primitive.
4. **Display lists** are handled in `level.cpp` and `font.cpp` by re-emitting, but
   `lightning.cpp` and `cf_star.cpp` are still stubbed and invisible.
5. **`texture.cpp`** reads back the texture matrix with `glGetDoublev`, which is
   approximated; it could just compute the matrix in C++.
6. **Audio needs a user gesture** — the browser blocks `AudioContext` until a
   click, so sound should start behind the first input.
