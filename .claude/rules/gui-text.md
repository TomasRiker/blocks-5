---
paths:
  - "Blocks5/src/gui*.{cpp,h}"
  - "Blocks5/src/font.{cpp,h}"
  - "Blocks5/src/help.{cpp,h}"
  - "Blocks5/data/*.xml"
  - "Blocks5/data/languages.txt"
---

# GUI, fonts, text and localization

**Text is cached as laid-out geometry, keyed on what it was laid out with.** `Font::renderText` looks a
string up in a cache — the glyph quads and, in a batch of their own, the keycap frames, which carry no
texture — and draws them three times: twice as a shadow, once as the text. The key is the string plus every
option the layout depends on (`tabSize`, `charSpacing`, `lineSpacing`, `charScaling`, `italic`), and
deliberately not `shadows`, which changes nothing built. **That key is what lets `setOptions` leave the
cache alone**: callers change those five constantly — a speech balloon sets `italic` and puts it back every
frame it is on screen, the credits animate `charScaling` — so a cache emptied on every change threw away
every cached string in the GUI twice a frame. Measured on the help page: median frame's render half **3.1 →
2.5 ms**; menu 1.7 → 1.5, editor 0.9 → 0.8.

**The cache is budgeted in quads, and the budget is shared.** `QUAD_BUDGET` (8192, half a megabyte of glyph
geometry), not a count of entries per font, which is the wrong unit twice: an entry is a
`std::vector<QuadVertex>` at 64 bytes a character, so thirty-two keycaps and thirty-two wrapped help pages
are the same number and two orders of magnitude apart, and four fonts holding 32 each is not one budget
but four. Eviction takes the oldest entry of any live font, from a registry `Font` keeps of itself, so a
font barely used stops holding what a busy one needs. The stamp is a counter and not `SDL_GetTicks()`, which
wraps at 49.7 days and then makes every standing entry look newer than every fresh one — a cache that evicts
what it has just built, for ever.

**`renderText` takes a `cache` flag for a string whose layout will not be asked for again**, a parameter and
never part of the key (which would double every entry asked for both ways). One caller uses it: the credits,
whose `charScaling` is `0.75 + 0.25 * alpha` and animated, so both draws build a key no frame will reuse.
Measured over six seconds of credits, **212 evictions → 0**. Everywhere else the cache was already working:
across the five oracle scenes plus the help page every lookup hits, with zero evictions, and the most it
ever holds is 825 quads / 52 KB. The memory was never the problem; the unit and the missing ceiling were.

**Measuring is cached too, in two tiers, and the first costs nothing.** `measureText` is asked about twice
as often as anything is drawn — `fitText` runs a binary search with one per probe, `adjustText` one per run
and per line. A laid-out string carries its own dimensions, so everything both measured and drawn is
measured free: every GUI widget, each asking its caption's size in the
`onRender` that draws it. The second tier is for strings nothing draws — the runs `adjustText` wraps, the
candidates `fitText` probes — and holds dimensions and no geometry, which for those would be 64 bytes a
character that never reaches the screen. Measured, every measure in the five oracle scenes was a walk and
none is; the help page goes from 2448 walks to **66 of 2376**, and those are the deactivated `GUI_EditBox`
behind the page asking for character positions every frame.

**That path is uncached on purpose.** The positions depend on the `offset` the caller passes, which is no
part of the key, and there is one per byte of the string; the four callers are the edit boxes, of which one
is on screen at a time.

**The dimensions cache is budgeted in bytes of key** — an entry is two numbers, so what one costs is how
long its key is — and nothing comes near it (the help page holds 22 entries and 1.1 KB). `DIM_BUDGET` is
64 KB, a ceiling for the one shape that could grow without one: stepping through a campaign, where every
level measures a fresh set of `fitText` candidates and a folder of single levels has no length anybody
promised.

**Two orderings inside it are load-bearing.** The dimensions are measured *before* the geometry entry is
inserted, because `measureText` reads that same cache and an entry standing in it but not yet measured would
answer with whatever was in the field. And `lookUpText` holds a *copy* of its key across the build, because
`cacheKey` returns a reference into one buffer per font and the measure builds a key of its own into it.

