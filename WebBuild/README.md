# Blocks 5 — WebAssembly port

An Emscripten build of the game, and the one behind the "play in your browser"
link for 1.2.0. It plays the whole campaign and runs both editors; recording
video is the only thing it cannot do (a screenshot lands in the downloads), and
the Manager in the main menu moves levels, campaigns, music and skins between
the browser and your machine. The Visual Studio build is untouched by all of
it: every change to `Blocks5/src` sits behind `#ifdef __EMSCRIPTEN__` or is a
standards-conformance or bug fix that MSVC also accepts.

## Building

Needs the Emscripten SDK and nothing else — every dependency is vendored in the
tree.

```sh
git clone https://github.com/emscripten-core/emsdk && emsdk/emsdk install latest && emsdk/emsdk activate latest
./build.sh              # into build/
./build.sh hooks        # with the test hooks, into build-test/
./build.sh asan         # under AddressSanitizer, into build-asan/
```

Serve `build/` over HTTP; `file://` will not work.

`build.sh` writes `htaccess` out as `.htaccess` beside `index.html`, which gives
Apache the headers the caching story depends on: a year of `immutable` for the
three stamped payload files, `no-cache, must-revalidate` for `index.html`, `sw.js`
and `manifest.json`, the wasm MIME type, and `ModPagespeed off`. The same for
nginx, which reads no per-directory file and wants this in the server block:

```nginx
location ~ ^/(blocks5-[0-9a-f]+\.(js|wasm|data)|touch_controls-[0-9a-f]+\.js)$ {
    add_header Cache-Control "public, max-age=31536000, immutable";
}
location ~ ^/(index\.html|blocks5\.html|sw\.js|manifest\.json|icon-[0-9a-z-]+\.png|apple-touch-icon\.png)$ {
    add_header Cache-Control "no-cache, must-revalidate";
}
types { application/wasm wasm; }
```

The two directions are deliberate. A stamped URL's contents can never change, so
asking about it could only confirm what is already there; `index.html` carries no
stamp, because it is the entry point and the place the current stamp is written
down, so serving *that* from a cache is exactly how a new build becomes invisible.
`.claude/rules/web.md` has the whole argument, mod_pagespeed included.

`build.sh` stages `data.zip`, the skin archives and the campaign into the tree it
preloads, but it does not build them: they are build products and are not in Git,
so a clean clone needs `Blocks5/pack.sh` first. Each of the three is warned about
by name if it is missing. `./build.sh clean` rebuilds from scratch.

## What the player downloads

`blocks5.data` is Emscripten's preload package: the `webroot/` tree concatenated,
uncompressed, four bytes of overhead. 12.6 MiB, of which

| | |
| --- | --- |
| `levels/campaigns/blocks.zip` | 7.90 MiB — the 42 levels and all ten music tracks |
| `data.zip` | 3.03 MiB — every sprite, dialog, font and sound effect |
| `levels/skins/*.zip` | 1.67 MiB — the four skins |
| the rest | 12 KiB — two example levels and three readmes |

Nothing here compresses further over the wire: it is Ogg Vorbis, PNG and
deflated zip all the way down. The music is 28 minutes of stereo at an average
of 39 kbit/s, so it is already about as small as it can be and still be music.

It used to be 20.9 MiB. `build.sh` copied `levels/*.xml` and `levels/*.ogg`,
which reaches into the *author's* working tree - the 42 source levels and the
ten music files that `blocks.zip` is built from. All 52 shipped a second time,
byte for byte identical to a member of the archive, for 8.3 MiB of the download.
`stage.bat` never did that: the Windows tree ships `levels\example01.xml`,
`example02.xml` and `readme.txt`, and nothing else. Neither does the game need
it, which is what made the duplication invisible - campaign music is played
from inside the archive (`gs_game.cpp`, `p_currentCampaign->getFilename() + pw +
"/" + musicFilename`) and the two example levels name no music at all.

The libraries the Visual Studio build takes from `libs/bin` as Windows binaries
are compiled from source here instead — zlib 1.3.1 with its `contrib/minizip`,
libogg 1.3.2, libvorbis 1.3.4, TinyXML 2.6.2. The Visual Studio build compiles the
same sources from the same directories, so the two cannot drift apart; `libs/bin`
is down to one import library for OpenAL, and nothing else. Before
they were vendored this build compiled whatever the upstream clones happened to
be at, which was a *different* version of every one of them.

