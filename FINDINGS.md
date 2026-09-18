Blocks 5 - findings from the 1.2.0 English sweep, and what became of each
==========================================================================

What the translation pass turned up while reading every comment in the tree:
bug candidates, comments that contradict the code, stale claims, doubts and
typos, each reported by an agent reading the code around it and most of them
re-read by a second agent that went back to the code independently.

Every entry was checked again against the tree as it stood on 2026-09-17, by
identifier and not by line number - the line numbers the reading agents gave
belong to the sweep's revision and have not survived it, so an entry names the
function or the phrase instead. The dispositions at that check:

| category | entries | fixed | not a defect | gone with the code | still open |
| --- | ---: | ---: | ---: | ---: | ---: |
| bug candidates | 73 | 47 | 2 | 3 | 21 |
| comments contradicting the code | 23 | 7 | 0 | 3 | 13 |
| stale claims | 54 | 20 | 1 | 5 | 28 |
| doubts and observations | 187 | 34 | 27 | 19 | 107 |
| typos and formatting | 18 | 9 | 1 | 3 | 5 |
| **all** | **355** | **117** | **31** | **33** | **174** |

Many entries came in twice (the first reader's and the second's), so the 174
open entries were about 120 distinct things. A branch straight after the check
then fixed everything that was worth a change in code - the eleven wrong
behaviours a player could meet, the latent hazards, the dead code, and the
three the author was asked to decide - so what is still open is comments and
documents that say one thing while the code does another, and a short list of
things worth knowing. Both come first; the settled record follows in short
form, each fix with the commit that made it. A finding that turned out to be
nothing is kept with its reason, because that is what stops the same false
alarm being raised again.


Still open: comments and documents that disagree with the code
--------------------------------------------------------------
Each is a rewording, and the fix is in the entry.

- `LinuxBuild/build.sh`: "without the three that do not come along" and
  "audiocapture does come along" over a grep that excludes two (stackwalker,
  pch); audiocapture compiles its Linux branch. Say two.
- `campaign.h`: `makeLooseRef()` and `loadSingleLevels()` are documented against
  the user's folder alone, while both walk the two content roots.
- `campaign.cpp`: "clear() sets both as well" over three assignments; and
  `makeMemberName()`'s "(campaign.cpp, save())" names its own file.
- `diamondmachine.h`: `sparkId` "marks the inward sparks" - both loops stamp
  it, and `abortConversion()` tells the kinds apart by the sign of
  `deltaColor.a`. `diamondmachine.cpp`: "d^moves = inAccel" for `IN_ACCEL`; the
  timetable diagram closes the inward bar at 80 while `SPARK_IN_END` is 92;
  `SPARK_IN_MIN_LIFE` (8) can never bind while `SPARK_IN_END` stays 92; "the
  last quarter belongs to the collecting alone" is the last fifth; and
  `updateSprites` hardcodes the diagram's 80.
- `main.cpp`: the version table says "no Blocks 5 folder" where the code tests
  `listDirectory().empty()`, so an existing empty folder is `not_played`; and
  the config.xml comment sits over the `progress.zip` copy it does not
  describe.
- `LinuxBuild/test/smoke.sh`: the music section restates the Export/Delete rule
  of the levels section thirty lines above; cut it to a pointer.
- `transfer.cpp`: "the seven names under which the game itself ships
  something" over an `isBuiltIn()` that asks the disk, not a list; and
  `list()`'s "nothing can be saved or imported under a shipped name" is false
  for the two example levels, which the `std::find` below handles.
- `transfer.h`: "both platforms" for three file-dialog paths; `classify()`'s
  "a tileset.xml" where the code wants `sprites.png` beside it.
- `u_crt.cpp`: the block over `toLinear()` claims `x*x`, which is the halation
  taps' arithmetic, not the function's (`pow(GAMMA_IN)`); "the macros below"
  and "once more below as a macro" for macros above; "the mouse conversion in
  engine.cpp" for a formula that lives in this file; `MASK_AVG`/`SCAN_AVG` for
  `maskAvg`/`scanAvg`; a present cost of 7.9 against the table's 7.8 in the
  same file; and two section banners back to back with the first labelling a
  section four lines further down.