**GUI** (`gui.cpp`, `gui_*.cpp`) is a retained-mode tree loaded from XML dialogs in `data/`
(`menu.xml`, `leveleditor.xml`, `options.xml`, …). Elements are addressed by dotted path —
`gui["Menu.DonatePane.Donate.Donate"]` — and wired with `sigslot` (`connectClicked(this,
&GS_Menu::handleClick)`); a game state that connects signals must derive from `sigslot::has_slots<>`,
which `GameState` already does.

**A click goes to the element under the cursor *now*.** `GUI::update()` recomputes `p_elementAtCursor`
at the **top** of the function, before the enter/leave dispatch and before the button handling — not at
the bottom, which would dispatch every click to whatever had been under the cursor at the end of the
*previous* logic tick. With a mouse that is invisible: you cannot click where the pointer is not, and
between arriving over a button and pressing it there is always at least one 20 ms tick. A finger has no
such gap. The other half is in `Engine`: a touch produces no `SDL_MOUSEMOTION` at all, so both button
events take the position from the event too, not only the motion event. Either half alone changes
nothing; the pair is what makes a tap land.

**A mouse-move event has to mean the mouse moved**, which is not the same question as whether
`cursorPos` changed. That position comes from `Engine::getCursorPosition()` and therefore through the
CRT filter's barrel distortion, so anything that changes the warp moves the cursor in game space with
the hand perfectly still — and the one control that changes the warp is the curvature slider, dragged
with the mouse. The synthetic `onMouseMove` set a new slider value, the value set a new curvature, and
the loop closed: measured with the hand held on the handle, the slider took 249 values in six seconds,
flipping between 0 and 1 at the logic rate. The step is what makes it reach that far — `getOverscan()`
is deliberately zero at curvature 0 and its full 1.3% the moment the slider leaves the stop, which a
third of the way out is a pixel and a half, and the bar turns 1.7 pixels into one unit. So
`GUI::update()` asks for both: the window's own cursor position (`Engine::getRawCursorPosition()`) must
have changed **and** the game-space one must have landed on another pixel. `noMoveCounter`, which times
the tooltips, reads the same answer.

Things about the widgets worth knowing, because getting any of them wrong is quiet:

- **`check()` is the user's click and fires `changed`; `setChecked()` is the display catching up and
  does not** — and `setChecked` must touch only `checked`, never the in-flight `newChecked`.
  `gui_checkbox.h` says why both halves matter and what each one broke.
- **Escape and Return belong to the dialog.** `GUI_EditBox` and `GUI_ListBox` forward both to the parent
  when they have nothing of their own to do, which lets a dialog implement Escape = Cancel and Return =
  OK while focus sits in a text field or a list.
- **A Ctrl combination is handled and done.** Both edit boxes take Ctrl+A, C, X and V in the letter's
  own `case`, and that case must `break` rather than run on into the character insert below it: what
  unicode such an event carries is the platform's choice, and under X11 it is the letter itself —
  measured, Ctrl+A selected everything and typed an "a" over it. Without Ctrl the same case falls
  through, because the letter is then text like any other.
- **A checkbox or radio button is hit on its caption too.** The caption is drawn by the toggle itself at
  `size.x + 10`, and `containsPoint` — a virtual on `GUI_Element`, which `getElementAt` calls instead of
  testing `size` inline — counts that strip as part of the control. The width is *measured*, not
  assumed: a fixed strip would steal clicks from whatever sits to the right, and options.xml puts
  language and detail radios in three tight columns. An empty `<Title>` measures zero, so a toggle that
  delegates its caption to a `<For>` label is unaffected.
