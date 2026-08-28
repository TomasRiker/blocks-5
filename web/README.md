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

Sound is gated on a click, because browsers refuse to start an `AudioContext`
without one - see below.

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
| `web_transfer.cpp` | the download/file-picker bridge behind Export and Import |
| `web_audio.cpp` | reads and resumes the `AudioContext` behind OpenAL |
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

## Click to start

A browser will not let a page start an `AudioContext` that was created without a
user gesture; it comes up `suspended` and stays that way. `Engine::init` opens
OpenAL long before anyone has touched the page, so without a gate the logo jingle
and the menu music were scheduled into a dead context and simply lost - the game
came up silent, with nothing on screen to explain why.

Emscripten does hang a resume on the first `mousedown`/`keydown`/`touchstart`
(`autoResumeAudioContext` in `libcore.js`), but it registers those listeners with
`{once: true}` and never checks whether the resume succeeded, so the one chance
can be spent for nothing. And it does not help with the real problem, which is
that the player is given no reason to click.

So `GS_Loading` now holds before the intro, on a black screen, showing a centred
`$WEB_CLICK_TO_START`. It only does this when `WebAudio::isSuspended()` says the
browser is actually blocking - a context that is already running (Firefox, or
Chrome started with `--autoplay-policy=no-user-gesture-required`) sees no prompt
at all. Any mouse button or key calls `WebAudio::resume()` as well, so a spent
`{once: true}` listener costs nothing. The hold ends as soon as the context
reports `running`, which also covers a click that landed beside the canvas and
was seen only by the browser; if a gesture has been seen but no answer arrives
within two seconds, the game starts anyway, on the grounds that a silent game
beats a screen that never moves.

The logo is deliberately not drawn during the hold: its entrance is timed to the
jingle, and both now begin together, one second after the click.

## Getting levels in and out

The Level Editor and Campaign Editor each gained an **Export...** and
**Import...** button, present only in the web build (the desktop build hides
them - there the files are already in `My Documents\Blocks 5\`).

Export hands the browser a Blob and clicks a hidden `<a download>`. A level is
serialised exactly as Save would write it, so it need not be saved first; a
campaign ships the password-protected zip the editor already produces, byte for
byte, which is why an imported campaign is immediately playable.

Import opens an `<input type="file">`, reads it with a FileReader, and writes it
to a staging path *outside* `/blocks5_home` - so a file that fails validation
never reaches IndexedDB. The completion is handed back to C++ through
`EMSCRIPTEN_KEEPALIVE` functions the JS calls, and each editor polls once per
logic tick; the handoff is tagged with a channel so a dialog resolving after the
user has switched editors is not consumed by the wrong one. The browser's
filename is only ever a *suggestion*: `sanitizeFilenameStem` (unguarded, in
util.cpp) reduces it to `[A-Za-z0-9_-]`, at most 64 characters, and C composes
every destination path. JS never does. A level is validated by parsing it and
checking for a `<Level>` root; a campaign by opening the archive and requiring a
`campaign.xml` that loads with at least one level. On success the import forces
an `FS.syncfs` so it is durable immediately rather than up to five seconds later.

Two honest limitations. An imported campaign **plays but does not open in the
campaign editor** - the editor only lists campaigns whose levels also exist
loose in `levels/`. And a campaign zip carries its levels and music but **not its
skins**, so a campaign built on a custom skin renders with the missing-skin
fallback unless the skin zip is shared separately.

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