Image decoding used to be this build's own `img_load.cpp`, because Emscripten's
`IMG_Load_RW` decodes through the browser and only accepts names of preloaded
files — it cannot read the synthesised `SDL_RWops` the game hands it for a member
of a password-protected zip. That file now lives in `Blocks5/src/img_load.cpp` and
both builds use it: the Visual Studio build dropped SDL_image for the same
stb_image decoder, so this is no longer a web-only substitution.

The OpenAL headers come from `libs/openal-soft-1.25.2`, the same ones the Visual
Studio build compiles against, but the implementation behind them here is
Emscripten's `-lopenal`, not OpenAL Soft — those headers are plain AL/ALC 1.1
and public domain, so they work against either.

minizip carries two local changes, both deliberate and both load-bearing:
`NOUNCRYPT` is commented out in `unzip.c`, which is what makes the game's
password-protected archives readable at all, and `IOWIN32_USING_WINRT_API` is
commented out in `iowin32.c`. Everything else is stock.

## What this build does and doesn't do

Working: boot, config, the user directory (on IDBFS, so saves persist), SDL
video, OpenGL, OpenAL, texture loading straight out of the encrypted `data.zip`,
the fixed-timestep main loop, mouse and keyboard input, and rendering — tile
layers, sprites, fonts, the GUI, particles and weather.

Amputated: video capture (`videorecorder_stub.cpp`), the SEH crash handler, and
the update checker. The $A_TOGGLE_CAPTURE_VIDEO action is not registered under
`__EMSCRIPTEN__`, so F12 does not appear in Options -> Controls; F11 does, and a
screenshot goes to the browser's downloads through `WebTransfer::downloadBytes`.

Sound is gated on a click, because browsers refuse to start an `AudioContext`
without one - see below.

No display lists on any build: everything draws through `Renderer`, which is
a vertex buffer and one shader, and the star wipe is a fan of
`Renderer::triangles` on both toolchains - `CF_Star` needs no GLU tessellator,
since the star is a fixed shape a fan covers exactly.

## The pieces

| file | what it does |
|---|---|
| `build.sh` | the whole build; also stages the runtime tree, mirroring `stage.bat` |
| `compat.h` | force-included; the MSVC CRT spellings `_stricmp` and `_strnicmp` |
| `platform_stubs.cpp` | SDL cursors, surface locking, a real `SDL_UpperBlit`, SDL 1.2's key names, and the pixel-format fields `SDL_CreateRGBSurface` leaves unset |
| `videorecorder_stub.cpp` | an inert VideoRecorder, so `engine.cpp` needs no edits — the real one is portable now, but nothing here captures audio and the browser has nowhere to put the file |
| `web_transfer.cpp` | the download/file-picker bridge under `Blocks5/src/transfer.cpp`: Blobs, `<input type="file">` staged by extension, `FS.syncfs` |
| `web_bluescreen.cpp` | what the Quit button does where a page cannot close its tab |
| `test_hooks.cpp` | the test hook's way into `Module["b5_test"]`; empty without `-DBLOCKS5_TEST_HOOKS` |
| `web_audio.cpp` | reads and resumes the `AudioContext` behind OpenAL |
| `pre.js` | mounts IDBFS at `/blocks5_home`, flushes it periodically, sizes the canvas, swallows the function keys, and wakes the `AudioContext` when the page comes back |
| `shell.html` | the page: viewport, boot screen, `locateFile` with the build's stamp, service worker registration |
| `sw.js`, `manifest.json`, `htaccess` | the offline cache, the install manifest and the Apache headers (see above) |
| `touch_controls.js` | the on-screen pad, an ordinary page file with a stamp of its own |
| `make_icon.py`, `make_text.py` | the icons and the boot screen's line, generated at build time from `data/` |
| `test/` | the Playwright harness and its scripts; see `test/README.md` |