- **Any element can carry `for="Name"`**, as `<label for>` does in a browser — it lives on `GUI_Element`,
  not on the text class, because a label is not always text: the two language flags in `options.xml` are
  `<StaticImage>` and belong to their radio button exactly as the word beside it does. A checkbox or
  radio target gets the whole set of mouse events forwarded (enter/leave included, since a toggle only
  fires on mouse-up if it believes the cursor is over it); anything else — an edit box, a list — just
  gets the focus, because forwarding a position measured against the *label* would drop an edit box's
  caret in an arbitrary place. The attribute is read in `GUI_Element::load`, which is not virtual:
  `readAttributes` is, and no subclass chains up to the base version, so a `for=` parsed there would
  work on some element types and silently vanish on others.
- **A `<StaticText>` label can size its own hit area.** Give it `w="-1" h="-1"` and it matches the text
  it actually draws, re-measured per frame so it follows a language switch; a hand-written width would
  be a guess that is wrong in the other language. `w`/`h` of 0 — the default — is still never hit. An
  image needs none of this: it already has the size of the sprite it shows.

**Text written to a fixed place has to be measured first.** `Font::renderText` neither wraps nor clips,
so a level whose title is longer than the space kept for it draws over whatever is beside it — in the
select screen across the description column and off the right edge, in the status bar across the Menu
button. `Font::fitText(text, maxWidth)` cuts it down and ends it in three dots; `Font::adjustText` wraps
instead, which is what the multi-line help pages want. Both skip over `<h>…</h>`: it draws nothing, so
it must not count toward a line's width, and a hard break landing inside it turned the markup into
visible text. The two callers keep the level *number* and the *filename* whole and shorten only the
title, since those are what tells two levels called *Unnamed Level* apart: the caption is measured once
with an empty title to learn what the frame costs, and the title gets the rest. A second pass over the
finished caption is the backstop for a filename so long that even the frame does not fit.

The cut may not land inside `<h>…</h>`, so `fitText` drops a half-cut tag entirely and closes whatever it
left open. **`<h>` ends with the string it began in**, and both `measureText` and `buildText` enforce
that with a counter: the option stack they push on belongs to the `Font` and not to the text, so markup
could never have carried across a call anyway. Without the counter an unclosed `<h>` leaves `italic` set
for the rest of the run and an extra `</h>` pops the *caller's* entry or `top()`s an empty stack — a level
titled `</h>Hello` crashed the game, and a level title is a file from a stranger.

**`measureText`'s position array is indexed by byte**, one entry per byte of the string and one behind it
— `text.length() + 1`, always. The bytes of `<h>` and `</h>` get an entry each even though they draw
nothing, all carrying the cursor the tag stands at. That is what the three callers need and already
assumed: the edit boxes look a position up under the same byte index their caret uses, and
`GUI_MultiLineEditBox` reads `[i + 1]` to size a selection. One entry per loop pass instead left the
array short — two per `<h>`, three per `</h>` — so a caret at the end of such a text read past the
vector.

**`<k>…</k>` is the keycap**, and the font draws the frame. It cannot go in the glyph batch — it carries
no texture — so the rectangles are collected while the text is laid out and drawn once the batch is
closed, which also carries them through the two shadow passes with the glyphs; a keycap without the same
shadow would look pasted on.

**The frame is drawn on exactly rows `capTop`..`capBottom` of a glyph cell**, two optional `<Font>`
attributes, and nothing about it is derived from the line. `lineHeight` and `offset` describe the line a
font is *set* at, and the ink is free to sit elsewhere in either direction: the note's font ends its
letters five rows above the foot of its line box, so a frame drawn on the line box sits under the word
instead of around it — while the tooltip font's letters are *taller* than its line, since `Backspace`
reaches a row above the capitals and a row below the baseline and a ten-row line has room for neither.

**That second case is why the frame carries its own height rather than the line's.** A frame fixed at the
line height and merely centred cannot be placed in a font whose ink does not fit inside the line: only
the sum of the two attributes is read, so every pair with the same sum gives the same frame and the next
sum moves it a whole row — always one row too high or one too low, with no third option. Saying the frame
outright is also what makes `verify.py`'s `font_metrics` check a straight comparison against the measured
ink. They default to the line box, which is what `font.xml` and `credits_font.xml` measure out to anyway.
Three files carry them: `tooltip_font.xml`, at rows 1..12, and the two skins that bring a `hintfont.xml`.

