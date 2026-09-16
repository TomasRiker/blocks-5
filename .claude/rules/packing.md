---
paths:
  - "Blocks5/pack.sh"
  - "Blocks5/zip_*.bat"
  - "Blocks5/stage.bat"
  - "Tools/strip_comments.py"
  - "Blocks5/levels/**"
  - "Blocks5/data/*.xml"
  - "Blocks5/data/languages.txt"
  - "Blocks5/src/campaign.cpp"
---

# Packing data.zip, the skins and the campaign

**`Blocks5/pack.sh` is all four in one, without Windows**, using the distribution's `7za` and
`optipng` and refusing to start without either. `7za` and not Info-ZIP's `zip -P`, although both
write traditional ZipCrypto: Info-ZIP sets bit 3 of the general purpose flags, writes sizes in a
trailing data descriptor, and takes its check byte from the time of day rather than the CRC.
`./pack.sh` does everything; `data`, `skins`, `campaign` narrow it; `--no-optipng` skips the slow
step.

**`levels/campaigns/blocks.zip` is a build product**, and why it must be rebuilt matters before
editing a level: `Campaign::load` serves a campaign's levels loose wherever all of them lie in
`levels/` — true in a working tree, never on an installed game, since `stage.bat` ships the archive
and the two examples and not the 42 sources. So a level edited and not packed changes what a
developer sees and nothing a player sees, with no error anywhere. `pack.sh campaign` and
`zip_campaign.bat` (which `Build.bat` calls) rebuild it from `levels/level_NN.xml`, numbering members
the way `makeMemberName` reads them back — entry *i* is `level_{i+1}.xml`, so the padded names in
`campaign.xml` are display text only.

**All 53 members have a source in the tree**, which is what lets the archive be untracked: 42 levels
and ten music tracks loose in `levels/`, `campaign.xml` in `levels/campaigns/blocks/` — a source
folder beside the archive, the same idiom as `levels/skins/<name>/`. Neither script reaches into the
archive it replaces, which cannot work once the file is a build product. Verified against the last
committed archive: same 53 members, every one byte-identical.

**XML and `languages.txt` reach `data.zip` without their comments.** `Tools/strip_comments.py` writes
stripped copies into a staging directory the packing scripts pack from, which is why `pack.sh` and
`zip_data.bat` call `7za` twice: images, sounds and the demo out of `data/`, XML and text out of
staging. Python does the stripping and is the one thing Windows packing has no bundled tool for —
where it is missing both scripts say so and pack those files as they stand. A note, not a stopped
build.

`languages.txt` is the easy half: `Engine::loadStringDB` reads a line at a time and its comment branch
does nothing, so a `//` line can go whole. The blank lines around one stay, because that parser counts
them and the next text line flushes the count into the string. Named explicitly rather than `*.txt`,
for the same reason the packing scripts name `password.txt` rather than globbing: a pattern sweeping
up every text file would one day take a line out of a password.

A comment is removed only if it has its lines to itself — a guard, not tidiness: a level stores one
tile id per character inside `<Row>`, so `<!--` is simply tiles 60, 33, 45, 45, and to a parser that
starts a comment, swallowing everything to the next `-->`. Asking whether the file is well-formed does
not help, because it *is*: ElementTree reads `<Row>aaa<!--bbb</Row><Row>ccc-->ddd</Row>` without
complaint. A row of tiles always has data before it on its line; a comment in a dialog never does. One
written after something else on the same line is reported and kept.
