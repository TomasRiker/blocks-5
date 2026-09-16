---
paths:
  - "Blocks5/src/{file,file_archived,file_real,filesystem,progressdb,transfer,campaign,gs_selectlevel,gs_menu,main}.{cpp,h}"
  - "Blocks5/src/manager.h"
  - "Blocks5/src/util.cpp"
  - "WebBuild/web_transfer.{cpp,h}"
  - "Blocks5/data/menu.xml"
  - "Blocks5/data/selectlevel.xml"
---

# The virtual filesystem, the progress database and the Manager

**Virtual filesystem.** `FileSystem::openFile` serves either a real file or a member of a zip archive,
selected by path syntax: `archive.zip/file.png` (no password), `archive.zip<plaintextpw>/file.png`, or
`archive.zip[encryptedpw]/file.png` (password encrypted with `PWEncrypt`; see `decryptPassword` in
`util.cpp`).

**`renameFile` renames where the platform can and copies where it cannot** — across a mount, since the browser
stages an upload outside the home directory, and for a member inside an archive, which has no name on disk.
The destination gives way only after the first attempt has failed: POSIX replaces it in one atomic step and
deleting it beforehand would open a window in which neither name exists, while Windows refuses the replacement
and needs the second try. Three callers want exactly that — `retireShadowingCopies`; `Campaign::save`, whose
swap otherwise wrote the whole archive a second time; and the progress database's crash-safety file.