**Data, and not the image measured at every start.** Where a font's letters sit is a constant of the art,
and a statistic recomputed at load would move every keycap in the game by a pixel because somebody redrew
one glyph — silently, with nothing in any diff to point at. `verify.py`'s `font_metrics` check is the
other half of writing it down: it reads the first and last inked row of every printable character out of
the font's PNG, takes the **mode** of each — the top of a capital and the line the writing sits on, where
a brace reaches higher and a comma lower than anything a key is ever called — and reports a font whose
frame would cut into its letters or sit off to one side, naming the two numbers to write. Same arrangement
as the committed `.ico`. (`read_png` in `WebBuild/make_icon.py` learned the narrow bit depths for it —
`credits_font.png` is a two-colour palette at one bit.)

**A keycap is an atom to `adjustText`.** A box cannot be broken across two lines, so the whole `<k>…</k>`
run moves down together, the way any typesetter treats an inline box — and the renderer is then never
asked to draw half a frame. That is why the run is measured rather than walked character by character:
the padding either side belongs to its width.

**The right side of the frame carries the slant.** An italic glyph leans right — its top is drawn
`options.italic` pixels further along than its foot, while the cursor advances by the upright width — so
a frame that ends where the cursor does cuts the last letter of the key name. That is every speech
balloon, which sets italic for the whole text. The advance after `</k>` grows by the same amount, or the
following word would move into the frame instead; the left side needs nothing, since the first letter's
foot still stands on the cursor.

**Between keycaps that belong together stands a half space** — `HALF_SPACE` in `font.h`, half of that
font's own space and a space in every other respect: measured like one, and a line breaks at one and
replaces it exactly as a break replaces a space. Each keycap already stands off its own frame, so a full
space either side of the slash leaves it adrift between the two keys instead of the pair reading as one
binding, and the same holds for the plus of a chord: `<k>Alt</k>·+·<k>Enter</k>`. It is a byte rather than
an element like `<k>` because breaking is a matter of characters: `adjustText` searches backwards for the
last one it may cut at, and an element would have to be taught to be a break as well as to be skipped over.

The byte is the **middle dot**, `\xB7` — the character an editor shows a space as, and the third of this
file's meaningful bytes beside `§` and `¶`. It has to be printable because the chords are written out by
hand in `languages.txt` (a `%BINDING{…}` cannot say *Alt*), and a control character there would be
invisible to whoever edits the line. `Engine::getBindingMarkup` writes the same byte around its slash. The
plus that joins a key to a *word* keeps its full space — `%BINDING{$A_PLANT_BOMB} + direction` — so the
help table shows the hierarchy: tight where keys bind to each other, loose where prose follows.

**A keyboard has two Enter keys and the game tells them apart nowhere.** `isReturnKey` (`util.h`) is the
one place that says so, and everything reading the SDL key itself goes through it: confirming a dialog,
playing the selected level, leaving the credits, the level editor's settings, and Alt+Enter for the
fullscreen. The named actions never needed it — a binding has a primary and a secondary, and
`$A_SAVE_IN_HOTEL` has used both since it was written. Both are called `Enter` in both languages — `Enter`
and `Num Enter` — which is the prefix the other seventeen keypad keys already carry. It is what a PC keycap
prints (a German board prints the hooked arrow and no word at all) and the word every neighbouring language
borrowed. `Return` was SDL's own name for the key, and since the fallback capitalises SDL's name, the
English entry for it was replacing nothing.

