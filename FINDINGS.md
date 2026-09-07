Blocks 5 - findings from the 1.2.0 English sweep
================================================

What the translation pass turned up while reading every comment in the tree.
The sweep itself was a translation and a convention pass; ten of the findings
have since been fixed, all of them cases where the code had already decided
what the fix must be. The Verified section below is the live list - what was fixed,
what is still open, what turned out to be harmless and what did not survive
checking. Everything under it is the unfiltered record of the reading.

Each entry is what an agent reported after reading the code around it. "confirmed"
means a second, independent agent went back to the code and reached the same
conclusion; "refuted" means one went back and disagreed. Neither is a substitute
for a compiler or a run of the game.

    73  bug candidates
    23  comments that contradict the code
    54  stale claims
    187 doubts and observations
    18  typos and formatting
    218 of them confirmed by a second agent


Verified
--------
The 73 bug candidates below were each read by a second agent that went back to
the code independently; 57 came back confirmed. Nine were then checked by hand
against the source, and a further ten settled the same way after the second
agent disagreed with the first.

Ten are fixed. Four are open and want a decision that is about the game rather
than about the code. The rest are real and harmless, or refuted - and the
refuted ones are the most useful part of this file, because they are what stops
the same false alarm being raised again.

Nothing is deleted here as it is dealt with. A finding that turned out to be
nothing is worth as much on the record as one that turned out to be real.


### Fixed in e99904d

Each was forced: a sibling block in the same function proved what was meant, or
the definition of the variable did. None changed what a working game does. The
reasoning is in the commit message and, where it is a gotcha, in the code.

- **hotel.cpp** - `onRemove()` now clears the static `Hotel*` the save action
  hangs off, guarded with `p_hotelToSave == this` exactly as `onUpdate()` guards
  it. Without that guard the fix would itself have broken the multi-player case:
  a level can hold several `Player` objects and `switchToNextPlayer()` cycles
  through them, so several characters can stand on several hotels at once and
  only the one under the active player holds the claim.
- **electronics.cpp:135, :151** - the sentinel loses an `F` and now matches the
  three sibling blocks. A saved game with no `oldValue<N>` no longer restores
  `0x7FFFFFFF` into the pin.
- **font.cpp:250, 257, 419, 426** - `r >= 3` becomes `r >= 2` and `r >= 4`
  becomes `r >= 3`. A tag ending a string is recognised instead of drawn, so a
  truncated level title no longer renders a literal `</h>`.
- **filesystem.cpp:213** - `openFile()` checks `p_file` before dereferencing it.
- **presets.cpp:423** - the Hint preset checks the `<Text>` element it asks for,
  as the lines above and below it already check theirs.
- **tileset.cpp:10** - the constructor sets `badTile` before `reload()`, which
  starts every tile from it.
- **gui.cpp:36** - `GUI::init()` tests `p_font` before using it three times, as
  the tooltip font beside it does.
- **util.cpp:292** - `vsprintf` becomes `vsnprintf`; the callers pass paths into
  a 1024-byte buffer.
- **singleton.h:21** - `operator=` returns `*this`.
- **Five locals handed to TinyXML** start at 0 - `level.cpp:241` (`ndc`),
  `level.cpp:352` (`destroyTime`, `ghost`), `player.cpp:468` (`inv`) and
  `e_barrage.cpp:61` (`up`) - because TinyXML leaves them untouched when the
  attribute is absent and every other site in the same functions writes the 0.

Fixed earlier, in 5085dd2, as conventions rather than defects: the four
`INLINE_SETTER` declarations carrying getter names in `gui_button.h`,
`gui_staticimage.h` and `gui_radiobutton.h`, and the two pairs of sibling GUI
elements sharing a name in `leveleditor.xml` and `selectlevel.xml`.


### Open - these want a decision, not a patch

1.  **bomb.cpp:102, :163 and projectile.cpp:219 - the debris fades in.**
    `-0.5 * -p.color.a / p.lifetime`. The two minus signs cancel, so the alpha
    delta is positive; every other particle block in the same files is plainly
    negative. It is obviously a typo and it has shipped for years, which means
    the look it produces may be the one everybody knows. Changing it changes
    what the game looks like.

2.  **e_gate.cpp:111 - `int z;` and a switch with no default.**
    Initialising it decides what an unknown `subType` publishes onto the output
    pin, and `subType` arrives unchecked from the level file. That is an answer
    about the electronics, not a missing `= 0`.

3.  **file_archived.cpp:471 - the buffers are written with the wrong lengths.**
    `p_filename` is allocated with `cde.filenameLength + 1` and `p_extraField`
    with `cde.extraFieldLength`, but both are written out using the *local*
    header's lengths. A zip writer may put a different extra field in each, so
    where `lfh.extraFieldLength` is the larger this reads past the end of the
    heap block. The over-read is certain; which of the two headers should be
    authoritative for the output is real zip reasoning and not a one-liner.

4.  **The leaks.** `manager.h:70` (a failed resource), `audiostream.cpp:61` (a
    failed music or sound load), `filesystem.cpp:213` and `:306` (every failed
    open, and `fileExists` opens with `FM_TEST` routinely), `sound.cpp:42` (four
    paths). Each is correct to fix and each is one object on a path taken rarely
    in a process that exits.


### Real, and nothing follows from it

Confirmed true, and not worth a commit on its own. Recorded so nobody
investigates them twice.

- **`engine.cpp:1129`** - the key-event queue is drained inside the per-key-slot
  loop rather than beside it, so it happens `NUM_KEY_SLOTS` times per tick
  instead of once. Idempotent; it reads as a misplaced brace.
- **`parameterblock.h:27`** - `operator=` calls `clear()` with no self-assignment
  guard, so `b = b` would empty the block. Nothing does that.
- **`gui_radiobutton.cpp:146`** - `changed` fires twice on a click that selects,
  once on a click that does not. No handler in the tree minds.
- **`gui_window.cpp:69`** - the title is localized twice; the measured string is
  localized once. Harmless while no localized body itself starts with `$`.
- **`diamondmachine.cpp:541`** - `p_soundInst` is never assigned, so the `stop()`
  branch is dead and an aborted conversion lets the sound run out.
- **`gs_leveleditor.cpp:1538`** - the `else if(!shift)` branch is unreachable; the
  three above it consume every case in which `!shift` holds.
- **`engine.cpp:4048`** - `line.find_first_of("//") == 0` matches a single leading
  slash, because `find_first_of` takes a character set. No line of
  `languages.txt` begins with one.
- **`linedrawer.cpp:71`** - `update()` returns before `vertices.clear()` below two
  points while `draw()` clears `dirty` regardless, and `draw()` indexes
  `vertices[0]` with no empty check. `glDrawArrays` with a count of zero reads
  nothing, so the undefined behaviour has no victim; the stale-beam half would
  need a run to see.


### Refuted

- **`p_soundInst->stop()` without a null check** in `conveyorbelt.cpp:56`,
  `laser.cpp:57`, and the same shape in `elevator.cpp`, `toxicgas.cpp` and
  `player.cpp`. The claim was that `Sound::createInstance` can return 0 and these
  destructors would then dereference it. It cannot, here: the only `return 0` in
  `createInstance` sits inside `if(!forceCreation)`, and every one of these
  callers passes `true`. The `if(p_soundInst)` in the constructors is belt and
  braces, not evidence of a reachable null. The comment above that `return 0`
  says as much - the looping sounds ask with `forceCreation` because it is "the
  difference between running and crashing".


Bug candidates
--------------
The unfiltered record of what the reading agents reported, kept as it was
written. The Verified section above supersedes it and says which of these were
fixed, which are open, and which did not survive.

### as_ogg.cpp

- **line 48** [confirmed]
  CONFIRMED: ~AS_Ogg calls ov_clear(&vorbisFile) unconditionally, including
  after a constructor that returned early with error 1 or 2, where vorbisFile
  was never initialised.
  `OggVorbis_File vorbisFile` is a plain member with no initialisation; on the
  error-1 path (openFile failed) it is untouched garbage when ov_clear reads
  and frees it. Unreachable only by accident: AudioStream::open returns 0 on
  `if(p_stream->getError())` without deleting p_stream, so the AS_Ogg leaks
  instead of being destroyed - and on the error-2 path the File* handed to
  ov_open_callbacks leaks too. Any caller that does delete a failed AS_Ogg
  crashes.

### as_wav.cpp

- **line 92** [confirmed]
  CONFIRMED: no RIFF word-alignment pad byte is skipped after an odd-sized
  chunk.
  All three seek sites advance by exactly chunkHeader[1]: line 79 and line 92
  seek to chunkDataOffset + chunkHeader[1], line 100 to p_file->tell() +
  chunkHeader[1] (equivalent, since nothing is read between the header and the
  seek). RIFF pads an odd-sized chunk with one byte that the size field does
  not count, so a WAV carrying an odd-sized LIST or fact chunk before 'data'
  leaves the loop one byte out of step and the next 8 bytes are read as a
  chunk header. The comment "skip the rest of this chunk" describes what the
  code does and is a faithful rendering of "den Rest dieses Chunks
  ueberspringen"; the gap is in the code, not the comment.

### audiostream.cpp

- **line 61** [confirmed]
  CONFIRMED. AudioStream::open() leaks p_stream on the getError() path: after
  the "+ ERROR: Could not create audio stream" printfLog it does "return 0;"
  with no "delete p_stream;". The caller receives 0 and has no handle to free.
  Every failed music or sound load leaks one AS_Wav or AS_Ogg plus whatever
  the decoder holds. (The unknown-extension path returns before allocating, so
  it is fine.)
  Reported, not fixed. The comment above it ("Laden erfolgreich?" -> "Loaded
  successfully?") only poses the question, so the translation neither hides
  nor touches the leak.

### bomb.cpp

- **line 102** [confirmed]
  CONFIRMED, with corrected magnitudes. p.deltaColor = Vec4d(0.0, 0.0, 0.0,
  -0.5 * -p.color.a / p.lifetime) - the double minus makes the alpha delta
  positive, so the debris particles brighten over their lifetime instead of
  fading. Same expression at bomb.cpp:163, and a third site at
  projectile.cpp:219.
  Sprites::sample (sprite.cpp:220) writes DEBRIS_ALPHA into the alpha, not the
  sampled texel's alpha, and DEBRIS_ALPHA is 0.25 (sprite.h:9). So p.color.a =
  0.25 + random(0.3, 0.5) = 0.55 to 0.75, and the delta is +0.5 * color.a /
  lifetime, about +0.005 per tick at lifetime 40-80: alpha ends half again as
  high as it started (0.83 to 1.13) instead of reaching 0. The debris still
  vanishes, but only because deltaSize = -p.size / p.lifetime shrinks it to
  nothing. Every other alpha delta in bomb.cpp and in the twenty-odd sites
  across the tree is -p.color.a / p.lifetime or a plain negative constant;
  only these three carry the extra minus. Not fixed, not commented.

### build_asan.sh

- **line 27** [confirmed]
  "Game sources, minus the four that cannot come along:" is followed by three
  entries (stackwalker, videorecorder, pch), and the grep -vE on line 32
  excludes exactly those three.
  Off-by-one in the prose. build.sh:34 says three and lists three.

### diamondmachine.cpp

- **line 542** [confirmed]
  CONFIRMED: p_soundInst is dead. The stop() branch at the end of onUpdate()
  can never run, so an aborted conversion leaves diamondmachine.ogg playing to
  its end.
  diamondmachine.h:41 declares a plain member SoundInstance* p_soundInst; the
  only write in the whole tree is the constructor's p_soundInst = 0 (line
  174). The sound is started at line 505 with
  Engine::inst().playSound("diamondmachine.ogg", false, 0.0, 100), whose
  return value is discarded - unlike ConveyorBelt/Laser/ToxicGas, which take
  an instance from p_sound->createInstance(true) and keep it. No comment
  claims otherwise, so the translation says nothing about it.

- **line 541**
  p_soundInst is never assigned anything but 0, so the stop() branch at the
  end of onUpdate() is dead and the conversion sound is never cut short.
  The constructor sets p_soundInst = 0 and the only sound is started with
  Engine::inst().playSound("diamondmachine.ogg", ...) at line 504, whose
  return value is discarded. The member exists solely to be able to stop the
  loop when the conversion is aborted; as it stands an aborted conversion
  leaves the sound running to its end. Translated as it is written - no
  comment claims otherwise.

### e_barrage.cpp

- **line 61** [confirmed]
  CONFIRMED. loadExtendedAttributes declares an uninitialised local `int up;`
  and passes it to TiXmlElement::Attribute("up", &up); tinyxml.cpp:597 only
  writes through the int pointer when attributeSet.Find(name) succeeds, so an
  absent attribute leaves the local as garbage and `this->up = up ? true :
  false;` then reads it. The neighbouring `Attribute("shownState",
  &shownState)` and e_clock.cpp:43 have the same shape but are safe, because
  those targets are members the constructor already set to 0.
  Pre-existing code, out of scope for a comment pass, and only reachable from
  a level file that omits the attribute - where it would give a barrage a
  random up/down state and, through updateProperties(), a random OF_MASSIVE.

### e_gate.cpp