`pushCurrentDir`/`popCurrentDir` maintain a search root, which is how `main.cpp` mounts `data.zip[...]` as the
asset root (the commented-out `fs.pushCurrentDir("data")` beside it switches to loose files for development).
User-writable state — saves, progress, custom levels, screenshots, videos — lives under
`getAppHomeDirectory()` = `My Documents\Blocks 5\`, never next to the executable.

**Levels, campaigns and skins have two roots, and the game folder wins.** What ships stays beside the
executable and is read from there, so it is always as new as the program; the user directory holds only what
the player made or imported. `FileSystem::resolveContentPath("levels/skins/space.zip")` asks the game folder
first and falls back to the user directory, and `isShippedContent` — "it exists in the game folder" — is
simultaneously the definition of undeletable, un-overwritable and un-saveable-over. The paths are absolute on
purpose: a relative one would be resolved inside the mounted `data.zip`.

The order matters and the other one is wrong. User-first lets a stale copy shadow a fresh shipped file — an
installation carrying a private copy of the campaign, the four skins and the two examples, frozen at whatever
version first installed them, is how a skin marker went missing on a machine that had just built the current
sources. Game-first has one cost, and it is why both editors refuse to save under a shipped
name: such a file could never be loaded again.

**Seven files belong to the player even though they ship with the game**, and for them the order is reversed:
`FileSystem::getPlayerFiles` lists the two example levels and the five `readme.txt`, `belongsToPlayer` makes
`isShippedContent` say no for them, and `resolveContentPath` looks in the user directory first, falling back
to the game folder's copy as a template. The two halves belong together — allowing the save while still
answering from the game folder would write a file that could never be read back. The list is hard-written
rather than a directory listing because a working tree also holds the forty-two campaign sources.

Only the five `readme.txt` are *copied* on a first start, and only because nothing in the game reads them:
without the copy they would sit in no folder at all. The examples are not copied — they are listed and
loadable straight out of the game folder, and the player's own version appears the moment they save one, so an
untouched installation keeps getting the newest examples and Delete stays greyed until there is something of
theirs to delete. That is why the Manager asks `Transfer::isRemovable` ("is there a copy in the user
directory") rather than `isBuiltIn`.

**On the first start of 1.2.0 the old copies are set aside.** `retireShadowingCopies` in `main.cpp` renames
every file in the user directory whose name the game folder also has to `<name>.bak` — renamed and not
deleted, because there is no way to tell from outside whether somebody edited one. `.bak` is inert everywhere:
every lister filters on the exact extension, and `convertPath` recognises an archive by `.zip/`, not `.zip`.

**`ProgressDB` keys on the campaign's bare filename, not its path**: a full-path key would silently reset
everyone's 42 levels the moment `blocks.zip` moved from the user directory into the game folder — no error,
nothing in the log, just a progress bar back at zero. `keyFor` strips the directory on the way in and out,
which migrates a `progress.zip` written with paths by reading it. Case
is **not** folded there, deliberately: under Linux `Blocks.zip` and `blocks.zip` are two different campaigns,
and joining their solved sets could never be undone.

**It holds nothing.** `query()` reads the file and `markSolved()` reads it, adds and writes it back; there is
no map that lives for the process. That is what makes the Manager able to import, merge and delete a progress
at all — a copy in memory would answer from what was there at startup, the next completed level would write it
straight back over the import, a delete would undo itself within one level, and merging would need a `clear()`
the class never had. Merging then needs no code of its own: read the imported file and mark everything solved.

The reads are per frame — `getLevelStatus` and the progress bar are both inside `GS_SelectLevel::onRender` —
so the screen keeps the answer while it is shown and re-reads in **`onGetFocus`, not `onEnter`**: coming back
from a played level is a *pop*, and `popGameState` gives the state underneath the focus without entering it
again, so a level just solved would still be shown unsolved. The count is clamped to the campaign's length, or
a merged database would draw the bar past its own frame and label it *45/42*.

**A save must not destroy what it is replacing.** Writing a member into a zip rebuilds the archive, and
`File_Archived` removes the old file before the new one exists (`remove` at `file_archived.cpp:526`), which
for a one-member archive is every save — so a crash or full disk in that window took everything. The database
is renamed to `progress.zip.saving` first and that file deleted only once the new one stands; `query()` puts
it back where the real file is missing or unreadable, and deletes it where the real file reads. The invariant:
**the backup exists exactly while a save is in flight**, so one found lying about is from a run that died, and
leaving it would mean the next unrelated fault restores a database months out of date.

That parse trusts nothing, because the Manager imports this file and it is therefore a stranger's: no root
element, no `campaign` attribute and a level index that is negative or absurd are all skipped rather than
crashing.

**A level somebody sent you is played from the level select screen, not from the editor.**
`Campaign::loadSingleLevels` builds a campaign that exists as no file: every loose `*.xml` in the
user's level folder, listed last in the campaign box under `$LS_SINGLE_LEVELS`. The editor gives the
puzzle away by design — `level.cpp`
skips the darkness there (`if(nightVision && !inEditor)`) and `teleporter.cpp` draws a line to every
teleporter's destination.

It carries **no progress**, and that is what `isSingleLevels()` is asked about in five places: every
level is unlocked, finishing one records nothing, the run ends back at the selection instead of at the
next level, and both the *next to do* button and the progress bar — frame, label and all — are hidden.
A bar that can never move reads as a fault, not as an empty one. Levels that have nothing to do with
each other have no order to earn.

The list is sorted by the **localized title**, not by filename: that is the line the player reads.
Getting it means parsing each level's XML for the one `title` attribute (`readLevelTitle`), cheaper
than a `Level::load` with all its objects and skins but still a parse per file at dialog entry. The
caption is `formatSingleLevelCaption` — `Title (filename.xml)` — because three levels called *Unnamed
Level* are otherwise indistinguishable, while inside a campaign the filename would say nothing but
`level_2.xml`.

**The whole select screen is keyboard-operable.** Left and right step through the levels, Home and End
jump to the ends, Return plays, Shift+Right is *next to do*, and up and down change the campaign. The
last four go through `GS_SelectLevel::onUpdate` only while the campaign list does **not** hold the
focus, because those are exactly the four keys `GUI_ListBox::onKeyEvent` handles itself; left, right and
Return are unconditional, since the list ignores the arrows and forwards Return for want of a submit
button. Everything runs through `pressButton`, which asks `isActive()` and `isReallyVisible()` first —
otherwise Return would start a locked level the mouse cannot even click.

**One Manager button in the main menu** opens a dialog that imports, exports and deletes, on all three
platforms — `src/transfer.cpp` over `WebBuild/web_transfer.cpp` in the browser, `GetOpenFileNameA`/
`GetSaveFileNameA` under Windows and `zenity`/`kdialog` under Linux, behind one interface: `beginImport`
starts it and `pollImport` is asked each tick, so an asynchronous dialog and a modal one look the same
to the caller.

`Menu.ManagerPane` holds the five kind radios in 92px columns, the list, *Refresh*, and a bottom row of
*Import*, *Export*, *Delete* and *Close* spread over the window's width. The two rows span the same
x=10..486 without sharing a grid: the captions decide the first width — "Zusammenfuehren" and
"Aktualisieren" are what the 92 is for — and forcing that grid on the second would leave a hole where a
fifth button would be. Import needs no selection and comes first; Export and Delete work on the
selection and grey themselves out without one.

**The progress database is the fifth kind, and it goes round the directory machinery rather than through
it.** It is one file, with one name, in the user directory *itself*, and nothing of the sort ever ships —
so `directoryFor` names it outright. An empty subdirectory would have been the obvious answer and is a
trap: `list()` would then read the game folder's own root, where `data.zip` lies, and `remove()` would
point at whatever it found there. `classify` recognises it by a `progress.xml` inside the archive, which
a zip's table of contents answers without the password, and `install` refuses one that does not parse —
the same guard a campaign has, so a damaged file cannot destroy a good one of the same name.

`Menu.ConfirmPane`, which must stay the **last** child in `menu.xml` so it draws last and takes the
clicks, asks before a delete and before an import replaces anything. Three columns, of which the middle
carries *Merge* and is shown only for a progress database, so *Yes* and *No* keep their places either
way; the text is wrapped, since the code sets it and a filename can be any length. **The two buttons are
renamed for an import** — *Replace* and *Cancel* — because yes and no are no answer to a question that
offers replacing and merging.

`Transfer::targetName` and `wouldReplace` are what a caller asks *before* the copy, and `install()` is
built on the same two, so the name asked about and the name written cannot drift apart. The whole import
waits for the answer, `finishImport()` included — in the browser that call deletes the staging file the bytes are in.

**`Transfer::isBuiltIn` keeps no list**; it asks whether the file exists in the game folder —
and answers no for the seven files that belong to the player. Three callers, all the same rule: an import
must not take such a name, and neither editor may save under one. Delete goes through `isRemovable`
instead, the stricter question. `Transfer::list` returns the union of both roots, sorted, and needs no
rule for a name in both because no path can create one. Case is the file system's problem rather than a
hand-rolled comparison's, which is right: on Windows `Blocks.zip` *is* `blocks.zip`, and `fileExists`
says so.

**A finished import updates the open list.** `pollImport` runs every tick from `onUpdate`, because the
browser's file dialog cannot be modal — so when it completes with the Manager still open, it switches the
kind radio to whatever `classify` decided, re-reads the list and selects the new entry. That is why the
pane deliberately stays open across the file dialog, export included: the Manager is a place you keep
working in. Escape belongs to the topmost pane: the confirmation first, then the Manager, and only with
both closed does it quit the game.

**Import takes one file and works out what it is** — `Transfer::classify`, by content and never by
extension: `OggS` at the front is music, an XML whose root is `<Level>` is a level, and an archive is a
campaign if it holds `campaign.xml` or a skin if it holds `tileset.xml` and `sprites.png`. Anything else
is refused. The browser stages the upload outside the home directory (C hands JS all three possible
staging paths and JS picks one by extension, so C still composes every path), `sanitizeFilenameStem`
reduces the name to `[A-Za-z0-9_-]`, and only then does anything reach IndexedDB.

**An import replaces a file of the same name**, for all four kinds alike, and `Transfer::install` is the
whole rule: sanitized stem, plus the kind's extension, plus a copy. Not a swerve to `stem_2`, because a
skin's filename *is* its identity — a level says `skin0="space"` and `Level::getSkinFilename` looks for
`levels/skins/space.zip`, so `space_2.zip` would leave every such level exactly as broken, only without a
visible cause — and the weaker form holds for the rest: a new version of your level means *your* level. A
re-imported campaign therefore keeps its progress, since `ProgressDB` keys on the filename.

The one refusal is `isBuiltIn`. `install` reports through `bool* p_replaced` whether it landed on an
existing file, so the toast says **Replaced** rather than **imported** — the only sign the player would
otherwise get that something of theirs is gone. A campaign is checked with `isImportableArchive` *before*
the copy, so a damaged archive cannot destroy a good one of the same name.

**What export writes is a plain copy.** That matters for skins: three of the four shipped ones are packed
with a password, and decrypting them on the way out would be a back door around the very protection they
are packed for. The recipient cannot open such an archive — but can still *use* it, because the password
rides along inside it as `password.txt` and `Level::getSkinFilename` reads that out of any skin archive
whatever its filename. A skin somebody made themselves has no password anyway, and that is the one people
actually share.

**A level can borrow the shipped campaign's music**: `musicFilename="blocks:music2.ogg"` resolves through
`Campaign::resolveMusicPath` to `levels/campaigns/blocks.zip[pw]/music2.ogg` instead of a file beside the
level, and `Campaign::save` deliberately does *not* pack such a track — it is already on every machine.
Without it a browser author has no music at all, since nothing can put an `.ogg` next to a level.