**Short messages are the Engine's, not a game state's.** `Engine::showToast(type, text, duration,
suppressSound)` slides a bar in at the top edge, holds it, and slides it out again — green for `TOAST_OK`,
red for `TOAST_ERROR`, 2 s and 4 s by default, with `teleport_failed.ogg` on an error unless the caller
says otherwise. It is drawn at the very end of `Engine::render`, after `GUI::display`, so it sits over the
GUI, the editors' panes and everything else. Everything with something short to say goes through it — both
editors, the menu, and four failures that would otherwise reach only the log: a skin missing or unloadable
(`Level::loadSkin`, one message per skin *name* rather than one per missing file), a music track that
cannot be opened (`Engine::playMusic`), a level that will not load (`Level::loadErrorLevel`, where all
three of `Level::load`'s failure paths already met), and a campaign that will not load (`Campaign::load`).
Each names the bare filename: the full path leads through an archive and its password and tells nobody
anything.

Two callers opt out. `Campaign::load` takes a `quiet` flag for `isImportableArchive`, which only asks
whether an imported file *is* a campaign — a skin that is not one is not a broken campaign. And
`loadErrorLevel` says nothing for the editor's palette levels (`cat<N>.xml`), which belong to the game and
are not a file anybody asked for. In the select-level preview the skin and level messages come without the
sound, because stepping through a broken campaign would otherwise beep at every keypress.

Several messages stack: the newest takes the top edge and pushes the older ones down a bar each. One that
has run out slides up by exactly one bar height, which puts it off the screen if it was on top and *behind*
its younger neighbour if it was not — hence the draw order, oldest first. Position and opacity move
together, 0.1 s each way and not counted against the hold time, because the bar is not fully opaque. Asking
twice for the same text and type does not stack two copies; the hold time becomes the longer of the two and
the sound plays again, because the sound answers the click and not the message.

**Language on first start** is the system's, not English. `Engine::detectSystemLanguage` asks
`GetUserDefaultUILanguage` on Windows, `navigator.languages` in the browser and `LANG` elsewhere, and
answers only `de` or `en` — every one of the 440 IDs in `languages.txt` has an English body and a German
one and nothing else, so detecting `fr` would give a wholly English game that merely believed otherwise.
The one `§fr:` and `§es:` in that file are its own header explaining what the tags mean. It runs only when
`config.xml` has no `<Language>`. Nothing ships a `config.xml` template — not the installer, not the web
build — which is what leaves the detection a chance to run at all.

**Localization.** Any user-facing string starting with `$` is an ID resolved against `data/languages.txt`
by `Engine::localizeString` / the free `loadString` helper. In that file a `$ID` line is followed by
per-language bodies tagged `§en:`, `§de:`, `§fr:`, `§es:` — that prefix is the section sign, 0xA7 in
Latin-1, not the pilcrow. A separate character, `¶` (0xB6), inserts a newline inside a body. Missing
translations fall back to English. Level titles, tooltips and menu captions in XML all use these IDs.

**No string names a key.** A message that writes "(F5)" or "Return/Enter" into its text is a lie to everyone
who rebound anything, so `%BINDING{$A_RESTART_LEVEL}` stands there instead and expands to whatever that
action is bound to now: both keys separated by a slash, one on its own, or the word for unassigned.
`%BINDING_OPTIONAL_RIGHT{…}` is the same with a leading space and *nothing at all* when the action is
unbound, which is what a button caption naming its own shortcut wants — "Restart Level" and not "Restart
Level ". `Engine::expandBindings` does it once, on the finished text, which is why `localizeString` is a
shell around `localizeStringRaw`: the raw half recurses through the `$ID` lookup and the English fallback,
and expanding on the way out of each would be the same work several times over. `verify.py`'s `bindings`
check reads every marker against the `registerAction` calls in `main.cpp`, because an action that does not
exist expands exactly like an unbound one and would otherwise be found by a player.

**`VirtualKey::niceName` is what a player reads**, as against `name`, which is SDL's, and `id`, which
config.xml holds and can therefore never be translated. It is a `$ID` and not finished text because the
language can change while the game runs. The table in `engine.cpp` spells each id out rather than composing
it from the key name, so that `verify.py`, which collects `"$…"` literals from the source, catches one that
`languages.txt` does not have; a key with no entry keeps SDL's own name with the first letter raised, which
is all `F5` needs. A joystick keeps a device word in front, localized separately, because `B3` beside a
keyboard key would say nothing about where it is.
