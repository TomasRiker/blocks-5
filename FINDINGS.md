Blocks 5 - findings from the 1.2.0 English sweep, and what became of each
==========================================================================

What the translation pass turned up while reading every comment in the tree:
bug candidates, comments that contradict the code, stale claims, doubts and
typos, each reported by an agent reading the code around it and most of them
re-read by a second agent that went back to the code independently.

Every entry was checked again against the tree as it stands on 2026-09-17, by
identifier and not by line number - the line numbers the reading agents gave
belong to the sweep's revision and have not survived it, so an entry names the
function or the phrase instead. The dispositions:

| category | entries | fixed | not a defect | gone with the code | still open |
| --- | ---: | ---: | ---: | ---: | ---: |
| bug candidates | 73 | 47 | 2 | 3 | 21 |
| comments contradicting the code | 23 | 7 | 0 | 3 | 13 |
| stale claims | 54 | 20 | 1 | 5 | 28 |
| doubts and observations | 187 | 34 | 27 | 19 | 107 |
| typos and formatting | 18 | 9 | 1 | 3 | 5 |
| **all** | **355** | **117** | **31** | **33** | **174** |

Many entries came in twice (the first reader's and the second's), so the 174
open entries are about 120 distinct things, and most of those are a comment
that says one thing while the code does another. The ones worth a change come
first; the settled record follows in short form. A finding that turned out to
be nothing is kept with its reason, because that is what stops the same false
alarm being raised again.


Still open: worth fixing
------------------------
Ordered by what it costs the player, then by what it costs the next reader.
Each names where the code is now and what the fix is.

### Wrong behaviour a player can meet

1. **The mute key and the focus loss share one stash, and lose the volumes.**
   `engine.cpp`, the `$A_TOGGLE_MUTE` branch of `update()` and
   `handleAppFocus()`. Both keep the pre-mute volumes in `oldSoundVolume` /
   `oldMusicVolume`; the key tests "both volumes are 0" with no `-1` guard where
   the focus path has one. Mute with F1, then let the window lose focus: the
   focus loss stashes 0.0 over the real volumes, the return restores 0.0 and
   sets the sentinel, and from then on F1 can never unmute, the mute icon stays
   up, and the settings are gone until the options dialog is opened. Both
   options sliders at 0 reach the same dead state the original entry described.
   Fix: give the key a `muted` flag of its own and restore only when it is set,
   and let `handleAppFocus()` skip its stash and restore while muted.
2. **`File_Archived::deleteArchivedFile()` dereferences a null `FILE*`.**
   Neither `fopen()` result is checked before the signature scan reads from
   `p_in`. The `FM_DELETE` branch of the constructor calls it straight away, so
   `FileSystem::deleteFile()` on an archive that is missing, or whose directory
   cannot be written, crashes rather than reporting. Fix: check both handles,
   close what opened, log and return 0.
3. **A paste near the right or bottom edge instances objects outside the level.**
   `GS_LevelEditor::paste()` refuses only when *both* corners of the destination
   are invalid; the cell loop then calls `instancePreset` for every cell, and
   `Level::hashObject()` tests the flat index `y * WIDTH + x` alone, so an
   object at `x == WIDTH` lands in the next row's first cell. Fix: `||` in the
   guard, or skip every invalid cell in the loop.
4. **The devil walks up twice as often as any other way.** `enemy.cpp`,
   `tryToMove(intToDir(random(0, 4)))`: `random(min, max)` is inclusive, so
   five values feed four directions and `intToDir`'s `% 4` folds 4 onto up.
   Every other draw in the file uses the inclusive form correctly. Fix:
   `random(0, 3)`. This changes gameplay, so the oracle will move on any scene
   with a devil in it, as it should.
5. **The update check's thread outlives the stack frame it writes to.**
   `main.cpp`, `getCurrentVersion()` (Windows, and only with `.update_checker`
   holding `1`; it is created holding `0`): `CreateThread`, a two-second
   `WaitForSingleObject`, then `return` - no `CloseHandle`, the thread not
   stopped, and its last act is `task.currentVersion = ...; task.finished =
   true;` into the local `Task` of a function that has returned. Fix: put the
   `Task` on the heap and let the thread own and free it, copy the result out
   only when `finished` was set inside the wait, and close the handle.
