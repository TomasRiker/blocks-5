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
never read, a source file missing from `Blocks5.vcxproj` or its `.filters`, a `gl*` call in a file that
does not own raw GL (a display list added back, say), a raw call in `texture.cpp` or `engine.cpp`
outside a `Renderer::DirectGL` bracket, a class whose header is not named after it, a render layer
written as a number, the version number
drifting across its four places, a member the constructor never sets, an asset filename not on disk or
spelled with different case (only Linux minds), a sound `playSound()` names that `gs_loading.cpp` does
not preload, a non-ASCII byte or CRLF in a source file, `if (` where the tree writes `if(`, a German
comment among the English. Exit 1 on any finding; `--list` names them, `--only NAME` runs one.
`Tools/README.md` has the table.

**The `comments` check reads further than the other twenty-one**, and the reason is a file it did not
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
id is `BASELINE` at the top of `verify.py`: indentation/whitespace, and uninitialised members. Code that
has worked for ten years is not a finding, and reporting it every run is how a check gets ignored.

**The comment-density half of `comments` judges every file regardless of age, and what it compares is
not what the phrase suggests**: comment lines against ***code* lines**, not against the whole file. So
the ceiling of 50% is reached at one comment line to two of code — a file that is half comments by line
count sits at 100% and is far over. Only files of 100 code lines or more are judged at all, which is why
several small headers stand at two thirds comment and are never looked at. `engine.h` is at exactly
50.0% (193 against 386) and passes only because the test is `>`: one more comment line in that file
fails the run, which is worth knowing before setting out to explain something in it.

**`ctor_init` takes back both of its exemptions for a pointer**, and that is measured rather than
strict for its own sake. A scalar left uninitialised is a wrong number; a pointer is a crash, and the
window is wider than it looks: a game state stands on the stack from the moment `pushGameState` puts it
there, while `onEnter` — which builds everything it owns — runs only at the next
`processGameStateChanges()`. So `getGameState()` names a state whose members are still whatever the heap
left in them. `GS_Game::p_level` was such a member for years, and the test hook asking a freshly pushed
state for its level is what finally read it: a segfault that appeared in one working tree and not in
another, since the value depends on what the allocator had put there. Both exemptions would have hidden
it — the constructor set one of the six pointers, and the member is far older than the baseline.
A pointer is found by its `p_` spelling, which is the one thing the naming convention buys the checks.

**Two of the checks police the renderer's convention** (ROADMAP 54). `raw_gl`
reads the whole tree with comments and strings blanked and reports every `gl*`, `glu*` or `glExt*`
call outside `RAW_GL_FILES`, the files that own raw GL - which is also what keeps display lists, wide
lines, `GL_QUADS` and the alpha test out of a game the browser has to run; an owner holding no call
is reported too, since a stale entry reads as a considered exception. `direct_gl_scope` reads the two
owners whose raw GL runs while the renderer may hold quads, `texture.cpp` and `engine.cpp`, and asks
that every raw call stand in a block that declared a `Renderer::DirectGL` before it, with the block,
chain and preprocessor rules `bracket_violations()` lists; `glGetError` and `glGetString` are queries
and exempt.

**`Tools/selftest.py`** injects each fault in turn, confirms the matching check fires, restores the file
byte-for-byte. Run it after touching `verify.py`. Not ceremony: the attribute check was inert when first
written, because `Attribute(` also matches the tail of `SetAttribute(`.

**`sh Tools/syntax.sh`** compiles all 123 sources with `i686-w64-mingw32-g++ -fsyntax-only`, the only way
to put a compiler over the Windows code from here. Three files never go through it — `main.cpp`,
`videorecorder.cpp`, `stackwalker.cpp`. The last two are left out of the web build for the same reasons;
`main.cpp` is compiled there, and the difference is what mingw cannot parse in it: the `__try`/`__except`
crash handler behind `#if defined(_WIN32) && !defined(_DEBUG)` — true under mingw, false under emcc. It
needs nothing checked in: the headers mingw and OpenAL Soft file differently (`<Windows.h>`,
`<Shlobj.h>`, `<al.h>`) are generated into a temp directory. It compiles with `-Wconversion` and drops
everything that says except two families, either of which fails the run: **an integer handed to a
float**, and **a double handed to a float**. MSVC reports both as C4244 at the project's level 3, mingw
only under `-Wconversion`. The first arrived one line per build as the files happened to recompile - an
int vector's component given to a float vector's constructor every time, in code that had compiled clean
here. `static_cast<Vec2f>(v)` is the spelling that says it on purpose, `static_cast<float>(n)` for a
single value. The second is what keeps the tree's one floating type one: with no `double` left to
declare, a double arrives only out of a library call, and the family it catches is the unqualified
`sin`, `cos` and `floor`, which are the C ones and take a double - so a float widens, goes through the
double routine and narrows back. `sinf` and its cousins are the spelling that does not. For a warning sweep add
`-Wall -Wextra` and compare against the same sweep before your change, because the tree emits
thousands of warnings that were all there in 2015.