- **line 111** [confirmed]
  CONFIRMED. In E_Gate::doLogic the else-branch declares `int z;`
  uninitialised and the switch over subType covers only cases 0-5 with no
  default, yet setValue(10, z) is executed unconditionally afterwards.
  subTypes 6 and 7 are taken by the two branches above, so the else-branch is
  reached for every other value. The constructor gives such a part two input
  pins and output pin 10, so once both inputs are connected and defined the
  early return at the top does not fire, the switch matches nothing, and z is
  read indeterminate. That a subType outside 0..7 is reachable is asserted by
  the comment eleven lines above ("subType comes out of the level file
  unchecked ... levels travel between players") and by the range guard in
  getToolTip(), which exists for exactly that case. changeInEditor() keeps
  subType in 0..7 (subType %= 8), so only a hand-edited or foreign level file
  gets there.

### electronics.cpp

- **line 135** [confirmed]
  Sentinel mismatch in loadExtendedAttributes: the oldValue guard is
  initialised to 0x7FFFFFFF (eight F) but tested against 0x7FFFFFF (seven F).
  Lines 135-137 read `v = 0x7FFFFFFF; p_element->QueryIntAttribute(attrName,
  &v); if(v != 0x7FFFFFF) (*i)->writeOldValue(v);`. When the level file has no
  `oldValue<N>` attribute, QueryIntAttribute leaves v at 0x7FFFFFFF, which is
  not equal to 0x7FFFFFF, so the sentinel itself is written into the pin as
  its old value instead of the attribute being skipped. The value branch four
  lines above uses 0x7FFFFFF consistently in both places, so the eight-F
  literal looks like a typo. Not fixed, per the instructions.

- **line 151** [confirmed]
  The same sentinel mismatch again, in the output-pin loop of the same
  function.
  Identical shape to line 135: `v = 0x7FFFFFFF;` followed by `if(v !=
  0x7FFFFFF) (*i)->writeOldValue(v);`. Both copies of the loop carry the
  defect, so a saved game whose Electronics element lacks `oldValue<N>`
  restores every output pin's old value as 0x7FFFFFFF.

### engine.cpp

- **line 1129** [confirmed]
  CONFIRMED. `while(!keyEventQueue.empty()) keyEventQueue.pop();` sits inside
  the `for(int i = 0; i < NUM_KEY_SLOTS; i++)` loop that clears
  keyData/buttonData, so the queue is drained once per key slot rather than
  once per tick.
  Harmless in result (passes after the first find it empty), but it is the
  only statement in that loop body that does not mention `i`, and it reads as
  a misplaced closing brace. No comment covers it.

- **line 1535** [confirmed]
  The $A_TOGGLE_MUTE toggle decides which branch to take from 'soundVolume ==
  0.0 && musicVolume == 0.0' without the 'oldSoundVolume != -1.0' guard that
  handleAppFocus (line 802) uses for the same restore.
  If the player drags both volume sliders to 0 in the options and then presses
  the mute key, the unmute branch runs with oldSoundVolume/oldMusicVolume
  still at their -1.0 sentinel. setSoundVolume clamps to [0,1], so nothing
  crashes and nothing gets louder - but the key is a dead key from then on: it
  can never unmute (there is nothing to restore) and never mute (the state
  already reads as muted, so the mute icon stays up). handleAppFocus guards
  exactly this case; the toggle does not. Reported, not fixed.

- **line 1128**
  In mainLoopIteration's per-tick reset, `while(!keyEventQueue.empty())
  keyEventQueue.pop();` sits inside the `for(int i = 0; i < NUM_KEY_SLOTS;
  i++)` loop that clears keyData/buttonData, so the queue is drained once per
  key slot instead of once per tick.
  Harmless in result (the second and later passes find it empty) but it reads
  as a misplaced closing brace, and it is the only statement in that loop body
  that does not mention `i`. If the intent was ever to drain the queue
  conditionally, the current nesting hides that. No comment covers it.

- **line 4048**
  loadStringDB() tests for a comment line with line.find_first_of("//") == 0,
  which matches a single leading '/' - find_first_of searches for any
  character of the set, not the two-character prefix. The comment under it
  reads "It is only a comment."
  A languages.txt line beginning with one '/' would be silently swallowed as a
  comment. Harmless today - no such line exists in the shipped file - but the
  intent is plainly line.find("//") == 0 and the comment describes the intent,
  not the behaviour. Left exactly as it stands.

### file_archived.cpp

- **line 401** [confirmed]
  CONFIRMED. In int File_Archived::deleteArchivedFile(), the else branch of
  the signature scan does "return false;" - i.e. 0, which the legend at lines
  168-169 defines as "not found - and then nothing is gone".
  Verified against the code: on an unrecognised signature the function returns
  0 with p_in and p_out still open (two leaked FILE handles) and the
  half-written "<archive>_" file left on disk. The FM_DELETE path at line 170
  then sets error = 9 (not found), and the FM_WRITE path at line 149 only
  clears archiveExists on -1, so it opens the archive with
  APPEND_STATUS_ADDINZIP and appends into an archive it has just started
  rewriting. The claim "nothing is gone" is false on both counts.

- **line 471** [confirmed]
  CONFIRMED. p_filename is allocated with cde.filenameLength + 1 and
  p_extraField with cde.extraFieldLength (lines 422 and 429), but both are
  written out with the local header's lengths: fwrite(p_filename, 1,
  lfh.filenameLength, p_out) and fwrite(p_extraField, 1, lfh.extraFieldLength,
  p_out).
  The local file header and the central directory entry may legitimately carry
  different extra fields, and zip writers routinely give them different ones.
  Wherever lfh.extraFieldLength > cde.extraFieldLength this reads past the end
  of the heap block; the same holds for the filename if the two lengths
  disagree. Only the central-directory copy was read into these buffers.

### filesystem.cpp

- **line 212** [confirmed]
  CONFIRMED, with a narrowing. openFile() leaves p_file at 0 when
  convertPath() yields an empty filePath, and the next statement is
  if(p_file->getError()) - a null dereference.
  convertPath() falls through to filePath = temp, so filePath is empty exactly
  when evalPath() returned "". That happens for openFile("") or for a path
  made only of "./" while the dir stack is empty (getCurrentDir() returns "").
  Once main.cpp has pushed data.zip[...] as the root, getCurrentDir() is
  non-empty and evalPath("") returns that instead, so the crash is reachable
  before the mount or after popCurrentDir, not from an empty musicFilename
  during play. The defect is real, the trigger narrower than reported.

- **line 306** [confirmed]
  CONFIRMED. readStringFromFile() returns "" on a zero-size file without
  closeFile(p_file).
  The other two exits (open failure, normal path) are correct: one never
  allocated, the other closes. Only the !size branch drops the open File, so
  an empty password.txt or an empty readme leaks a handle on every read.

- **line 213**
  openFile() dereferences a null p_file when filePath is empty. p_file is
  initialised to 0 and only assigned inside if(!filePath.empty()); the very
  next statement is if(p_file->getError()).
  convertPath() leaves filePath empty for an empty path, so openFile("") - or
  any call whose evalPath result is empty - crashes instead of returning 0.
  Reachable from data-driven names (an empty musicFilename, an empty skin
  name).

### font.cpp

- **line 257** [confirmed]
  The r >= 4 guard on </h> (and r >= 3 on <h>, line 250; the same pair again
  at lines 420 and 427 in measureText) is one too strict, so a tag that ends
  the string is never recognised and its four characters are drawn as glyphs.
  r is text.length() - i - 1, i.e. the bytes remaining AFTER i, so </h> at i
  needs only r >= 3 and <h> only r >= 2 to be in bounds.
  Verified: "<h>Title</h>" tokenises as OPEN, T, i, t, l, e, and then the
  literal characters <, /, h, >. It is not academic, because cutWithEllipsis()
  deliberately appends "</h>" at the very end of its result to close what the
  cut left open - so a level title truncated by fitText() with an <h> still
  open comes back as "...</h>" and renderText draws the closing tag on screen.
  tagLength() and tagEndingAt() in the same file use text.compare() and have
  no such restriction, so the two halves of the file disagree about what a
  trailing tag is.

### gs_leveleditor.cpp

- **line 1538** [confirmed]
  The 'else if(!shift)' branch in GS_LevelEditor::draw() - the one carrying
  '// delete the objects at this cell' - is unreachable.
  The three branches above it are 'objectType != "Rail" && objectType !=
  "Hint" && !shift', 'objectType == "Rail" && !shift' and 'objectType ==
  "Hint"'. Together they consume every case in which !shift holds: Rail goes
  to the second, Hint to the third, everything else to the first. So control
  only reaches the fourth branch while shift is held, and its own condition
  then denies it. Nothing at the target cell is ever cleared through that
  path; the comment describes code that cannot run.

### gui.cpp

- **line 40** [confirmed]
  CONFIRMED. GUI::init() calls Manager<Font>::inst().request("font.xml") on
  line 36, then p_font->getOptions() (37), options.shadows = 1 (38),
  p_font->setOptions(options) (39), and only then if(!p_font) return false; on
  line 40.
  Three dereferences precede the guard, so a request() that returned 0 would
  crash before it can fire. The p_toolTipFont block right below (lines 42-46)
  has the same two calls in the opposite order - request, then
  if(!p_toolTipFont) return false;, then getOptions() - which is what makes
  the ordering above look like a slip. No comment claims either ordering, so
  nothing in the translation depends on it.

### gui_button.h

- **line 39** [confirmed]
  CONFIRMED, and wider than reported: INLINE_SETTER is given getter names at
  gui_button.h:39 and :41, and the same mistake stands in gui_staticimage.h:28
  and gui_radiobutton.h:42.
  gui_element.h:13 defines INLINE_SETTER(TYPE, SETTERNAME, MEMBERNAME) as void
  SETTERNAME(const TYPE&), so each pair becomes a const no-arg getter and a
  one-arg setter of the same name - a legal overload, which is why it
  compiles. Nothing in the tree calls either accessor (grep for
  PositionOnTexture finds only these declarations, the members, and the
  unrelated Engine icon members), so the missing setPositionOnTexture /
  setClickedPositionOnTexture has never been noticed. Not fixed: code.

### gui_radiobutton.cpp

- **line 146** [confirmed]
  CONFIRMED. GUI_RadioButton::onMouseUp does check(); changed(this);. check()
  early-returns only when checked is already true, otherwise it calls
  setChecked() and ends with changed(this) itself - so a click that actually
  selects an unchecked radio button emits changed twice, while a click on an
  already-selected one emits it once.
  Verified by reading both functions: check() at gui_radiobutton.cpp:209-216
  (if(checked) return; setChecked(); changed(this);) and onMouseUp at 136-150.
  GUI_CheckBox::onMouseUp (gui_checkbox.cpp:87-101) emits exactly once
  (checked = newChecked; changed(this);). The weighting is the inverse of
  CLAUDE.md's rule that check() is the user's click and the one thing that
  fires changed. The German comment says "Signal ausloesen" once, which is
  what the translation renders.

### gui_radiobutton.h

- **line 42** [confirmed]
  CONFIRMED: same copy-paste - INLINE_SETTER(Vec2i, getPositionOnTexture,
  positionOnTexture) names the setter getPositionOnTexture.
  Identical expansion to the gui_staticimage.h case. The intended
  setPositionOnTexture is unreachable by name. gui_button.h:39/41 (outside
  this batch) has the same shape.

### gui_scrollbar.cpp

- **line 413** [confirmed]
  CONFIRMED, but the stated precondition is backwards: the divisor
  (dragBarHeight + 2 * size.x - size.y) is zero in the ordinary 'content fits'
  case, not only for a degenerately small scrollbar.
  When pageSize >= areaSize, updateValues (gui_scrollbar.cpp:376-388) sets
  dragBarY = size.x and dragBarHeight = size.y - 2 * size.x, so dragBarHeight
  + 2 * size.x - size.y is exactly 0. The drag bar then fills the whole track,
  so any press between the two arrow buttons sets dragging = true (line 274)
  and the first mouse move reaches setDragBarY: 0.0/0 -> NaN when areaSize ==
  pageSize, +-inf otherwise, then cast to int. GUI_ListBox never hides its
  scrollbar for a short list (gui_listbox.cpp:290-292), so a short list plus a
  drag inside the bar is the reachable path. Native x86 absorbs the bad cast;
  on wasm a NaN-to-int cast is the trap CLAUDE.md describes as looking like a
  hang. Line 418 is the mirrored horizontal case.

### gui_staticimage.h

- **line 28** [confirmed]
  INLINE_SETTER(Vec2i, getPositionOnTexture, positionOnTexture) declares the
  setter under the getter's name, so no setPositionOnTexture exists.
  INLINE_SETTER expands to 'void SETTERNAME(const TYPE& ...)', so line 28
  defines a second overload of getPositionOnTexture rather than a setter. It
  compiles because the two differ in parameters, but any caller writing
  p->setPositionOnTexture(v) fails to build, and the class has no visible
  setter. The same copy-paste sits at gui_radiobutton.h:42 and
  gui_button.h:39/41 (outside this batch). Not fixed - translation pass only.

### gui_window.cpp

- **line 69** [confirmed]
  CONFIRMED: localizeString is applied twice to the window title. Line 67
  stores localizeString(this->title) in the local `title`, line 68 measures
  that string, and line 69 passes localizeString(title) - the
  already-localized string - to renderText.
  Engine::localizeString (engine.cpp:4114) is not idempotent in general: it
  resolves a leading '$' through stringDB and then strips a \xA7<lang>: body
  out of whatever it holds. A localized body that itself begins with '$' or
  carries a \xA7 section would therefore be reduced a second time, so the
  measured string (localized once) and the drawn string (localized twice)
  would differ and the centring on line 69 would be computed from the wrong
  width. Harmless with today's languages.txt, but that data file is the only
  thing keeping the two in step. No comment covers it; nothing was translated
  around it.

### hotel.h

- **line 21** [confirmed]
  CONFIRMED. The static Hotel* p_hotelToSave is cleared in exactly two places
  - Hotel::onUpdate when the player is no longer on the field (hotel.cpp:71)
  and Hotel::onSave (hotel.cpp:86) - and nowhere else in the tree.
  grep over Blocks5/src finds no other writer. ~Hotel() is empty and Hotel
  overrides no onRemove(), so a level that ends, restarts or is left while the
  player stands on the hotel leaves the pointer aimed at a deleted object.
  gs_game.cpp:395-397 then dereferences it on the next $A_SAVE_IN_HOTEL press
  - including in a level that has no hotel at all, where nothing can ever
  clear it first.

### level.cpp

- **line 241** [confirmed]
  CONFIRMED. Blocks5/src/level.cpp:240-243: "int ndc;" uninitialized,
  p_level->Attribute("numDiamondsCollected", &ndc), then numDiamondsCollected
  = ndc.
  Same shape as the two above, on the extendedAttributes path. Attribute(name,
  int*) leaves the int alone when the attribute is missing or unparsable, so a
  save carrying extendedAttributes="1" without numDiamondsCollected starts
  with a garbage diamond count - and Level::update() compares
  getNumDiamondsCollected() >= numDiamondsNeeded to decide when the exit stops
  being a ghost.

- **line 352** [confirmed]
  CONFIRMED. Blocks5/src/level.cpp:351-357 reads "int destroyTime;" and "int
  ghost;" with no initializer, then QueryIntAttribute("destroyTime",
  &destroyTime) / QueryIntAttribute("ghost", &ghost) and
  setDestroyTime(destroyTime) / setGhost(ghost ? true : false).
  TiXmlElement::QueryIntAttribute returns TIXML_NO_ATTRIBUTE and leaves the
  variable untouched when the attribute is absent, so an <Object> in an
  extendedAttributes save that lacks destroyTime= or ghost= passes whatever
  was on the stack. Every other query in the same function writes "temp = 0;"
  first. A garbage ghost value makes an object non-solid or invisible-solid; a
  garbage destroyTime schedules a destruction at an arbitrary tick. The
  comment at line 263-267 states the very assumption that makes this safe
  elsewhere ("a missing attribute counts as correct, because
  TiXmlElement::Attribute leaves the value untouched"), and it only holds
  where the local was initialized.

- **line 1586** [confirmed]
  CONFIRMED. Level::getTileAt() returns -1 for an invalid position or layer
  while its return type is uint, i.e. 0xFFFFFFFF. isFreeAt() (line 1284) and
  isFreeAt2() (line 1321) then write "if(tileID)", which is true for that
  value, and hand it to p_tileSet->getTileInfo().
  It is harmless only because TileSet::getTileInfo() answers badTile for any
  id >= 256 (Blocks5/src/tileset.cpp:175-178) - a clamp in another class that
  nothing here points at. The comment at 1575 explains why the layer is
  bounds-checked; nothing explains the -1 sentinel or that its safety depends
  on TileSet.

- **line 2047** [confirmed]
  CONFIRMED. addNewObjects() inserts objectsToAdd into objects first (line
  2043) and only then reads "uint uid = objects.back()->getUID();" (line
  2047). objects.back() is therefore the last of the objects just added, and
  Object's constructor sets uid = 0 (Blocks5/src/object.cpp:51), so uid is 0
  on every batch and the new objects are numbered 1..n again.
  sortObjects() (Blocks5/src/level.cpp:1249-1272) uses getUID() as its final
  tiebreak behind depth and getRealShownPosition().y, so two objects from
  different batches that share a depth and a y have duplicate UIDs and an
  unstable draw order. Reading the UID before the insert, or from
  objects[objects.size() - objectsToAdd.size() - 1], looks like what was
  meant. No comment in the file claims either way.

### leveleditor.xml

- **line 214** [confirmed]
  CONFIRMED. Two <StaticText> elements named Static6 inside the same Settings
  window: line 173 (the Title label, carrying for="Title") and line 214 (the
  graphics-style heading).
  GUI::getElement walks the child list and takes the FIRST name match
  (gui.cpp:437-446), so LevelEditor.SettingsPane.Settings.Static6 always
  resolves to the label at line 173 and the heading at 214 is unreachable by
  its dotted path. Latent rather than live: neither element is looked up from
  code, and the for="Title" link resolves the target name "Title" (the
  EditBox), which is unique in that window.

### lightbarriersender.cpp

- **line 35** [confirmed]
  CONFIRMED. `Vec2d dir;` and `Vec2d p;` in onRender are default-constructed,
  and vec.h's `Vec() {}` (line 59) leaves every member uninitialised.
  First loop pass copies the garbage into oldP/oldDir; the first point still
  gets added because of the `i == beam.begin()` short circuit. On the second
  pass `dir != oldDir` compares a valid dir against an oldDir derived from
  uninitialised memory, and that comparison is what decides whether the point
  reaches the LineDrawer. So the second beam point is added or dropped on a
  read of indeterminate memory. Note oldDir and oldP directly above are
  explicitly zero-initialised, which makes the omission look accidental.

### lightning.cpp

- **line 167** [confirmed]
  CONFIRMED. `Vec2i pos = b.points[start];` in generateSecondaryBranch, where
  Branch::points is std::vector<Vec2d> and the identical loop in generate()
  uses `Vec2d pos`.
  vec.h line 285 defines `template<typename U> operator Vec<U, DIM>() const`,
  so the Vec2d-to-Vec2i narrowing compiles silently. `pos += random(10.0,
  15.0) * dir` then converts a Vec2d back to Vec2i on every one of the up-to-8
  steps, so each step's fractional part is discarded rather than only the
  final coordinate - the branch drifts off the direction it was handed.
  `r.points.push_back(pos)` converts back to Vec2d. Reads as a typo for Vec2d.
  Not fixed, not commented.

### main.cpp

- **line 120** [confirmed]
  getCurrentVersion() waits 2 s on the update-check thread, then returns
  without CloseHandle(thread) and without stopping the thread. The thread's
  last act is to write into `Task task`, a local of the function that has
  already returned.
  On a slow or hanging connection WaitForSingleObject times out,
  getCurrentVersion returns "", the frame dies, and InternetReadFile can
  complete afterwards - `task.currentVersion = buffer; task.finished = true;`
  then writes into a dead stack frame. Plus a leaked thread handle on every
  start with the checker enabled. The comment I translated ("Run the query in
  a thread and allow it two seconds at most") states the timeout as if that
  were the whole story.

- **line 234** [confirmed]
  First agent's flag #4 confirmed: FILE* p_file = fopen(tempFilename, "at");
  is used by fprintf on line 235 and fclose on line 236 with no NULL check.
  An unwritable temp path makes fopen return NULL and the tool crashes instead
  of reporting. Windows-only path, pre-existing.

### manager.h

- **line 71** [confirmed]
  CONFIRMED. Manager<T>::request() leaks p_item when construction fails: `T*
  p_item = new T(filename); if(p_item->error) { printfLog(...); return 0; }` -
  no delete, no insertion into `items`.
  The object is unreachable afterwards: find() and exit() both walk `items`
  only, and the failed object was never inserted. Every failed request() of a
  missing texture, sound or font leaks one T. Confirms the first agent's
  reading; the comment above it ("load the object afresh and return it")
  describes only the success path.

- **line 76**
  Manager<T>::request() leaks p_item when construction fails: on p_item->error
  it logs and returns 0 without deleting the object and without inserting it
  into `items`.
  The object is never reachable again - not through find(), not through
  exit(), which only walks `items`. Every failed request() of a missing
  texture, sound or font leaks one T. The comment above it says "load the
  object afresh and return it", which is exactly what the error branch does
  not do.

### mirror.cpp

- **line 41** [confirmed]
  CONFIRMED, and worse than reported. reflectLaser tests `subType != 0` (line
  41) and reflectProjectile tests `subType != 1` (line 98), while
  changeInEditor normalises with `subType %= 2` (line 161) and getToolTip
  reads `subType % 2` (line 175). updateSprites (line 25) additionally uses
  `subType == 0 ? 160 : 352`.
  A level file carrying subType 2 reflects neither lasers nor projectiles, yet
  draws the cannon-mirror sprite (the `? :` sends everything non-zero to 352)
  while getToolTip's `% 2` names it the laser mirror - a mirror that looks
  like one mirror, is described as the other, and does nothing. `dir` is
  normalised consistently everywhere via `this->dir % 4`; subType is not.

### options.cpp

- **line 367** [confirmed]
  CONFIRMED: the first agent's finding is exactly right. changed is set false
  only in the constructor (line 52); changed = true runs at the foot of every
  handleClick body (line 367), including the OK and Cancel branches
  themselves. show() never resets it.
  Verified by grep: the only three sites are options.cpp:52 (constructor),
  :361 (if(changed) engine.loadConfig()) and :367 (changed = true). One
  correction to the reasoning: during show() the pane is still hidden - it
  only becomes visible on the closing getChild("Options")->focus(), because
  GUI_Element::bringToFront() calls show() up the parent chain - so the
  changed signal fired by setSelection(-1) inside show() early-returns on
  if(isVisible()) and does NOT set the flag. It is the OK or Cancel click
  itself that sets it. Net effect is the same: from the first time the dialog
  is closed until the process ends, changed is stuck true, so every later
  Cancel calls engine.loadConfig() even when the player touched nothing. In
  the browser, where CLAUDE.md notes config.xml is written only when somebody
  asks for it, that reloads whatever happens to be on disk rather than the
  state the dialog was opened with.

### pin.cpp

- **line 75** [confirmed]
  CONFIRMED. "The input is undefined now." sits over p_input->setValue(-1),
  which is a no-op: Pin::setValue (line 196) returns immediately unless type
  == PT_OUTPUT, and p_input is by construction the input pin in both branches
  of the two-line selection above it.
  Verified the whole chain: Electronics::setAllOutputsToUndefined
  (electronics.cpp:332) writes only into outputPins, and
  Electronics::propagateOutputs (electronics.cpp:340) -> Pin::propagate writes
  only into pins still in connectedPins. So a disconnected input keeps its
  last value for ever instead of going to -1. Pin::writeValue is the method
  that bypasses the type check and would actually do it. Comment translated as
  written, code untouched.

- **line 124** [confirmed]
  CONFIRMED. The #ifdef EDGY_CONNECTIONS block in getConnectionPath cannot
  compile if the macro is defined: pin1 and pin2 are redeclared at function
  scope (already declared as const Vec2i& at lines 96-97, no nested braces in
  between), and Vec2i d at line 151 collides with the const Vec2d d(x0) of the
  spline at line 115.
  No build defines EDGY_CONNECTIONS, so nothing exercises it. Also confirmed
  the secondary point: the loop above has already pushed eleven spline points
  into path, so path.front() at line 142 is the first spline point and not the
  start that line 128 pushed. Left exactly as found; no comment lives inside
  the block.

### player.cpp

- **line 468** [confirmed]
  int inv; is left uninitialised and then passed to
  p_element->Attribute(attrName, &inv); TiXml leaves the int untouched when
  the attribute is missing, so inventory[i] takes whatever was on the stack.
  loadExtendedAttributes is the load side of a mid-game save, where
  saveExtendedAttributes always writes all eight inventory%d attributes - but
  a hand-edited or foreign save file with one missing gives the player a
  random number of bombs, diamonds or masks. The neighbouring calls
  (contamination, walk, touch, push, plantBomb) read into members that already
  hold a value, so only this one is exposed. Compare the constructor, which
  does zero all eight.

### presets.cpp

- **line 423** [confirmed]
  CONFIRMED. In the "Hint" branch, p_element->FirstChildElement("Text") is
  stored in p_text and immediately dereferenced by p_text->GetText() with no
  null check, while p_element itself and the returned char* p_textChr are both
  guarded.
  A level XML with <Object type="Hint" .../> and no <Text> child crashes while
  loading. Level files arrive from strangers through the Manager
  (Transfer::classify accepts any XML whose root is <Level>), so this is
  reachable from imported content. The two-thirds-guarded shape points at an
  oversight.

- **line 692** [confirmed]
  CONFIRMED. The "Damage" branch calls
  p_element->QueryDoubleAttribute("rotation", &rotation) without the
  if(p_element) guard that every comparable branch has; the two branches
  beside it ("ToxicGas", "Projectile") do not touch p_element at all.
  instancePreset is public, p_element has no default argument, and
  player.cpp:236/256 calls it as instancePreset("Bomb", position, 0) - a null
  element is a normal argument. The comment above the group says these types
  only ever occur in saved games, which is presumably the assumption the
  missing guard rests on, but nothing in the signature or in the editors
  enforces it.

- **line 424**
  p_element->FirstChildElement("Text") is dereferenced by p_text->GetText()
  with no null check, while the surrounding code carefully guards p_element
  itself and the returned char*.
  A level XML containing <Object type="Hint" .../> with no <Text> child
  crashes the game while loading. Level files arrive from strangers through
  the Manager (Transfer::classify accepts any XML whose root is <Level>), so
  this is reachable from imported content. The two-thirds-guarded shape -
  p_element checked, p_textChr checked, p_text not - looks like an oversight
  rather than an invariant.

### progressdb.cpp

- **line 26** [confirmed]
  CONFIRMED. load() returns early only when progress.zip is absent. After
  doc.Parse(), doc.FirstChildElement("ProgressDB") is dereferenced on line 27
  with no null check, and line 30 builds std::string campaign directly from
  p_campaignInfo->Attribute("campaign").
  A progress.zip that exists but whose progress.xml does not parse, is
  truncated, or has another root element gives p_progressDB == 0 and crashes
  at startup. TiXmlElement::Attribute returns NULL when the attribute is
  missing, and std::string(NULL) is undefined behaviour. The inner
  LevelCompleted walk right below is carefully guarded (level = -1,
  QueryIntAttribute, if(level != -1)), which makes the two unguarded lines
  look like an oversight rather than an invariant.

### projectile.cpp

- **line 219** [confirmed]
  CONFIRMED but downgraded: p.deltaColor = Vec4d(0.0, 0.0, 0.0, -0.5 *
  -p.color.a / p.lifetime); the two minus signs do cancel, so the debris alpha
  rises by 0.5 * color.a over the lifetime instead of falling. However the
  identical expression appears twice more, at bomb.cpp:102 and bomb.cpp:163,
  so it is a repeated idiom across two files rather than a stray typo in this
  one. And it is very likely invisible: p.color.a is set on the line above to
  sampled.a + random(0.3, 0.5), which for an opaque texel is already 1.3 to
  1.5 and is clamped at draw time, so the particle is saturated from its first
  frame and the rising ramp changes nothing on screen. What actually removes
  the debris is p.deltaSize = -p.size / p.lifetime plus the 'size <= 0' erase
  in ParticleSystem::update, not the alpha.
  Refutes the 'debris fades in and never fades out' framing: the fade-out is
  carried by size, not alpha, and the alpha ramp is clamped away. The double
  negation is still worth cleaning up for readability and for consistency with
  the plain -p.color.a / p.lifetime used at projectile.cpp:189, laser.cpp:186
  and twenty other sites, but it is not a visible defect and it is not local
  to this file - any fix should touch bomb.cpp:102 and bomb.cpp:163 in the
  same pass. Reported only, not fixed.

### selectlevel.xml

- **line 20** [confirmed]
  CONFIRMED. Two <StaticText> elements named Static3 as direct children of
  SelectLevel: line 12 (the campaign-description heading) and line 20 (the
  select-level heading).
  Same first-match lookup as above, so the element at line 20 cannot be
  reached by name. Also latent: nothing in Blocks5/src addresses either one.
  Note that options.cpp:92's comment "options.xml, Static3" points at a
  different file, where Static3 is unique.

### singleton.h

- **line 21** [confirmed]
  Singleton<T>::operator= is declared to return Singleton<T>& but its body is
  empty - no return statement.
  Falling off the end of a non-void function is undefined behaviour. It is
  protected and, as far as this file shows, never called (the point of
  declaring it is to suppress the implicit one), but MSVC and GCC both warn
  and a derived class assigning to itself would hit it. The copy constructor
  on line 13 likewise ignores rhs, which is intentional here.

### smoke.js

- **line 26** [confirmed]
  CONFIRMED, and the comment is the wrong half: "Without a selection in the
  list the four buttons below it are disabled" - the code disables three, not
  four.
  Blocks5/src/options.cpp:155-157 deactivates ResetSelected, PrimaryKey and
  SecondaryKey only, and the selection branch at options.cpp:293 carries its
  own comment "With no selection there is nothing to rebind and nothing to
  reset; all three buttons hang off it". ResetAll (options.xml:110,
  options.cpp:35/332) is never deactivated and stays clickable with no
  selection - clicking it calls Engine::resetActions(). So the test's
  three-name loop matches the code and it is the German comment that counts a
  fourth button. CLAUDE.md repeats the same claim ("Those two and the two key
  buttons all grey out without a selection"), so either the doc and the
  comment are stale or ResetAll is missing a deactivate().

- **line 79** [confirmed]
  CONFIRMED contradiction: "Emscripten's SDL reports a hidden page as
  SDL_WINDOWEVENT anyway, which the game does not listen for" is false of the
  current engine.
  Blocks5/src/engine.cpp:923 has `case SDL_WINDOWEVENT` inside `#ifdef
  __EMSCRIPTEN__`, mapping FOCUS_GAINED/SHOWN and FOCUS_LOST/HIDDEN onto
  handleAppFocus() - and smoke.js's own later block (line 115ff, the
  'visibilitychange' entry of the loop) exists to test exactly that branch.
  The argument the stale clause supports is unaffected: a hidden tab gets no
  requestAnimationFrame, so no logic tick polls the event, which is why pre.js
  suspends the AudioContext a layer below the engine. Translated as written;
  the clause wants deleting or narrowing to "the game never sees it, because
  nothing polls".

### sound.cpp

- **line 42** [confirmed]
  CONFIRMED, and it is four error paths, not three: every failure after
  AudioStream::open() succeeds leaks the AudioStream.
  The alGenBuffers failure (line 33), the format failure (line 43), the read
  failure (line 56) and the alBufferData failure (line 70) all `return`
  without `delete p_stream`; only the success path at line 75 deletes it. The
  read and buffer paths do delete[] p_data, which shows the omission is the
  stream alone. The first agent missed the alGenBuffers path at line 27-34.

- **line 84** [confirmed]
  CONFIRMED: ~Sound() deletes every SoundInstance in `instances` but never
  erases them from the static `allInstances` set.
  ~Sound() (lines 78-85) iterates `instances` and deletes each entry; neither
  it nor ~SoundInstance() touches `allInstances`. Only Sound::update() (line
  136) erases from both. Sound::getFreeSource() then walks `allInstances` and
  dereferences each entry ((*i)->looping, ->priority, ->timestamp) and may
  call onLoseSource() on it, so after any Sound is destroyed - which
  Manager<Sound>::destroy does on the last release() - it reads freed memory.

### soundinstance.cpp

- **line 26** [confirmed]
  CONFIRMED, and wider than reported: targetVolume and targetPitch are left
  uninitialised on BOTH branches of the constructor.
  The else branch (lines 24-29) sets only timestamp, priority and looping,
  leaving volume, pitch, targetVolume, targetPitch, volumeSlideSpeed,
  pitchSlideSpeed and pauseAtSlideEnd indeterminate; getVolume() (line 71) and
  getPitch() (line 88) have no !sourceID guard and return those members
  straight out, and Level and Player code reads getVolume() on stored
  instances. The success branch (lines 9-23) sets volume and pitch through
  setVolume(1.0)/setPitch(1.0) but never targetVolume or targetPitch -
  harmless only because volumeSlideSpeed and pitchSlideSpeed start at 0.0, so
  update() does not reach them until slideVolume()/slidePitch() writes both.

### streamedsound.cpp

- **line 93** [confirmed]
  CONFIRMED. stop() calls alDeleteSources(1, &sourceID) but leaves sourceID at
  its old non-zero value.
  Verified by reading the whole class: sourceID is set only in the constructor
  (0) and in play() from Sound::getFreeSource(), never cleared. setVolume()
  (line 117) and setPitch() (line 128) guard on if(sourceID) and then call
  alSourcef on it, and update() (line 171) calls setVolume() whenever
  Engine::wasVolumeChanged() reports a change - so between a stop() and the
  next play() those calls address a deleted source name. pause() (line 101)
  and resume() (line 106) have no guard at all and do the same
  unconditionally. Harmless today only because AL raises AL_INVALID_NAME and
  nothing reads alGetError there. The comment above the call says no more than
  "delete the sound source" and gives no hint that sourceID stays stale.

### tileset.cpp

- **line 97** [confirmed]
  CONFIRMED, both halves. uint id =
  static_cast<uint>(p_tileElement->Attribute("id")[0]) has no null check and
  no range check, and id indexes the 256-entry tiles[] at line 118.
  TiXmlElement::Attribute(const char*) returns 0 when the attribute is absent
  (libs/tinyxml-2.6.2/tinyxml.cpp:577-583), so a <Tile> without an id
  dereferences null. And char is signed on all three targets, so an id byte of
  0x80 or above sign-extends to a negative int and static_cast<uint> makes it
  ~4.29e9; tiles[id] = info then writes far outside the array and maxTileID
  takes the same value. The read path is guarded (getTileInfo returns badTile
  for id >= 256, line 177), the write path is not. A skin archive can arrive
  through the Manager's import, i.e. from a stranger. Marginal third case:
  id="" gives '\0', so such a tile is written to tiles[0], which renderTile
  treats as empty and never draws.

- **line 10**
  The constructor calls reload() at line 10 and only then initialises badTile
  at lines 12-14, yet reload() starts every tile from badTile (line 94:
  TileInfo info = badTile).
  badTile is a plain TileInfo member: int type and int destroyTime are POD,
  and Vec's default constructor has an empty body (src/vec.h:61-63), so on the
  first reload() the copied values are indeterminate. A <Tile> that names no
  type attribute keeps that garbage instead of the intended -1, and the
  if(info.type == 2) branch at line 107 reads it - so whether such a tile gets
  debris sprites and a destroy time is undefined. Every later reload() is
  fine, because by then badTile has been set. Not a comment problem: no
  comment mentions the ordering.

### util.cpp

- **line 318** [confirmed]
  printfLog's crash-log branch calls fprintf(p_file, ...), fflush(p_file) and
  fclose(p_file) without checking p_file for NULL, while the identical block
  for log.txt twelve lines above is guarded by if(p_file).
  Not touched - reporting only. If crash_log.txt cannot be opened (read-only
  user directory, disk full, or the home directory not yet created), fopen
  returns NULL and the very next call dereferences it. This runs while
  writingCrashLog is true, i.e. from inside the SEH crash handler, which is
  precisely where a second fault destroys the report being written.

- **line 285**
  printfLog formats into a static char text[1024] with vsprintf, which has no
  bound. Callers pass filenames and full paths (e.g. Level::loadErrorLevel,
  Engine::playMusic), and a user directory plus an imported filename can
  exceed 1024 bytes.
  Not flagged by the first agent and not touched here. vsnprintf would be the
  bounded form; the same shape appears in decryptPassword, which writes into
  char step1[1024] from an input string of unbounded length. Both are
  stack/static buffer overflows reachable from file data.

### verify.py

- **line 167** [confirmed]
  CONFIRMED: `if f == 'pch.cpp': pass` is a dead branch, and the comment
  describes an exemption that is never applied.
  pch.cpp is in fact listed in Blocks5.vcxproj (line 718, with
  <PrecompiledHeader>Create</PrecompiledHeader>) and in
  Blocks5.vcxproj.filters, so the check passes for it either way and nothing
  is currently broken. The comment's claim ("listed with a rule of its own")
  is true of the project file; what is wrong is that the branch does nothing -
  either `pass` should be `continue`, or both the branch and the comment
  should go. Translated as it stands.

### videorecorder.cpp

- **line 331** [confirmed]
  CONFIRMED. The six-line comment about the VBV buffer ("one second", with the
  measured 3091 vs 2877 kbit/s) sits above createParam.num_layers = 1, but
  createParam.vbv_size_bytes is never assigned anywhere in the game's sources
  - grep finds the name only inside Blocks5/libs/minih264/minih264e.h - and
  createParam was memset to 0 two lines earlier.
  The comment documents a setting the code does not make, and it describes
  exactly the failure mode that is therefore live: in minih264e.h the
  rate-control corrections at lines 10778, 10783, 10906 and 11294 are all
  guarded by if (enc->param.vbv_size_bytes), so at 0 they are skipped and
  desired_frame_bytes is only a per-frame starting value. Either the
  assignment was lost or the comment was written against an intended change.
  Translated as it stands; not fixed.

### audiocapture.cpp

- **line 827**
  NEW: the Linux threadProc calls printfLog from the capture thread, which the
  Windows comment at line 482 says must never happen
  The comment above SDL_SemPost states that printfLog must not be used in the
  capture thread because it has a static buffer and is not thread-safe, which
  is why the Windows half only stores initResult and lets the main thread log
  it (616). util.cpp:286 confirms the static char text[1024]. But the Linux
  threadProc (788-846) logs a read failure directly at line 827, inside that
  same thread, while the main thread may be logging too. Either a real race or
  a comment that no longer describes both halves. The German said the same;
  translated as written.

### conveyorbelt.cpp

- **line 56** [refuted]
  First agent's suspicion 1 is REFUTED as stated, but a worse hazard sits
  behind it. Premise refuted: Sound::createInstance(bool forceCreation) has
  exactly one "return 0" (sound.cpp:103-108), the 10 ms lockout, and
  forceCreation == true skips it; the rest of the function returns "new
  SoundInstance". So p_soundInst is never null while numInstances > 0, and the
  unguarded p_soundInst->stop() in onRemove() is unreachable by that route.
  The constructor's "if(p_soundInst)" is belt and braces, and the
  "if(!p_soundInst) return;" in onElectricitySwitch() has no reachable trigger
  either (the editor never dispatches - Level::setElectricityOn guards with
  "if(!inEditor)"). Real hazard instead: SoundInstance's constructor tolerates
  Sound::getFreeSource() returning 0 and leaves sourceID == 0;
  SoundInstance::toBeRemoved() (soundinstance.cpp:186-188) then returns true
  unconditionally, and Sound::update() (sound.cpp:130-137) deletes the object.
  Nothing clears the static, so p_soundInst DANGLES rather than going null -
  and a null check would not catch it. Both onRemove()'s stop() and
  onElectricitySwitch()'s four calls then touch freed memory.
  getFreeSource() returns 0 only when alGenSources fails and every existing
  instance is looping (it evicts the oldest non-looping one otherwise,
  sound.cpp:160-179). The looping sounds are exactly the ones that hold their
  instance in a static - conveyorbelt, player (toxic and mask), laser,
  elevator, toxicgas - so a level dense enough to exhaust the sources with
  looping sounds alone is the case that hits it. Same shape in all five files.

### findgerman.py

- **line 15**
  The DE regex used by checkbatch.py's German check contains entries that are
  ordinary English words - at least 'so', 'also', 'was', 'die' and 'man' - so
  it fires on correct English and no well-written translated file can reach
  ALL CLEAN.
  Confirmed beyond this file: running findgerman.py -v on WebBuild/sw.js
  reports 13 lines, of which lines 10, 12, 15, 19, 24 and 27 are the author's
  own English ('so no cache on the way', 'It is also why...') while only lines
  45 and 80-84 and 99 are genuinely German. Every agent in this sweep will hit
  the same noise, and a real leftover German line can be lost among the false
  positives. Suggest dropping 'so', 'also', 'was', 'die' and 'man' from the
  list, or requiring two distinct hits on a line. Reported, not touched - the
  checker is outside my file list.

### gs_campaigneditor.cpp

- **line 155**
  Clicking Load with a filename that does not exist does nothing and says
  nothing.
  `if(FileSystem::inst().fileExists(path))` at line 155 has no else, so a
  typed-in campaign name that resolves to no file falls out of handleClick
  silently. That is exactly the failure the two "With no filename nothing
  happens here at all - the click goes nowhere and nobody learns why" comments
  in this same function (lines 203, 262) exist to have fixed;
  gs_leveleditor.cpp's Load branch covers the case with a
  "$LE_ERROR_FILE_DOESNT_EXIST" toast. Not touched.

### hotel.cpp

- **line 66**
  Hotel::p_hotelToSave is a static raw Hotel* that nothing clears when the
  Hotel is destroyed - not the destructor, and Hotel overrides no onRemove().
  It is only cleared from Hotel::onUpdate (when the player steps off) and from
  onSave. If the level ends, restarts or is left while the player is standing
  on the hotel, the pointer outlives the deleted object; gs_game.cpp:395-397
  then dereferences it on the next $A_SAVE_IN_HOTEL press, and in a level with
  no hotel at all nothing ever clears it first.

### laser.cpp

- **line 57** [refuted]
  Flagged: onRemove() calls p_soundInst->stop() unguarded while the
  constructor writes if(p_soundInst). The null path cannot be reached here.
  Sound::createInstance(bool forceCreation) (sound.cpp:87) returns 0 only
  inside if(!forceCreation) on the 10 ms lockout; past that it always returns
  a newly constructed SoundInstance. Laser passes true, so p_soundInst is
  non-null whenever the first-instance branch ran, and both the constructor's
  increment and onRemove's decrement sit inside the same
  if(!level.isInEditor()) guard. What is left is the asymmetry itself - the
  constructor guards, onRemove does not - which is a readability wart, not a
  crash. Not touched: comments only.

### linedrawer.cpp

- **line 71**
  LineDrawer::update() returns before vertices.clear() when points.size() < 2,
  while draw() clears dirty unconditionally - so a stale vertex array would be
  redrawn and never recomputed, and vertices[0] on an empty vector is
  undefined behaviour.
  Confirmed as a defect in the class contract, refuted as a live bug. Neither
  caller can reach it: Laser::updateBeam pushes the start point
  (laser.cpp:204) and then unconditionally pushes a second one inside
  while(true) before any break (laser.cpp:216), and LightBarrierSender does
  the same (lightbarriersender.cpp:110, 119), so a non-empty beam always holds
  at least two elements. The render loops add both begin() and last, giving
  points.size() >= 2 at every draw(). The empty-vector read is guarded too,
  since both render paths sit inside if(!beam.empty()) (laser.cpp:78,
  lightbarriersender.cpp:32). Not translated-comment territory - no comment in
  these files mentions it.

### parameterblock.h

- **line 27**
  NEW, noticed while reviewing: operator = calls clear() before copying, with
  no self-assignment guard. b = b therefore deletes every container and then
  iterates rhs.values, which is the same map and now empty, leaving the block
  silently empty rather than unchanged.
  Latent rather than active - a ParameterBlock is normally assigned from a
  freshly built one - but it is the classic missing 'if(this == &rhs) return
  *this;'. No comment claims otherwise; reported only.


Comment contradicts the code
----------------------------
A comment, a header or CLAUDE.md says one thing and the code does another.
The comment was translated as it stood; which side is wrong is a separate
question.

### build.sh

- **line 54** [confirmed]
  CONFIRMED. The heading says "The game's sources without the three that do
  not come along here" and the third entry says audiocapture "does come
  along". The grep on line 58 is grep -vE '/(stackwalker|pch)\.cpp$', so
  exactly two files are excluded, not three.
  Heading and list disagree, and the code sides with the list. Translated as
  written; the German has the same contradiction.

### build_asan.sh

- **line 29** [confirmed]
  videorecorder is excluded here because it is "portable now, but nothing here
  captures audio"; build.sh:36 excludes it because it "encodes in a thread of
  its own, and there is none here; no audio either".
  CONFIRMED. Two different reasons for the same exclusion in two sibling
  scripts. CLAUDE.md sides with build_asan.sh on portability (LinuxBuild is "a
  real GCC compile of every source, videorecorder.cpp included") and with both
  on audio ("The browser has no loopback at all; there open() fails and videos
  are silent"), while still listing videorecorder.cpp among the three files
  left out of the web build. build.sh's German was translated as it stands.

- **line 44** [confirmed]
  "TinyXML 1 is linked as a prebuilt .lib on Windows; its sources are not
  vendored." The very next line compiles tinyxml, tinyxmlparser, tinyxmlerror
  and tinystr out of $GAME/libs/tinyxml-2.6.2/, and build.sh:51 says the
  opposite: "TinyXML 2.6.2 is vendored in the tree and compiled here exactly
  as the Visual Studio project compiles it."
  The comment is stale - CLAUDE.md's rule is that no compiled code without
  source is anywhere in the tree and libs/bin holds nothing but OpenAL32.lib.
  Left as written; only the "so" was reworded.

### burst.js

- **line 22** [confirmed]
  console.log('FEHLGESCHLAGEN: ' + e.message) - the failure banner of the test
  script stays German, because rule 4 forbids touching string literals.
  WebBuild/test/smoke.js:182 has the identical string.
  In a tree that is being made wholly English this is the one German word a
  developer running the browser tests will actually see on screen, and it sits
  beside harness.js's report() which already prints 'OK' and 'problem(s)'. It
  is developer-facing tooling output, not a game string with a $ID, so
  translating it to 'FAILED' would be safe - but that is a code change and
  belongs to the separate pass, in both files at once.

### campaign.h

- **line 24** [confirmed]
  CONFIRMED. makeLooseRef() is documented as a "reference to a loose file in
  the user's level folder", but it resolves through
  FileSystem::resolveContentPath(), which asks the game folder first
  (CLAUDE.md: "Levels, campaigns and skins have two roots, and the game folder
  wins"), and its own body comment says the two example levels sit with the
  game.
  Same shape as the previous finding: the header line names only one of the
  two roots the code searches.

- **line 27** [confirmed]
  CONFIRMED. loadSingleLevels() is documented as "the individual levels in the
  user's folder", but campaign.cpp:158-160 lists both roots -
  fs.getGameDirectory() + "levels" and fs.getAppHomeDirectory() + "levels" -
  merged, sorted and uniqued, and its own inline comment at campaign.cpp:156
  says "Both roots".
  The header sentence omits the shipped example levels, which the function
  deliberately includes. Rendered as written.

### cf_rewind.h

- **line 30** [confirmed]
  CONFIRMED as a disagreement between two comments. cf_rewind.h names the
  on-screen display "REWIND"; the block comment above render() in
  cf_rewind.cpp names it "<< REW". The geometry both describe is the same
  (word at source x 0 width OSD_TEXT_WIDTH = 162, arrows at x 162 width 56,
  both at y 112), so only the wording of the text differs.
  At most one of the two matches what is actually drawn in misc.png. Not
  decidable from the source - it needs somebody to look at the region
  (0,112)-(162,176) of misc.png.

### diamondmachine.h

- **line 36** [confirmed]
  CONFIRMED. The comment on sparkId says it is the id the *inward* sparks of
  the conversion are marked with, but diamondmachine.cpp stamps p.id = sparkId
  on both kinds: line 237 (outward, the block being taken apart) and line 311
  (inward). abortConversion() filters the whole particle list on `if(p.id !=
  sparkId) continue;` at line 365 and then tells the two apart by the sign of
  deltaColor.a, so the id demonstrably covers the outward debris too - which
  is what CLAUDE.md describes ("the outward ones ... are sucked back").
  Translated as written, per the pass rules. The comment understates what the
  field covers; a reader would conclude the outward debris carries id 0 and is
  invisible to abortConversion(), and would then not understand line 365 or
  the inward/outward test that follows it.

### engine.cpp

- **line 49** [confirmed]
  CONFIRMED. The MASTER_HEADROOM comment measures the mix at -8.8 LUFS / peak
  0 dBFS before the headroom and -15.5 LUFS / peak -0.9 dBFS after; CLAUDE.md
  gives +0.9 dBFS and -1.1 dBFS for the same two measurements.
  Both cannot hold, and the argument turns on the -1 dBTP ceiling the comment
  cites: -0.9 dBFS sits just outside it, -1.1 dBFS just inside. Translated as
  written. The glossary lists this pair as a known discrepancy; somebody has
  to re-measure and make the two agree.

- **line 45**
  The MASTER_HEADROOM comment gives the measured true peak as 0 dBFS before
  the headroom and -0.9 dBFS after it. CLAUDE.md gives the same two
  measurements as +0.9 dBFS and -1.1 dBFS.
  Both cannot be right, and the argument in the comment turns on the -1 dBTP
  ceiling: at -0.9 dBFS the shipped value would sit just outside the standard
  it cites, at -1.1 dBFS just inside. Translated as written (0 and -0.9);
  somebody has to re-measure and make the two agree. The glossary lists this
  pair as a known discrepancy.

### engine.h

- **line 116** [confirmed]
  CONFIRMED, and it is not a translation artefact. The German original reads
  "Mit einer gemeinsamen Textur zeichnete der zweite hinein, waehrend der
  erste noch daraus las, und dann zeigten beide denselben Text", i.e. the pool
  is presented as load-bearing for correctness. hint.cpp:212 makes the same
  claim ("Its own texture, not a shared one: ... the one must not draw into
  the sheet the other is being read from"), while CLAUDE.md says sharing one
  texture "was not *wrong* - each note bakes immediately before it draws, and
  GL runs the commands in order" and calls the pool a cost measure.
  The code supports CLAUDE.md rather than the comment: Hint::bakeNote() bakes
  into the target immediately before the draw in the same frame, and it
  early-returns when the text is unchanged (`if(noteTexture && wanted ==
  bakedText) return;`), so with a shared texture the second note would re-bake
  every frame - expensive, but each draw still follows its own bake in command
  order. Two source comments now assert a correctness failure that the third
  description denies; one of the three is wrong. Left exactly as the German
  stands.

### main.cpp

- **line 281** [confirmed]
  The version table says not_played and "<= 1.0.7" both mean "no 'Blocks 5'
  folder exists in the user directory", but the code branches on
  fs.listDirectory(homeDirectory).empty().
  An existing but empty "Blocks 5" folder - which a player can produce by
  deleting its contents, or which a partly failed first start can leave behind
  - takes the not_played path even though the folder does exist. The comment
  describes existence; the code tests emptiness. I translated the claim as
  written.

### smoke.sh

- **line 131** [confirmed]
  The comment block above the music section restates, nearly word for word,
  the Export/Delete rule already given in the block at line 100, thirty lines
  earlier.
  Both say the list is the union of both roots, that everything listed can be
  exported, that only what is in the user directory can be deleted, and that
  the first entry is selected. Only the second paragraph of the later block
  (what stage.bat ships, and the ten music tracks present when running out of
  the working directory) adds anything. Under the pruning rules one of the two
  paragraphs is redundant, but deleting a whole comment block is beyond a
  translation pass, so I kept both and tightened the repeat.

### teleporter.cpp

- **line 111** [confirmed]
  CONFIRMED. getToolTip() switches on subType % 2, while every other use of
  subType in the file tests it for equality against 0 or 1.
  updateSprites() (line 24) picks the white tint only for subType == 0 and
  cyan otherwise; onUpdate() (lines 73-74) applies the no-player/no-enemy rule
  only for subType == 1 and refuses the teleport for any other non-zero value.
  The tooltip instead folds every even subType onto $TT_TELEPORTER and every
  odd one onto $TT_TELEPORTER_NO_PLAYER. saveAttributes() (line 106) writes
  subType back verbatim, so a level carrying subType 2 would draw cyan, refuse
  to teleport anything, and still claim in its tooltip to be the plain
  teleporter. Separately, the trailing "return toolTip;" on line 117 is
  unreachable for any non-negative subType, since subType % 2 can then only be
  0 or 1.

### transfer.cpp

- **line 151** [confirmed]
  CONFIRMED. The comment over the isBuiltIn() guard in install() calls the
  reserved set "die sieben Namen, unter denen das Spiel selbst etwas
  mitliefert" (translated literally: "the seven names under which the game
  itself ships something"). isBuiltIn() keeps no list of seven: it asks
  FileSystem::isShippedContent(), i.e. belongsToPlayer() == false AND the file
  exists in the game folder - an open-ended set. The comment at line 216 says
  exactly that and explicitly denies there is a list.
  Seven is the size of FileSystem::getPlayerFiles (filesystem.cpp:59-69):
  levels/example01.xml, levels/example02.xml and five readme.txt - the set
  isBuiltIn deliberately answers *no* for. Under either reading the number is
  wrong for the passage it stands over: a shipped installation reserves the
  campaign blocks.zip plus the four skin archives (five names of a Transfer
  kind; the five readme.txt are neither .xml, .zip nor .ogg and never reach a
  Kind), and if the two examples are counted in as shipped files the total is
  seven but those two are precisely the ones isBuiltIn lets through.
  Translated as written and left alone.

- **line 187**
  list()'s comment claims "Ein Name kann nur einmal vorkommen - unter einem
  mitgelieferten Namen laesst sich weder speichern noch einspielen -" ("A name
  can occur only once - nothing can be saved or imported under a shipped
  name"). That is false for the seven files FileSystem::belongsToPlayer()
  covers: isBuiltIn() returns false for levels/example01.xml and
  example02.xml, so a player may save their own copy while the game folder
  still holds one, and the name then exists in both roots.
  The code already handles it - the std::find guard at line 201 drops the
  second occurrence, which is the "comparison further down" the same comment
  names, and isRemovable()'s comment at lines 228-230 describes exactly this
  case for the two example levels. So the parenthetical reason is
  stale/overstated while the conclusion and the guard are right. Rendered as
  written; a separate pass should decide whether the parenthesis should name
  the player files as the exception.

### u_crt.cpp

- **line 275** [confirmed]
  The comment block over toLinear() ends "The conversion is x*x rather than
  pow(x, GAMMA_IN) - gamma 2.0 instead of 2.4", but toLinear() itself is
  pow(max(c, 0), GAMMA_IN). The x*x squaring is in the halation block (t0*t0
  ... t7*t7, line 410).
  The claim is true of the halation taps and false of the function the comment
  is attached to, so a reader editing toLinear() to "restore" x*x would
  silently change the vertical resampling as well - toLinear() is also used
  for c0/c1 and the convergence fetches. Translated as it stands.

### util.h

- **line 47** [confirmed]
  CONFIRMED (flagged by the first agent). The doc comment for isSafeMemberName
  lists separators, drive colon, < > [ ], "..", a leading dot or tilde and
  control characters, but the implementation also refuses the double quote,
  the pipe, the question mark and the asterisk (strchr("/\\:<>[]\"|?*", c))
  and refuses any name longer than 100 characters (name.length() > 100).
  A caller reading only the header would not expect isSafeMemberName("my
  level?.xml") or a 120-character name to fail. The sanitizeFilenameStem
  comment directly above does state its 64-character cap, so the omission
  looks accidental.

### web_bluescreen.cpp

- **line 70** [confirmed]
  CONFIRMED. The comment derives "80 * 0.6 = 48em of text width" and the CSS
  three lines below sets max-width:52em.
  The two numbers do not agree and the comment does not mention slack. Either
  52em is deliberate headroom for a font wider than 0.6em per character, or
  one of the two drifted. Translated as written and left alone.

### zip_skins.bat

- **line 1** [confirmed]
  CONFIRMED from git show HEAD: in both zip_skins.bat and
  zip_skins_no_optipng.bat exactly the three header comment lines carried CRLF
  while every code line was LF. They are LF now, since the harness requires
  it, and no code line was involved.
  zip_data.bat's own comment asserts that this file, "like the rest of the
  tree", has no Windows-style line endings, and gives that as the reason it
  avoids GOTO. Two sibling scripts contradicted that claim. Recorded so the
  follow-up pass knows the mixed endings existed.

### cf_rewind.cpp

- **line 185**
  The block comment above render() names the on-screen display "<< REW", while
  the member comment in cf_rewind.h line 30 names it "REWIND". CLAUDE.md says
  the sheet holds 'the word' in the left 162 pixels and 'the two triangles' in
  the 56 next to it.
  Two comments in the same effect give two different texts for the same
  sprite; at most one matches the artwork. Not fixable from the source - it
  needs somebody to look at misc.png.

### gl_compat.cpp

- **line 87**
  NEW. The comment says 'The game uses three masks - GL_TRANSFORM_BIT,
  GL_ENABLE_BIT and GL_ALL_ATTRIB_BITS - which is why the mask is honoured,
  not ignored', but glPopAttrib does not honour it per bit.
  Both tests OR the wanted bit with GL_ALL_ATTRIB_BITS (lines 153 and 155),
  and GL_ALL_ATTRIB_BITS is 0x000FFFFF, which already contains
  GL_TRANSFORM_BIT (0x00001000) and GL_ENABLE_BIT (0x00002000). So f.mask &
  (GL_TRANSFORM_BIT | GL_ALL_ATTRIB_BITS) is true for any of the twenty
  attribute bits: a push of GL_ENABLE_BIT alone also restores the matrix mode,
  and a push of GL_TRANSFORM_BIT alone also restores the eight capabilities.
  Benign as the code stands, because glPushAttrib saves both unconditionally
  (139-141), so what is restored is always what was saved - but the comment's
  claim is not what the code does. The German said the same; not a translation
  error.

### syntax.sh

- **line 32**
  The comment says the game writes <Windows.h>, <Shellapi.h>, <Shlobj.h> and
  <al.h>, but the loop under it also generates VersionHelpers.h, and the two
  echo lines generate al.h and alc.h.
  Two of the six generated shims (VersionHelpers.h, alc.h) are not named in
  the comment, so a reader adding a shim would not know the list is
  incomplete. Translated as it stands.


Stale claims
------------
A claim that was true once. A count that has moved, a name that was renamed,
a file that is no longer read.

### Build.bat

- **line 375** [confirmed]
  In the ':doclean' explanatory comment, the two 'deliberately NOT touched'
  paths are written as 'levels\campaigns\blocks.zip and
  misc\3p_campaigns\*.zip', without the 'Blocks5\' prefix. The real files are
  Blocks5/levels/campaigns/blocks.zip and Blocks5/misc/3p_campaigns/*.zip (six
  archives: easy, hell, legoland, rcg-holland, rolfspecial, thief).
  Build.bat does PUSHD "%~dp0" and runs from the repo root, where neither
  'levels\campaigns\' nor 'misc\3p_campaigns\' exists. Every neighbouring path
  in the same region carries the prefix - the code right below it says CALL
  :rmfile "Blocks5\levels\skins\blocks_01.zip" - so the comment names two
  directories in a coordinate system the rest of the file does not use, and a
  reader checking the claim finds nothing there.

### CLAUDE.md

- **line 72** [confirmed]
  CONFIRMED (first agent's flag 1). CLAUDE.md line 72 says pack.sh uses "zip
  -9 -P in place of 7za a -tzip -mx=9 -p". The script does the opposite: line
  46 refuses to run without 7za, lines 81 and 83 invoke "7za a -tzip -mx=9
  [-p...]", and the header comment argues at length that Info-ZIP's zip must
  NOT be used because it writes a different encrypted-entry format (bit 3 /
  data descriptor, time of day instead of the CRC as the check byte).
  CLAUDE.md describes an earlier version of the script. The comment matches
  the code; the doc does not. A doc pass should reconcile them - and the
  parenthetical "(both write traditional ZipCrypto, which is what minizip
  reads)" is exactly the assumption the comment warns against relying on.

### as_ogg.h

- **line 25** [confirmed]
  The private member `FILE* p_file;` is never written and never read anywhere
  in as_ogg.cpp.
  The constructor's `File* p_file` is a local of a completely different type -
  `File`, the virtual-filesystem class - and it goes out of scope at the end
  of the constructor. So the member is dead weight with a confusing name, and
  its type is the C stdio `FILE`, which this file never touches. as_wav.h has
  a genuine `File* p_file` member, which is probably where this one was copied
  from.

### build.sh

- **line 110** [confirmed]
  CONFIRMED. "would recompile all 167 units" - counted here today: 121 .cpp
  from Blocks5/src after the grep, plus linux_window.cpp, plus 4 TinyXML, plus
  49 C sources (14 zlib/minizip, 2 libogg, 2 impl, 22 libvorbis, 9 shine) =
  175, which is also what $total on line 105 prints.
  Sources have been added since the figure was written. Kept at 167 in the
  translation, as the German has it.

- **line 109**
  "would recompile all 167 units" - the script's own $total is 175 today: 121
  files from Blocks5/src after the grep, plus linux_window.cpp, plus 4
  TinyXML, plus 51 C sources (19 zlib/minizip/ogg/impl, 23 libvorbis, 9
  shine).
  Sources have been added since the number was written. Kept at 167 in the
  translation.

### build_asan.sh

- **line 2** [confirmed]
  The header of build_asan.sh calls the script build.sh, and the usage lines
  at 5-6 invoke ./build.sh.
  CONFIRMED. Copied header; anyone following it runs the ordinary build
  instead of the sanitizer one, which is also the only script that would tell
  them $OUT is build-asan (line 11).

### campaign.cpp

- **line 87** [confirmed]
  CONFIRMED. The constructor comment says clear() sets "both", but three
  members follow: numUnlockedLevels, iHaveABonusLevel and singleLevels (and
  clear() sets six things in all).
  The count is left over from before singleLevels existed. Translated
  faithfully as "both"; a later pass decides whether it becomes "all three".

### cf_rewind.cpp

- **line 66** [confirmed]
  The comment over OSD_TEXT_WIDTH / OSD_ARROWS_WIDTH / OSD_HEIGHT says 'wie
  rewind.png aufgeteilt ist' (how rewind.png is divided up) and 'Die Hoehe ist
  die des ganzen Bildes' (the height is that of the whole image), but the
  constructor at line 87 requests "misc.png", and render() reads the sprite
  from a region at source y = 112. There is no rewind.png in Blocks5/data
  (only rewind.ogg and rewind.wav); CLAUDE.md also still describes it as
  'data/rewind.png, 256x64'.
  The OSD art was evidently folded into misc.png without the comment (or
  CLAUDE.md) following. As written the comment sends a reader to a file that
  does not exist, and its claim about the height being the whole image is
  false for a 112px-offset region of a sheet.

### diamondmachine.cpp

- **line 514** [confirmed]
  CONFIRMED: if(counter >= 100) hardcodes the value CONVERSION_TICKS holds,
  seventeen lines after setConversionProgress() divides by CONVERSION_TICKS.
  CONVERSION_TICKS = 100 at line 70 and the header comment both drive the
  timetable; lowering the constant would leave this test behind and the block
  would be converted at a tick the inward sparks do not aim for.
  updateSprites() at line 430 has the same shape with min(counter, 80) - the
  80 is the header diagram's fifth-frame mark, also unnamed.

- **line 281**
  The comment writes the constant as "d^moves = inAccel", but the constant is
  IN_ACCEL; no identifier named inAccel exists.
  Present in the German original the same way ("d^moves = inAccel"), so it was
  rendered as written. Only a naming drift in the comment - the arithmetic
  below it is pow(IN_ACCEL, 1.0 / moves) and is correct.

- **line 513**
  if(counter >= 100) hardcodes the value that CONVERSION_TICKS holds, three
  lines after setConversionProgress() divides by CONVERSION_TICKS.
  The header comment and the progress calculation both go through
  CONVERSION_TICKS = 100; changing the constant would leave this test behind,
  and the block would be converted at a different point than the sparks aim
  for. updateSprites() has the same shape with its min(counter, 80).

### e_barrage.h

- **line 1** [confirmed]
  CONFIRMED. The include guard is `#ifndef _E_BARRIER_H` / `#define
  _E_BARRIER_H` while the file is e_barrage.h and the class is E_Barrage - a
  leftover from an earlier "Barrier" spelling. `grep -rn _E_BARRIER_H
  Blocks5/` finds it only in these two lines, so nothing collides today.
  Not a comment, so out of scope for this pass. A future e_barrier.h taking
  the obvious guard name would silently include as empty.

### engine.cpp

- **line 280** [confirmed]
  CONFIRMED. In HEAD the comment over the joystick button loop is
  byte-identical to the one over the keyboard loop eighteen lines above (`//
  alle Tasten als VK einfuegen`), although the loop runs over
  SDL_JoystickNumButtons and builds ids `Joystick1 B1`.
  A copy of the keyboard label that was never adjusted. The translation says
  "add every button as a VK", which is what the code does; I left that in
  place rather than putting a wrong label back into English.

- **line 277**
  The comment over the joystick button loop reads `// alle Tasten als VK
  einfuegen` - byte-identical to the comment over the keyboard loop 18 lines
  above - although the loop is over SDL_JoystickNumButtons and the ids it
  builds are `Joystick1 B1` and so on.
  It is a copy of the keyboard loop's label that was never adjusted. Rendered
  as 'add every button as a VK', which is what the code does; reporting it
  because that is a change of word, not a translation of the German as it
  stands.

- **line 3626**
  The comment in detectSystemLanguage() says "Of the 349 strings in
  data/languages.txt". The file holds 386 $IDs today (385 with an en body and
  385 with a de body).
  Counted directly: 386 lines start with '$' in Blocks5/data/languages.txt.
  The rest of the claim still holds - there is exactly one \xA7fr: body and
  exactly one \xA7es: body in the file, both inline rather than at the start
  of a line - so only the 349 is stale. CLAUDE.md repeats the same 349, so
  both places drift together. Translated as written and left alone.

### gl_compat.cpp

- **line 28** [confirmed]
  CONFIRMED. Section headings are numbered 2, 3 and 4 with no section 1, and
  the 'Row length on upload' section between 2 and 3 carries no number.
  Line 20's '(immediate-mode variants moved to gl_immediate.cpp)' is where
  section 1 went; the numbering was left behind. A reader looking for section
  1 finds nothing. Not renumbered here - renumbering is a decision for the
  pass that handles these, not for a translation review.

- **line 21**
  '// (immediate-mode variants moved to gl_immediate.cpp)' is archaeology - a
  comment about what the file used to contain.
  Against CLAUDE.md's rule that a comment never says what the code used to do.
  It is already English, so this pass left it alone; a cross-reference ('the
  immediate-mode variants are in gl_immediate.cpp') would carry the useful
  half without the history.

### glextensions.cpp

- **line 106** [confirmed]
  The two filters that survive a missing GL 2.0 are named "Nearest und
  Bilinear" in the comment. Their getName() values - what config.xml,
  options.xml, the startup log and the test hook all carry - are Sharp and
  Smooth.
  The glossary's first trap is exactly this: a filter named by anything other
  than its getName() invents a filter that exists nowhere. Here the German
  already used the GL sampling-mode words, so the drift is in the source, not
  in the rendering; translated as written and reported instead of renamed.
  Note also that getEffectiveUpscaler falls back to a fixed Sharp, not to
  whichever is first available.

### gs_campaigneditor.cpp

- **line 248** [confirmed]
  The comment above WebTransfer::syncHome() says the immediate IndexedDB write
  happens "so wie beim Level-Editor" (as in the level editor), but the level
  editor does no such thing.
  gs_leveleditor.cpp contains no __EMSCRIPTEN__ block at all and never calls
  WebTransfer::syncHome(); the only callers in the tree are this site,
  main.cpp:273 and transfer.cpp:263/340. A level saved in the browser
  therefore waits for the five-second interval in WebBuild/pre.js:215 that
  this comment contrasts itself against. The five-second figure itself still
  holds (setInterval(Module['b5_sync'], 5000)). Translated as written.

### gs_game.h

- **line 36** [confirmed]
  CONFIRMED: void updateMusic(); is declared and never defined or called.
  grep -rn "updateMusic" over the whole repository (--include=*.cpp
  --include=*.h --include=*.c) returns exactly one hit, this declaration in
  the private section of GS_Game. No definition anywhere, so nothing links
  against it and nothing calls it - a dead declaration. Not a translation
  matter; the line carries no comment.

### gs_leveleditor.cpp

- **line 115** [confirmed]
  The comment says the checkbox is kept in step 'every frame' (German 'jedes
  Bild'), but LevelEditorGUI::onUpdate is driven by GUI::update() and
  therefore runs once per 20 ms logic tick, not once per rendered frame.
  Translated as written per the brief. The distinction matters in the browser,
  where a rendered frame and a logic tick are explicitly not the same thing
  (CLAUDE.md, 'Anything that reads the rendered frame must bind the FBO
  itself').

### gs_menu.cpp

- **line 167** [confirmed]
  CONFIRMED. The comment names only the donation question as the case where
  Escape must not quit; the condition on lines 170-172 also requires
  !gui["Menu.CrtPane"]->isVisible().
  The CRT offer pane is the second exception and the comment never mentions
  it, so a reader checking comment against condition finds the comment one
  pane short. Translated as written ("Not while the donation question is
  open"), not repaired.

### harness.sh

- **line 30** [confirmed]
  CONFIRMED as an observation, one claim in it REFUTED. Every user-facing
  string in this file is still German (lines 30, 41, 47, 49, 52, 79, 91, 99,
  107, 112, 174, 214, 218-221, 229, 252-253, 264, 267, 276, 282-283):
  FEHLGESCHLAGEN, IN ORDNUNG, "Warte auf das Fenster ...", "ist aelter als:",
  "kein Element", "ist nicht sichtbar", "ist abgeschaltet", "ginge an ... es
  liegt etwas darueber", Spielzustand, sichtbar/verschwunden,
  "Beanstandung(en) (Bilder in ...)", "der Testhaken antwortet nicht".
  Correctly left alone by the translation pass - they are string literals.
  The first agent's stated risk that "smoke.sh's expectations may match on
  some of these words" does not hold: LinuxBuild/test/smoke.sh never parses
  harness.sh's output. It only passes its own (also German) text into
  b5_ok/b5_note, and greps only its own run.log for "ERROR". So the two files
  can be changed independently; the reason to do them together is consistency
  of the visible output, not a broken expectation. Either way this is a
  separate pass - it is code, not comments, and checkbatch.py's code-only
  reduction would reject it here.

### hint.cpp

- **line 178** [confirmed]
  CONFIRMED. The German destructor comment says the texture is handed back "da
  oben in onUpdate()" - "up there". The destructor is at line 166, onUpdate()
  at line 452, i.e. far below.
  The locator points the wrong way. The first agent dropped the two words and
  kept the fact ("The texture is handed back in onUpdate() as soon as the note
  is invisible"), which I left as is; nothing in the file needs fixing beyond
  a decision on whether a locator is wanted at all.

### img_load.h

- **line 13** [confirmed]
  CONFIRMED. The header claims zip_data.bat and zip_skins.bat "pack nothing
  but *.png" (German: "packen auch nur *.png ein").
  Blocks5/zip_data.bat packs "*.png *.ogg *.txt *.dat" in one 7za call and
  "*.xml" from the staging directory in a second; zip_skins.bat is the same
  shape plus password.txt and hintscroll.txt. The sentence is only true read
  as "of the image formats, only PNG ever gets in", which is the point it is
  arguing, but as written it is false. Translated as it stands.

- **line 14**
  The header claims "zip_data.bat und zip_skins.bat packen auch nur *.png ein"
  - that the two packing scripts pack nothing but *.png.
  Both scripts pack far more than PNGs (the XML dialogs, the OGG music and
  effects, password.txt, hintscroll.txt, and per CLAUDE.md password.txt is
  deliberately packed in a second, unencrypted pass). The claim is only true
  if read as "of the image formats, only PNG ever gets in", which is what the
  sentence is arguing for. Translated as written; the wording is misleading as
  it stands.

### level.cpp

- **line 899** [confirmed]
  CONFIRMED. "// set every alpha value to zero" heads two commented-out GL
  calls (glClearColor/glClear at lines 900-901); the code that actually runs
  is the block at 903-911 labelled only "// dirty workaround".
  The comment documents dead code, and the label on the live code says it is a
  workaround without saying what it works around - a reader has to
  reverse-engineer that the glColorMask/zero-blend quad is the alpha clear.
  Translated as it stands.

### level.h

- **line 29** [confirmed]
  CONFIRMED. The comment's figure of 220 level files matches nothing countable
  in the tree.
  Counting XML files whose root element is <Level>: 188 in the whole tree, of
  which 48 are WebBuild build-output copies (build-asan 44, build 2,
  build-test 2), leaving 140 real sources - 86 in Blocks5/misc/3p_levels, 44
  in Blocks5/levels, 8 in Blocks5/data, 2 in Tools/testlevels. The claim the
  number supports still holds: I checked all 188 and every one carries
  width="40" and height="25". Only the count is stale. Translated as written.

- **line 30**
  The comment claims 'all 220 shipped and foreign level files in the tree name
  exactly these values'.
  Counting XML files whose root is <Level>: 140 in the source tree (44 under
  Blocks5/levels, 86 under Blocks5/misc/3p_levels, 8 in Blocks5/data, 2 in
  Tools/testlevels), or 188 if the three WebBuild build-output copies of
  Blocks5/levels are counted too. Neither is 220. Every one of them does carry
  width="40", so the claim the number supports still holds; only the figure
  looks stale. Translated as written.

### main.cpp

- **line 326** [confirmed]
  CONFIRMED. `bool quit = false;` at line 326 is never assigned true anywhere
  in the file, and is then tested at line 456 (`if(quit) return 0;`).
  grep over main.cpp finds exactly two occurrences of `quit`: the declaration
  and the test. Dead variable and dead branch - either a path that set it was
  removed, or the pair should go. Left exactly as found.

### make_text.py

- **line 97** [confirmed]
  CONFIRMED. The comment claims the font.xml offset is used as the first row;
  render() never applies it to the layout.
  read_font returns offset (line 53), render binds it (84) and passes it
  straight back out as the fifth return value (131); out_h is height + pad_y
  from SHADOW_OFFSETS only (98-100) and the main text is blitted at blit(0, 0,
  None, 1.0) (130). Both callers discard it with '_' (153, 180). In the game
  the offset does mean a start row - font.cpp:223 is Vec2i cursor(0, offset) -
  so the baked PNG lacks the vertical lead the game's own text has, and
  shell.html compensates with top_inset instead. Translated as written.

### mobile.js

- **line 277** [confirmed]
  CONFIRMED: five German strings remain in an otherwise English file - lines
  277 and 278, lines 297 and 298, and the wait label 'den Neustart' at line
  303.
  Same reason: string literals may not move in this pass. Every other
  ok()/bad() message in mobile.js is English, so these five are the only
  German a test run prints, which makes them the odd ones out rather than a
  consistent choice.

### pin.cpp

- **line 50** [confirmed]
  CONFIRMED. In Pin::connect, p_output is initialised at line 50 and
  reassigned at line 52, then never read; only p_input is used (line 55). The
  comment above deliberately announces both ("work out which is the input and
  which the output"), matching the German "Ein- und Ausgang bestimmen".
  Dead local. Harmless but every -Wunused build warns on it, and the comment
  promises a use the code does not have. Not touched - it is code, and the
  comment is a faithful rendering of the German.

### platform_stubs.cpp

- **line 39** [confirmed]
  CONFIRMED as a literal inaccuracy, though the conclusion still holds: the
  comment claims each call site "first switches the surface alpha off with
  SDL_SetAlpha(s, 0, 0)".
  At Blocks5/src/texture.cpp:174 SDL_SetAlpha is applied to the destination
  p_rgba only; the blit source is p_parent->p_rgba, whose SDL_SRCALPHA was
  cleared earlier, when the parent was created (texture.cpp:68). In SDL 1.2 it
  is the source flag that governs blending, and that flag is clear on every
  source blitted here (p_surface at texture.cpp:67 and engine.cpp:413,
  p_parent->p_rgba via texture.cpp:68), so the stub's plain row copy is the
  right semantics - but not for the reason the comment states site by site.
  Not fixed.

- **line 266** [confirmed]
  CONFIRMED. The --wrap note says "handles all five call sites" of
  SDL_CreateRGBSurface (German: "alle fuenf Aufrufstellen").
  There are four: Blocks5/src/texture.cpp:66, Blocks5/src/texture.cpp:173,
  Blocks5/src/img_load.cpp:51 and Blocks5/src/engine.cpp:412. The fifth grep
  hit, img_load.cpp:49, is the word SDL_CreateRGBSurfaceFrom inside a comment.
  Translated as written; not fixed.

- **line 38**
  Same comment claims each call site switches the surface alpha off with
  SDL_SetAlpha(s, 0, 0) before blitting.
  At Blocks5/src/texture.cpp:174 SDL_SetAlpha is applied to the destination
  p_rgba only; the blit source is p_parent->p_rgba, whose SDL_SRCALPHA flag
  was cleared earlier when the parent was created (texture.cpp:68). The claim
  holds in effect but not literally per call site, and it is the source flag
  that governs blending in SDL 1.2.

- **line 109**
  The --wrap comment says "alle fuenf Aufrufstellen" (all five call sites) of
  SDL_CreateRGBSurface; translated as written.
  There are four: Blocks5/src/texture.cpp:66, Blocks5/src/texture.cpp:173,
  Blocks5/src/img_load.cpp:51 and Blocks5/src/engine.cpp:412 (the fifth grep
  hit, img_load.cpp:49, is the word SDL_CreateRGBSurfaceFrom inside a
  comment).

### pre.js

- **line 63** [confirmed]
  The keydown-swallowing comment says 'F11 and F12 are left alone: they are
  screenshot and video recording, and neither of those exists in this build.'
  Only the F12 half is still true - screenshots do exist in the web build.
  CONFIRMED from the code. Blocks5/src/main.cpp:568 registers
  $A_CAPTURE_SCREENSHOT on SDLK_F11 unconditionally; the #ifndef
  __EMSCRIPTEN__ block starts only at line 570 and wraps just
  $A_TOGGLE_CAPTURE_VIDEO on SDLK_F12 (main.cpp:576). main.cpp's own comment
  inside that block states it outright: 'Screenshots do exist there; they are
  only downloaded instead of stored.' CLAUDE.md agrees ('The browser gets
  screenshots too', F11 unblocked once GL_BGR and SDL_SaveBMP_RW left the
  path). Consequence: F11 is also the browser's own fullscreen key, so in the
  web build one press both toggles browser fullscreen and downloads a
  screenshot. Note this comment is the author's own English in HEAD, not a
  product of the translation sweep - it is a pre-existing staleness, not a
  translation defect. Reported, not fixed.

### smoke.js

- **line 31** [confirmed]
  CONFIRMED: every operator-facing string in smoke.js is still German - the
  fourteen h.note()/console.log() texts and 'FEHLGESCHLAGEN: ' at line 181 -
  while all the comments around them are English.
  Rule 4 keeps string literals byte-exact and the code-token check would fail
  on any change, so they are untouched by design. They belong to whichever
  pass is allowed to move strings; the sweep's goal is an all-English tree and
  these are what a reader of the test output actually sees.

### streamedsound.cpp

- **line 277** [confirmed]
  CONFIRMED. The four "under Windows" claims describe the non-Emscripten
  branch, but that branch is also what the native Linux build compiles and
  runs.
  Every guard in this file is #ifdef __EMSCRIPTEN__ / #else, never _WIN32, and
  LinuxBuild/build.sh line 58 globs the whole of Blocks5/src/*.cpp minus
  stackwalker.cpp and pch.cpp - so streamedsound.cpp is built with GCC and the
  decoder thread, SDL_CreateThread and SDL_CreateSemaphore are exactly what
  runs on Linux. The distinction the comments are actually drawing is browser
  against native. Affected: streamedsound.cpp:213 ("Under Windows the decoder
  thread calls this every ten milliseconds"), streamedsound.cpp:277 (the
  #ifdef header), streamedsound.h:68 ("the decoder thread under Windows,
  update() in the browser"). Only streamedsound.h:61-62, "Under Windows a real
  kernel object sits behind it (WaitForSingleObject)", is genuinely a
  Windows-specific claim and is correct as it stands. Left as written.

### testhooks.cpp

- **line 172** [confirmed]
  CONFIRMED, first agent's flag. The comment "Where the game sees the cursor
  and what it is pressing on. From outside, a tap that does not arrive
  otherwise looks exactly like a button that does not react." sits above the
  two lines that emit "appActive" (lines 176-177), while the fields it
  describes, "mouseDown" and "cursor", are emitted at lines 206-209, after the
  "paused" and "actionsDown" blocks. "appActive" itself is left without a
  comment.
  CLAUDE.md attributes exactly this reasoning to cursor and mouseDown ("the
  dump reports cursor and mouseDown so that a tap that does not arrive can be
  told apart from a button that does not react") and documents appActive
  separately. Left where it stands and translated as written; moving it is a
  code-adjacent edit for the separate pass.

- **line 173**
  The comment "Where the game sees the cursor and what it is pressing on. From
  outside, a tap that does not arrive otherwise looks exactly like a button
  that does not react." sits directly above the two lines that emit
  "appActive", but it describes "cursor" and "mouseDown", which are emitted
  about thirty lines further down (the mouseDown/cursor block after
  actionsDown).
  The comment has drifted from the code it explains, most likely when
  actionsDown and paused were inserted between them. CLAUDE.md attributes
  exactly this reasoning to cursor and mouseDown ("the dump reports cursor and
  mouseDown so that a tap that does not arrive can be told apart from a button
  that does not react"), and appActive is documented separately there. Left
  where it stands and translated as written.

### u_crt.cpp

- **line 162** [confirmed]
  The comment over CURVE_X/CURVE_Y says the numbers "come from the macros
  below", but CRT_CURVE_X and CRT_CURVE_Y are defined ABOVE, at lines 70-71.
  The neighbouring EDGE_ROWS comment (line 169) says "From the macro above"
  for a macro defined in the same block, so the file contradicts itself.
  Translated as written ("below").

- **line 163** [confirmed]
  PARTLY CONFIRMED. "the shader and the mouse conversion in engine.cpp have to
  compute the same curvature". Engine::warpToSource (engine.cpp:2486) is a
  one-line forwarder to the filter; the second copy of the formula is in this
  file at lines 588 and 601.
  The cursor mapping itself really is in engine.cpp (getCursorPosition at
  3509, setCursorPosition at 3546) and does go through the same numbers, so
  the sentence is not false - but a reader sent there for the formula does not
  find it. Kept as written.

- **line 213** [confirmed]
  The CRAWL_SPEED passage says "the number stands once more below as a macro,
  like the curvature"; CRT_CRAWL_SPEED is defined above, at line 79.
  Same direction error as line 162 - both read as though the macro block once
  sat at the foot of the file. Rendered as written.

- **line 226** [confirmed]
  CONFIRMED. The comments name MASK_AVG and SCAN_AVG (lines 226 and 447); the
  shader declares maskAvg (line 466) and scanAvg (line 451), and neither
  upper-case name exists anywhere in the tree.
  Grepping the names the comments give finds nothing. CLAUDE.md carries the
  same mismatch ("MASK_AVG and scanAvg"). Names left exactly as the German had
  them.

- **line 225**
  Two comments name shader constants MASK_AVG and SCAN_AVG (line 225 "MASK_AVG
  and SCAN_AVG", line 446 "beam/SCAN_AVG"); the shader declares maskAvg (line
  466) and scanAvg (line 451), and no MASK_AVG or SCAN_AVG identifier exists
  anywhere.
  Grepping for the names the comments give finds nothing. CLAUDE.md carries
  the same mismatch ("MASK_AVG and scanAvg"), so the drift is in both places.
  Names left exactly as the German had them.

### util.cpp

- **line 265** [confirmed]
  The comment in isSafeMemberName reads "Quelldateien sind ISO-8859-1: ohne
  die Umdeutung nach unsigned waere jeder Umlaut negativ und fiele durch den
  Steuerzeichentest." - translated as written.
  Two things look off. CLAUDE.md states every source file in Blocks5/src is
  now pure ASCII, so the premise no longer holds; and the encoding of the
  source is beside the point anyway - what actually needs the cast is the
  runtime string 'name', which comes from a zip member or a filename and may
  carry Latin-1 bytes >= 0x80. The cast is right, the stated reason is not the
  one that still applies.

### web_bluescreen.h

- **line 10** [confirmed]
  CONFIRMED, and narrower than the first agent said. "Emscripten build only;
  under Windows SDL_QUIT quits the game as always" names Windows, but the
  non-Emscripten path in engine.cpp:1057-1064 (done = true) is what the native
  Linux build takes as well.
  The accurate word is "outside the browser", covering both native targets.
  Worth adding: the #else stub in web_bluescreen.cpp is dead code in every
  build - the file appears only in WebBuild/build.sh's source list, not in
  Blocks5.vcxproj and not in LinuxBuild/build.sh, and engine.cpp includes
  web_bluescreen.h only under #ifdef __EMSCRIPTEN__. Not fixed.

### zip_data.bat

- **line 20** [confirmed]
  CONFIRMED. Lines 20-21 are still German and user-facing: ECHO " Hinweis:
  Python fehlt, die Kommentare bleiben in den XML-Dateien." and ECHO " Zu
  holen bei https://www.python.org/downloads/". The same two lines are
  zip_data_no_optipng.bat lines 10-11.
  They are ECHO arguments, i.e. code to the code-only extractor, so a
  comments-only pass cannot touch them. If the whole tree is to be English
  these two lines (in both files) need a deliberate code change; note the
  second line's leading spaces align it under the first, so a longer English
  word changes the padding too.

- **line 21**
  Two user-facing ECHO lines are still German: "Hinweis: Python fehlt, die
  Kommentare bleiben in den XML-Dateien." and "Zu holen bei
  https://www.python.org/downloads/". Same two lines in
  zip_data_no_optipng.bat lines 9-10.
  They are ECHO arguments, i.e. code as far as codeonly.py's strip_bat is
  concerned - editing them fails the CODE CHANGED check, so a comments-only
  pass cannot touch them. If the whole tree is to be English, somebody has to
  change these deliberately as a code change.

### barrage.h

- **line 6** [refuted]
  REFUTED. "Blockade" is simply the author's ordinary German noun for this
  object type and it is used consistently at seven sites in HEAD:
  barrage.cpp:24, barrage.h:6, barrage2.cpp:23, barrage2.h:6,
  barrage2panel.h:6 ("Blockadenbodenplatte"), barrageswitch.h:6
  ("Blockadenschalter") and e_barrage.cpp:25. It names no second concept the
  class does not have.
  "barrage" is therefore the right rendering, matching the class names, the
  object type strings ("Barrage", "Barrage2") and the $TT_BARRAGE_A ids - and
  it agrees with the already-translated sibling files (e_barrage.cpp "//
  barrage", barrageswitch.cpp "// switch"). Nothing was harmonised away.

### burst.js

- **line 7**
  The header points at a README.md section by its German title, 'Einen Effekt
  im Bild nachmessen'. That heading lives in WebBuild/test/README.md, which is
  still German and is not in my batch. I rendered the quoted title as
  'Measuring an effect in the picture'.
  A cross-file reference by heading text: whoever translates
  WebBuild/test/README.md must use exactly that English heading, or the
  pointer goes nowhere. Worth grepping for once that README is done.
  (WebBuild/README.md, one level up, is already English and has no such
  section - the reference resolves against test/README.md, so it is not itself
  stale today.)

### pack.sh

- **line 18**
  CLAUDE.md describes pack.sh as using "zip -9 -P in place of 7za a -tzip
  -mx=9 -p", but the script requires 7za and refuses to run without it (line
  45), and its header comment argues at length that Info-ZIP's zip must NOT be
  used because it writes a different encrypted-entry format.
  The two documents contradict each other about the central tool. CLAUDE.md
  appears to describe an earlier version of the script; the comment I
  translated is the one that matches the code. Not fixed - a doc pass should
  reconcile them.


Doubts and observations
-----------------------
Noticed while reading, not confidently a defect. Kept because the reader who
knows the code may recognise one at a glance.

### Build.bat

- **line 9** [confirmed]
  The header says '/sdk:VERSION Windows SDK version for v141 and newer.
  Without it, 10.0, which MSBuild resolves to the newest installed 10.x'. With
  neither /sdk: nor /toolset: given, NEEDSDK stays empty (lines 161-166) and
  Build.bat passes no /p:WindowsTargetPlatformVersion at all.
  The 10.0 then comes from the .vcxproj files, not from Build.bat - which is
  what the block comment at 153-160 and CLAUDE.md both say ('the projects set
  it to 10.0 ... which is what Build.bat used to have to supply'). The
  effective value is the same either way, so nothing misbuilds, but the header
  line reads as a claim about what this script passes and is only true about
  the outcome.

- **line 38** [confirmed]
  Lines 38-60 of the header are pure archaeology: the decade-long v120 pin,
  tinyxml_STL.lib carrying /FAILIFMISMATCH:"_MSC_VER=1800" and LNK2038,
  sdlmain.lib being pre-UCRT and importing __iob_func, PWEncrypt losing its
  gets() call, stdext::hash_map being replaced, libs\msinttypes-r26 going with
  ffmpeg, __STDC_CONSTANT_MACROS and __STDC_LIMIT_MACROS going with the shim.
  Every one of those libraries and files is gone from the tree.
  The project's own rule is 'A comment says what the code does and why, never
  what it used to do', and this sweep's brief is to take that archaeology out.
  Twenty-three of the header's thirty-four prose lines are the account of a
  constraint that no longer exists; what still holds is the two sentences
  around them - v143 and v145 are the tested toolsets, and the /toolset:
  plumbing for v120/v140 is kept but untried (lines 30-31, 58-60). Not cut
  here because the passage is already English and rewriting English prose is
  outside a translation pass; flagged so the pruning pass can decide. Line 466
  ('Open this file in an editor for the toolset notes') is what points readers
  at it, so if the block is trimmed that pointer should be re-checked.

- **line 309** [confirmed]
  The packing failure message prints 'tools\7za.exe is needed for this step.'
  The executable is Blocks5/tools/7za.exe. Same for the header's '/optipng run
  tools\optipng over the PNGs before packing' at line 12; the file is
  Blocks5/tools/optipng.exe.
  Both lines are printed (or read) with the repo root as the current directory
  - the POPD on line 298 has already left Blocks5\ by the time the error at
  306-311 runs - so the path as written does not resolve. It is correct from
  inside the zip_*.bat scripts, which is presumably where the wording came
  from, and CLAUDE.md uses the same short form but explicitly in a 'from the
  Blocks5 directory' context. Here that context is absent, so the one line a
  user sees when packing fails points at a file that is not where it says.

- **no line given**
  Build.bat, zip_skins.bat and zip_skins_no_optipng.bat contained no German at
  all and were not modified.
  All three were already fully English - comments, ECHO output, usage banner
  and error messages alike. git diff is empty for them, so their CRLF endings
  are trivially intact.

- **line 478**
  The ':onlytoolset' comment says 'Keep SHOWTS only if it is a plain vNNN',
  but the guard at 483-484 accepts anything that starts with 'v' and is at
  most five characters - one more than a plain vNNN.
  '%SHOWTS:~5%' is empty for any string of five characters or fewer, so a
  five-character value beginning with v survives the filter and reaches the
  banner. It still rejects everything the comment names as the reason for the
  check - an MSB1001 error line, v143_xp, ClangCL - so this is slack rather
  than a defect; the comment simply describes a tighter test than the code
  performs.

### README.md

- **line 99** [confirmed]
  burst.js's header points at a README section by title. The heading at
  WebBuild/test/README.md:99 is still German: "## Einen Effekt im Bild
  nachmessen". burst.js now says README.md under "Measuring an effect in the
  picture".
  Confirms the first agent's flag. Whoever translates WebBuild/test/README.md
  must give that heading exactly this English, or the pointer resolves to
  nothing. The section's own arithmetic ("Spiel-Pixel" -> "Foto-Pixel") is
  what burst.js calls "the conversion from game to screenshot coordinates", so
  that half of the reference is accurate today.

- **no line given**
  Still entirely German and not in anybody's modified set (git status does not
  list it). It documents both of my files: lines 78-97 for make_ico.py ('baut
  das Programmsymbol fuer Windows; die .ico ist eingecheckt, weil der
  Windows-Build kein Python laufen laesst') and lines 110-128 for
  encode_sounds.py, whose three usage lines still carry German glosses ('alle
  veralteten' / 'nur diese' / 'alle, ob veraltet oder nicht') that are the
  twins of encode_sounds.py:7-9, now English.
  Outside my two files so I did not touch it, but it leaves the tooling docs
  half-translated and the two halves of the same usage table in different
  languages. Worth assigning.

### arrow.cpp

- **line 111** [confirmed]
  CONFIRMED: Arrow::turn() does a bare `dir++`, while changeInEditor() at line
  102 does `dir++; dir %= 4;`.
  turn() is called for every arrow from Level::turnArrows() (level.cpp:1642)
  each time the magnet fires, so dir grows without bound, and saveAttributes
  writes that unwrapped number into the level XML on a mid-game save. Nothing
  misbehaves today - onUpdate and allowMovement both take `dir % 4` and `90.0
  * shownDir` is equivalent mod 360 - but the asymmetry between the two
  increment sites looks unintended and the saved attribute is no longer the
  0..3 the editor writes.

### as_wav.cpp

- **line 141** [confirmed]
  REFUTED as a defect in this file, CONFIRMED as behaviour: the end-of-stream
  branch sets error = 0, clearing an error a previous read may have set.
  AS_Ogg::read (as_ogg.cpp:61-65) has the identical pair in its end-of-file
  branch: eos = true; error = 0;. So the convention is shared by both
  AudioStream implementations - reaching the end is not an error state -
  rather than an oversight in AS_Wav. The mechanical observation stands
  (getError() can go 1 -> 0 once the stream ends), but it is the class's
  design, not a translation issue. The comment "No! Adjust!" renders the
  German "Nein! Anpassen!" exactly and says nothing about the error state
  either way.

### audiocapture.cpp

- **line 63** [confirmed]
  AudioRing::overflowed is written in four places and read nowhere in the tree
  It is initialised in the constructor and in allocate(), set true in push()
  when the oldest samples are dropped, and cleared at the start of both
  threadProc capture runs (lines 511 and 816) - but a grep over Blocks5/src
  finds no reader at all, in this file or in videorecorder.cpp, which is the
  only consumer of AudioCapture. Either the overflow was meant to be reported
  somewhere (a log line, or a warning in the recorder) and that half is
  missing, or the member is dead. No comment mentions it, so the translation
  could not say either way.

- **line 115** [confirmed]
  CONFIRMED: clearRing() locks p_mutex with no null check, unlike the other
  three AudioRing entry points
  push() bails on !p_ring (123), available() on (!opened || !p_mutex) (192),
  read() on (!opened || !p_ring) (202); clearRing() calls
  SDL_LockMutex(p_mutex) straight away. Unreachable now, since both open()
  paths return before SDL_CreateThread when allocate() fails and clearRing()
  is only called from inside threadProc (506, 813). Still the one unguarded
  door in the struct.

- **line 941** [confirmed]
  CONFIRMED: AudioCapture::start() guards on opened under Windows but not
  under Linux
  Line 654 is "if(p_impl->opened) p_impl->capturing = true;", line 941 is the
  bare "p_impl->capturing = true;" (the browser stub at 1003 is empty). The
  two halves are otherwise deliberately symmetric. Harmless today - with no
  thread running, capturing has no reader, and getNumSamplesReady() returns 0
  because available() checks opened at line 192 - but nothing in the file
  claims the asymmetry is intended.

- **line 113**
  clearRing() locks p_mutex with no null check, unlike every other AudioRing
  entry point
  push() bails on !p_ring, available() on (!opened || !p_mutex) and read() on
  (!opened || !p_ring), but clearRing() calls SDL_LockMutex(p_mutex) straight
  away. It is not reachable now, because both open() paths return before
  SDL_CreateThread when allocate() fails, and clearRing() is only called from
  inside threadProc. It is still the one unguarded door in a struct whose
  other three are guarded.

- **line 939**
  AudioCapture::start() guards on opened in the Windows build but not in the
  Linux one
  Line 652 (Windows) is "if(p_impl->opened) p_impl->capturing = true;"; line
  939 (Linux) is the bare "p_impl->capturing = true;". The two files are
  otherwise deliberately symmetric. It is harmless today - with no thread
  running, capturing has no reader, and getNumSamplesReady() returns 0 because
  available() checks opened - but the asymmetry looks unintended rather than
  reasoned, and there is no comment claiming it is.

### barrage2.cpp

- **line 23** [confirmed]
  CONFIRMED. barrage.cpp and barrage2.cpp are near-identical: same members
  (up, shownState, color), same updateSprites shape, identical onUpdate,
  changeInEditor, saveAttributes, getColor and updateProperties. They differ
  only in the three sprite coordinates, in the type strings checked inside
  change() ("Barrage" against "Barrage2"), and in change() being bool change()
  against int change(bool up) with the extra early-out and the -1/0/1 return.
  Not a translation defect. It does mean the two "// If an object is standing
  there right now, it cannot be done!" comments are a matched pair maintained
  by hand; both are byte-identical after this sweep (barrage.cpp:71,
  barrage2.cpp:72, tab-indented).

### barrage2.h

- **line 6** [confirmed]
  CONFIRMED, and not introduced by the translation. The file header is
  byte-identical to barrage.h's, so neither names which of the two barrage
  types it describes - but that was already true of the German: HEAD has "/***
  Klasse fuer eine Blockade ***/" in both files, byte for byte.
  The two classes differ in how they are switched (Barrage::change() toggles,
  Barrage2::change(bool up) is told the state) and the game distinguishes them
  everywhere else ($TT_BARRAGE_A against $TT_BARRAGE_B). A convention pass may
  want to name the type in each header; a translation pass cannot, since it
  would be adding a fact the source does not carry.

### build.sh

- **line 37** [confirmed]
  CONFIRMED. Every user-visible string the script prints is still German: line
  37 "sdl-config nicht gefunden - libsdl1.2-dev fehlt.", line 99
  "FEHLGESCHLAGEN: $1", line 117 "### UEBERSETZEN FEHLGESCHLAGEN ###", line
  118 "### $total Uebersetzungseinheiten in Ordnung ###", line 124 "### LINKEN
  FEHLGESCHLAGEN ###", line 130 "(Achtung: data.zip fehlt - Blocks5/pack.sh
  baut es)".
  Rule 4 forbids touching a string literal, so they are untouched - but a tree
  that is to be English needs a separate pass for this script's own output.

- **line 52** [confirmed]
  findgerman.py's word list contains the English words so, was, also, die, man
  and hat, so every English comment line using one of them is reported as
  German.
  CONFIRMED by testing findgerman.DE against plain English: 'so both builds
  run the same parser', 'it was hidden', 'the man page', 'a hat trick', 'also
  see below' and 'the die is cast' all match. That is why five pre-existing
  English comment lines (build.sh 52, 129, 147, 150 and build_asan.sh 44) had
  their 'so' reworded away with no change of meaning. Unless those six get an
  English-counterpart guard, every later batch will keep bending its English
  around them.

- **line 231** [confirmed]
  echo "### PWA: manifest.json, 4 Symbole, sw.js (cache blocks5-$version) ###"
  - a German word in build output, and the Python heredoc's error message at
  line 187, 'kein %%LOADTEXT%% in der Seite'.
  CONFIRMED, untouched under the string-literal rule. Both are text a reader
  of the build output sees, and the tree is going English; changing either
  would move code tokens and fail the batch, so it belongs to a later pass.

- **no line given**
  Pre-existing cosmetic quirk, not a regression: line 187's message is a
  %-format, so '%%LOADTEXT%%' renders as '%LOADTEXT%' - the reader is told the
  token is spelled with one percent sign on each side rather than two.
  The German original had exactly the same shape ('kein %%LOADTEXT%% in der
  Seite'), so behaviour is unchanged and this is not the translation's doing.
  Reporting rather than repairing, per the glossary's rule. Fixing it would
  mean doubling the signs again ('%%%%LOADTEXT%%%%'), which is a readability
  judgement for the author.

- **line 82**
  "switching between the two kinds of build would recompile all 160 units"
  (German: "jede der 160 Einheiten").
  The define only reaches the C++ compile, and ls Blocks5/src/*.cpp minus the
  three excluded is 120, plus 8 WebBuild sources and 4 tinyxml sources = 132
  C++ units; counting the 38 C sources as well gives 170. 160 matches neither.
  Translated as written.

- **line 235**
  echo "### PWA: manifest.json, 4 Symbole, sw.js (cache blocks5-$version) ###"
  - German word inside a shipped string literal.
  Untouched under the string-literal rule, but it is build output a reader
  sees, and the tree is going English. Same for the Python heredoc's error
  message at line 191, 'kein %%LOADTEXT%% in der Seite'.

### build_asan.sh

- **line 42** [confirmed]
  The libvorbis file list here contains misc.c; build.sh's otherwise identical
  list does not.
  CONFIRMED. libs/libvorbis-1.3.4/PROVENANCE.txt line 35: "misc.c - a
  debugging allocator, not part of the library", and lines 37-41 record that
  it was in the file lists until the first v143 build, that upstream's
  lib/Makefile.am lists misc.h but never misc.c, and that it defines a pthread
  mutex at file scope. The file does open with #include <pthread.h> and a
  PTHREAD_MUTEX_INITIALIZER at file scope. The asan script never got the fix.

### cf_colorblend.cpp

- **line 40** [confirmed]
  The colour quad's alpha ramp is asymmetric for any timing != 0.5: both sides
  of the peak use the slope 1/(1 - timing), so at t = 0 the alpha is 1 -
  timing/(1 - timing) rather than 0. With the timing = 0.1 that
  lightpanel.cpp:53/59 and lightswitch.cpp:47/52 pass, the quad appears at
  0.89 opacity in the first frame of the crossfade instead of fading in from
  nothing - the old image is barely visible at all before it is covered. The
  fade-out side (timing..1) is correct and reaches 0 at t = 1.
  It may well be deliberate for a light switch (an instant flash to black,
  then a slow fade up into the new scene), and at the default timing = 0.5 the
  two slopes coincide, so nothing shows there. Reported rather than touched:
  the rules say translate only, and there is no comment on the formula to
  render either way.

### cf_rewind.cpp

- **line 46** [confirmed]
  'Wie viele es sind, haengt am Tempo des Bandes.' (How many there are depends
  on the speed of the tape) sits directly over const int NOISE_BARS = 5, which
  is a fixed count. Only the per-bar speed varies (0.6 + 0.5 * i) and eased
  scales their travel, not their number.
  Read as a statement about a real recorder it is true; read as a comment on
  the constant under it, it describes something the code does not do. A reader
  looking for where the count is derived from the speed will not find it.

- **line 131** [refuted]
  REFUTED. The "texture coordinates are in pixels" comment sits inside
  drawStrip(), where it is exactly true: Crossfade::setupTexCoords() loads a
  texture matrix scaling by 1/screenPow2Size, and drawStrip() passes shift and
  sourceY unscaled. drawSnow() is only ever called between render()'s own
  glLoadIdentity() on the texture matrix, and the "--- Noise ---" comment
  states that in place ("A texture matrix of its own: the noise image is
  sampled in 0..1, not in pixels of the screen").
  The two conventions are each documented where they apply; there is no
  contradiction and nothing to merge. Left alone.

### cf_star.cpp

- **line 6** [confirmed]
  CONFIRMED. The header says a triangle fan from the centre covers the star
  (German: Dreiecksfaecher), but renderStar() emits GL_TRIANGLES and repeats
  the centre vertex in each of the 10 triangles; GL_TRIANGLE_FAN does not
  appear.
  The coverage is geometrically identical, so nothing is broken - but the word
  names a GL primitive the code deliberately does not use, and elsewhere in
  the tree (cf_slices, hint.cpp) the choice of primitive is load-bearing for
  WebGL, so a reader may take it literally.

### cf_zoom.cpp

- **line 91** [confirmed]
  First agent's suspicion 3 is CONFIRMED as a fact, with a smaller effect than
  it sounds. The colour quad's alpha reads t after the 25-iteration loop has
  overwritten it. ts starts at t - 0.25 and the loop's last assignment (i =
  24) is t = clamp(t - 0.01, 0.0, 1.0), so the white flash is driven by the
  trailing slice, exactly one 0.01 step behind - and behind the already
  remapped half-parameter (2t before the midpoint, 2 - 2t after), not behind
  the raw transition parameter.
  The visible difference is 0.01 in a squared alpha, i.e. nothing; but the
  reuse of t as both the transition parameter and the per-slice loop variable
  hides the substitution, and no comment mentions it. If the loop count or the
  0.01 step ever changes, the flash silently follows.

- **line 88**
  The colour quad's alpha uses t after the 25-iteration loop has overwritten t
  with clamp(ts, 0.0, 1.0) of the last slice.
  The parameter t that the two branches at the top computed is gone by the
  time "// draw the colour quad" runs, so the white flash is driven by the
  trailing slice's clamped value rather than by the transition parameter. It
  may well be deliberate, but the comment says nothing about it and the reuse
  of the variable hides the difference.

### checkbatch.py

- **line 51** [confirmed]
  checkbatch.py cannot print ALL CLEAN for WebBuild/pre.js, and the file is
  not at fault. It reports '13 German comment line(s) left'; all 13 are false
  positives on the English words 'so' (11 hits) and 'also' (2 hits), which sit
  in findgerman.py's DE regex at line 15.
  CONFIRMED, and this is the first agent's second suspicion. The 13 flagged
  lines are 5, 39, 65, 73, 79, 93, 98, 114, 125, 129, 158, 179, 182 - e.g.
  line 5 'Back it with IDBFS so saves, progress and custom levels survive a
  reload.' and line 182 'which also catches the Fullscreen API without a
  second code path.' Re-running findgerman.comment_lines over the file with
  the English-ambiguous entries (so also was die der den dem des man hat alle
  um wo bei es sie sein ob wie ist) removed from DE reports zero hits, which
  proves no German remains. I did not edit findgerman.py (outside my file
  list) and I deliberately did not reword the author's English to dodge the
  regex: those 13 lines are unmodified HEAD content that this sweep never
  touched, so rewriting them would be pure churn on committed prose. The other
  three checkbatch conditions all pass for this file: code identical to HEAD
  (empty diff), 0 non-ASCII bytes, no CRLF. Suggested fix for the checker:
  drop 'so', 'also', 'was', 'die', 'der', 'den', 'dem', 'des', 'man', 'hat',
  'alle', 'um', 'bei', 'es', 'sie', 'sein', 'ob', 'wie', 'ist' from DE, or
  require two distinct hits on a line.

### diamondmachine.cpp

- **line 22** [confirmed]
  CONFIRMED, and it is in the German original: in the TIMETABLE diagram the
  inward "emitted" bar closes on the 80 column, while SPARK_IN_END is 92 and
  the prose four lines below says 92.
  On the German's own columns the inward bar's closing pipe is at column 53,
  which is where the "80" label sits (columns 52-53); 92 would fall around
  column 58. The picture and the text disagree by twelve ticks. I put the
  translated bars back on exactly the German columns rather than moving the
  bar to 92 - that is a correction, not a translation.

- **line 272** [confirmed]
  CONFIRMED: SPARK_IN_MIN_LIFE (8) can never bind as long as SPARK_IN_END
  stays 92.
  inRamp() returns 0.0 for counter > SPARK_IN_END, and spawnCount(0.0) is 0,
  so no inward spark is emitted past 92. The smallest value max() ever sees on
  the right is CONVERSION_TICKS - 92 + 1 = 9. The comment over the constant
  ("The last ones still need some distance") reads as though the floor does
  work; it is purely defensive.

- **line 463** [confirmed]
  CONFIRMED as loose: "The last quarter belongs to the collecting alone"
  (German "Das letzte Viertel") does not match the constants - it is the last
  fifth.
  smokeRate() returns 0.0 from counter >= SPARK_OUT_END = 53, and an outward
  spark emitted at 53 dies at 53 + OUT_LIFE = 80, so nothing but inward sparks
  is in the air from 80 to 100 (the last fifth) and no smoke is emitted for
  the last 47 ticks. Rendered as written.

- **line 271**
  SPARK_IN_MIN_LIFE (8) can never bind: inRamp() stops emitting past
  SPARK_IN_END = 92, so the smallest life max() ever sees is 100 - 92 + 1 = 9.
  The comment over the constant says the last sparks still need some distance,
  which reads as though the floor does work. It is only ever defensive as long
  as SPARK_IN_END stays at 92.

### e_flipflop.cpp

- **line 15** [confirmed]
  CONFIRMED as described, and harmless. The constructor's switch over subType
  creates pins only for 0, 1 and 2 and has no default, so a subType outside
  that range yields a part with no pins at all, output pin 10 included.
  Electronics::getValue/getOldValue/setValue all go through getPinByID, which
  returns 0 for an unknown id, and each caller guards on it (getValue returns
  -1, setValue does nothing), so nothing crashes. areAllInputsConnected() and
  isAnyInputUndefined() iterate an empty vector and answer true/false
  respectively. The part is simply inert and unconnectable, which no comment
  says. Same unchecked-subType source as the e_gate.cpp finding.

- **line 94** [confirmed]
  CONFIRMED. The comment on the RS case says "ungetaktet" (rendered
  "unclocked"), while the part's own tooltip $TT_FLIP_FLOP_RS reads "RS
  flip-flop, level-triggered" / "RS-Flipflop, pegelgesteuert" in
  data/languages.txt.
  Both are true of an RS latch, but they are two different words for the same
  part inside one source tree, so a reader comparing the comment with the
  tooltip has to work out that they agree. Rendered as written ("unclocked"),
  deliberately not harmonised with the tooltip. Note that the neighbouring
  "flankengesteuert" -> "edge-triggered" does match the shipped English of
  $TT_FLIP_FLOP_D and $TT_FLIP_FLOP_JK exactly.

- **line 128**
  The JK case (case 2) in E_FlipFlop::doLogic has no closing `break;`.
  Harmless today because it is the last case of the switch, but it is the one
  case that differs from the other two, and any case added after it would
  receive the JK block's fall-through. Purely a latent trap; no comment claims
  otherwise.

### e_lightbarrierreceiver.cpp

- **line 43** [confirmed]
  CONFIRMED: the "value" extended XML attribute of E_LightBarrierReceiver is
  dead state - saved and loaded, but never readable.
  Level::load's non-editor path (level.cpp:413-429) calls frameBegin() on
  every object before it updates the LightBarrierSenders, and
  E_LightBarrierReceiver::frameBegin() sets value = 0, so the loaded number is
  gone before anything reads it; the sender then sets it again through
  reflectLaser() in the same pass. On the editor path that whole block is
  skipped, but updateSprites() reads only dir and never value, so nothing is
  visible there either. The first agent's parallel to E_Multiplexer only half
  holds: E_Multiplexer::updateSprites() does read value (value == -1 ? 0 :
  (value == 0 ? 32 : 64)) and the editor never runs Electronics::updateAll, so
  there the saved value really does pick the sprite the editor shows. Not
  fixed; no comment claims otherwise.

### e_pulsepanel.cpp

- **line 54** [confirmed]
  CONFIRMED as a fact, but it looks deliberate rather than a copy-paste slip.
  Three parts play e_valueswitch_on.ogg / e_valueswitch_off.ogg: E_ValueSwitch
  (e_valueswitch.cpp:62), E_PulseSwitch (e_pulseswitch.cpp:84) and
  E_PulsePanel (e_pulsepanel.cpp:54). All three are the value-switch family
  and all three set the same output pin 10 to a 0/1 value; both files are
  preloaded once in gs_loading.cpp:271-272, so verify.py's preload check would
  have caught an unintended name.
  One shared click sound across the family is a reasonable choice; nothing in
  the code says so, so a single line of comment at one of the three sites
  would earn its place. Not a defect, and no comment existed to translate.

- **line 40** [refuted]
  The German line "// Befindet sich ein Objekt auf dem Panel, das vorher noch
  nicht da war?" is byte-identical in panel.cpp:18, which belongs to another
  agent's batch. The glossary says a line that repeats verbatim must come out
  identical everywhere, but I cannot coordinate the two.
  The natural English rendering, "...that was not there before", is rejected
  by checkbatch.py: "was" is in findgerman.py's German function-word list, so
  the line is reported as untranslated. I used "that had not been there
  before" to get past it. Whoever translates panel.cpp, fire.cpp:57 and
  teleporter.cpp:62 will hit the same trap and may pick a different escape,
  leaving four near-copies of one sentence. Worth a single agreed wording in
  the review pass.

### electricitypanel.cpp

- **line 48** [confirmed]
  CONFIRMED, and at two call sites rather than one: electricitypanel.cpp:48
  and :53 both play "electricityswitch.ogg" with the default priority (0),
  while ElectricitySwitch::onTouchedByPlayer (electricityswitch.cpp:37) plays
  the same file with priority 100.
  Sound::getFreeSource (sound.cpp:160-176) steals the audio source from the
  lowest-priority non-looping instance when alGenSources fails, so the panel's
  click is the first to be cut off under source pressure while the switch's
  identical click is protected. Priority 100 is the tree's convention for
  one-shot event sounds (about thirty call sites), including the structurally
  identical sibling lightpanel.cpp:52/58. The e_* electronics parts do
  consistently pass no priority (e_valueswitch.cpp:63, e_pulsepanel.cpp:54),
  so the outlier is specifically the two Panel-derived sites. No comment
  anywhere states which is intended.

### encode_sounds.py

- **line 54** [confirmed]
  First agent's flag #5 confirmed: user-facing output is still German in all
  three Tools files - encode_sounds.py lines 54, 62, 65-66; make_ico.py lines
  142, 172, 174; syntax.sh lines 27, 76, 78.
  String literals, out of scope for a comment-only pass, but they are what the
  author reads when a tool runs. If the tree is to be English they need their
  own pass, together with the make_ico docstrings above.

- **no line given**
  Pre-existing arithmetic bug, NOT introduced by the translation: line 65
  prints a negative count when a named .wav does not exist. `done` is
  incremented only after the existence check (line 59), but `bad` is
  incremented before the `continue` (line 54), so `done - bad` goes negative.
  Measured: `python3 Tools/encode_sounds.py nosuchsound` prints '-1 of 1
  file(s) encoded, 1 error(s)'. The German printed the same '-1'. Left alone -
  a translation pass renders the claim, it does not repair it.
  Reported rather than fixed, per the glossary's rule on source defects found
  during translation. A one-line fix would be to increment `done` before the
  existence check, or to print `len(wavs) - bad`.

- **line 60**
  User-facing output in all three Tools files is still German: '%s fehlt',
  '%s: kein Bitratenwert ging durch', '%d von %d Datei(en) kodiert%s', ', %d
  Fehler' here; '%s ist %dx%d - fuer ein Symbol wird ein Quadrat gebraucht',
  '%s: %d Bilder aus %dx%d-Kunst, %d Bytes' and '%dx + %d Rand' in
  make_ico.py; "i686-w64-mingw32-g++ nicht gefunden - kein mingw-w64
  installiert.", "$n Quelldateien uebersetzen fehlerfrei" and "### FEHLER ###"
  in syntax.sh.
  These are string literals, which rule 4 puts out of scope for this pass, but
  they are what the author actually reads when a tool runs - if the tree is to
  be English, they need their own pass.

### enemy.cpp

- **line 277** [confirmed]
  `tryToMove(intToDir(random(0, 4)))` draws five values for four directions.
  `random(int, int)` in util.cpp is `min + mt.randInt(max - min)` and
  MTRand::randInt(n) is inclusive of n, so random(0, 4) returns 0..4.
  Object::intToDir switches on `dir % 4`, so 4 folds onto 0 and the devil's
  face wanders upward twice as often as in any other direction. Every other
  call in this file uses the inclusive form correctly (random(0, 1) with
  s==0/s==1), which is what makes this one look unintended. It is plain
  gameplay behaviour, not a crash, and no comment claims otherwise - so
  nothing was changed.

### engine.cpp

- **line 764** [confirmed]
  CONFIRMED. Engine::exit() frees only `SDL_FreeCursor(SDL_GetCursor())`,
  while setupCursor() builds both p_cursor1x (line 4210) and p_cursor2x (line
  4211) and updateCursorSize() (line 4273) switches between them.
  Whichever of the two is not current at exit is never freed - a process-exit
  leak natively, and exit() never runs at all in the browser, but the
  asymmetry against the two members the constructor zeroes at lines 86-87
  looks unintended.

- **line 782** [confirmed]
  CONFIRMED, and it happens twice: the comment over the browser loop-state
  namespace (line 782) and the saveTimePlayed comment (line 1610) both name
  `emscripten_set_main_loop`, while the only call is
  `emscripten_set_main_loop_arg(emMainLoopIteration, this, 0, 1)` at line 857.
  Shorthand rather than an error, but the trailing `1` of that call is
  load-bearing (CLAUDE.md: "The flag is load-bearing and must not be tidied
  away"), and naming the argument-less function sends a reader looking for the
  wrong call. Translated as written.

- **line 789** [confirmed]
  CONFIRMED. `firstEventRecorded` is declared inside `#ifdef RECORD` in the
  native branch of mainLoop() (line 841), but unconditionally in the
  browser-side anonymous namespace at line 789.
  In a web build without RECORD it is a namespace-scope variable nothing ever
  reads. The comment beside the native declaration explains why the browser
  copy lives up there, so the placement is deliberate; it is the missing
  `#ifdef` around the browser copy that looks accidental.

- **line 1582** [confirmed]
  The STRESS_TEST block declares 'static int wurst = 0;' - a German joke
  identifier surviving into a tree that is being made all-English.
  It is code, not a comment, so it is out of scope for a translation pass, but
  it is the kind of thing the sweep is meant to surface. It is inside #ifdef
  STRESS_TEST and never compiled in a shipped build. Reported, not renamed.

- **line 761**
  Engine::exit() frees only the cursor SDL currently has set
  (`SDL_FreeCursor(SDL_GetCursor())`), while setupCursor()/createCursor()
  build two, p_cursor1x and p_cursor2x, and updateCursorSize() switches
  between them.
  Whichever of the two is not current at exit is never freed. Only a
  process-exit leak natively, and in the browser exit() never runs at all, but
  the asymmetry against the two members the constructor initialises to 0 looks
  unintended.

- **line 781**
  The comment above the browser loop-state namespace names
  `emscripten_set_main_loop`, but the call at the foot of Engine::mainLoop()
  is `emscripten_set_main_loop_arg(emMainLoopIteration, this, 0, 1)`.
  Shorthand rather than an error, but the trailing `1` of that call is
  load-bearing (CLAUDE.md: 'The flag is load-bearing and must not be tidied
  away'), and naming the argument-less function is exactly the kind of drift
  that makes a reader look for the wrong call. Translated as written.

- **line 840**
  `firstEventRecorded` is declared inside `#ifdef RECORD` in the native branch
  of mainLoop(), but unconditionally in the browser-side anonymous namespace
  at the top of the file.
  In a web build without RECORD it is a namespace-scope variable nothing ever
  reads. The comment beside the native declaration explains why the browser
  copy lives up there, so the asymmetry is deliberate as far as placement goes
  - it is the missing `#ifdef` around the browser copy that looks accidental.

- **line 2596**
  The comment over screenshot() says the shot is always the internal 640x480
  frame, while the line under it reads 'useFrameBuffer ? screenSize :
  displaySize'.
  The claim holds only because handleResize pins the window to 640x480 when
  there is no framebuffer object, so displaySize equals screenSize on that
  path - an invariant two functions away. The 'always' is true today but rests
  on fixWindowSize/handleResize continuing to clamp. Translated as written;
  flagged in case the author wants the comment to name the dependency.

- **line 2598** [refuted]
  REFUTED. The comment over screenshot() says the shot is always the internal
  640x480 frame, while the line under it reads `const Vec2i
  shotSize(useFrameBuffer ? screenSize : displaySize);`.
  No contradiction: without a framebuffer object handleResize() forces `width
  = screenSize.x; height = screenSize.y` (line 2452ff) and fixWindowSize()
  nails the window to that size, so displaySize is 640x480 on exactly that
  path. The claim holds on both branches.

- **line 3531**
  setCursorPosition() guards its warp with if(screenSize.x > 0 && screenSize.y
  > 0) while getCursorPosition() guards the same round trip with if(w > 0 && h
  > 0), and setCursorPosition ignores the w/h it just computed in that test.
  The two comments claim to be exact inverses of one another, and CLAUDE.md
  says the same. With a degenerate present rect the two paths would disagree
  rather than both fall through to the unmapped position. No division is
  involved, so nothing crashes; reporting it as an asymmetry the comment does
  not lead a reader to expect.

### eye.cpp

- **line 64** [confirmed]
  CONFIRMED. instancePreset("Enemy", ...) is static_cast to Enemy* and
  dereferenced at once with p_enemy->setInvisibility(50), with no null check.
  Presets::instancePreset starts with Object* p_theObject = 0
  (presets.cpp:161) and falls through the if/else chain returning 0 for a name
  it does not know; several branches also return 0 when p_element is absent.
  Harmless while "Enemy" stays in the chain, but the eye is the one place that
  spawns an object from a name at runtime, and no other failure here is even
  logged.

### file_archived.cpp

- **line 371** [confirmed]
  CONFIRMED. Neither fopen() result is checked: p_in is used at
  fread(&signature, 1, 4, p_in) in the scan loop at line 377 and p_out at the
  first fwrite, both unguarded.
  A missing archive or an unwritable directory gives a null FILE*. Every other
  failure path in this file logs with printfLog and returns an error code;
  this one does not exist at all.

### file_real.cpp

- **line 85** [confirmed]
  The FM_DELETE branch sets error = 9 where every other failure path in this
  file sets error = 1.
  Nothing in the file explains the 9 and the comment above it does not mention
  it; callers only test getError() for truth, so the distinction is invisible.
  Either a deliberate code the comment forgot to name, or a stray value.

- **line 114** [confirmed]
  CONFIRMED. tell() calls ftell(p_handle) with no null check, and getSize()
  routes every non-FM_READ mode through it; File_Real::size is left
  uninitialised in the FM_TEST, FM_LIST and FM_DELETE branches (it is a member
  of File_Real, not of File, and the base constructor does not touch it).
  For a file opened FM_LIST or FM_DELETE p_handle is 0, so getSize() would
  dereference null. No current caller asks a listing or deletion handle for
  its size, so this is latent.

- **line 112**
  tell() calls ftell(p_handle) without checking p_handle, and getSize() routes
  every non-FM_READ mode through it; size itself is left uninitialised in the
  FM_TEST, FM_LIST and FM_DELETE branches.
  For a file opened FM_LIST or FM_DELETE p_handle is 0, so getSize() would
  dereference null. No current caller does it, so this is latent rather than
  live.

### filesystem.cpp

- **line 144** [confirmed]
  CONFIRMED as written, and worse than reported: SHGetFolderPathA is given
  char path[256] where the documented contract is a buffer of at least
  MAX_PATH (260) characters, and its return value is not checked, so on
  failure path is an uninitialised stack buffer that is then converted to
  std::string.
  A redirected Documents folder on a long path overflows the buffer; a failing
  call reads uninitialised stack. Never hit in practice because
  CSIDL_MYDOCUMENTS is short and the call succeeds, but neither is guaranteed.

- **line 414** [confirmed]
  CONFIRMED. evalRelativePath() uses i < path.length() - 2 and i <
  path.length() - 1, the exact construct the comment above convertPath (line
  345) argues against.
  On a path of length 1 or 2 the size_t subtraction wraps and the guard is
  always true; the loop body then calls substr(i, 3) on a shorter string.
  Harmless today because substr with pos <= size() does not throw and the
  truncated result cannot equal "../" or "./", so it is an inconsistency with
  the neighbouring reasoning rather than a live fault. (Length 0 never enters
  the loop.)

### font.cpp

- **line 133** [confirmed]
  In the Emscripten build the entire display-list cache is bookkeeping with
  nothing behind it: glGenLists in the constructor, the free-slot scan, the
  LRU scan over stringCache and the erase all run, but the list is only ever
  recorded (glNewList) and replayed (glCallList) inside #ifndef
  __EMSCRIPTEN__, where the web build calls renderTextPure again instead.
  So every string still costs a hash lookup, and once 32 strings are live, a
  full linear scan of the cache per draw, to pick a slot that is then never
  used. Reported rather than changed - the comments describe the mechanism
  accurately, and removing the dead half is a code decision.

- **line 137** [confirmed]
  listFree is a uint bitmask of numLists = 32 slots, but the free-slot scan
  and the clear beside it shift a signed literal: 1 << listIndex, and ~(1 <<
  listIndex) on the next line. At listIndex 31 that shifts into the sign bit
  of an int.
  Undefined behaviour before C++20 and the one slot the scan can legitimately
  reach, since stringCache.size() < numLists only guarantees that some bit of
  32 is free - it does not exclude bit 31. Works on every compiler this tree
  is built with, which is why it has never shown. Not touched: this is a code
  change, not a comment.

### game.xml

- **line 36** [confirmed]
  CONFIRMED (first agent's flag). The comment accounts for the three pixels
  the cell sticks past the top edge (y=-3) but not for the two it sticks past
  the right edge: x=562 plus w=80 is 642 against a 640-wide picture. Same
  comment and same button in leveleditor.xml line 158.
  True but incomplete as written, so it was rendered as written. Harmless in
  effect: with inset="8" the drawn disc spans x 570..634 and y 5..69, entirely
  inside the picture, which is what "there is nothing but border up there
  anyway" is claiming.

### gl_compat.cpp

- **line 38** [confirmed]
  CONFIRMED as an incomplete claim, but harmless today. The comment says the
  remembered GL_UNPACK_ROW_LENGTH means a strided surface no longer arrives
  skewed; the repack covers only one pixel format.
  glTexImage2D repacks only when format == GL_RGBA && type == GL_UNSIGNED_BYTE
  (line 56); any other format with unpackRowLength != width falls through to
  emscripten_glTexImage2D unmodified and would still arrive skewed. In
  practice nothing hits that: the only two sites that set the parameter,
  texture.cpp:81 and texture.cpp:188, both upload GL_RGBA/GL_UNSIGNED_BYTE
  (82, 189) and reset it to 0 on the next line. A future non-RGBA upload
  between the two glPixelStorei calls would be silently skewed.

### gs_campaigneditor.cpp

- **line 192** [confirmed]
  CONFIRMED: the `else if(!confirmed)` in the Load branch can never be false.
  It is the else of `if(!editor.wasChanged() || confirmed)` at line 157, so
  reaching it already implies confirmed == false. The New (line 120) and Quit
  (line 274) branches raise the same message box with a plain `else`, which is
  what this one effectively is. Harmless, but the condition reads as if it
  guarded something. No comment covers it.

- **line 328**
  `selected != items.size() - 1` compares an int against a size_t.
  In the Down handler; the signed value is converted to unsigned, and an empty
  list would make the right-hand side wrap to SIZE_MAX. It is unreachable in
  practice because `selected != -1` short-circuits first (getSelection()
  returns -1 on an empty list), and the code is unchanged since the baseline.
  Noted only because a compiler warning sweep would flag it.

### gs_game.cpp

- **line 467** [confirmed]
  CONFIRMED, with a correction to the diagnosis: on the cameFromEditor path
  levelNumber and p_currentCampaign are never assigned in onEnter, and
  levelNumber++ at line 467 then increments a member that holds whatever the
  previous campaign run left in it - or, on the very first run of the process,
  an indeterminate value.
  GS_Game::GS_Game() (line 238) initialises only engine, showCursor and
  ignoreNextCursorMovement. onEnter sets levelNumber and p_currentCampaign
  only in the else branch (line 594-595, the campaign path). When the level
  finishes, ownLevel is computed as cameFromEditor || (p_currentCampaign &&
  ...), so the short circuit does keep the indeterminate p_currentCampaign
  from being read - but levelNumber++ at line 467 runs unconditionally, before
  status is set to -3. Every later read is guarded: db.setLevelCompleted and
  the isBuiltIn() branch sit behind !ownLevel, and onLeave's
  setCurrentLevel(levelNumber) is behind p_selectLevel, which is 0 on this
  path (gs_leveleditor.cpp:763 passes only "levelDocument"; only
  gs_selectlevel.cpp:478 passes "selectLevel"). So nothing visible goes wrong
  today, but the increment itself is on an uninitialised uint on the first
  editor trial run of a session.

### gs_leveleditor.cpp

- **line 285** [confirmed]
  CONFIRMED: 'if(buttons & 3)' fires on the left button too, where every other
  site pairs it with a preceding '& 1' test.
  grep of the file gives '& 1' guards at 149/164/228/254/342 and 'else if(...
  & 3)' at 155/272/343, plus drawStartButtons at 1181-1182/1189-1190. Line 285
  is the one '& 3' with no '& 1' in front of it, so left-click also runs the
  body. Harmless as it stands: the body only clears p_currentPin/p_startPin.

- **line 492** [confirmed]
  CONFIRMED: Ctrl+Y is undo and Ctrl+Z is redo, the reverse of the usual
  binding.
  case SDLK_y calls editor.undo() at 492-493, case SDLK_z calls editor.redo()
  at 495-496. Natural on a QWERTZ keyboard where Y and Z are swapped, so very
  likely deliberate; on QWERTY the two commands sit on each other's key. Left
  exactly as it is.

- **line 783** [confirmed]
  '#ifdef CHECK_IF_IT_REALLY_IS_A_LEVEL' is defined nowhere in the tree, so
  the block it guards - and the comment inside it - is never compiled.
  grep over Blocks5 finds the name only at this one line. It reads as an
  opt-in check somebody left behind rather than a mistake, so it is only
  reported, not touched.

- **line 1767** [confirmed]
  CONFIRMED: GS_LevelEditor::paste() rejects only when BOTH corners of the
  destination are invalid ('&&'), so a paste near the right or bottom edge
  instances objects outside the level.
  Level::setTileAt guards itself with isValidPosition (level.cpp:1593), but
  Level::hashObject (level.cpp:2078) only tests the flat index 'p.y * WIDTH +
  p.x' against [0, WIDTH*HEIGHT), so an object at x == WIDTH lands in the
  spatial hash at the first cell of the next row instead of being rejected.

- **line 284**
  'if(buttons & 3)' where the right mouse button (& 2) appears to be meant.
  Everywhere else the pattern is 'if(buttons & 1) ... else if(buttons & 3)',
  where the else makes & 3 behave as & 2. Here there is no preceding & 1 test,
  so this fires on the left button as well. Harmless as it stands - the body
  only clears p_currentPin/p_startPin - but it is the one site where the idiom
  is not equivalent to & 2. Same shape at lines 155, 271 and 342, and with
  drawStartButtons at 1182/1190, all of them protected by their else.

- **line 851**
  The mandated English 'With no filename nothing happens here at all - the
  click goes nowhere and nobody learns why' now contradicts the code, which
  shows a $ERROR_NO_FILENAME toast; the German said 'passierte hier frueher
  gar nichts' (previously).
  Dropping 'frueher' is correct archaeology pruning, but it turns a past-tense
  account of the pre-toast behaviour into a present-tense claim the code
  disproves. The phrase is fixed by the glossary and appears byte-identical at
  four sites (gs_leveleditor.cpp:851, 909 and gs_campaigneditor.cpp:203, 262),
  so if it is reworded it must be reworded at all four together, not in this
  file alone. Not touched.

### gs_loading.cpp

- **line 119** [confirmed]
  CONFIRMED. gestureTime is written in only two places - the -1 at line 215
  and waitTime at line 119 - and read in exactly one, the sign test
  gestureTime >= 0 at line 133. The stored millisecond value is never
  consumed.
  The header comment at gs_loading.h:40 promises a time; only the sign is ever
  consulted, so either a use is missing or a bool would do. Comment translated
  as it stands.

### gs_menu.cpp

- **line 200** [confirmed]
  CONFIRMED. time is uint (gs_menu.h:50) and is set to 0 in onGetFocus when
  the title level is loaded fresh, so for the first 25 logic ticks
  keyData.find(time - 500) is a lookup on a key near 2^32.
  Harmless in practice - the unordered_map lookup simply misses, which is also
  the intended 500 ms delay before the demo starts - but the miss depends on
  unsigned wraparound rather than a guard, and the same file explicitly guards
  that shape 80 lines later for .donation_asked (timePlayed >=
  lastAskedForDonation before the subtraction). The asymmetry is worth a look.

### gs_selectlevel.cpp

- **line 62** [confirmed]
  shown = status ? title : "???" hides the title only for status == 0
  (locked). status == -1, the locked bonus level, is truthy, so its real title
  is displayed.
  Twelve lines further down, line 114 treats the two identically - if(status
  == 0 || status == -1) darkens the preview and prints $LS_LEVEL_LOCKED or
  $LS_BONUS_LEVEL_LOCKED over it - so a locked bonus level gets the locked
  treatment everywhere except in its own caption. Either the title reveal is
  intended for the bonus level or the test wants the same pair as line 114. No
  comment states which, so I translated the surrounding block as written.

- **line 72**
  In onRender(), p_currentCampaign->isSingleLevels() is dereferenced inside
  if(p_currentLevel) with no null check, while the same pointer is guarded as
  p_currentCampaign && ... at lines 140, 383 and 391.
  Safe as long as loadLevel() stays the only place that sets p_currentLevel,
  since it dereferences p_currentCampaign itself and onLeave() clears both
  together - but the asymmetry with the guarded uses in the same file is the
  kind of thing that stops being true after an edit. No comment mentions the
  invariant.

### gui_checkbox.cpp

- **line 154** [confirmed]
  CONFIRMED: readAttributes answers a <Checked> element with check(true),
  which is the user-click path and fires the changed signal, rather than
  setChecked(true).
  gui_checkbox.h:28 states the rule explicitly - check() is the user's click,
  setChecked() is the display catching up - and loading a dialog from XML is
  not a click. It is harmless only because nothing is connected to changed
  while the dialog is being built. Not fixed: code.

- **line 153**
  readAttributes answers a <Checked> element with check(true), the user-click
  path that fires the changed signal, rather than with setChecked(true).
  gui_checkbox.h states the rule the other way round: check() means the user
  clicked, setChecked() means the display caught up. Loading a dialog is not a
  click. It is harmless today only because nothing is connected to changed at
  load time.

### gui_editbox.cpp

- **line 216** [confirmed]
  CONFIRMED: the case SDLK_a / SDLK_c / SDLK_v / SDLK_x block has no break, so
  control falls into default: and the character-insert path runs for the same
  key press.
  That fall-through is what keeps a plain a, c, v or x typeable, and under
  Ctrl event.keysym.unicode is a control code below 32 so the guard at line
  220 rejects it. The exposure is a layout where AltGr - which SDL reports
  with a Ctrl modifier - puts a printable character on one of those four keys:
  the clipboard branch and the insertion would both run. No comment in the
  file mentions the deliberate fall-through, which is itself worth a note. Not
  fixed: code.

- **line 344** [confirmed]
  CONFIRMED: onRender measures with measureText(text, &dim, &charPositions,
  Vec2i(4, 0)) and draws the text at x = 4, while getIndexAt measures with
  measureText(text, 0, &charPositions) - no offset - and then tests
  charPositions[i].x + 6 >= position.x + scroll.
  Font::measureText adds the offset argument into every entry it pushes
  (p_outCharPositions->push_back(cursor + offset)), so the positions
  getIndexAt hit-tests against are 4 pixels left of where the glyphs are
  actually drawn. The + 6 absorbs part of that, but the two functions no
  longer agree on where a character starts, so a click near a character
  boundary can land the caret one character out. Not fixed: code.

- **line 217**
  The case SDLK_a / SDLK_c / SDLK_v / SDLK_x block ends without a break, so
  after the if(ctrl) body it falls through into default:, which inserts the
  character.
  The fall-through is what makes a plain a, c, v or x typeable, and with Ctrl
  the unicode is a control code below 32 so nothing is inserted. But on a
  layout where AltGr (which sets a Ctrl modifier) produces a printable
  character on one of those four keys, the clipboard branch and the insertion
  would both run for the same key press.

### gui_listbox.cpp

- **line 160** [confirmed]
  CONFIRMED. SDLK_PAGEUP (line 160) and SDLK_PAGEDOWN (line 163) compute the
  page size with GUI::inst().getFont()->getLineHeight(), while onRender (line
  67), setSelection (line 266), getIndexAt (line 281) and updateScrollBar all
  use p_font->getLineHeight().
  GUI_Element::p_font starts as GUI::inst().getFont() (gui_element.cpp:25) but
  a <Font> child element replaces it (gui_element.cpp:365-369). A list box
  with its own font would therefore page by a different number of rows than it
  draws and scrolls by, and the selection could land outside the visible area.
  No shipped dialog sets <Font> on a ListBox, so it is latent.

- **line 161**
  PAGEUP and PAGEDOWN measure a row with
  GUI::inst().getFont()->getLineHeight(), while every other line in the file
  (onRender, getIndexAt, updateScrollBar, setSelection) uses
  p_font->getLineHeight().
  p_font is the element's own font. A list box whose font differs from the
  GUI's would page by a different number of rows than it draws and scrolls by,
  so the selection would land off-screen. Same statement at line 164 for
  PAGEDOWN.

### gui_multilineeditbox.cpp

- **line 226** [confirmed]
  CONFIRMED. case SDLK_a/SDLK_c/SDLK_v/SDLK_x has no break after the if(ctrl)
  block (line 202-226) and falls through into default:, where the key is
  inserted as text.
  Ctrl+A/C/V/X therefore also run the insert branch and are saved only by
  event.keysym.unicode being a control code below 32 while Ctrl is held (the
  guard is c >= 32 || c < 0). An SDL reimplementation that reports the
  printable character with Ctrl down would paste and then type a 'v'.
  gui_editbox.cpp has the identical shape, so it is a shared pre-existing
  pattern, not a local slip.

- **line 367** [confirmed]
  CONFIRMED as written, but unreachable today. static_cast<uint>(text.length()
  - 1) underflows to 0xFFFFFFFF for an empty text, at line 367 in getIndexAt
  and line 395 in findLineEnd.
  In getIndexAt the loop condition i < text.length() is false at once for an
  empty text, so line 367 is never evaluated; findLineEnd returns at line 390
  (cursor >= text.length()) before reaching line 395. Both guards are one edit
  away from moving, and the expression carries no comment saying it depends on
  them.

### gui_radiobutton.cpp

- **line 257** [confirmed]
  CONFIRMED: GUI_RadioButton reads its Image filename raw and has no
  setRawImageFilename and no onUpdate, unlike GUI_Button and GUI_StaticImage.
  readAttributes calls setImageFilename(p_imageFilename) with the XML text
  unlocalized - no localizeString, no per-frame re-resolve.
  GUI_Button::onUpdate (gui_button.cpp:121-136) and GUI_StaticImage::onUpdate
  re-resolve a $ID image every frame precisely so a language switch reaches
  the picture. A radio button whose Image is a $ID would keep the old picture
  until the next start. Possibly deliberate if no shipped dialog needs it, but
  the asymmetry is real.

### gui_radiobutton.h

- **line 35** [confirmed]
  CONFIRMED as an understatement: the comment says setChecked() only makes the
  display catch up, but GUI_RadioButton::setChecked also unchecks every
  sibling radio button of the same group.
  gui_radiobutton.cpp:190-207 walks p_parent->getChildren() and clears
  p_rb->checked for every sibling of the same type and group. The comment's
  real contrast is with check(), which is setChecked() plus the changed signal
  (gui_radiobutton.cpp:209-215). The German said exactly this and CLAUDE.md
  uses the same phrasing, so the translation was left as it stands.

- **line 40**
  GUI_RadioButton has setImageFilename but no setRawImageFilename and no
  onUpdate, unlike GUI_Button and GUI_StaticImage.
  Both of those classes re-resolve a $ID image filename every frame precisely
  so a language switch reaches the picture (the comments in gui_button.cpp:125
  and gui_staticimage.cpp:62 say why). A radio button that shows a localized
  image would keep the old one until the next start. Possibly deliberate - no
  shipped dialog may need it - but the asymmetry is worth a look.

### gui_window.cpp

- **line 67** [confirmed]
  CONFIRMED as fact, but not a defect on its own: the local std::string title
  shadows the GUI_Window member title, which is why line 67 has to write
  this->title while line 69 reads the local.
  Reported only because it is what makes the double localizeString above easy
  to miss - lines 67 to 69 have to be read together. No comment claims
  anything about it.

### harness.js

- **line 9** [confirmed]
  checkbatch.py reports 5 'German comment lines' in harness.js (lines 9, 125,
  174, 218, 233). None of them is German - all five contain the English word
  'so', which findgerman.py's DE regex lists as a German function word.
  harness.js was already fully English at HEAD and I did not touch a byte of
  it.
  This is a detector bug, not a translation gap, and it is why the batch does
  not print ALL CLEAN. Fix is one token: drop 'so|' from the DE alternation in
  findgerman.py. I did not reword the author's already-correct English to work
  around it, and I did not edit shared sweep tooling on my own initiative.
  Note that my own three files were reworded around 'so' to get them clean,
  which is itself a small distortion the fix would let a later pass undo.

- **no line given**
  The first agent listed harness.js as translated, but git diff shows no
  change at all.
  Not a defect - I checked the pre-sweep revision (git show
  d36b28a:WebBuild/test/harness.js) and every string and comment in it was
  already English before the translation began. Nothing was missed; the claim
  is simply vacuous. I re-read the whole file and found no German, no
  non-ASCII and no CRLF.

### harness.sh

- **line 203** [confirmed]
  CONFIRMED. The stated reason for the 60 ms hold - "60 ms is long enough for
  one run to see both events" - does not follow from the model given two
  sentences earlier, which says the damage is done when a poll falls BETWEEN
  the press and the release.
  SDL_PollEvent runs once per rendered frame, every 200 ms under llvmpipe. The
  probability that a poll lands inside the press-release window is that
  window's length over 200 ms, so a 60 ms hold is split about five times as
  often as a 12 ms one, not less often. The measurement itself (every fifth
  press doubled at xdotool's ~12 ms, none at 60 ms) is not in question - only
  the explanation of it. The likelier mechanism is the other half of the same
  paragraph: events are queued, so nothing is ever lost; what matters is that
  the release be read before SDL_EnableKeyRepeat(140, 60)'s first-repeat
  deadline elapses, and a hold that is long enough for the press and release
  to be drained by the same poll pass achieves that. Translated exactly as the
  German has it.

- **no line given**
  The message I translated at line 173 - raise SystemExit('no element "%s"' %
  name) - can never reach a person.
  The heredoc catches it three lines later with 'except SystemExit as e:
  print('', end=''); sys.exit(0)', so the text is discarded and the exit code
  is 0. Callers detect the miss by the empty stdout instead (b5_click's '[ -n
  "$shown" ]'). Pre-existing; I translated it as written and changed no
  control flow.

### hint.cpp

- **line 37** [confirmed]
  The comment states 0.30 'macht den Halbmesser 38 Bildpunkte gross' - makes
  the RADIUS 38 pixels. CLAUDE.md says the same constant 'makes the bead 38
  pixels across', i.e. the diameter.
  ROLL_LENGTH * NOTE_HEIGHT / (ROLL_TURNS * 2 * PI) = 0.30*400/PI = 38.2,
  which is the radius, so the source comment is right and CLAUDE.md's 'across'
  is the one that is off by a factor of two. Translated as 'makes the radius
  38 pixels'; CLAUDE.md needs the correction, not the file.

- **line 504** [confirmed]
  CONFIRMED. Hint::onCollect(Player* p_player) has an empty body and never
  names p_player.
  Deliberate per its own comment - it exists only to stop Object::onCollect()
  making the note disappear - but it draws an unused-parameter warning under
  -Wall (Tools/syntax.sh passes -w, LinuxBuild does not). Left exactly as it
  stands.

- **line 357**
  The comment claims the level draws "den Blitz" right after the note and that
  it wants no texture. Verified against the code: the note draws on layer 42,
  level.cpp:967 renders layer 42 and 969-980 draws the flash quad with no
  texture bound.
  The claim holds for Level::flash and not for the thunderstorm lightning
  (level.cpp:889, which binds p_lineTexture). Raised only because the shared
  glossary's Traps table assigns "lightning" to this site; the code says
  flash.

### hotel.cpp

- **line 61** [confirmed]
  CONFIRMED with a nuance the first agent missed. state runs -1 -> (player off
  the field) 0 -> (player on the field) 1 and stops there; nothing ever sets
  it to 2, so say("$G_HOTEL_WELCOME", 0.5) and p_hotelToSave = this run on
  every logic tick the player stands on the hotel.
  The nuance: state == -1 is set by the constructor and again by onSave(), and
  it is what suppresses the greeting - a player who starts on the hotel, or
  who has just saved, gets no welcome until they step off (the else branch
  sets state = 0) and back on. So the 0 -> 1 step is a re-arm, not a once-only
  latch, and state 1 is a standing "player is here" condition. Object::say
  only assigns sayText/sayTime, so the visible effect is a welcome bubble
  refreshed every tick that therefore never expires while the player stands
  there; that may well be intended, and no comment in the file says. One
  further wrinkle: the else branch only resets state when the front object is
  absent or is not of type "Player", so a second, non-active Player standing
  on the hotel clears p_hotelToSave but leaves state at 1.

### lightbarriersender.cpp

- **line 176** [confirmed]
  CONFIRMED as described. `counter++` is the last statement inside the
  beam-tracing `while(true)` loop in onUpdate(), not a once-per-tick
  increment.
  counter is the pulsing phase: onRender computes `x = counter * 0.8` and
  feeds it to sin() and cos() for both beam colours. As written the phase
  advances by the number of beam steps traced this tick, so a beam that
  changes length - a block pushed into it, a mirror turned - jumps the pulse
  instead of continuing it. Nothing in the code or the comments claims this is
  wanted; the increment reads as belonging outside the loop, beside
  beam.clear().

- **line 172**
  counter++ sits inside the beam-tracing while(true) loop rather than once per
  onUpdate() call.
  counter drives the beam's pulsing phase (x = counter * 0.8 in onRender), so
  the phase advances by the number of beam steps traced this tick, not by one
  tick. A beam that gets longer or shorter - a block pushed into it, a mirror
  turned - jumps the phase instead of continuing it. Nothing in the code says
  this is wanted; it reads like the increment belongs outside the loop, beside
  beam.clear().

### linux_window.h

- **line 20** [confirmed]
  CONFIRMED. The comment gives "no X11 running here" as the meaning of a false
  return from setFixedSize, but linux_window.cpp:85 returns p_hints != 0, so a
  failed XAllocSizeHints also returns false under a perfectly good X11.
  The header states one cause for false where the code has two. Rendered as
  the German has it. (setFullScreen's identical sentence is accurate - every
  false there is a missing or non-X11 window manager info.)

- **line 21**
  "false again means 'no X11 running here'" for setFixedSize, but the
  implementation returns p_hints != 0, so a failed XAllocSizeHints also yields
  false.
  The header states one cause for false where the code has two. Rendered as
  the German has it.

### main.cpp

- **line 24** [confirmed]
  First agent's flag #6 confirmed: reinterpret_cast<int>(result) on the
  HINSTANCE returned by ShellExecuteA.
  A 64-bit build would not compile it, and would truncate the handle if it
  did. Harmless today because only Debug|Win32 and Release|Win32 exist, but it
  pins the project to Win32.

- **line 168** [confirmed]
  First agent's flag #3 confirmed: unsigned int length1 = static_cast<unsigned
  int>(strlen(p_in) / 7) * 4; is written and never read - grep finds exactly
  one occurrence in the file.
  Dead variable in decryptPassword; it looks like the intended bound for the
  loop below, which re-evaluates strlen(p_in) each iteration instead. No
  comment mentions it, so nothing in the translation depends on it.

- **line 337** [confirmed]
  CONFIRMED. The config.xml comment sits immediately above
  `if(versionInitialized == "<= 1.0.7") success &= fs.copyFile("progress.zip",
  ...)`, which it does not describe.
  It belongs to the directory-initialization block above it (the
  createDirectory run), not to the progress.zip copy under it. The German
  comment sat in the same place, and rule 6 keeps each comment attached to the
  same code, so the misplacement is preserved.

- **line 224**
  The retireShadowingCopies header lost its two version numbers in
  translation: German "Bis 1.1.2 wurde alles ... kopiert. Seit 1.2.0 liegt es
  im Spielordner" became "Older installations copied ... It lives in the game
  folder now".
  Defensible pruning of archaeology under the sweep's rules, and the standing
  reason survives intact - but this function exists exactly to clean up after
  specific versions, and the comment four hundred lines further down still
  names 1.2.0 and 1.2.1 concretely. If the author wants the two version
  numbers back, this is the one place in these four files where a fact was
  deliberately dropped. Left as the first agent had it.

### make_ico.py

- **line 2** [confirmed]
  First agent's flag #1 confirmed: the module docstring (lines 2-34) and the
  docstrings of reduce_to_art (58), render (82-83) and dib_entry (101-103) are
  still German.
  They are triple-quoted string literals, which rule 4 and codeonly.py both
  treat as code, so a comment-only pass cannot touch them; checkbatch does not
  look at them either (findgerman.comment_lines only reads # lines in .py).
  The module docstring is the tool's usage text - line 137 does raise
  SystemExit(__doc__) - so it is user-facing. It carries the whole
  integer-scale argument (the 20 -> 16 + 2px, 40 -> 32 + 4px table and the
  no-power-of-two note) that CLAUDE.md also states; a pass allowed to move
  string literals is needed.

- **no line given**
  The docstring says "20, 24 and 40 do not" come out as whole multiples of 16,
  but 24 is deliberately absent from DEFAULT_SIZES - the comment above that
  tuple (line 48) explains that Windows scales 24 down from the 32 instead.
  The docstring's claim is true as arithmetic but lists a size the script
  never renders, so the two passages read as if they disagree. Present in the
  German exactly the same way; rendered as written.

- **line 49**
  New: the DEFAULT_SIZES comment claims 24 is "the one size where the next
  integer step down would be 1x", but 20 is 1x as well - render() computes
  scale = max(1, size // width) with width 16, so 20 gives 1x with a 2px
  margin and 24 would give 1x with a 4px margin.
  What actually singles 24 out is the size of the margin, not the scale
  factor. The German says the same thing ("die einzige Groesse, bei der die
  naechstkleinere ganzzahlige Stufe die 1x waere") and CLAUDE.md repeats it,
  so this is the source being loose, not the translation. Rendered as written.

### object.cpp

- **line 14** [confirmed]
  CONFIRMED: the comment over FLASH_STRENGTH/FLASH_DECAY claims the flash is
  over after "just under eight ticks, a good 0.15 s", but the arithmetic gives
  25 ticks and 0.5 s.
  FLASH_DECAY is 0.8 and Object::frameBegin() applies it exactly once per
  logic tick (flashAmount *= FLASH_DECAY, zeroed below 1.0/256.0 -
  object.cpp:930-934). flashAmount is written nowhere else besides flash() and
  the constructor. 0.8^n < 1/256 first holds at n = 25, i.e. 25 * 20 ms = 0.5
  s; 0.8^8 is 0.168, still 43 times above the cutoff. The comment's
  cross-reference is otherwise accurate: Level::update() carries the identical
  decay, flash *= 0.8 with the same 1/256 cutoff (level.cpp:1149-1155), as
  does the HUD-icon loop (level.cpp:1000-1006). CLAUDE.md repeats the same
  claim ("about eight ticks, a sixth of a second"), so either both figures are
  stale or the intended decay is faster. Translated as written, not corrected.

- **line 128**
  The comment asserts "for five of the seven switches that is the default
  white" - a count I could not verify from the code.
  Seven object types call flash() on themselves and forward it to a neighbour:
  barrageswitch, cannonswitch, e_pulseswitch, e_valueswitch,
  electricityswitch, lightswitch and magnet, so "seven switches" checks out as
  a count of flashing switch-like objects (panel.cpp and e_pulsepanel.cpp
  flash but do not forward). Which five of them draw with the default white
  sprite colour is a property of the sprite data, not of the source, so the
  "five" is unverifiable from here. Flagged only so a later pass can confirm
  it against the sprite definitions.

### options.cpp

- **line 231** [confirmed]
  CONFIRMED: handleClick writes language, both volumes, the detail level, the
  upscaler and all six CRT sliders into the Engine unconditionally (lines
  231-268, inside if(isVisible())) before it dispatches on the element name.
  Read the whole function: the name checks start only at line 270
  ("CrtSettings"). So the name == "Cancel" call also writes the widget state
  into the Engine first and is only then undone by loadConfig() at line 361 -
  and only when changed is true. The same holds for the two internal refresh
  calls, handleClick(getChild("Options.Actions")) from applyKeyGrab and
  handleClick(p_actions) from the reset branch, where no widget changed at
  all. Both German comments ("Wirkt sofort", "Die Roehrenregler wirken sofort
  ... Abbrechen nimmt sie ueber loadConfig() zurueck") describe the intent;
  translated as written.

- **line 150**
  The comment "Start with no selection. setSelection() reports only a real
  change: if it already stood at -1, the branch below does not run." states a
  reason that cannot arise on this path.
  GUI_ListBox::setSelection (gui_listbox.cpp:252-277) does fire changed only
  on a real change, so the first half holds. But the Options pane is invisible
  for the whole of show() - the constructor hid it and it is shown again only
  by the closing getChild("Options")->focus(), via GUI_Element::bringToFront()
  - so handleClick's if(isVisible()) is false and the "Actions" branch never
  runs from show() whatever the old selection was. The four explicit
  setTitle("")/deactivate() lines that follow are therefore always needed, not
  just in the already-at--1 case the comment names. Rendered as the German has
  it.

### options.xml

- **line 135** [confirmed]
  CONFIRMED (first agent's flag). The Cancel button's caption is a
  hand-inlined Latin-1 localized string, raw bytes 0xA7 "de:Abbrechen" 0xA7
  "en:Cancel", where every other caption in the file is a $ID. $CANCEL exists
  in languages.txt with exactly those two bodies and is what leveleditor.xml
  (three sites) and campaigneditor.xml use for the same button.
  Verified present at HEAD, so it is pre-existing and not a translation
  artifact. It is element text and therefore code under codeonly.py's XML
  stripper, so it may not be touched in this pass - which is why checkbatch
  reports one problem for options.xml and cannot print ALL CLEAN. Everything
  else it checks is clean: no code changed for any of the six files, no German
  comment line left, LF endings.

### pack.sh

- **line 42** [confirmed]
  CONFIRMED (first agent's flag 2). Every user-facing echo string is still
  German: "unbekannt: $arg" (42), "7za fehlt - sudo apt install p7zip-full"
  (46), "(python3 fehlt, die Kommentare bleiben in den XML-Dateien)" (52),
  "(optipng fehlt, die PNGs bleiben wie sie sind)" (56), " nichts zu packen
  fuer $target" (79), " $name fehlt" (114), "fertig" and "### FEHLER ###"
  (144).
  Rule 4 forbids touching string literals, so they stay - correctly. But these
  are the visible output of a build script in a tree that is to become
  English, so a later pass will want them. Listing so they are not overlooked.

- **line 134** [confirmed]
  CONFIRMED but not a defect (first agent's flag 3). The data branch is
  written as `[ "$what" = all ] || [ "$what" = data ] && { packData || fail=1;
  }` while the skins branch immediately below uses a plain `if ... then` for
  the identical decision. Verified by running the three cases: all -> runs,
  data -> runs, skins -> skipped, which is bash's left-to-right (A || B) && C.
  Behaviour is correct, but the mixed ||/&& idiom is the classic shape that
  reads as a precedence bug, and no comment says why one branch is written
  differently from the other. Readability doubt only; nothing to translate
  here.

- **no line given**
  sh -n Blocks5/pack.sh fails: "line 72: Syntax error: '(' unexpected".
  Pre-existing and not a defect: line 72 is `local files=()`, a bash array,
  and the shebang is #!/bin/bash. I confirmed the failure is identical on the
  pre-change file from git (HEAD), and that `bash -n` passes on both. The
  correct check for this file is bash -n, not sh -n.

- **line 41**
  Every user-facing echo string is still German: "unbekannt: $arg", "7za fehlt
  - sudo apt install p7zip-full", "(python3 fehlt, die Kommentare bleiben in
  den XML-Dateien)", "(optipng fehlt, die PNGs bleiben wie sie sind)", "
  nichts zu packen fuer $target", " $name fehlt", "fertig" and "### FEHLER
  ###".
  Rule 4 forbids touching string literals, so they were left alone - but if
  the whole tree is to be English these are the visible output of a build
  script and a later pass will presumably want them. Listing so they are not
  overlooked.

- **line 133**
  The data branch is written as `[ "$what" = all ] || [ "$what" = data ] && {
  packData || fail=1; }` while the skins branch two lines below uses a plain
  `if ... then`.
  It happens to be correct - bash groups this left to right as `(A || B) && C`
  - but the mixed `||`/`&&` idiom is the classic shape that reads as a
  precedence bug, and it is inconsistent with the `if` used for the identical
  decision immediately after. No comment describes it. Not touched; flagging
  as a readability doubt rather than a defect.

### parameterblock.h

- **line 73** [confirmed]
  CONFIRMED as a fact of the file, and correctly left alone: throw "Falscher
  Parametertyp!" is a German string literal, and the sweep's rules keep string
  literals untouched. The neighbouring failure at line 63 throws a bare 0, so
  the two error paths of the same function are not even the same thrown type.
  Only an English pass that is explicitly extended to literals may change it,
  and a catch site keyed on the type would have to be checked first (grep for
  catch sites before touching either throw). Flagged, not fixed.

### presets.cpp

- **line 562** [confirmed]
  Six electronics branches - E_Value, E_ValueSwitch, E_PulseSwitch,
  E_PulsePanel, E_Gate, E_FlipFlop - construct their object inside
  if(p_element), so instancePreset returns 0 when called without an element.
  Every other branch constructs unconditionally and falls back to default
  arguments.
  The editor palette (cat<N>.xml) supplies an element, so this is not hit in
  practice today, but the asymmetry means these six types silently produce a
  null Object where the other fifty produce a working default. Any caller that
  placed one of them without an element would get a null back from a function
  whose other 50 paths never return null.

### soundinstance.cpp

- **line 156** [confirmed]
  `abs(targetVolume - newVolume)` and `abs(targetPitch - newPitch)` call
  unqualified abs() on doubles.
  pch.h includes <cmath> and not <cstdlib>, so the floating-point overloads
  are almost certainly the ones found and this is fine in practice. Flagging
  it only because if ::abs(int) ever won the overload, the difference would
  truncate to 0, `0 < 0.01` would always hold, and every
  slideVolume()/slidePitch() would snap to its target on the first tick
  instead of easing - a silent behaviour change with no compiler complaint.

### sounds.xml

- **line 7** [confirmed]
  CONFIRMED (first agent's flag). "der Kodierer bekaeme weniger Aussteuerung
  fuer dieselbe Rechenlast" - the argument for not encoding quieter is stated
  against computational load, where the effect described (quantisation noise
  relative to full scale) is a function of the bitrate, which the paragraph
  above has already fixed as the constant. Rendered literally as "for the same
  computational load".
  Reads like "Bitrate" was the word meant. Not repaired in a translation pass.

### syntax.sh

- **line 31** [confirmed]
  First agent's flag #2 confirmed: the comment names <Windows.h>,
  <Shellapi.h>, <Shlobj.h> and <al.h>, but six shims are generated - the loop
  on line 37 also writes VersionHelpers.h, and lines 40-41 write al.h and
  alc.h.
  Two of the six generated headers are unnamed, so a reader adding a shim
  would not know the list is partial. Pre-existing in the German and repeated
  in CLAUDE.md, which names only three. Translated as it stands.

- **no line given**
  "$n source files compile without errors" says "1 source files" when a single
  file is checked (sh Tools/syntax.sh engine.cpp).
  Pre-existing and faithful: the German printed "1 Quelldateien uebersetzen
  fehlerfrei" with the identical wart. Fixing it needs an if, which would be a
  behaviour change, so I left it. Flagging in case the formatting pass wants a
  plural-agnostic wording.

### testhooks.cpp

- **line 257** [confirmed]
  CONFIRMED, first agent's flag. The comment justifies the polling cost with
  "A stat() on a file that does not exist, fifty times a second, costs
  nothing", but pollRequests() calls fopen(requestPath, "rb") and returns when
  that fails - there is no stat() anywhere in the function.
  The cost argument still holds (a failed open of a missing file is the same
  order), but the named syscall is not the one in the code, so a reader
  grepping for stat() finds nothing. Translated as written.

- **line 258**
  The comment justifies the polling cost with "A stat() on a file that does
  not exist, fifty times a second, costs nothing", but pollRequests() does no
  stat(); it calls fopen(requestPath, "rb") and returns on failure.
  The argument still holds - a failed open of a missing file is the same order
  of cost - but the named syscall is not the one in the code, so a reader
  grepping for stat() finds nothing. Translated as written.

### texture.cpp

- **line 140** [confirmed]
  CONFIRMED, first agent's flag. createSubTexture() does "new
  Texture(filename)"; the constructor (texture.cpp:5-15) ends in reload(),
  which decodes the whole parent image out of the archive and creates a GL
  texture. Only afterwards are p_parent, offset and size assigned and reload()
  called a second time, which this time takes the loadSubTexture() branch and
  calls cleanUp() on that first result.
  Every sub-texture therefore costs one complete, discarded IMG_Load_RW plus a
  GL texture create and delete. Not a correctness bug and no comment claims
  otherwise, so nothing was translated around it - reported only.

- **line 145**
  createSubTexture() does "new Texture(filename)", whose constructor
  immediately calls reload() and decodes the whole image from the archive;
  only afterwards are p_parent, offset and size assigned and reload() called a
  second time, which this time takes the loadSubTexture() branch.
  Every sub-texture therefore costs one complete, discarded IMG_Load_RW plus a
  GL texture create and delete. Not a correctness bug and no comment mentions
  it, but it is the kind of thing the sweep is asked to surface. Not touched.

- **line 263**
  checkDimensions() logs "WARNING: Creating non-pow2 texture!" for every NPOT
  texture, which now includes every imported skin - the exact case
  applyWrapMode() twenty lines above exists to support and calls normal.
  The warning and the wrap-mode comment disagree about whether a
  non-power-of-two texture is a problem. Nothing translated around it; the
  German ("Das koennte Aerger machen!") was rendered as it stands.

### tileset.cpp

- **line 117** [confirmed]
  CONFIRMED as a comment/code mismatch, mild. The comment (German: "Tile-Typ
  eintragen") sits over tiles[id] = info, which stores the whole TileInfo.
  The statement records position, type, destroy time and the sprites list, not
  only the type. Rendered as it stands ("record the tile type") rather than
  corrected, which is the right call for this pass.

### touch_controls.js

- **line 1** [confirmed]
  CONFIRMED that the file was already fully English at HEAD - git show
  HEAD:WebBuild/touch_controls.js has no German comment. The nine edits are
  rewordings that dodge findgerman.py's word list, which contains the ordinary
  English words "so", "was", "also", "man", "die", "hat", "den", "ob", "um",
  "wo".
  Two consequences for the rest of the sweep. Every agent will be pushed away
  from "so" and "was" in full-line comments tree-wide, which costs natural
  English (here "..., so a rebinding is followed for free" became "..., and a
  rebinding is followed for free", losing the causal link). And
  findgerman.comment_lines only inspects lines that are entirely a comment, so
  German left in a trailing comment after code would pass silently - the three
  trailing comments in this file are English, but the gap is real.

- **line 10**
  This file was already fully English at HEAD - it had no German comments at
  all.
  It was on my list, so I report rather than assume. The only edits are the
  nine reworded lines described in the file note, forced by checkbatch's word
  list flagging the English word "so"; that same list will flag "so" and "was"
  in every other agent's output too, which is worth knowing before the tree
  loses a very natural English word everywhere.

### toxicgas.cpp

- **line 143** [confirmed]
  CONFIRMED. updateSound() computes volume = 1.0 / (40.0 * 20.0) *
  numInstances and clamps it to [0.5, 1.0] (clamp(value, minValue, maxValue),
  src/util.h:14).
  40 x 20 = 800 is a full level of fields, so the ramp only leaves its lower
  stop above 400 gas objects; in practice every gas cloud plays at exactly
  0.5. The consequence is that setVolume(0.0) in the constructor (line 30) is
  dead - updateSound() at line 34 raises it to 0.5 before a frame is drawn, so
  the sound never fades in from silence. No comment claims anything about
  either, so nothing was translated here.

### transfer.h

- **line 11** [confirmed]
  The forward declaration class Campaign; is not used anywhere in the header -
  no declared function takes or returns one.
  Not a comment matter and not touched, but it is the sort of leftover the
  sweep is meant to surface.

- **line 24** [confirmed]
  The classify() comment describes an archive as recognised by "whether a
  campaign.xml or a tileset.xml lies inside it"; CLAUDE.md says a skin needs
  tileset.xml AND sprites.png.
  Either the header comment is an abbreviation of the real rule or the rule
  has changed; transfer.cpp decides. Rendered as the German has it.

- **line 6**
  NEW. The file header says "Everything here exists on both platforms" and
  then names only Windows and the browser; transfer.cpp has three file-dialog
  paths - GetOpenFileNameA, zenity/kdialog (line 595) and web_transfer.cpp -
  and CLAUDE.md says the Manager works "on all three platforms".
  "Both platforms" predates the native Linux dialog. Translated as written; a
  reader on Linux is told his platform does not exist here.

### u_crt.cpp

- **line 394** [confirmed]
  CONFIRMED as a discrepancy. The halation comment gives the relative present
  cost as "7.9 then falls to 4.2"; CLAUDE.md's table says crt 7.8 with the
  same 4.2.
  One of the two has drifted by 0.1 and the code cannot decide which. Kept the
  source's 7.9.

- **line 469** [confirmed]
  CONFIRMED. Two section banners sit back to back with no code between them:
  "give the light back, edge, gamma" (line 469) then "flicker, part 2: the
  brightness" (line 470); the statement under both is the flicker multiply,
  and the BRIGHTNESS, vignette and gamma lines the first banner announces come
  at 474-477.
  The first heading labels a section it does not open. Both banners kept, in
  place and in order.

- **line 393**
  The halation cost is given as "7.9 then falls to 4.2"; CLAUDE.md's table of
  relative present cost says crt 7.8, with the same 4.2 for BLOOM_STRENGTH =
  0.
  One of the two figures has drifted by 0.1. Kept the source's 7.9.

- **line 468**
  Two section banners sit back to back with no code between them: "give the
  light back, edge, gamma" immediately followed by "flicker, part 2: the
  brightness" (line 469), and the statement under both is the flicker
  multiply. The gamma, vignette and edge lines the first banner announces come
  four lines later (473-477).
  The first heading labels a section it does not open, which reads as a
  leftover from an earlier ordering of the shader tail. Both banners kept, in
  place and in order.

### util.cpp

- **line 158** [confirmed]
  CONFIRMED (flagged by the first agent). prepareForTinyXML() returns its
  argument unchanged, has no comment, and grep over the whole tree (outside
  libs/ and .git) finds exactly one occurrence - this definition. No
  declaration in any header, no caller.
  Dead code whose name promises escaping it does not perform. Anything that
  started calling it would silently get no escaping.

### web_transfer.cpp

- **line 180** [confirmed]
  abandon() resets only importStatus, busy and importName. The <input
  type=file> that openPicker appended stays in the DOM with its change and
  cancel listeners and its 300000 ms timeout live, so a file chosen after
  abandon() still runs done(): FS.writeFile writes the staging file and
  _blocks5_importComplete sets importStatus. The comment - German and English
  alike - says the dialog is discarded.
  Confirms the first agent's flag, from the code. Benign today: the only
  caller is Transfer::abandonImport (Blocks5/src/transfer.cpp:341), reached
  only from GS_Menu::onLeave (Blocks5/src/gs_menu.cpp:334), which deletes the
  three staging files immediately after. But importStatus is a file-scope
  volatile that survives the state change, so on the next entry to the menu
  with the Manager open, pollImport could hand the caller an IMPORT_OK it had
  given up on, with the staging file recreated behind it. The comment claims
  more than the code does; translated as written, not repaired.

- **line 179**
  abandon() only resets importStatus, busy and importName on the C side. The
  <input type=file> built by openPicker stays in the DOM with its
  change/cancel listeners and its 300 s timeout live, so a file chosen after
  abandon() still runs done(): it writes the staging file and sets
  importStatus, and the next pollImport would report an import the caller had
  given up on. The comment I translated says 'Discards a dialog that is still
  open'.
  Benign at the one call site today - Transfer::abandonImport() is called from
  GS_Menu::onLeave, after which nothing polls - but the comment claims more
  than the code does, and any future caller that abandons while the Manager
  stays open would see a stale IMPORT_OK. I translated the claim as written
  rather than repairing it. Note the staging file is written regardless, which
  is what the deleteFile() sweep at the top of openPicker exists to clean up
  on the next attempt.

### zip_data.bat

- **line 33** [confirmed]
  CONFIRMED. optipng and the first 7za are reached through the relative
  ..\tools\ (lines 32-33, after PUSHD data); the second 7za through the
  absolute "%~dp0tools\7za" with an absolute "%~dp0data.zip" (line 36, after
  PUSHD "%XMLSRC%"). Same pair in zip_data_no_optipng.bat lines 22 and 25.
  The absolute form is necessary - %XMLSRC% is %TEMP%\blocks5-xml whenever
  Python is present, where ..\tools would not resolve - but the two idioms sit
  three lines apart with nothing saying why they differ, and when Python is
  missing %XMLSRC% is data and both forms would work. The German comment
  explains the two 7za calls, never the two path forms.

- **no line given**
  This file and zip_data_no_optipng.bat are LF, NOT CRLF - contrary to the
  brief's claim that "the .bat files under Blocks5/ and Build.bat are CRLF". I
  kept them LF.
  The LF is deliberate and load-bearing, and the file's own header comment
  says so: "single line IFs only, no bracketed blocks and no GOTO - this file,
  like the rest of the tree, has no Windows-style line endings, and cmd
  miscounts when it jumps in such a file." Converting them to CRLF to match
  the brief would have contradicted the documented constraint (and would have
  been a behaviour change, not a text change). zip_skins.bat,
  zip_skins_no_optipng.bat and Build.bat genuinely are CRLF and still are -
  verified with file(1) after the edit. Note the inconsistency across the four
  Blocks5 .bat files is itself pre-existing.

### CLAUDE.md

- **no line given**
  Line 168 says 'python3 Tools/verify.py fourteen static checks over the whole
  tree', but CHECKS now holds fifteen (the fifteenth being `comments`).
  Documentation drift, outside my file list so left alone. Noting it because
  CLAUDE.md is the wording authority I translated against and the count is the
  one thing in that sentence that is wrong.

### audiocapture.h

- **line 6**
  The header comment says the capture always yields 16 bit stereo interleaved
  samples "unabhaengig davon, in welchem Format das Geraet selbst arbeitet",
  but the interface never states the sample rate contract - open() takes
  sampleRate with a default of 48000 and there is no comment saying whether
  the implementation resamples or simply accepts what the device gives.
  CLAUDE.md asserts both halves end at 16-bit stereo 48 kHz, with Windows
  converting the format itself and the PulseAudio server resampling. The
  comment covers the format but not the rate, so a reader of the header alone
  cannot tell whether a device running at 44.1 kHz would come out at 48 kHz or
  at 44.1. Translated as written; not extended.

- **line 13** [refuted]
  PARTLY REFUTED. The header states the sample format (16 bit, stereo,
  interleaved) and is silent about the rate, as the first agent says - but the
  implementation does honour open(sampleRate). Windows resamples in the
  capture thread (audiocapture.cpp: srcRate = p_format->nSamplesPerSec,
  resampleStep = (double)srcRate / (double)sampleRate, with the linear
  interpolation loop around resamplePos); Linux passes spec.rate = sampleRate
  to pa_simple_new and lets the server convert; the stub build ignores the
  argument and open() fails. So a 44.1 kHz device does come out at the
  requested 48 kHz.
  A documentation gap in the header only, not a behavioural doubt. Any fix
  here would be adding a sentence the German never had, which is out of scope
  for a translation pass.

### barrage2panel.cpp

- **line 20**
  The sprite in Barrage2Panel::updateSprites is labelled "// switch" (German
  "// Schalter") although the class is a floor panel, and the header calls it
  "a barrage panel".
  Rendered as the German stands. It is the author's shorthand - a floor panel
  is a switch you step on - rather than a contradiction, but it is the one
  place in these six files where the comment and the class name use different
  words for the same object.

### burst.js

- **no line given**
  Line 22 still prints the German verdict: console.log('FEHLGESCHLAGEN: ' +
  e.message). It is the last FEHLGESCHLAGEN anywhere in the tree.
  Outside my four-file list, so I did not touch it. It is the odd one out now
  that smoke.js and all ten LinuxBuild sites say 'FAILED: ', and it sits in
  the same directory as the two scripts that were translated. Whoever owns
  WebBuild/test should render it 'FAILED: ' to match. Nothing reads the line,
  so the change is safe.

### campaign.cpp

- **line 399** [refuted]
  MOSTLY REFUTED. music can never hold two entries with the same member name,
  because push_back runs only when known is false - so the inner loop matches
  at most one entry and the warning is emitted at most once per level, not
  "once per matching earlier entry". Three levels naming the same member from
  three different sources do produce two warnings, but that is one per
  offending level, which is defensible.
  What is left is only a missing break: after known is set the loop keeps
  scanning the rest of music for no result. No comment claims otherwise, and
  nothing is reported wrongly.

### conveyorbelt.cpp

- **line 77** [refuted]
  First agent's suspicion 2 is REFUTED: the intent does hold.
  Level::setElectricityOn (level.cpp:2181-2194) dispatches onElectricitySwitch
  to every object in one synchronous loop, so no belt's onUpdate() can run in
  the middle of a dispatch. The first belt reached acts and sets soundChanged;
  the rest return early. Clearing the flag at the top of every belt's
  onUpdate() only re-arms it for the next switch, which is what is wanted with
  one shared SoundInstance behind N belts.
  One residual edge remains, and it is not what was flagged: if the
  electricity were toggled twice inside the same tick (two panels, or a panel
  and a switch, updating in the same object loop), the second change would
  find soundChanged still true and no belt would follow it, leaving the sound
  sliding the wrong way until the next switch.
  ElectricitySwitch/ElectricityPanel would have to fire in one tick for that,
  which is unlikely but not structurally prevented.

### e_gate.cpp

- **line 92**
  The comment reads "Undefined inputs give an undefined output", but the
  condition under it is `if(!areAllInputsConnected() ||
  isAnyInputUndefined())` - it also covers an input that is not connected at
  all.
  Pre-existing in the German ("Undefinierte Eingaenge fuehren zu einem
  undefinierten Ausgang."), so it was translated as written. CLAUDE.md
  describes the rule more fully as "an undefined state for
  unconnected/unsettled inputs"; the comment names only half of the condition.

### e_pulseswitch.cpp

- **line 57** [refuted]
  REFUTED. $TT_PULSE_TRIGGER_ONE / $TT_PULSE_TRIGGER_ZERO and
  $TT_PULSE_PANEL_ONE / $TT_PULSE_PANEL_ZERO all exist in
  Blocks5/data/languages.txt (lines 1072, 1077, 1082, 1087). The id family
  follows the part's player-facing name, not the class name: E_PulseSwitch is
  "Pulse trigger" / "Impuls-Taster", E_PulsePanel is "Pulse panel" /
  "Impuls-Bodenplatte".
  The mismatch between "switch" in the class/filename and "trigger" in the id
  is intentional and correct; the two parts do mirror each other, and each id
  names what the player reads in the tooltip. Note the header comment stays
  "Class for a pulse switch" because the German says "Pulsschalter" -
  rendering it as "pulse trigger" would be repairing the source inside a
  translation pass.

- **line 92** [refuted]
  REFUTED as a defect. onTouchedByPlayer(0) is the tree's established idiom
  for "an OF_ACTIVATOR object pressed this switch" and appears at seven sites:
  e_pulseswitch.cpp:92, e_valueswitch.cpp:70, magnet.cpp:43,
  barrageswitch.cpp:47, cannonswitch.cpp:58, electricityswitch.cpp:45,
  lightswitch.cpp:61.
  Checked every one of the seven handlers: none dereferences p_player - they
  call flash() and then act on the level or play a sound. Safe by construction
  across the whole family, not by accident in this one class.

### elevator.cpp

- **line 253**
  NEW: `int blink;` is read uninitialised when a saved game has no `blink`
  attribute.
  TiXmlElement::Attribute(const char*, int*)
  (libs/tinyxml-2.6.2/tinyxml.cpp:597) only writes through the pointer when
  the attribute exists, otherwise it leaves the target untouched. `int blink;`
  at elevator.cpp:253 is a fresh local, so `this->blink = blink ? true :
  false;` on line 255 reads indeterminate memory for a file that lacks the
  attribute. The three lines above are safe only because
  moveCounter/newDir/origDir are already-initialised members. The same pattern
  with fresh locals occurs in electronics.cpp:195-198 (sourcePinID, targetX,
  targetY, targetPinID). Found while checking the flagged suspicions; not a
  translation matter and not fixed.

### exit.cpp

- **line 54**
  The German "Level geschafft!" and "Es kann nur einen geben!" both carry an
  exclamation mark that the English drops ("Level done.", "There can be only
  one.").
  Deliberate: the glossary's Voice section allows an exclamation mark only on
  a one-word answer. Recorded so a later pass does not read the loss of tone
  as an error.

### laser.cpp

- **line 160**
  Confirmed: if(destroyTime < 5) destroyTime--; runs every tick and
  destroyTime is a plain int (object.h:195), so the counter keeps going
  negative after the burst at !destroyTime has fired.
  Once destroyTime reaches 0 the vaporize burst fires and disappear(0.2) is
  called; on the next tick destroyTime is -1, which is what stops the burst
  repeating during the 0.2 s the object still lives. It works, but only
  because the test is == 0 rather than <= 0, and a signed counter drifting
  downward for the rest of the object's life is easy for a later change to
  break silently.

- **line 369**
  Confirmed: the debris burst for a destroyed obstacle is placed at beamPosF *
  16, while the identical burst in lava.cpp:300 uses p_obj->getPosition() *
  16.
  beamPosF is beamPos/16 sampled at the top of the while(true) loop, before
  beamPos is snapped onto the obstacle at lines 233-234, so it is the cell in
  which isFreeAt2 detected the hit. For a destroyed tile that is the right
  cell; for a destroyed object that is mid-slide its shown position can be up
  to a tile away from that cell, so the sparks can start from the neighbouring
  field. No comment claims a rule either way - this is the asymmetry with
  lava.cpp, not a contradicted comment. Note also that laser.cpp destroys
  tiles as well as objects, which is why an object position could not simply
  be used here.

- **line 391**
  New, minor: the parameter bool on of Laser::onElectricitySwitch shadows the
  member double on (laser.h:29).
  Inside the function every on is the parameter, which is what the body wants,
  so nothing is wrong today; but it is the same shadowing shape as the lava
  case above and belongs in the same convention pass. No comment mentions it.

### lava.cpp

- **line 282**
  Object* p_destroyed = p_obj; is assigned and then used only once, on the
  very next statement group, while p_obj remains in scope and is used again at
  line 300 for the same object.
  The alias says nothing the code does not, and having two names for one
  pointer in eighteen lines invites the two to drift apart if the block is
  edited. No comment claims anything about it.

- **line 289**
  Half confirmed: the inner for(int i = 0; i < n; i++) does shadow the
  enclosing std::vector<Object*>::const_iterator i (lava.cpp:269). The
  companion claim about laser.cpp:357 is wrong - there is no enclosing i
  there.
  In laser.cpp the beam's while(true) loop closes at line 303, and the debris
  loop at 357 sits inside if(!infinity) with no iterator in scope, so nothing
  is shadowed. The lava case is real: the outer iterator is live across the
  inner loop, so a future use of i inside the debris block would silently pick
  up the int. Worth a rename in the convention pass, not here.

### linux_window.cpp

- **line 85**
  setFixedSize does XFree(p_hints) inside the if-block and then returns
  p_hints != 0 on line 85, i.e. it reads a pointer variable whose value is
  indeterminate after the free.
  Harmless in practice (the value is only compared, never dereferenced) but
  formally an indeterminate read; a saved bool would say the same thing.
  Noticed while checking the header's claim about false; not fixed, no comment
  describes it.

### make_icon.py

- **no line given**
  'A phone scales an image that small up for the home screen itself' -> '...
  up itself for the home screen'; and the dangling participle 'Enlarged
  beforehand by pure pixel replication, every edge stays hard' -> '..., the
  image keeps every edge hard'
  The German 'selbst' says the phone does the scaling; with 'itself' stranded
  after 'home screen' it reads as 'the home screen itself', which is a
  different claim. The participle attached to 'every edge', but what is
  enlarged is the image. Both are docstring-only; the docstring is printed as
  the usage text by 'raise SystemExit(__doc__)', so it is user-facing.

### make_text.py

- **no line given**
  Pre-existing, not translation: the 'empty text' SystemExit at line 95 is
  unreachable - line 93's max() over an empty glyph list raises ValueError
  first. Identical in HEAD; I confirmed both versions traceback the same way.
  Reported rather than repaired, per the rule against fixing source inside a
  translation pass. Also pre-existing: as_js's docstring writes d:"..." while
  the code emits d:'...', and make_icon.py's usage block does not mention the
  third positional argument that main() accepts as the canvas size (line 167).
  All three were the same in the German.

### menu.xml

- **line 7**
  NEW. "column 0 unpressed, column 80 and 210 respectively pressed" holds only
  for the large cells. The small buttons' unpressed column is 160, not 0
  (selectlevel.xml: Image u="160" u2="210" on all five 50x50 buttons), which
  is also what "packed with no gaps" implies - large at 0 and 80, small at 160
  and 210.
  The German says "Spalte 0 ungedrueckt" without qualification and was
  rendered as written. It misleads only outside menu.xml, which has no small
  buttons; the file that does have them is selectlevel.xml, whose own comment
  repeats the offset rule but not the column layout.

- **line 85**
  '92 pixels are enough for each of the eight captions' counts the four kind
  radios plus the four bottom buttons, but the Refresh button at line 120 is
  also 92 wide and carries a caption ($TR_REFRESH).
  Refresh uses tooltip_font.xml where the eight use the default font, so it is
  plausibly excluded on purpose - but the count reads as covering every 92px
  caption in the pane, and it does not.

- **line 87** [refuted]
  PARTLY REFUTED (first agent's flag). "92 pixels are enough for each of the
  eight captions" counts the four kind radios plus the four bottom buttons,
  which is exactly what the sentence names; the Refresh button is neither a
  kind radio nor one of the bottom four, so the count of eight is right for
  the group described. It remains true that Refresh (line 120) is a ninth 92px
  caption sitting in the fourth column (x=298), so a reader checking every
  92px caption in the pane finds one the count does not cover.
  Refresh draws $TR_REFRESH ("Refresh"/"Aktualisieren") in tooltip_font.xml
  while the eight use the default font, so it is plausibly excluded on
  purpose. Comment left as it stands.

### mobile.js

- **no line given**
  Considered and deliberately rejected: aligning line 278's 'every payload
  file carries the build stamp' with CLAUDE.md's and the neighbouring
  comment's 'the build's stamp'.
  The string is single-quoted JavaScript, so the possessive would need an
  escaped apostrophe - a real escaping hazard bought for a purely cosmetic
  gain. 'the build stamp' is correct and unambiguous, so I left it. Mentioning
  it only so the next reviewer does not spend the same minute on it.

### selftest.py

- **no line given**
  The docstring says the change 'goes back in a finally', but the restore
  lives in Patch.__exit__, a context manager - there is no finally in the
  file.
  The German said the same ('geht in einem finally zurueck'), so this is a
  faithful rendering of a claim that was already loose. Reported rather than
  repaired, per the rule that a translation pass does not fix the source.

### shell.html

- **line 176**
  checkbatch.py reports CODE CHANGED for shell.html although no code moved.
  codeonly.py maps .html to strip_xml, which removes only <!-- --> comments.
  Every // comment inside <script> is therefore treated as code, so
  translating the JS comment above var B5_BUILD trips the check. Re-reducing
  the file with a stripper that also removes C-style comments inside <script>
  gives output byte-identical to HEAD. The checker's .html stripper needs to
  run strip_c over <script> blocks; no source change is called for.

### sound.cpp

- **line 102**
  The createInstance() comment ends 'behind it waits a null pointer with
  setVolume() on it in the next line', but at the call site setVolume() is two
  lines further on, not the next one.
  player.cpp:50 assigns p_toxicSoundInst, line 51 releases the Sound and line
  53 calls setVolume(0.0); the same shape at lines 59/60/62 for mask. The
  German said 'in der naechsten Zeile' and was translated as written. The
  claim it is making - that a returned 0 is dereferenced immediately after -
  is correct; only the line count is loose.

### sprite.cpp

- **line 81**
  Sprites::add() silently overwrites the last sprite once numSprites has
  reached MAX_SPRITES.
  `if(numSprites < MAX_SPRITES) numSprites++;` is followed unconditionally by
  `Sprite& sprite = sprites[numSprites - 1];`, so a caller that adds one
  sprite too many gets a reference to the previous one and replaces it rather
  than being refused. Saturating like this may well be deliberate, and no
  comment claims otherwise - noted because nothing in the file says which it
  is.

### streamedsound.cpp

- **line 105**
  The pumpBuffers() comment's "stops the source without emptying it" leaves
  the same pronoun ambiguity the German had ("ohne sie zu leeren" - the source
  or the queue).
  Both readings land on the same fact - the queued buffers survive the
  underrun - so I kept the ambiguity rather than resolving it in a direction
  the German does not commit to. Noting it in case the author wants the noun
  spelled out.

### strip_xml_comments.py

- **no line given**
  The two ValueError texts ('CDATA with no end', 'comment with no end') are
  effectively unreachable from main().
  main() calls readTree() first, and a document with an unterminated comment
  or CDATA section is not well-formed, so tree is None and the file is passed
  through before stripComments() ever runs. Translated anyway; the messages
  render correctly when stripComments() is called directly. Not introduced by
  this pass.

### sw.js

- **line 10**
  checkbatch.py reports 6 German comment lines in sw.js and 8 in shell.html;
  all 14 are English.
  findgerman.py's DE word list contains "so" and "also", which are English
  words, and the match is case-insensitive with no English counter-test.
  Thirteen of the fourteen flagged lines were already English at HEAD
  (sw.js:10,12,15,19,24,27 and shell.html:48,60,103,116,140,147,212); the
  fourteenth, shell.html:177, is a newly translated line that uses "so"
  correctly. ALL CLEAN is unreachable on these files without rewriting sound
  English to dodge the heuristic, so I left the prose alone.

### tileset.h

- **line 16** [refuted]
  REFUTED as a defect in the comment. "all nine tileset.xml in the tree" is
  internally consistent with the enumeration the same sentence gives: the four
  shipped skins, their archives and the third-party lego skin = 4 + 4 + 1 = 9.
  Git holds five
  (Blocks5/levels/skins/{blocks_01,blocks_02,blocks_03,space}/tileset.xml and
  Blocks5/misc/3p_skins/lego/tileset.xml); the other four live inside
  levels/skins/*.zip, which pack.sh builds and .gitignore excludes. The German
  counts the archives deliberately, so the number is right for a packed tree
  and the translation carries it faithfully. Nothing to fix here.

- **line 17**
  "all nine tileset.xml in the tree" counts the four skin archives.
  Only five tileset.xml are in Git (four shipped skins plus
  misc/3p_skins/lego); the other four live inside levels/skins/*.zip, which
  are gitignored build products. The count is right for a packed tree and
  wrong for a fresh checkout.

### u_crt.h

- **line 36**
  The first agent's doubt about getOverscan()'s unit ("fractions of half the
  picture width" on line 36 against "fractions of the picture" on line 77) is
  largely refuted by the code, and both comments are translated as written.
  u_crt.cpp:580-583 computes fade = 2.0 * (crtEdgeRows * 2.0 / frameSize.y)
  and fringe = 2.0 * crtConvergenceMax / frameSize.x. The division by
  frameSize is exactly what the member comment claims - it turns source rows
  and columns into fractions of the picture - and the separate factor 2.0 then
  converts to fractions of half the picture, which is the unit the
  getOverscan() comment names and the right one for warp coordinates running
  -1..1 (u_crt.cpp:592-596 uses k = 1 + getOverscan() on such coordinates).
  The two comments describe different steps rather than contradicting each
  other; at most the member comment is loose. No factor-of-two error in the
  code.

### u_sharpfit.cpp

- **line 36**
  Wording forced by the checker, not by the German: "100 unter GLSL ES" is
  rendered "100 wherever GL_ES is defined" (same in upscaler.cpp line 10).
  findgerman.py's word list contains "es", matched case-insensitively, so any
  comment line containing the standalone token "ES" is reported as German and
  checkbatch.py refuses to go clean. GL_ES is one word to the regex and is
  exactly the macro the shader's own #ifdef tests, so the claim is unchanged -
  but if a later pass relaxes the detector, "under GLSL ES" is the more
  idiomatic wording. The same list also flags English "so" and "was", which is
  why several sentences read "such that", "therefore" and "Hence" rather than
  "so that", "and so" and "So".

### u_smooth.h

- **line 8**
  The German "Verwaschen ist sie trotzdem" has no grammatically correct
  antecedent - the only feminine nouns in reach are "die Hardware" and "die
  Arbeit", and neither is what is washed out.
  The English resolves the pronoun to "The picture is washed out all the
  same", which is the only sensible reading and is what the glossary's
  "resolve the pronouns" rule asks for. Reported rather than left ambiguous,
  in case the author wants the German-side wording noted; the translation
  itself needs no change.

### verify.py

- **no line given**
  FIXED - smaller Germanisms: 'without libs/ and the build output', 'shows up
  only in that the button no longer does anything', 'says a sound belongs
  quieter', 'expected is something between 0 and 1', 'exactly as File_Real in
  file_real.h', 'belong to whatever lays them out'.
  Each is German word order carried into English. Rendered as 'not libs/, not
  the build outputs', 'shows up only as a button that no longer does
  anything', 'says a sound should play quieter than its file' (CLAUDE.md's own
  wording), 'expected something between 0 and 1', 'just as File_Real', 'belong
  to whatever declares them'. The sound_volumes summary also became 'must
  exist, with a factor under 1', so every --list line that states a rule
  states it with 'must'.

- **line 3**
  The module docstring, all eleven check docstrings, and every user-facing
  message string in verify.py, selftest.py and strip_xml_comments.py are still
  German after this pass.
  Python docstrings are string literals. codeonly.py's strip_hash deliberately
  keeps triple-quoted runs as code (its own comment says so), and I confirmed
  by experiment that changing one docstring word makes checkbatch.py report
  CODE CHANGED. Rule 4 forbids touching string literals besides. This is by
  far the largest body of German left in these files - roughly 150 lines in
  verify.py alone, including the naming, ctor_init, sounds and comments
  rationales - and it needs a deliberate decision (relax the code-token
  reduction for .py docstrings, or exempt these three files) rather than a
  quiet exception.

- **line 348**
  The comment "The GUI reads its attributes through readAttributes(), not
  through these names; the XML is written there, not read" sits above a return
  that implements no GUI exception at all.
  Nothing in check_xml_attrs excludes gui_*.cpp, and no gui_*.cpp calls
  SetAttribute/SetIntAttribute/SetDoubleAttribute, so the rule the comment
  states is neither implemented nor needed by the current code. Either an
  exclusion was dropped from the code or the comment is stale. Rendered
  literally.


Typos and formatting
--------------------
Spelling, indentation, a trailing space. Harmless, and cheap to fix while
passing.

### diamondmachine.cpp

- **line 277** [confirmed]
  CONFIRMED: the German at HEAD line 277 spelled it "zurueklegt" (one c
  missing). Gone with the translation; the English carries the same claim
  ("the distances it really covers").
  Reported because the sweep asks for source typos to be listed rather than
  silently corrected.

- **line 275**
  The German comment about the number of moves spelled it "zurueklegt" (one c
  missing) - now gone with the translation.
  Noted only because the sweep asks for typos in the source comments to be
  reported rather than silently corrected; the English line carries the same
  claim ("the distances it really covers").

### engine.cpp

- **line 1791** [confirmed]
  CONFIRMED. `printfLog(" Render target: %dx%d in a %dx%d texture\n")` pads to
  column 20, matching the Vendor/Renderer block at lines 510-518, while the
  two neighbouring lines from the same feature - " Shaders: switched off
  (-noshader)" (line 1666) and " Framebuffer objects: switched off (-nofbo)"
  (line 1737) - pad to column 24.
  The three describe one subsystem and print next to each other, so the ragged
  column shows in every log. Cosmetic, and it lives inside string literals
  this pass may not touch.

- **line 1792**
  The startup log's label column is inconsistent: printfLog(" Render target:
  %dx%d in a %dx%d texture\n") pads the value to column 20, matching the
  Vendor/Renderer/Double buffering block at lines 510-518, while the two
  neighbouring lines from the same feature - " Framebuffer objects: switched
  off (-nofbo)" (line 1738) and " Shaders: switched off (-noshader)" (line
  1666) - pad to column 24.
  The three lines describe one subsystem and print next to each other, so the
  ragged column is visible in every log. Cosmetic only, and it lives inside
  string literals that this pass may not touch. Reported, not fixed.

- **line 3508**
  NEW. The German original had a doubled word across the line break: "... der
  Zeiger sitzt auf dem Glas. Ohne" / "// Ohne Woelbung gibt warpToSource die
  Koordinate unveraendert zurueck." The English carries the meaning once and
  the duplication is gone.
  Rendering "Without / Without curvature" would have been nonsense, so the
  translation silently repairs it. Recording it because the sweep is not
  supposed to repair the source without saying so - the fault was in the
  German comment, not in the code.

### file_archived.cpp

- **line 280** [confirmed]
  CONFIRMED. " uLong crc = crc32(0, 0, 0);" is indented with four spaces where
  the rest of the file (and the tree) uses tabs.
  Code, untouched by this pass; noted for the convention/formatting pass.

### gl_compat.cpp

- **line 20** [confirmed]
  CONFIRMED as archaeology - '// (immediate-mode variants moved to
  gl_immediate.cpp)' says what the file used to contain, against CLAUDE.md's
  rule.
  It was already English at HEAD, so it is not a product of the German sweep
  and was left alone by both passes. A cross-reference ('the immediate-mode
  variants are in gl_immediate.cpp') would keep the useful half and would also
  give the missing section 1 heading somewhere to point.

### gui_multilineeditbox.cpp

- **line 306** [confirmed]
  Trailing space after `else` ("\telse ").
  Pre-existing in HEAD, untouched by this pass; noted for the formatting pass
  since the tree is otherwise free of trailing whitespace here.

### hint.cpp

- **line 213** [confirmed]
  CONFIRMED in HEAD: "beim Schritt von einem Zettel auf den nachbarn" -
  lowercase "nachbarn" for the noun Nachbarn.
  A German spelling slip that disappears in English ("to its neighbour").
  Nothing left to fix in the file; reported because the sweep is meant to
  surface it.

### main.cpp

- **line 506** [confirmed]
  Line 506 (between the data.zip pushCurrentDir and the "Alternatively"
  comment) is a lone tab - trailing whitespace on an otherwise blank line.
  Pre-existing and untouched; noted for the formatting pass rather than fixed
  here, since only comments may move in this sweep.

### presets.cpp

- **line 88** [confirmed]
  CONFIRMED. The preset type string is "Amboss" (line 88 and line 369) while
  its tooltip id is "$TT_AMBOS" with one s (line 376); languages.txt line 843
  spells it $TT_AMBOS too, so the id is self-consistent.
  Both are string literals and were left untouched: the type name is written
  into every level file that contains one, and the id has to keep matching
  languages.txt. Noted only because the two spellings of the same German word
  differ.

### vec.h

- **line 304** [confirmed]
  The German comment reads "Multipliktion mit Skalar von links" -
  Multiplikation misspelled.
  Rendered as the intended "multiplication by a scalar from the left" rather
  than carried across as a typo, per the rule that the meaning is what gets
  translated. Flagged so the record shows the German was misspelled and the
  English is not a silent rewrite.

### videorecorder.cpp

- **line 329** [confirmed]
  CONFIRMED, and already resolved. The German at HEAD read "wird die Datei
  blosss kleiner" - "blosss" with three s.
  A plain typo in the pre-translation text. It does not survive into the
  English ("the file merely gets smaller"), so there is nothing left to fix in
  the working tree; recorded because the sweep reports typos rather than
  silently absorbing them.

### campaign.cpp

- **line 16**
  The makeMemberName() comment ends with the cross-reference "(campaign.cpp,
  save())" while sitting in campaign.cpp itself, a few hundred lines above
  that save().
  The German has exactly this, so it was rendered as written; the filename
  half of the reference tells a reader of this file nothing.

### engine.h

- **line 228**
  consumeKeyPress: the comment calls the bit the "pressed in this frame" flag
  (German "das \"in diesem Bild gedrueckt\"-Kennzeichen"), but bit 2 of
  keyData is cleared once per *logic tick*, not per rendered frame -
  engine.cpp:1115 clears `keyData[i] &= ~(2 | 4)` inside the
  `while(timeToProcess >= logicRate)` loop, and engine.cpp:992 itself says
  "consumeKeyPress() covers only the same tick".
  In the browser most rendered frames run no logic tick, so "frame" and "tick"
  are not interchangeable - the tree's own timing argument turns on that
  distinction. Translated faithfully from the German rather than corrected.

### pre.js

- **line 126**
  Line 126 says 'the on-screen controls - a sibling of it - simply vanish';
  CLAUDE.md and the glossary (Bildschirmsteuerung -> 'on-screen pad') call the
  same thing the on-screen pad.
  Not a translation defect - this is unmodified HEAD prose written in English
  by the author, so it is outside this sweep's diff and changing it would be
  churn on committed code. Flagging it only so a later consistency pass can
  decide whether the tree should say 'on-screen pad' everywhere. Deliberately
  not touched.

### smoke.sh

- **line 87** [refuted]
  REFUTED as a meaning-changing typo: the German "also lande jeder Klick
  daneben" is a Konjunktiv I in a Konjunktiv II chain ("Bliebe ... saehe ...
  also lande ...").
  Unusual but grammatical and unambiguous - it is the consequence of the
  counterfactual, not a present-tense assertion. "every click would land in
  the wrong place" is the right rendering, and no note in the file is needed.

### web_bluescreen.h

- **line 15**
  "A key press or a click reloads the page" - the code also registers restart
  on touchstart (web_bluescreen.cpp:94), which the comment does not name.
  The German ("Ein Tastendruck oder Klick") is equally incomplete, so this is
  faithful, not a translation error. On a phone the touch is the only way in.
  Not fixed.