6. **A mirror or a teleporter with a `subType` outside 0..1 does nothing and
   lies about it.** `mirror.cpp` tests `subType != 0` in `reflectLaser` and
   `!= 1` in `reflectProjectile`, draws with `== 0 ? ... : ...`, normalises only
   in `changeInEditor` (`%= 2`) and names the tooltip through `% 2`;
   `teleporter.cpp` is the same shape with `getToolTip()` as the odd one out.
   The loader (`presets.cpp`) hands the file's value through unchecked, so a
   foreign level with `subType="2"` gets a mirror that reflects nothing, looks
   like one kind and is described as the other. Fix: clamp in the constructor
   with a warning, as `E_Gate` does since its own fix, and drop the per-site
   `% 2`.
7. **Page Up and Page Down in the actions list page by the wrong font.**
   `GUI_ListBox`'s two page keys size the page with the GUI's font while every
   other line of the file uses the element's own `p_font`; `options.xml` gives
   the `Actions` list `tooltip_font.xml` (line height 10 against 15), so a page
   is 5 rows where 7 are drawn. The selection stays visible; only the step is
   wrong. Fix: `p_font->getLineHeight()` at both keys.
8. **A locked bonus level shows its real title.** `gs_selectlevel.cpp`,
   `shown = status ? title : "???"`: `getLevelStatus()` answers `-1` for the
   bonus level while it is locked, which is truthy, although twelve lines on
   `status == 0 || status == -1` darkens the preview and prints the locked
   caption. Fix: the same pair in the title test, unless the reveal is meant.
9. **The campaign editor's Load says nothing for a name that is no file.**
   `CampaignEditorGUI::handleClick`, the `Load` branch: `if(fileExists(path))`
   has no `else`, so a typed name that resolves to nothing falls out silently -
   the failure the "nothing would otherwise happen here" comment in the same
   function exists to prevent for the empty name, and which the level editor
   answers with `$LE_ERROR_FILE_DOESNT_EXIST`. Fix: the same toast.
10. **`encode_sounds.py` prints a negative count.** `bad` is counted before the
    `continue` for a missing `.wav` and `done` only after it, and the summary
    prints `done - bad`: `python3 Tools/encode_sounds.py nosuchsound` says
    "-1 of 1 file(s) encoded, 1 error(s)". Fix: count only successful encodes
    and print `done`.
11. **`CF_ColorBlend` is 89% opaque in its first frame, or its comment is
    wrong.** `cf_colorblend.cpp` uses one slope, `1 / (1 - timing)`, on both
    sides of the peak, so with the `timing = 0.1` that `lightpanel.cpp` and
    `lightswitch.cpp` pass the colour quad starts at 0.889 rather than the
    "transparent at both ends" the comment promises; only the default 0.5 makes
    the two slopes coincide. Either two slopes (`t < timing ? t / timing :
    (1 - t) / (1 - timing)`) or a comment saying the flash is meant to be
    instant. A look question for the author.

### Latent, and cheap to close

Undefined behaviour on paper, a hazard behind a precondition nobody has hit, or
a guard every sibling has and one site lacks. None is known to misbehave today.

- **`as_wav.cpp` skips no RIFF pad byte.** All three seek sites advance by the
  chunk size alone; an odd-sized `LIST` or `fact` chunk before `data` throws the
  loop one byte out of step and the load fails with "WAV file ... is
  incomplete". Only a user level's `musicFilename` reaches the decoder (the
  shipped stock is Ogg), and every `.wav` in the tree has even chunks. Fix:
  advance by `size + (size & 1)`.
- **`GUI_ScrollBar::setDragBarY()` divides 0 by 0 when the content fits.** With
  `pageSize >= areaSize` the bar fills the track and the divisor is exactly 0;
  any press on the bar sets `dragging` and the first move computes `NaN` cast
  to `int`. Measured harmless on both platforms - x86 gives `INT_MIN`, which the
  clamp turns into 0, and the browser build's conversions are all saturating
  (`i32.trunc_sat_f64_s`, no trapping variant in the wasm), so the trap the
  entry feared cannot happen - but it is undefined behaviour reachable from any
  short list. Fix: return early when `pageSize >= areaSize`.
- **`deleteArchivedFile()`'s unknown-signature branch leaks both handles** and
  leaves the empty `<archive>_` on disk; the `FM_WRITE` caller then appends a
  second member of the same name into the intact archive, where the stale one
  keeps winning, and `FM_DELETE` reports "not found". Only a record the scan
  does not know reaches it (zip64, a data descriptor after a flag-bit-3 entry,
  a truncated file, where a short `fread` also leaves `signature`
  uninitialised). Fix: close both, remove the side file, initialise
  `signature`, and return a distinct code both callers treat as "cannot
  rewrite".
