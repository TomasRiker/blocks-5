# Blocks 5 — WebAssembly port (spike)

An Emscripten build of the game. **Status: it renders and plays.** The menu,
level select and gameplay all draw correctly in a browser, with no GL errors.
Everything here is additive: the Visual Studio build is untouched, and every
change to `Blocks5/src` sits behind `#ifdef __EMSCRIPTEN__` or is a
standards-conformance or bug fix that MSVC also accepts.

## Building

Needs the Emscripten SDK and nothing else — every dependency is vendored in the
tree.

```sh
git clone https://github.com/emscripten-core/emsdk && emsdk/emsdk install latest && emsdk/emsdk activate latest
./build.sh
```

Serve `build/` over HTTP; `file://` will not work.

`build.sh` also packs `data.zip` and the skin archives into the staged tree, so a
clean clone needs no other preparation. `./build.sh clean` rebuilds from scratch.

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

Amputated: video capture (`videorecorder_stub.cpp`), screenshots
(`Engine::screenshot` returns early), the SEH crash handler, and the update
checker. The
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
| `compat.h` | force-included; MSVC CRT spellings and the `random()` clash with POSIX |
| `gl_immediate.cpp` | intercepts immediate mode and re-emits every attribute per vertex (see below) |
| `gl_compat.cpp` | the GL entry points Emscripten declares but never implements |
| `platform_stubs.cpp` | SDL cursors, SDL surface locking |
| `videorecorder_stub.cpp` | an inert VideoRecorder, so `engine.cpp` needs no edits — the real one is portable now, but nothing here captures audio and the browser has nowhere to put the file |
| `web_transfer.cpp` | the download/file-picker bridge behind Export and Import |
| `web_audio.cpp` | reads and resumes the `AudioContext` behind OpenAL |
| `pre.js` | mounts IDBFS at `/blocks5_home`, flushes it periodically, sizes the canvas, and wakes the `AudioContext` when the page comes back |

One of those deserves explanation.

**`gl_immediate.cpp`.** Emscripten's GL emulation computes a block's vertex count
as `4 * floatsWritten / bytesPerVertex` and asserts the result is whole — which
only holds if every vertex carries every attribute. Like most fixed-function code,
this game sets a colour once and then emits four vertices, and most of its 120
`glBegin` blocks are shaped that way. Rather than rewrite them all, this file
buffers each block and replays it with the current colour and texcoord attached to
every vertex.

**`gluLookAt` is not merely missing, it is wrong.** Everything else in
`gl_compat.cpp` fills a gap; this one corrects Emscripten. `libglemu.js` calls

    mat4.lookAt(GLImmediate.matrix[cur], [ex,ey,ez], [cx,cy,cz], [ux,uy,uz])

which is gl-matrix 2.x's "destination first" convention - but the gl-matrix
bundled with Emscripten is 1.x, where the signature is
`mat4.lookAt(eye, center, up, dest)`. So the current matrix goes in as the eye,
every real argument shifts one place along, and the result is written into the
three-element array that was meant to be the up vector and then dropped. The
modelview matrix is never assigned. Nothing errors, nothing warns: `gluLookAt`
is simply a no-op, and after the `glLoadIdentity` that precedes every call in
this game the camera sits at the origin looking down -Z.

Five places depend on it, and all five were quietly broken: the cube transition
(`cf_cube.cpp`) showed a flat still instead of a rotating cube, the end-of-level
zoom (`cf_zoom.cpp`) neither panned to the player nor rolled, `cf_slices.cpp`
and the unused `cf_camera.cpp` likewise, and the credits scene never moved.
`glMultMatrixd` is sound - `mat4.multiply(current, m)` post-multiplies, which is
the GL order - so `gl_compat.cpp` defines `gluLookAt` itself, Mesa's version on
top of it. Checked against a reference implementation for each of those cameras,
including `cf_zoom`'s rotated up vector: worst element error 4.7e-7, which is
float32 readback noise.

`gluPerspective` is correct but *replaces* the current matrix where real GLU
multiplies into it. Every call site here does `glLoadIdentity` first, so the two
agree; it goes through `glFrustum` now anyway, so they still would if one did not.

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

## Getting levels in and out

The Level Editor and Campaign Editor each gained an **Export ...** and
**Import ...** button, present only in the web build (the desktop build hides
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

One honest limitation remains: a campaign zip carries its levels and music but
**not its skins**, so a campaign built on a custom skin renders with the
missing-skin fallback unless the skin zip is shared separately.

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
