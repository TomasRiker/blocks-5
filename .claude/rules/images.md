---
paths:
  - "Blocks5/src/img_*.{cpp,h}"
  - "Tools/make_ico.py"
  - "WebBuild/make_icon.py"
  - "WebBuild/make_text.py"
  - "WebBuild/manifest.json"
  - "Blocks5/libs/stb/**"
---

# Images and icons

**Images** are decoded by `img_load.cpp`, not SDL_image. The game needs exactly one function from it —
`IMG_Load_RW`, called from `texture.cpp` and for the window icon — and reads every texture through its own
`SDL_RWops` over the encrypted `data.zip`. stb_image (`libs/stb`, one header) does that in about eighty
lines; PNG and JPEG are enabled, and every image the game ships is a PNG.

**Writing one needs no library at all**, and `img_save.cpp` is the mirror image of that reasoning: decoding
PNG is hard, encoding it is not, because zlib does the work and zlib is already compiled into all three
builds for minizip's sake. `compress2()` returns exactly the zlib datastream an `IDAT` chunk is defined to
hold and `crc32()` is the checksum every chunk carries, so the whole encoder is a signature, three chunks
and a filter loop. Screenshots are PNG because of it — `SDL_SaveBMP` and its 900 KB files are gone.

Two things there are worth knowing. The per-row filter heuristic — pick the filter whose bytes have the
smallest sum of magnitudes read as signed — is twenty lines and worth them: measured on a real 640x480
screenshot, 239 KB against 283 KB for no filtering, out of a 900 KB bitmap. And **the alpha channel is not
free**, the opposite of what it looks like: `glReadPixels` must ask for `GL_RGBA` because that is the only
combination WebGL 1 allows, but a channel of nothing but 255 does not collapse in the deflate — it pushes
every prediction one byte apart. The same frame is 330 KB as RGBA and 247 KB as RGB, so the encoder takes a
source layout and a destination layout separately and drops the channel while building each row, before the
filter ever sees it.

**The browser gets screenshots too**, and this is what unblocked them: the two reasons F11 was refused there
were `GL_BGR` and `SDL_SaveBMP_RW`, and neither is in the path any more. There is nowhere sensible to *put*
the file — the IndexedDB is for saved games, and filling a player's quota with pictures they can never look
at is not a trade worth making — so the bytes go straight into their downloads through
`WebTransfer::downloadBytes`, the same Blob mechanism the Manager's export uses. It copies the heap slice
(`new Uint8Array(HEAPU8.subarray(...))`) rather than handing the Blob a view, because `ALLOW_MEMORY_GROWTH`
can invalidate one at any allocation. `$A_TOGGLE_CAPTURE_VIDEO` is still the one action `main.cpp` withholds
from the web build.

**Every icon is generated from `data/window.png`** — four for the web by `WebBuild/make_icon.py`, seven for
Windows by `Tools/make_ico.py`; `make_text.py` uses the same PNG reader, and all three are stdlib only.
**Pixel replication at an integer factor, never a resize**: a scaler that smooths turns 16x16 pixel art into
a blur, and that is the whole reason these scripts exist. The `.ico` is **committed rather than generated**,
because the Windows build runs no Python; `verify.py`'s `windows_icon` check stops it going stale.

The web icons come in three kinds because a launcher does two different things with one:

- `purpose: "any"` (192 and 512) is shown **as it is** — the drawing edge to edge, transparency intact.
- `purpose: "maskable"` (512) is **cropped to a shape the launcher picks**, and only a centred circle of 80%
  of the width is guaranteed to survive; every transparent pixel becomes a hole in that shape. Bob is a
  full-bleed circle reaching 119% of the width across the diagonal, so masked he would lose his rim and cap.
  It is therefore drawn at **10x (320px) centred on an opaque 512 canvas** — content radius 191px against
  the 205px allowed, and still an integer scale. **Never label one icon `"any maskable"`** unless it
  satisfies both, which a full-bleed drawing cannot.
- `apple-touch-icon.png` exists because iOS reads neither the manifest icons nor any transparency:
  full-bleed like the "any" pair, but opaque.

`Blocks5/src/icon1.ico` carries **seven images** — 16, 20, 32, 40, 48, 64, 256 — because the shell smoothly
scales one itself for any size the file does not hold, downward included (256 to 40 averages six source
pixels into one). Where the size is not a multiple of 16 — 20 and 40 — the next scale down is centred with a
transparent margin rather than rendered at 1.25x, which would double some columns and not others. **24 is
deliberately absent**: the one size where the next step down is 1x, and that much margin is visible, so
Windows scales it from the 32. Every entry is a 32bpp DIB (the 256 a PNG), since full 8-bit alpha has worked
since XP; the 1-bit AND mask is still written alongside for legacy paths that read only that. **No
power-of-two restriction** on either kind: an `.ico` entry stores each edge in one byte (0 meaning 256), and
a manifest's `sizes` is free text. `Blocks5/setup/setupicon.ico` is the installer's own graphic — a monitor
and a disc, not Bob — and is left alone.
