# tools

Three check scripts for working on the tree. Two of them need nothing but
Python 3 and run in seconds.

## verify.py

Static checks. They look for the sort of mistake that appears silently while
editing and that neither the compiler nor a look at the diff will find.

    python3 Tools/verify.py            everything
    python3 Tools/verify.py --list     the names with one line of explanation
    python3 Tools/verify.py --only gui_paths
    python3 Tools/verify.py --quiet    the summary only

Exit code 1 as soon as anything is reported.

| check | what it checks |
| --- | --- |
| `encoding` | Pure ASCII and LF in the sources, CRLF in the shipped files. |
| `project_files` | A new source file must be in the `.vcxproj` and in its `.filters`. |
| `naming` | The filename is the class name in lower case. |
| `version` | The version number lives in four places and must not drift. |
| `gui_paths` | Every element path in the code must exist in a dialog XML. |
| `strings` | Every `$ID` must be in `languages.txt`, in English and in German. |
| `xml_attrs` | An attribute written and read nowhere is dead weight or a typo. |
| `config` | What `Engine::saveConfig` writes must be read back again. |
| `ctor_init` | Scalar members that the constructor does not set. |
| `assets` | A filename in the code must exist on disk, spelled exactly so. |
| `sounds` | Every sound `playSound()` names must be preloaded in `loadSounds()`. |
| `sound_volumes` | Every sound in `data/sounds.xml` must exist, with a factor under 1. |
| `style` | Tabs, no space after a keyword, no whitespace at line end. |
| `windows_icon` | The program icon must match `data/window.png`. |
| `comments` | English comments - a German line among them is always a leftover. |

Two of them - `style` and `ctor_init` - judge only what has come in since the
state before the overhaul. Code that already stood there in 2015 and has run
daily ever since is not a finding, and reporting it at every run is how a
check gets ignored. The state compared against is `BASELINE` at the top of
`verify.py`.

## selftest.py

Proves that the checks find something. A set of checks that always says "ok"
may long since have stopped matching what it looks for without anybody
noticing - the attribute check was inert when it was first written, because
`Attribute(` also matches the tail of `SetAttribute(`, so every written
attribute counted as read.

The script therefore injects, for each check, exactly the fault it is meant to
catch, runs it, and puts the file back byte for byte afterwards.

    python3 Tools/selftest.py

## syntax.sh

Compiles every source of the game with `i686-w64-mingw32-g++ -fsyntax-only`. It
is the only way to put a compiler over the Windows code from here, and it costs
half a minute.

    sh Tools/syntax.sh              all 120 files
    sh Tools/syntax.sh engine.cpp   only this one

Three files never go through it - `main.cpp`, `videorecorder.cpp` and
`stackwalker.cpp`. The last two are left out of the web build for the same
reasons; `main.cpp` is compiled there, because what mingw cannot parse in it is
the SEH crash handler, and that sits behind `#if defined(_WIN32)`. Nothing has
to be checked in for it: the headers mingw and OpenAL Soft file under other
names (`<Windows.h>`, `<Shlobj.h>`, `<al.h>`) are written as forwarding headers
into a throwaway directory.

## What else runs

    LinuxBuild/build.sh         compiles and links the native build
    LinuxBuild/build.sh hooks   the same with the test hooks, into build-test/
    LinuxBuild/test/smoke.sh    drives the native build through the GUI
    WebBuild/build.sh           compiles and links the browser build
    WebBuild/build.sh hooks     the same with the test hooks, into build-test/
    WebBuild/test/smoke.js      drives the browser build through the GUI

See `LinuxBuild/README.md` and `WebBuild/test/README.md`.


Generators
----------

Two of the scripts here make a file that ships rather than checking one; both
are stdlib only:

    python3 Tools/make_ico.py Blocks5/data/window.png Blocks5/src/icon1.ico

`make_ico.py` builds the program icon for Windows; the `.ico` is committed,
because the Windows build runs no Python, and the `windows_icon` check is what
keeps it current.

    python3 Tools/strip_comments.py --out DIRECTORY Blocks5/data

