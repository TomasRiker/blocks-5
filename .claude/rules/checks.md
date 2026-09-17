---
paths:
  - "Tools/verify.py"
  - "Tools/selftest.py"
  - "Tools/syntax.sh"
  - "Tools/compile_db.sh"
  - "Tools/make_ico.py"
  - "Tools/README.md"
---

# The static checks: verify.py, selftest.py and syntax.sh

**`Tools/verify.py`** looks for the mistake that leaves no trace in a diff and that no compiler sees: a
`gui["…"]` path no dialog XML knows, a `$ID` missing from `languages.txt`, an XML attribute written and
never read, a source file missing from `Blocks5.vcxproj` or its `.filters`, a display list added back, a
class whose header is not named after it, a render layer written as a number, an object that draws raw
geometry outside a `Renderer::DirectGL` bracket or changes texture state outside `GL::`, the version number
drifting across its four places, a member the constructor never sets, an asset filename not on disk or
spelled with different case (only Linux minds), a sound `playSound()` names that `gs_loading.cpp` does
not preload, a non-ASCII byte or CRLF in a source file, `if (` where the tree writes `if(`, a German
comment among the English. Exit 1 on any finding; `--list` names them, `--only NAME` runs one.
`Tools/README.md` has the table.

**The `comments` check reads further than the other twenty-three**, and the reason is a file it did not
catch: `WebBuild/htaccess` was wholly German through the whole translation sweep, because it has no
extension and `source_files()` walks `.cpp`, `.h` and `.c` under `Blocks5/src`, `WebBuild`, `PWEncrypt`
and `ShowUserDir` — never `LinuxBuild`, and never a script. `prose_files()` is the second list: the
sources plus every `.js`, `.sh` and `.py` in `LinuxBuild`, `WebBuild` and `Tools`, plus `htaccess` by
name, each with the marker its comments begin with. Only the language half uses it; the density guard
stays on the sources, since a shell script has no ratio worth judging, and counts `//` lines only — a
`/* */` block is code to it.

The language check reads the two languages against each other rather than searching for one, because
both word lists contain traps: *the particles die* is English although `die` is a German article, and
*so weit kommt das nicht* is German although `so` is English. A line is reported when the German words
outnumber the English. The `style` check learned the same from the other side — it skipped `//` lines
but not the body of a `/* */` block, and where the German never wrote `for (`, an English sentence does.

The attribute check exists because renaming `numLayers` to `NUM_LAYERS` once took the XML attribute
string with it, silently disabling the level size guard for every editor-saved level. That is the shape
of bug this file is for. **The Windows icon is checked for the same reason**: `Blocks5/src/icon1.ico` is
committed, not generated — the Windows build runs no Python — so it sits unchanged when the art moves,
and it had. `windows_icon` compares its 16x16 image against `data/window.png` and insists on the sizes
the shell asks for; `Tools/make_ico.py` rebuilds it.

**Two checks judge only what changed since `95660bb`**, the last commit before the 2025 overhaul, whose
id is `BASELINE` at the top of `verify.py`: indentation/whitespace, and uninitialised members. The
comment-density half of `comments` is an absolute 50% and judges every line. Code that has worked for
ten years is not a finding, and reporting it every run is how a check gets ignored.

**Four of the checks police the renderer's convention** - `direct_gl`, `gl_state`, `gl_doors` and
`display_lists` - and `RENDERER-REDESIGN.md` (ROADMAP 54) replaces them with two, `raw_gl` and
`direct_gl_scope`, once every draw goes through the renderer. `direct_gl` is the first of those two
scoped to what the level reaches: a `glBegin` in an `onRender` source has to stand in a block that
declared a `Renderer::DirectGL` before it, read with the block, chain and preprocessor rules its
docstring lists.

**`Tools/selftest.py`** injects each fault in turn, confirms the matching check fires, restores the file
byte-for-byte. Run it after touching `verify.py`. Not ceremony: the attribute check was inert when first
written, because `Attribute(` also matches the tail of `SetAttribute(`.

**`sh Tools/syntax.sh`** compiles all 124 sources with `i686-w64-mingw32-g++ -fsyntax-only`, the only way
to put a compiler over the Windows code from here. Three files never go through it — `main.cpp`,
`videorecorder.cpp`, `stackwalker.cpp`. The last two are left out of the web build for the same reasons;
`main.cpp` is compiled there, and the difference is what mingw cannot parse in it: the `__try`/`__except`
crash handler behind `#if defined(_WIN32) && !defined(_DEBUG)` — true under mingw, false under emcc. It
needs nothing checked in: the headers mingw and OpenAL Soft file differently (`<Windows.h>`,
`<Shlobj.h>`, `<al.h>`) are generated into a temp directory. It passes `-w`; for a warning sweep swap
that for `-Wall -Wextra` and compare against the same sweep before your change, because the tree emits
thousands of warnings that were all there in 2015.