- **The five objects holding a `SoundInstance*` across ticks never ask
  `Sound::isLiveInstance()`.** A `SoundInstance` built while
  `Sound::getFreeSource()` returns 0 keeps `sourceID == 0`, reports itself
  removable, and is deleted at the next `Sound::update()` - and nothing clears
  the holder's pointer, so `conveyorbelt.cpp`, `laser.cpp`, `elevator.cpp`
  (which has no null guard at all), `toxicgas.cpp` and `player.cpp` then call
  into freed memory. The precondition is `alGenSources` failing with every
  listed instance looping, i.e. an exhausted source pool, which OpenAL Soft's
  default 256 makes remote. `hint.cpp` and `diamondmachine.cpp` show the
  pattern that closes it. Fix: `isLiveInstance()` before each use, or make
  `createInstance(true)` delete a source-less instance and return 0, which
  then wants the null guards in `elevator.cpp` and `player.cpp`.
- **`SoundInstance`'s constructor leaves seven members indeterminate** on the
  no-source branch (`volume`, `pitch`, `targetVolume`, `targetPitch`, both
  slide speeds, `pauseAtSlideEnd`), and `targetVolume`/`targetPitch` on the
  other. Nothing outside the file reads them any more and a source-less
  instance dies at the next update, so the exposure has shrunk. Fix: set every
  member before the branch.
- **`StreamedSound::stop()` leaves `sourceID` stale** after `alDeleteSources`;
  `setVolume`/`setPitch` guard on it and `pause`/`resume` do not. Unreachable
  as the tree stands (`stop()` runs only from the destructor, `pause`/`resume`
  have no callers). Fix: `sourceID = 0` after the delete.
- **`printfLog`'s crash-log branch opens `crash_log.txt` unchecked** while the
  `log.txt` block directly above guards its `fopen`; it runs inside the SEH
  handler, where a second fault loses the report. Fix: the same `if(p_file)`.