- `util.h`: `isSafeMemberName()`'s doc omits the four other Windows-reserved
  characters and the 100-character cap the code refuses.
- `web_bluescreen.cpp`: the comment derives 48em, the CSS says `52em`; say
  which is slack. `web_bluescreen.h`: "under Windows SDL_QUIT quits" is true
  outside the browser altogether, the `#else` stub is dead in every build, and
  "a key press or a click" omits the touch the code also takes.
- `Tools/syntax.sh`: the comment names four of the six generated shims, two of
  which are idle (`shellapi` is written lowercase in the sources,
  `VersionHelpers.h` appears only in `stackwalker.cpp`, which the script never
  compiles) while `alc.h` is needed by every file; and "1 source files".
  `checks.md` names three.
- `Build.bat`: the `/sdk:` header line describes an outcome the project files
  produce, not something the script passes; the header's lines 38-56 are the
  archaeology of constraints that no longer exist, with line 474 pointing at
  them; the `:onlytoolset` comment describes a four-character test the guard
  performs as five; and `:doclean`'s "misc\3p_campaigns\*.zip" lacks the
  `Blocks5\` every path below it carries.
- `WebBuild/build.sh`: the "no %%LOADTEXT%% in the page" message is %-formatted
  and prints one percent sign each side.
- `img_load.h`: "zip_data.bat and zip_skins.bat pack nothing but *.png" - they
  pack ogg, dat, xml and txt, and `pack.sh` is unnamed; the point is that no
  other image format ever gets in.
- `level.h`: "all 220 shipped and third-party level files" - 142 today, every
  one 40x25; drop the number.
- `gs_campaigneditor.cpp`: "straight into the IndexedDB ... as in the level
  editor" - the level editor makes no such call, so a level saved in the
  browser waits for the five-second interval. The better fix is the call
  itself in the level editor's save path; and `else if(!confirmed)` where the
  `else` already implies it.
- `gs_leveleditor.cpp`: "keeps the display in step every frame" for an
  `onUpdate` that runs per tick. `engine.h`: `consumeKeyPress` takes the
  "pressed in this frame" flag off, which is cleared per tick.
- `gs_menu.cpp`: the Escape comment names the donation question, not the CRT
  offer pane the condition also tests.
- `make_text.py`: "the offset from the font.xml as the first row" is returned
  and discarded, never applied; the empty-text `SystemExit` is unreachable
  behind a `max()` that raises first; `as_js`'s docstring writes `d:"..."` for
  a `d:'...'` the code emits. `make_icon.py`'s usage omits the third
  positional argument `main()` accepts; `make_ico.py`'s "the one size where
  the next integer step down would be 1x" holds for 20 as well - the margin is
  what singles 24 out.
- `platform_stubs.cpp`: "all four call sites ... first switch the surface alpha
  off" - three blit sites, and it is the source's flag, cleared when the parent
  was made; "all five call sites" of `SDL_CreateRGBSurface` - four; and
  `build_asan.sh` no longer exists.
- `testhooks.cpp`: the "where the game sees the cursor" comment sits over
  `appActive`, thirty lines from the `mouseDown` and `cursor` it describes;
  "a stat() on a file" for an `fopen()`.
- `gs_loading.cpp`: `gestureTime` is documented as a time and only its sign is
  ever read. `gs_selectlevel.cpp`: `onRender()` dereferences
  `p_currentCampaign` unguarded on an invariant `loadLevel()` keeps and nothing
  states. `gui_window.cpp`: the local `title` shadows the member. `laser.cpp`:
  `onElectricitySwitch(bool on)` shadows the member `float on`; the debris of
  a destroyed object starts from the beam's cell rather than the object's
  position as `lava.cpp` does. `lava.cpp`: the debris loop's `int i` shadows
  the live iterator.
- `streamedsound.cpp`: "stops the source without emptying it" - the queue.
  `sounds.xml`: "less signal level for the same computational load" - the
  same bitrate. `audiocapture.h`: the header states the sample format and not
  the rate contract the code honours.
- `cf_rewind.cpp`: "How many there are depends on the speed of the tape" over a
  fixed `NOISE_BARS = 5` - where they sit does. `cf_star.cpp`: "a triangle fan"
  for plain triangles with the centre repeated. `e_gate.cpp`: "undefined inputs
  give an undefined output" over a test that also covers an unconnected one.
  `e_flipflop.cpp`: "unclocked" where the tooltip says "level-triggered".
  `barrage2panel.cpp`: `// switch` on a panel. `barrage2.h` and `barrage.h`
  carry the same header and name no variant. `tileset.cpp`: "record the tile
  type" over the whole `TileInfo`. `texture.cpp`: "Creating non-pow2 texture!
  This could cause trouble" for the case `applyWrapMode()` exists to support.