`strip_comments.py` puts the XML files and `languages.txt` without their
comments into a staging directory, and that is what gets packed - the notes in
the dialogs and in the string table stay in the sources and are nobody's
business who opens `data.zip`. `pack.sh` and `zip_data.bat` call it of their own
accord; calling it by hand is only needed to look at the result. Unlike
`make_ico.py` it therefore runs under Windows too - and where Python is missing,
both packing scripts say so and pack those files as they stand instead of
stopping the build.

Each format has its own rule and its own check afterwards. An XML comment goes
only if it has its lines to itself, and the tree of tags, attributes and text is
compared before and after. In `languages.txt` a line beginning with `//` goes
whole, and the string table `Engine::loadStringDB` would build is compared
before and after - the blank lines around a comment stay, because that parser
counts them. Any other text file is copied through untouched: a skin's
`password.txt` is one of those.

A comment is removed only if it has its lines to itself. That is not a matter
of tidiness: a level stores one tile id per character inside `<Row>`, so `<!--`
would be the tiles 60, 33, 45, 45 - to a parser the start of a comment like any
other. A row of tiles always has data before it on its line, a comment in a
dialog never does. One written after something else on the same line is
reported and kept in the archive.


Sounds
------

Every effect has its .wav source beside the .ogg that ships, in
`Blocks5/data` - `pack.sh` takes only the `*.ogg`. Change a .wav and the .ogg
has to be produced again:

    python3 Tools/encode_sounds.py            everything out of date
    python3 Tools/encode_sounds.py ricochet   only this one
    python3 Tools/encode_sounds.py --force    all of them, out of date or not

Only what is older than its .wav is re-encoded, and that is no luxury: two runs
over the same source do not deliver the same file. The Ogg pages carry a random
stream id, and with it the checksums of the page headers change - twenty-four
bytes of nine thousand, for the same audio. Without the check every run would
rewrite fifty-five binary files.

The script encodes **one to one**, with no level change. 96 kbit/s is what the
greater part of the stock carries; less is a trap for short effects. At
45 kbit/s the encoder smears a transient far enough that the decoder is
decibels out - `thunder.ogg` came out 4.9 dB below its source - and the
quantisation noise of the last long block (2048 samples, 46 ms) still stands
at -46 dBFS at the end of the file instead of at -88. But libvorbis does not
accept 96 at every sample rate: at 11025 Hz mono it stops at 48, hence the
search downward instead of a fixed number.

**Where a sound should play quieter, that stands in `Blocks5/data/sounds.xml`**
and is applied at playback, not at encoding. The .wav thereby stays the
unaltered source at full resolution, the .ogg is reproducible, and the encoder
gets the full level for the same computational load. A factor baked into the
.ogg would live only in the compressed file, and since the .wav beside it stays
loud, the next re-encode loses the intent - eight files were exported that way,
and re-encoding them from their sources would have made them up to 6.8 dB
louder. The `sound_volumes` check keeps the table and the stock in step.

Two things belong in the .wav itself, not in the encoder and not in the table.
The endpoints must lie on zero - a half cosine of 5 ms in and out, and
correspondingly less for very short sounds. **A sound that runs in a loop gets
no fade**, because there the end is the beginning: conveyorbelt, elevator, gas,
laser, mask, rain, thunderstorm, toxic. And a DC offset has to go: it costs
headroom, it clicks at both ends, and where an envelope has been laid over it,
it travels with that envelope and so is not even a fixed value that could
simply be subtracted. A 20 Hz high-pass takes it and leaves everything audible
standing - measured, it costs at most 0.8 dB of loudness to ITU-R BS.1770 while
removing up to 6.6 dB of RMS.

**For a looping sound that high-pass has to be convolved circularly.** Such a
sound *is* periodic; convolved linearly, both ends take the filter's transient,
and the step at the seam came out 10 to 12 dB worse than before.

What cannot be repaired this way is clipping: the clipped peaks are gone, and
getting back under full scale would mean lowering the level. Eleven files still
carry it.