No GL file among them. The build links against Emscripten's plain WebGL library,
with no `-sLEGACY_GL_EMULATION` and no shim of its own: everything the game draws
goes through `Renderer` and the present filters' programs, which is what made
that possible (`RENDERER-REDESIGN.md`), and it is also what keeps it so - a
fixed-function call anywhere in `Blocks5/src` is an undefined symbol at this
link, so the desktop cannot quietly grow one the browser lacks.

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

## Coming back to the tab

Switching away from the tab and back used to kill the music for the rest of the
session, and it took two fixes because it has two halves.

The game's half: a hidden page gets no `requestAnimationFrame`, so the main loop
stops, and with it `StreamedSound::pumpBuffers` - the only thing that refills the
OpenAL queue in this build, since there are no decoder threads here. Four buffers
of a quarter second each means the music runs dry after one second and the source
goes `AL_STOPPED`. The old restart condition was `AL_BUFFERS_QUEUED == 0`, which
an underrun never produces: coming back, the refill hands the source four fresh
buffers and the queue is 4 again. `pumpBuffers` now also restarts a source that
reports `AL_STOPPED`, and leaves `AL_PAUSED` alone so a deliberate pause survives.

The browser's half: Chrome suspends the `AudioContext` of a backgrounded page,
and Emscripten's unlocker is `{once: true}` and was spent on the first click of
the session, so nothing would ever resume it. `pre.js` adds a `visibilitychange`
and `focus` listener that does - from a real DOM event, which is exactly what the
main loop is not at the moment the page returns.

Measured by stubbing out `requestAnimationFrame` for four seconds, which is what
a hidden tab amounts to: the streaming source reads `STREAMING/PLAYING q=4 p=0`
before, `STREAMING/STOPPED q=4 p=4` while hidden - a stopped source with a full
queue, the state the old check could not see - and `STREAMING/PLAYING q=4 p=0`
again half a second after the frames resume.

## Getting levels, campaigns, music and skins in and out

One **Manager** button in the main menu, on all three platforms, with Import,
Export and Delete. The platform-independent half is `Blocks5/src/transfer.cpp`; this file is only the
bridge under it - Blobs, `<input type="file">` and `FS.syncfs`, nothing about
levels or skins. Windows uses `GetOpenFileNameA`/`GetSaveFileNameA` through the
same interface: `beginImport()` starts the dialog and `pollImport()` is asked
once a logic tick, so the browser's asynchronous picker and Windows' modal one
look identical to the caller.

Import reads the file and works out what it is by content, never by extension -
`OggS` is music, an XML rooted at `<Level>` is a level, an archive with a
`campaign.xml` is a campaign, one with `tileset.xml` and `sprites.png` is a
skin. Everything else is refused. The upload is staged *outside* `/blocks5_home`
so a rejected file never reaches IndexedDB; C passes all three possible staging
paths down and JS picks one by extension, so C still composes every path and JS
composes none. The browser's filename is only a suggestion, run through
`sanitizeFilenameStem`. On success the import forces an `FS.syncfs` so it is
durable immediately rather than up to five seconds later.

The Manager asks for the kind first, lists what is installed, and re-reads that
list on every switch and on *Refresh*. What it writes is a plain copy - including for
a password-protected skin, where decrypting on the way out would be a back door
around the reason it is packed that way. Such an archive cannot be opened by the
recipient but is still perfectly usable by their game: the password travels
inside it as `password.txt`, and `Level::getSkinFilename` reads that from any
skin archive under any filename.

A level can also borrow a track from the shipped campaign with
`musicFilename="blocks:music2.ogg"`, which is what makes music usable here at
all: the browser has no way to drop an `.ogg` next to a level, and
`Campaign::save` knows not to pack a track that every installation already has.

## Opening a campaign that arrived as a zip

An imported campaign used to play but refuse to open in the Campaign Editor, and
so did the shipped `blocks.zip` - on Windows too. The editor's whole model rests
on loose files in `levels/`: `Campaign::save` re-reads every level from there,
the *Available Levels* pane lists what is there, and `originalLevelsExist()`
refused to load a campaign unless every `<Level>` entry existed there as well.
Nothing in the tree had ever copied a member back out of a campaign archive.
(The shipped campaign failed that test because its `campaign.xml` named
`level_03b.xml` at position 3 while the file shipped loose is `level_03.xml` -
same bytes, different name. That entry has since been corrected inside
`blocks.zip`, so the official campaign now resolves from the loose files again,
which is what lets a level edited in the Level Editor feed back into it.)