- `game.xml` and `leveleditor.xml`: the Quit button's comment accounts for the
  three pixels past the top and not the two past the right edge. `menu.xml`:
  "unpressed at column 0" holds for the large cells; the small ones sit at 160.
- `engine.cpp`: two comments name `emscripten_set_main_loop` for a call to
  `emscripten_set_main_loop_arg` whose trailing `1` is load-bearing;
  `firstEventRecorded` sits unconditionally in the browser namespace and under
  `#ifdef RECORD` natively.
- `options.cpp`: the "start with no selection" comment gives a reason that
  cannot arise, since the pane is hidden throughout `show()`.
- `pre.js`: "the on-screen controls" where everything else says the pad.
- `presets.cpp`: the type is spelled "Amboss" and its tooltip id `$TT_AMBOS`;
  the type name is in every level file, so if anything moves it is the id.
- `Tools/verify.py`: the sentence about a GUI exception in `check_xml_attrs`
  describes an exclusion that is neither implemented nor needed.
  `Tools/selftest.py`: "goes back in a finally" for a `Patch.__exit__`.
  `LinuxBuild/test/harness.sh`: the 60 ms hold's explanation does not follow
  from its own model (a longer hold makes a poll landing inside the window
  more likely, not less; what the measurement supports is that press and
  release are drained by one poll and the hold stays under the 140 ms repeat
  delay); and the `no element` message is discarded by the `except SystemExit`
  three lines below it.
- `WebBuild/test/smoke.js`: "the four buttons below it are disabled" - three,
  and `input.md` says the same of *Reset all*, which stays clickable because
  resetting all needs no selection; "SDL_WINDOWEVENT ... which the game does
  not listen for" - it does, and the same script tests the branch; what is
  true is that nothing polls it while the tab is hidden.
- `Blocks5/pack.sh`: the data and campaign branches are `[ ] || [ ] && { }`
  beside an `if` for the same decision - correct, unexplained.


Still open: worth knowing, not worth a change
---------------------------------------------
- `barrage.cpp` and `barrage2.cpp` are near-identical by design (two object
  types differing in sprites and in how `change()` is told the state); their
  "If an object is standing there right now" comments are a matched pair
  maintained by hand.
- `Hotel::onUpdate()` re-arms the welcome bubble every tick the active player
  stands on it: a standing "player is here" display, not a latch, and a
  second, non-active player leaves `state` at 1 - invisible either way.
- `ConveyorBelt`'s shared `soundChanged` flag would miss a second electricity
  toggle inside one tick; nothing in the tree does that.
- `E_PulsePanel`, `E_PulseSwitch` and `E_ValueSwitch` share one click sound; a
  line saying so would earn its place.
- `MASTER_HEADROOM` (`engine.cpp`) cites a -1 dBTP ceiling and reports -0.9
  dBFS after the headroom; whether a sample peak of -0.9 meets a true-peak
  ceiling of -1 is the author's to re-measure. The two sets of numbers the
  sweep found are one set now.