- **The Linux capture thread logs through `printfLog`** (`audiocapture.cpp`,
  the PulseAudio `threadProc`'s read-failure warning), which the Windows half's
  own comment forbids: a static buffer and `localtime`. Fix: store the failure
  as `initResult` is stored and let the main thread log it.
- **`Pin::disconnect()` calls `setValue(-1)` on an input pin**, which is a
  no-op (`setValue` returns unless the pin is an output), under the comment
  "The input is undefined now." Unreachable in play - the editor is the only
  caller and never runs logic. Fix: `writeValue(-1)` and `writeOldValue(-1)`.
- **`pin.cpp`'s `#ifdef EDGY_CONNECTIONS` block cannot compile**: it redeclares
  `pin1`/`pin2` in the same scope and its `Vec2i d` collides with the spline's
  `const Vec2d d`; the macro is defined nowhere. Fix: delete it.
- **`presets.cpp`'s "Damage" branch reads its attribute without `if(p_element)`**
  where every comparable branch guards, and six electronics branches
  (`E_Value`, `E_ValueSwitch`, `E_PulseSwitch`, `E_PulsePanel`, `E_Gate`,
  `E_FlipFlop`) construct *inside* `if(p_element)` and so hand back 0 where the
  other fifty branches build a default. No caller hits either today. Fix: the
  guard, and construct after the block.
- **`LightBarrierSender::onRender()` and `Laser::onRender()` compare against an
  uninitialised `Vec2d`**: `dir` and `p` are default-constructed (empty
  `Vec()`), so on a beam's second point `dir != oldDir` reads garbage - a
  spurious or missing collinear vertex, invisible. Fix: initialise both and
  keep the second point unconditionally.
- **`Lightning::generateSecondaryBranch()` walks a `Vec2i`** where `generate()`
  walks a `Vec2d`, truncating each step. Fix: `Vec2d pos`.
- **Locals handed to TinyXML uninitialised, the same shape the sweep fixed
  five of**: `int blink` in `Elevator::loadExtendedAttributes`, and
  `sourcePinID`, `targetX`, `targetY`, `targetPinID` in
  `Electronics::loadExtendedAttributes`. Files the game writes always carry
  them. Fix: `= 0`.
- **`E_FlipFlop`'s constructor has no `default:`** for a `subType` outside
  0..2, which yields a part with no pins (inert, never crashing), and the JK
  case lacks its `break` (harmless as the last case, a trap for the next). Fix:
  log and clamp; add the `break`.
- **`Eye` dereferences `instancePreset("Enemy", ...)` unchecked.** It returns 0
  for an unknown name. Fix: `if(p_enemy)`.
- **`File_Real::tell()` calls `ftell(p_handle)` with no null test** and
  `getSize()` routes every non-read mode through it; `size` is set only on the
  read and write paths. No caller asks a listing or deletion handle for its
  size. Fix: guard, and `size = 0` in the constructor.
- **`FileSystem::getAppHomeDirectory()` hands `SHGetFolderPathA` a
  `char[256]`** where the contract is `MAX_PATH`, and ignores the `HRESULT`.
  Fix: `MAX_PATH`, check, fall back.
- **`evalRelativePath()` writes `i < path.length() - 2`**, the `size_t`
  subtraction the `convertPath` comment above it argues against; harmless
  because the short `substr` cannot match. Fix: `i + 3 <= path.length()`.
- **`GS_Game`'s constructor leaves `levelNumber` and `p_currentCampaign`
  uninitialised**, and the editor's trial run increments `levelNumber` on the
  indeterminate value before every later read is guarded away. Fix: initialise
  both in the constructor.
- **`gs_leveleditor.cpp`'s mode-6 `if(buttons & 3)`** has no preceding `& 1`
  test, so the left button runs a body meant for the right; it only clears two
  pointers. Fix: `& 2`.
- **`GS_Menu` looks up `keyData.find(time - 500)`** for the first 25 ticks on a
  key near 2^32; the miss is the intended delay, by wraparound. The same file
  guards the same shape for `.donation_asked`. Fix: `if(time >= 500)`.
- **`GUI_CheckBox::readAttributes` answers `<Checked>` with `check(true)`**, the
  user-click path that fires `changed`, not `setChecked(true)`; harmless only
  because nothing is connected while the tree is built. Fix: `setChecked`.
- **`GUI_MultiLineEditBox` writes `static_cast<uint>(text.length() - 1)`** at
  two places fenced off only by the guard above each. Fix: `i + 1 ==
  text.length()`.
- **`GUI_RadioButton` never re-resolves a `$ID` image**, unlike `GUI_Button` and
  `GUI_StaticImage`; no shipped dialog gives one a localized image. Fix: mirror
  `GUI_StaticImage`, or a note that radio-button images are never localized.
- **`Engine::setCursorPosition()` guards on `screenSize`** (always 640x480)
  where `getCursorPosition()` guards on the present rectangle it just computed,
  although both promise to be exact inverses. Fix: `if(w > 0 && h > 0)`.
- **`Arrow::turn()` does a bare `dir++`** where `changeInEditor` wraps with
  `%= 4`; every reader takes `dir % 4`, but a mid-game save writes the unwrapped
  number. Fix: `dir = (dir + 1) % 4`.
- **`AudioRing::clearRing()` locks `p_mutex` unchecked** where the other three
  entry points guard, and the Linux `AudioCapture::start()` lacks the Windows
  half's `if(opened)`. Both unreachable today. Fix: the guards.
- **`CF_Zoom` draws its white flash from the loop's trailing `t`**, one 0.01
  step behind the transition parameter, because the loop reuses the variable.
  Invisible; the reuse hides it. Fix: a loop variable of its own.
- **`ElectricityPanel` plays `electricityswitch.ogg` at priority 0** where the
  switch and the structurally identical `lightpanel.cpp` pass 100, so under
  source pressure the panel's click is the first cut. Fix: 100.
- **`WebTransfer::abandon()` discards nothing on the page.** It resets the C
  side, while the `<input type=file>` keeps its listeners and its 300 s
  timeout; a file chosen after leaving the menu is written and installed on
  the next visit, and a second picker can be credited with the first one's
  late result. Fix: keep the pending input in a `Module` slot and finish and
  remove it in `abandon()`, which makes the header's "discards a dialog that
  is still open" true.
- **`Texture::createSubTexture()` decodes and uploads the whole parent image
  first**, because `new Texture(filename)` ends in `reload()` before `p_parent`
  is set, then reloads as a sub-texture and throws the first result away: two
  full decodes of `sprites.png` per skin load. Fix: a constructor that takes
  parent, offset and size.
- **`Sprites::add()` silently replaces the last sprite** once `numSprites` has
  reached `MAX_SPRITES` (4). Fix: say so in a comment, or assert.
- **`Laser`'s `destroyTime` keeps decrementing below zero** for the rest of the
  object's life; the burst fires once only because the test is `== 0`. Fix:
  guard the decrement and fire on the transition.
- **`Options::changed` is set true by the OK and Cancel clicks themselves** and
  reset only in the constructor, so from the first close on every Cancel
  reloads `config.xml`; the reload restores what is already in memory, so
  nothing visible changes. Fix: `changed = false` at the top of `show()`.
- **`options.xml`'s Cancel caption is an inline `\xA7de:Abbrechen\xA7en:Cancel`**
  while `$CANCEL` exists and every other caption is a `$ID`. Fix: `$CANCEL`.
- **`PWEncrypt/main.cpp` writes to an unchecked `fopen()`** of a temp file
  (Windows). Fix: `if(p_file)`.
- **Dead code**: `prepareForTinyXML()` in `util.cpp` returns its argument and
  has neither declaration nor caller; `GS_Game::updateMusic()` is declared and
  never defined; `bool quit` in `main.cpp` is never set; `AS_Ogg`'s `FILE*
  p_file` member is never touched (the constructor's `File* p_file` is a
  local); `transfer.h` forward-declares a `Campaign` nothing there uses;
  `Pin::connect()`'s `p_output` is computed and never read; `e_barrage.h`'s
  include guard is `_E_BARRIER_H`; `gs_leveleditor.cpp` carries an `#ifdef
  CHECK_IF_IT_REALLY_IS_A_LEVEL` block nothing defines;
  `E_LightBarrierReceiver` saves and loads a `value` that `frameBegin()` zeroes
  before anything can read it.
- **Three that are the author's to judge by eye or ear**, each with a comment
  that promises something the numbers do not deliver: `toxicgas.cpp`'s volume
  ramp `1 / (40 * 20) * numInstances` clamped to `[0.5, 1]` never leaves 0.5
  below 400 clouds, and the constructor's `setVolume(0.0)` is overwritten in
  the same call, so the sound never fades in; `LightBarrierSender`'s `counter++`
  sits inside the beam-tracing loop, so the pulse phase advances by the beam's
  length in steps and jumps when the beam changes length; and `object.cpp`'s
  flash comment says "just under eight ticks, a good 0.15 s" over
  `FLASH_DECAY = 0.8`, which reaches 1/256 after 25 ticks, half a second
  (`objects.md` repeats the claim; 0.5 would be the decay that fits it).

### Comments and documents that still disagree with the code

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
  `onElectricitySwitch(bool on)` shadows the member `double on`; the debris of
  a destroyed object starts from the beam's cell rather than the object's
  position as `lava.cpp` does. `lava.cpp`: the debris loop's `int i` shadows
  the live iterator.
- `sound.cpp`: "setVolume() on it in the next line" is three lines on in
  `player.cpp`. `streamedsound.cpp`: "stops the source without emptying it" -
  the queue. `sounds.xml`: "less signal level for the same computational load"
  - the same bitrate. `audiocapture.h`: the header states the sample format and
  not the rate contract the code honours.
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
- `SoundInstance::update()` calls `abs()` on doubles; the floating overload is
  the one in scope through `pch.h` (checked with the build's own flags).
- `tileset.h`'s "all nine tileset.xml in the tree" counts the four skin
  archives, as its own enumeration says; five are in Git.
- Ctrl+Y is undo and Ctrl+Z redo, QWERTZ-natural and documented in the tooltips.
- `gs_leveleditor.cpp`'s paste and `Level::hashObject()`: see item 3 above -
  the hash's flat index is what turns a bad paste into an object in the next
  row.


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

### Not a defect

Kept with the reason, so the alarm is not raised twice.

- **`p_soundInst->stop()` without a null check** in `conveyorbelt.cpp`,
  `laser.cpp`, `elevator.cpp`, `toxicgas.cpp`, `player.cpp`: the only
  `return 0` in `Sound::createInstance` sits inside `if(!forceCreation)`, and
  every one of these callers passes `true`. The dangling hazard behind them is
  real and listed above; the null is not.
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
- **`SoundInstance`'s `abs()` on doubles**: the floating overload, checked.
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