The mapping was never actually missing, though: it is the ordinal. `Campaign::save`
has always written entry *i* as member `level_{i+1}.xml`, and the play path has
always read it back that way, which is exactly why an imported campaign played.
So a level is now a `Campaign::LevelRef` - a *source directory* plus a *member
name*, with the `<Level>` text demoted to a display label. A campaign whose
entries all exist loose is served from those loose files, bit for bit as before,
so the local authoring loop (edit a level, reopen the campaign, save) is
unchanged. Any other campaign is served entirely out of its own archive, and its
entries are marked *(in campaign file)* in the editor. The choice is made once
per campaign rather than per entry, so a foreign campaign can never quietly bind
to a same-named level the player happens to own.

Nothing is written into `levels/`. Saving such a campaign repacks it from the
archive it came from, which is why `Campaign::save` now builds a sibling
`~campaignsave.zip` and only then replaces the destination: the old code deleted
the destination first, which for an archive-backed campaign would have destroyed
the levels it was about to read. The `musicFilename` attribute is checked with
the new `isSafeMemberName` (unguarded, in util.cpp) before it is appended to a
path - it comes out of a foreign file, and the old code concatenated it
unchecked, which packed whatever it happened to name into a zip the user can
then export.

What this does not give you: an archive-backed level cannot be opened in the
Level Editor, which still works on loose files only. And *Available Levels*
hides any loose file whose name matches a campaign entry, so a player who owns a
different `level_01.xml` cannot add it to a campaign that already has an entry of
that name without removing that entry first.

## What was actually wrong

Worth recording, because none of it was predictable from reading the code. The
first, the fourth and the fifth concern the legacy GL emulation the build ran
on until stage 3 of `RENDERER-REDESIGN.md` took it out; they stay for the
technique.

1. **`glPushAttrib`/`glPopAttrib` as no-ops turned the screen black.** The
   texture binding of the time bracketed a `glMatrixMode(GL_TEXTURE)` edit with
   them, so the matrix mode stayed `GL_TEXTURE` after the first bind and every
   `glPushMatrix`/`glTranslated` in the game transformed texture coordinates
   instead of geometry. Nothing errored. A shim restored the mode and the
   enables until the emulation went; nothing pushes any more.
2. **`SDL_BlitSurface` is implemented on a 2D canvas.** It `drawImage`s from a
   source canvas, which only exists for surfaces Emscripten's own SDL created
   from an image. Every surface this game blits is written directly in memory, so
   the blit copied nothing and every texture uploaded fully transparent.
3. **Emscripten numbers keysyms SDL2-style** (`scancode | 1<<10`), so `SDLK_F7`
   is 1088 and `SDLK_LSHIFT` 1249, against `Engine`'s 512-entry key tables. The
   overflow read back as Shift+F7, which is the unlock-all-levels cheat.
4. **`GL_INT` is not a valid vertex-attribute type in WebGL**, and
   **`GL_UNPACK_ROW_LENGTH` does not exist** — both silently ignored after
   raising `INVALID_ENUM`. The upload no longer asks for a row length: a 32-bit
   SDL surface's rows are tight on every platform, and `texture.cpp` checks
   that rather than assuming it.
5. **Emscripten's own `gluLookAt` was a no-op.** `libglemu.js` called
   gl-matrix's `mat4.lookAt` in the 2.x argument order while bundling the 1.x
   library, so every argument slid one place, the result landed in the
   three-element up vector and the modelview was never assigned. The cube
   transition, the end-of-level zoom, the slices and the credits all stood
   still. A shim defined `gluLookAt` itself, Mesa's version, until `Mat4` in
   `vec.h` took those cameras onto the CPU and the emulation went.

Emscripten's own legacy-GL texturing, texture matrices and immediate mode were
all fine; each was ruled out with a standalone 40-line test program before
suspicion moved on. That is the technique worth reusing: when the whole port
misbehaves, isolate the platform feature in a program small enough to be
obviously correct.