- `hint.cpp` explains the note pool as a read-while-written hazard; in-order
  GL rules that out, and the real reason is the per-note bake cache (a note
  re-bakes only when its text changes, so a shared sheet would show the
  other's text), which `engine.h` states. The claims no longer contradict.
- `Options::handleClick` writes every widget into the engine before it looks
  at the name, and Cancel takes it back through `loadConfig()`: the documented
  design, and idempotent.
- `File_Real` reports `error = 9` for a failed delete, the shared
  delete-failure code both file classes use; callers test truth only.
- `util.cpp`'s `decryptPassword()` writes fixed buffers with no bound on its
  input, but the only input is the two compile-time bracketed paths in
  `main.cpp` and `campaign.cpp`.
- `E_Gate::doLogic()` still has no `default:` in its switch; the constructor's
  clamp makes every reachable value a case. Cheap insurance, nothing more.
- `Level::getTileAt()` answers `-1` as a `uint` for an off-map cell, which
  `TileSet::getTileInfo()` maps to `badTile` and the editor's autotiler reads
  as "no neighbour" - so 0 is not the fix; a sentence at both ends is.
- `SoundInstance::update()` calls `abs()` on floats; the floating overload is
  the one in scope through `pch.h` (checked with the build's own flags).
- `tileset.h`'s "all nine tileset.xml in the tree" counts the four skin
  archives, as its own enumeration says; five are in Git.
- Ctrl+Y is undo and Ctrl+Z redo, QWERTZ-natural and documented in the tooltips.


Settled
-------

### Fixed during the sweep's follow-up

- **hotel.cpp** - `onRemove()` clears the static `p_hotelToSave`, guarded with
  `== this` as `onUpdate()` guards it, since several characters can stand on
  several hotels. (e99904d)
- **electronics.cpp** - the `oldValue<N>` sentinel matches its test,
  `0x7FFFFFF` in both places, so a save without the attribute no longer writes
  the sentinel into the pin. (e99904d)
- **font.cpp** - the tag guards read `r >= 2` / `r >= 3`, so a tag ending a
  string is recognised and a truncated title no longer draws a literal
  `</h>`. (e99904d)
- **filesystem.cpp** - `openFile()` checks `p_file` before dereferencing it;
  `readStringFromFile()` closes the file on the empty path. (e99904d, cd0013a)
- **presets.cpp** - the Hint preset checks its `<Text>` element. (e99904d)
- **tileset.cpp** - `badTile` is set before `reload()` starts every tile from
  it; a `<Tile>` without an id, or with an empty one, is an error rather than a
  null dereference, and the id byte no longer sign-extends past the 256-entry
  table. (e99904d, 9f2e72f)
- **gui.cpp** - `GUI::init()` tests `p_font` before using it. (e99904d)
- **util.cpp** - `printfLog` formats with `vsnprintf`. (e99904d)
- **singleton.h** - `operator=` returns `*this`. (e99904d)
- **level.cpp, player.cpp, e_barrage.cpp** - the five locals handed to TinyXML
  start at 0 (`ndc`, `destroyTime`, `ghost`, `inv`, `up`). (e99904d)
- **gui_button.h, gui_staticimage.h, gui_radiobutton.h** - the `INLINE_SETTER`
  declarations carry setter names; **leveleditor.xml, selectlevel.xml** - no
  two siblings share a name. (5085dd2)
- **file_archived.cpp** - `deleteArchivedFile()` copies each surviving member's
  local record verbatim, sized from the local header, instead of allocating
  from the central directory's lengths and writing the local ones. (a5cf3e0)
- **e_gate.cpp** - the constructor clamps `subType` to 0..7 and logs, which
  closes the uninitialised `z` in `doLogic()`. (a5cf3e0)
- **bomb.cpp, projectile.cpp** - the debris alpha delta is `-p.color.a /
  p.lifetime`; the double minus made it climb. (9830da6)
- **manager.h, audiostream.cpp, sound.cpp, filesystem.cpp** - a resource whose
  constructor failed, a stream that could not read its file, the four error
  returns of `Sound`'s constructor and every `File` that could not open are
  deleted rather than leaked; `~Sound` and `~AS_Ogg` survive a half-built
  object. Measured with AddressSanitizer: 193100 bytes in 1100 allocations
  before, none after. (cd0013a)
- **engine.cpp** - the key-event queue is drained once per tick, beside the
  per-slot loop; `loadStringDB()` tests a comment line with
  `compare(0, 2, "//")`; `exit()` frees both cursors; the joystick loop's
  comment is its own; `detectSystemLanguage()` counts 440 strings; the
  `STRESS_TEST` joke identifier is gone. (dec6da9, a29ed8f, later)
- **gui_window.cpp** - the title is localized once, and the measured string is
  the drawn one. (dec6da9)
- **diamondmachine.cpp** - `p_soundInst` is assigned, and an aborted conversion
  slides the sound down to zero rather than pausing it, since a paused
  instance is never reaped; `if(counter >= CONVERSION_TICKS)` names the
  constant. (dec6da9, later)
- **sound.cpp** - `~Sound` takes its instances out of the static
  `allInstances`, which `getFreeSource()` walks; `Sound::isLiveInstance` makes
  keeping an instance pointer across ticks a supported thing. (dec6da9)
- **parameterblock.h** - `operator=` returns early on self-assignment; the
  thrown string is English. (a29ed8f)
- **gui_radiobutton.cpp** - `onMouseUp` fires `changed` once, through
  `check()`. (a29ed8f)
- **gs_leveleditor.cpp** - the unreachable `else if(!shift)` branch of `draw()`
  is gone, and the Hint placement clears under `!shift` like every other; the
  four "nothing would otherwise happen here" comments are conditional and
  identical. (f684d02)
- **level.cpp** - spawned objects get UIDs no loaded object has (`nextUID`),
  so `sortObjects()`'s tiebreak is total. (53a85ae)
- **progressdb.cpp** - the read guards the root element, a missing `campaign`
  attribute and the level index; "nothing here may be taken on trust".
  (9096884)
- **videorecorder.cpp** - `vbv_size_bytes` is set, and the comment describes
  the measured effect of setting it. (d4bb8d9)
- **Tools/verify.py** - the dead `pch.cpp` branch is gone; docstrings and
  messages are English; the GUI-exception sentence is the one thing left
  (above). (37b6e3b, later)
- **gui_editbox.cpp, gui_multilineeditbox.cpp** - the Ctrl block ends in
  `break`, with the platform reason beside it, so Ctrl+A no longer types an
  "a"; `getIndexAt()` measures at the offset the text is drawn at.
- **audiocapture.cpp** - the unread `overflowed` member is gone.
- **linedrawer.cpp** - `draw()` returned on an empty vector; the class went with
  the renderer, and `Renderer::polyline` returns on fewer than two points.
  (dec6da9, 1a88c34)
- **Build.bat, zip_data.bat** - `7za.exe` and `optipng.exe` live in `Tools\` and
  the scripts name them through `%~dp0`, one idiom throughout; the packing
  message names the right path. (8a6cc98)
- **Every German string in the tooling and the tests** - `build.sh` (both),
  `pack.sh`, `harness.sh`, `smoke.sh`, `smoke.js`, `mobile.js`, `burst.js`,
  `encode_sounds.py`, `make_ico.py`, `syntax.sh`, `zip_data.bat`, and the
  Python docstrings - reads English now; `WebBuild/test/README.md` and
  `Tools/README.md` are English, and `burst.js`'s pointer at the README's
  heading resolves.
- **Comments and docs corrected since**: `MASTER_HEADROOM` has one set of
  measurements; the note pool's three descriptions no longer contradict;
  `cf_rewind.cpp` names `misc.png` and its strip, and its render comment names
  the display by what the artwork shows ("REWIND" and two triangles, decided by
  looking at the sheet); `hint.cpp`'s destructor comment no longer points "up
  there"; `hint.cpp`'s radius is the radius; `pre.js` explains why every
  function key is swallowed; `streamedsound.cpp` says "Windows/Linux";
  `util.cpp`'s `isSafeMemberName` names the runtime data as its reason;
  `packing.md` says `7za`, as the script does; `CLAUDE.md` counts twenty-two
  checks; `LinuxBuild/build.sh` prints its own unit count instead of a stale
  one; `main.cpp` drops no `reinterpret_cast<int>` on a handle and no dead
  `length1`; `lava.cpp` has no `p_destroyed` alias; the `zip_*.bat` line
  endings are uniform per file and `zip_data.bat` says which file carries
  what; `u_sharpfit.cpp` says "the embedded shading language"; `make_icon.py`'s
  two sentences read as meant; the German spelling slips (`zurueklegt`,
  `nachbarn`, `blosss`, `Multipliktion`, the doubled "Ohne") vanished with the
  translation; the four-space indent in `file_archived.cpp`, the trailing
  space in `gui_multilineeditbox.cpp` and the lone tab in `main.cpp` are gone.

### Fixed on the branch after the check

Engine and sound (c8adb02):

- **engine.cpp, engine.h** - the mute key and a lost focus no longer write 0
  into the volume settings. `muted` and `appActive` silence the output through
  `getEffectiveSoundVolume()` / `getEffectiveMusicVolume()`, which the sound
  instances and the music streams read; the settings stay what the user chose,
  so F1, the focus, the options dialog and config.xml can no longer overwrite
  each other's copy of them. `setCursorPosition()` guards on the present
  rectangle, as its inverse does.
- **sound.cpp** - `createInstance()` deletes an instance that got no source
  and returns 0, so no object can keep a pointer the next `update()` reaps;
  the five holders guard the 0 (`elevator.cpp` and `player.cpp` gained theirs),
  and the comment on `forceCreation` says what a 0 costs now: a level whose
  ambience never starts, not a crash (2af6608).
- **soundinstance.cpp** - the constructor sets every member before asking for
  a source.
- **streamedsound.cpp** - `stop()` clears `sourceID`.
- **audiocapture.cpp** - `AudioRing::clearRing()` tests the mutex; the Linux
  `start()` tests `opened`; the Linux thread stores a failed read and
  `stop()` / `close()` log it on the main thread.
- **as_wav.cpp** - the chunk walk skips the RIFF pad byte after an odd-sized
  chunk. **as_ogg.h** - the unused `FILE*` member is gone.
- **toxicgas.cpp** - the hiss slides to its volume, 0.05 per tick, instead of
  jumping, so it fades in from the first cloud on (the author's decision).
- **electricitypanel.cpp** - the click plays at priority 100 like the switch's.

Files and logging (70523c3):

- **file_archived.cpp** - `deleteArchivedFile()` checks both `fopen()`
  results, starts `signature` at 0, tests the `fread`, and on a record it
  cannot rewrite closes both files, removes the side file and returns -2 -
  which `FM_WRITE` turns into an error instead of appending a second member
  of the same name, and `FM_DELETE` into "not deleted" (2af6608 starts
  `outArchive` at 0 for that early return).
- **file_real.cpp** - `tell()` answers 0 without a handle; `size` starts at 0.
- **filesystem.cpp** - `getAppHomeDirectory()` hands `SHGetFolderPathA` a
  `MAX_PATH` buffer and falls back to the working directory when the call
  fails; `evalRelativePath()` tests `i + 3 <= length()` instead of subtracting
  from a `size_t`.
- **util.cpp** - the crash log's `fopen()` is checked; `prepareForTinyXML()`
  is gone. **PWEncrypt/main.cpp** - the temp file's `fopen()` is checked.
- **main.cpp** - the update check's `Task` lives on the heap and is freed by
  whichever of the caller and the thread lets go last; the handle is closed;
  the result counts only when the thread was seen to finish. The dead `quit`
  is gone.

Editors and game states (c946ebb):

- **gs_leveleditor.cpp** - `paste()` refuses a rectangle with either corner
  outside the level; the `CHECK_IF_IT_REALLY_IS_A_LEVEL` block is gone.
  **level.cpp** - `hashObject()` tests both coordinates, not the flat index.
- **gs_campaigneditor.cpp** - Load shows `$LE_ERROR_FILE_DOESNT_EXIST` for a
  name that is no file.
- **gs_game.cpp, gs_game.h** - `levelNumber` and `p_currentCampaign` start at
  0; the undefined `updateMusic()` declaration is gone.
- **gs_menu.cpp** - the demo's key lookup waits for `time >= 500` instead of
  wrapping.
- **gs_selectlevel.cpp** - a locked bonus level shows "???" like any other
  locked level.

Objects (731d13b):

- **enemy.cpp** - the devil draws `random(0, 3)`: four ways, each as likely.
- **mirror.cpp, teleporter.cpp** - the constructor clamps `subType` to 0..1
  with a warning, as `E_Gate` does, and the tooltips switch on the value
  itself. **e_flipflop.cpp** - the same for 0..2, and the JK case has its
  `break`.
- **eye.cpp** - the spawned enemy is tested before use.
- **arrow.cpp** - `turn()` wraps `dir` and `shownDir` together, so a save
  carries 0..3 and the animation keeps turning the same way.
- **laser.cpp** - `destroyTime` stops at the burst, which fires on the step
  that reaches 0; `onRender()` starts its beam vectors at 0 and always keeps
  the first two points (**lightbarriersender.cpp** the same).
- **lightbarriersender.cpp** - `counter` advances once per tick, outside the
  beam loop, as the laser's does (the author's decision: the pulse runs at one
  rate whatever the beam's length).
- **lightning.cpp** - the secondary branch walks a `Vec2f`.
- **elevator.cpp, electronics.cpp** - the TinyXML locals start at 0.
- **presets.cpp** - the "Damage" branch tests `p_element`; the six electronics
  branches construct after the attribute block, so a preset asked for without
  an element still yields an object.
- **pin.cpp** - `disconnect()` writes the input's value and old value
  directly; `connect()` drops its unread `p_output`; the `EDGY_CONNECTIONS`
  block is gone.
- **sprite.cpp** - `add()` says that it reuses the last slot past
  `MAX_SPRITES`.
- **e_lightbarrierreceiver.cpp, e_lightbarrierreceiver.h** - `value` is
  neither saved nor loaded, since `frameBegin()` zeroes it before anything
  reads it. **e_barrage.h** - the include guard is `_E_BARRAGE_H`.
- **object.cpp, objects.md** - the flash comment says 25 ticks, half a second,
  which is what `FLASH_DECAY = 0.8` does; the decay itself is the author's
  choice and stays.

GUI, options, crossfades, textures, the browser and the tools (4bf936c):

- **gui_listbox.cpp** - Page Up and Page Down size the page by the list's own
  font.
- **gui_scrollbar.cpp** - `setDragBarY()` returns when the content fits.
- **gui_checkbox.cpp** - `<Checked>` goes through `setChecked()`, so loading
  fires nothing.
- **gui_multilineeditbox.cpp** - the two end-of-text tests add instead of
  subtracting.
- **gui_radiobutton.cpp, gui_radiobutton.h** - a `$ID` image is re-resolved
  on a language switch, as the button's and the static image's are.
- **options.xml** - Cancel's caption is `$CANCEL`. **options.cpp** - `changed`
  starts false in `show()`, so Cancel reloads config.xml only when something
  was touched.
- **cf_zoom.cpp** - the zoom loop has a variable of its own; the white flash
  uses the transition's `t`.
- **cf_colorblend.cpp** - each side of the peak has its own slope, so the
  colour is transparent at both ends whatever `timing` is, and `timing` is
  clamped away from 0 and 1.
- **texture.cpp, texture.h** - a sub-texture is built by a constructor that
  copies straight out of the parent's pixels, so the sheet is decoded once per
  skin.
- **transfer.h** - the unused `Campaign` forward declaration is gone.
- **web_transfer.cpp, web_transfer.h** - `abandon()` closes the picker on the
  page side: a file chosen afterwards is neither written nor reported.
- **Tools/encode_sounds.py** - the summary counts the encodes that succeeded.

### Not a defect

Kept with the reason, so the alarm is not raised twice.

- **`p_soundInst->stop()` without a null check** in `conveyorbelt.cpp`,
  `laser.cpp`, `elevator.cpp`, `toxicgas.cpp`, `player.cpp`: at the time of
  the check the only `return 0` in `Sound::createInstance` sat inside
  `if(!forceCreation)`, and every one of these callers passes `true`. The
  dangling hazard behind them was real and is fixed above, and since that fix
  `createInstance()` can answer 0, which every holder now tests.
- **`gs_leveleditor.cpp`'s bare `if(buttons & 3)` in the pin-connection mode**:
  either button on empty space cancels a pending connection, which is what a
  click away from a pin should do; the `& 2` the entry proposed would leave a
  left click doing nothing there.
- **`as_wav.cpp` sets `error = 0` at end of stream**: `as_ogg.cpp` does the
  same; reaching the end is not an error state, by the design of both.
- **`cf_rewind.cpp`'s "texture coordinates are in pixels"** holds where it
  stands (the image state carries a texel scale) and the noise has its own note
  (drawn with the identity); two conventions, each documented where it applies.
- **`E_PulsePanel` plays the value switch's click**: all three of the
  value-switch family do, deliberately.
- **`file_real.cpp`'s `error = 9`** is the shared delete-failure code, now
  explained in place.
- **Ctrl+Y undo, Ctrl+Z redo**: documented in the tooltips.
- **`Hotel`'s welcome refreshed every tick**: a standing display, as above.
- **`Object`'s "five of the seven switches" draw white**: verified against the
  seven `flash()` callers; only the barrage switch and the cannon switch tint.
- **`Options::handleClick` writes before dispatching**: the documented design.
- **`SoundInstance`'s `abs()` on floats**: the floating overload, checked.
- **`tileset.h`'s nine `tileset.xml`**: counts the archives, as it says.
- **`u_crt.h`'s two overscan units**: different steps of one computation.
- **`u_smooth.h`'s pronoun**, `exit.cpp`'s dropped exclamation marks,
  `mobile.js`'s "build stamp", `main.cpp`'s dropped version numbers: deliberate
  choices of the translation, recorded so they are not read as errors.
- **`campaign.cpp`'s music warning**: members are unique in the list, so it
  fires at most once per offending level; the missing `break` costs a scan.
- **`ConveyorBelt`'s `soundChanged`**: the dispatch is synchronous, the first
  belt acts and the rest return.
- **`e_pulseswitch.cpp`'s tooltip ids** follow the player-facing name
  ("Pulse trigger"), and `onTouchedByPlayer(0)` is the family's idiom; none
  of the seven handlers dereferences the pointer.
- **`barrage.h`'s "barrage"** is the author's consistent noun.
- **`pack.sh` under `sh -n`**: it is a bash script; `bash -n` passes.
- **`Tools/strip_comments.py`'s unreachable `ValueError`s**: input validation
  of a function callable on its own.
- **`harness.js`, `touch_controls.js`, `sw.js`, `shell.html`, `pre.js`
  reported as German**: the sweep's own detector matched the English words
  "so", "also", "was", "die"; the files were English, and the detector is gone.
- **`Build.bat`, `zip_skins*.bat` "not modified"**: they were English already.

### Gone with the code

- `WebBuild/build_asan.sh` (three-file heading, tinyxml claim, `misc.c`,
  copied header): folded into `build.sh`, which is one script because a copy
  drifts.
- `gl_compat.cpp` and `gl_immediate.cpp` (the `glPopAttrib` mask claim, the
  section numbering, the row-length repack, the archaeology line): removed by
  the renderer redesign, with the texture matrix and every fixed-function call.
- `font.cpp`'s display-list cache and its `1 << 31` shift: the font caches
  quads under a budget now.
- `glextensions.cpp`'s "Nearest und Bilinear" fallback names, `engine.cpp`'s
  `useFrameBuffer` screenshot branch and the ragged `-nofbo`/`-noshader` log
  lines, `linux_window.cpp`'s `setFixedSize()` and its `p_hints` read: the GL
  2.0 floor took the fallbacks.
- `level.cpp`'s commented-out alpha clear and "dirty workaround": rewritten as
  a colour-mask scope.
- `linedrawer.cpp`: replaced by `Renderer::polyline`.
- `findgerman.py`, `checkbatch.py`, `codeonly.py` and every finding about their
  word list: the sweep's tools, never committed.
- `hint.cpp`'s "the level draws the flash right after the note": the note
  draws on its overlay layer and the comment is gone.
- `menu.xml`'s "eight captions": the Manager has five kinds now and the
  sentence was rewritten.
