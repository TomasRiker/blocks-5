#!/bin/bash
# transition.sh - the level editor's transition tool, on lawn and sand and on
# the cave wall: what its strokes leave, read off the level as the editor saves
# it.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/transition.sh
#
# The editor loads a level the script writes, the tool is worked over it with
# the mouse, and the level is saved under another name and compared row by row.
# The shapes are the cases. On the ground, an area is stroked along its inside
# and one along its outside, the ground the stroke starts on reaching half a
# tile over the edge either way. One is stroked in a zigzag across its edge and
# has to come out as clean as the other two. A path one tile wide is traced
# over, and a single tile and a hole one tile wide are clicked, and they have to
# grow soft edges rather than vanish. On the wall, a rock mass with a bay and a
# spur for its inner corners is stroked over, one is stroked only beside it, and
# one has a hole and a notch in it. The two beside the bricks along the level's
# edge have to join them with no edge. Nothing outside the shapes may change,
# and a second stroke over a finished edge has to change nothing, which the undo
# depth the hook reports says.
set -u
B5_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# A private home like undo.sh's, which the level is written into for the
# editor's Load to find.
export XDG_DATA_HOME="${B5_TRANSITION_XDG:-/tmp/blocks5-transition-xdg}"
B5_PRIVATE_HOME="$XDG_DATA_HOME/blocks5"
rm -rf "$XDG_DATA_HOME"
mkdir -p "$B5_PRIVATE_HOME/levels"
printf 1.2.0 > "$B5_PRIVATE_HOME/.initialized"

# Lawn everywhere on layer 0 and sand (4) where it says, bricks (M) round the
# edge of layer 1 and rock (V) where it says. The rock is all one piece as the
# pen draws it, so every edge and corner in the result is the tool's.
python3 - "$B5_PRIVATE_HOME/levels/transition.xml" <<'PY'
import sys
W, H = 40, 25
ground = [['.'] * W for _ in range(H)]
walls = [[' '] * W for _ in range(H)]
for x in range(W): walls[0][x] = walls[H - 1][x] = 'M'
for y in range(H): walls[y][0] = walls[y][W - 1] = 'M'
def fill(layer, x0, y0, x1, y1, tile):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1): layer[y][x] = tile
fill(ground, 2, 3, 9, 8, '4'); fill(ground, 5, 3, 6, 4, '.')        # stroked inside
fill(ground, 14, 3, 21, 8, '4'); fill(ground, 17, 7, 18, 8, '.')    # stroked outside
fill(ground, 2, 12, 9, 17, '4'); fill(ground, 8, 16, 9, 17, '.')    # zigzag
fill(ground, 2, 20, 10, 20, '4')                                    # path
ground[13][13] = '4'                                                # single tile
fill(ground, 17, 12, 21, 16, '4'); ground[14][19] = '.'             # hole
fill(walls, 25, 3, 33, 9, 'V'); fill(walls, 31, 3, 33, 5, ' '); fill(walls, 34, 7, 35, 8, 'V')
fill(walls, 33, 12, 38, 18, 'V')
fill(walls, 14, 19, 24, 23, 'V'); walls[21][17] = ' '; fill(walls, 22, 19, 24, 20, ' ')
rows = lambda layer: ''.join('<Row>%s</Row>' % ''.join(r) for r in layer)
open(sys.argv[1], 'w', encoding='latin-1').write(
    '<?xml version="1.0" ?><Level title="!transition" skin0="" width="40" height="25" '
    'numLayers="2" numDiamondsNeeded="0" electricityOn="0" nightVision="0" '
    'lightColorR="255" lightColorG="255" lightColorB="255">'
    '<Layer>%s</Layer><Layer>%s</Layer>'
    '<Object type="Player" x="12" y="10" character="0" active="1"/></Level>' % (rows(ground), rows(walls)))
PY

source "$B5_HERE/harness.sh"

# Pressed on the centre of the first cell, moved through the centres of the
# rest and let go of. A pause at each, so that the editor sees the cursor
# there and draws the line between two of them, rather than between two
# further apart.
stroke()
{
	local a=("$@") i
	b5_mouseAt $((a[0] * 16 + 8)) $((a[1] * 16 + 8)); xdotool mousedown 1; sleep 0.4
	for ((i = 2; i < ${#a[@]}; i += 2)); do
		b5_mouseAt $((a[i] * 16 + 8)) $((a[i + 1] * 16 + 8)); sleep 0.2
	done
	sleep 0.3; xdotool mouseup 1; sleep 1.0
}

# Either closes the menu once it has done its work.
editorMenu()   # $1 Load or Save, $2 the file name
{
	b5_click LevelEditor.ShowMenu
	b5_click LevelEditor.MenuPane.Menu.Filename
	b5_chord ctrl a
	b5_type "$2"
	b5_click "LevelEditor.MenuPane.Menu.$1"
}

trap b5_stop EXIT
b5_start
b5_waitForState GS_Menu
b5_click Menu.LevelEditor
b5_waitForState GS_LevelEditor
editorMenu Load transition
b5_click LevelEditor.Mode3

stroke 2 3  4 3  4 5  7 5  7 3  9 3  9 8  2 8  2 3
stroke 13 2  22 2  22 9  19 9  19 6  16 6  16 9  13 9  13 2
stroke 3 13  1 11  4 12  1 15  3 18  6 16  8 18  10 15  8 14  10 12  6 11  3 12
stroke 2 20  10 20
stroke 13 13
stroke 19 14
for y in 2 3 4 5 6 7 8 9 10; do stroke 24 $y 36 $y; done
stroke 32 11  32 19  38 19
stroke 32 11  38 11
for y in 19 20 21 22 23; do stroke 13 $y 25 $y; done

editorMenu Save transition_out
saved="$B5_PRIVATE_HOME/levels/transition_out.xml"
if [ ! -s "$saved" ]; then
	b5_note "the level was not saved as $saved"
else
	while read -r verdict what; do
		if [ "$verdict" = ok ]; then b5_ok "$what"; else b5_note "$what"; fi
	done < <(python3 - "$B5_PRIVATE_HOME/levels/transition.xml" "$saved" <<'PY'
import re, sys
def layers(path):
    s = open(path, encoding='latin-1').read()
    return [re.findall(r'<Row>(.*?)</Row>', l) for l in re.findall(r'<Layer>(.*?)</Layer>', s, re.S)]
before, after = layers(sys.argv[1]), layers(sys.argv[2])
# Each case as it must come out, by layer and top left cell, with a margin
# that has to stay as it was.
cases = [
    ('an area stroked along its inside grows half a tile', 0, 0, 1, [
        '............',
        '.0111201112.',
        '.3444534445.',
        '.3444cd4445.',
        '.3444444445.',
        '.3444444445.',
        '.3444444445.',
        '.3444444445.',
        '.6777777778.',
        '............']),
    ('an area stroked along its outside shrinks half a tile', 0, 12, 1, [
        '............',
        '............',
        '..01111112..',
        '..34444445..',
        '..34444445..',
        '..34a77b45..',
        '..345..345..',
        '..678..678..',
        '............',
        '............']),
    ('an area stroked in a zigzag across its edge comes out clean', 0, 0, 10, [
        '............',
        '.0111111112.',
        '.3444444445.',
        '.3444444445.',
        '.3444444445.',
        '.3444444445.',
        '.3444444a78.',
        '.34444445...',
        '.67777778...']),
    ('a path one tile wide traced over grows soft edges', 0, 0, 19, [
        '.01111111112.',
        '.34444444445.',
        '.67777777778.',
        '.............']),
    ('a single tile clicked grows soft edges', 0, 11, 11, [
        '.....',
        '.012.',
        '.345.',
        '.678.',
        '.....']),
    ('a hole one tile wide clicked grows soft edges', 0, 16, 11, [
        '.......',
        '.44444.',
        '.4a7b4.',
        '.45.34.',
        '.4c1d4.',
        '.44444.',
        '.......']),
    ('rock stroked over gets its edges and its inner corners', 1, 23, 1, [
        '               ',
        '               ',
        '  PQQQQR       ',
        '  UVVVVW       ',
        '  UVVVVW       ',
        '  UVVVVYQQR    ',
        '  UVVVVVVVYQR  ',
        '  UVVVVVVVOij  ',
        '  Ziiiiiiij    ',
        '               ']),
    ('rock stroked only beside it, against the bricks on the right', 1, 31, 11, [
        '        M',
        '  PQQQQQM',
        '  UVVVVVM',
        '  UVVVVVM',
        '  UVVVVVM',
        '  UVVVVVM',
        '  UVVVVVM',
        '  ZiiiiiM',
        '        M']),
    ('rock with a hole and a notch, against the bricks at the bottom', 1, 12, 18, [
        '               ',
        '  PQQQQQQR     ',
        '  UVOiTVVW     ',
        '  UVW UVVYQQR  ',
        '  UVYQoVVVVVW  ',
        '  UVVVVVVVVVW  ',
        'MMMMMMMMMMMMMMM']),
]
covered = set()
for what, layer, x0, y0, rows in cases:
    wrong = []
    for dy, row in enumerate(rows):
        got = after[layer][y0 + dy][x0:x0 + len(row)]
        covered |= set((layer, x0 + dx, y0 + dy) for dx in range(len(row)))
        if got != row: wrong.append('row %d is "%s" where "%s" was expected' % (y0 + dy, got, row))
    print('ok' if not wrong else 'bad', what + ('' if not wrong else ': ' + ', '.join(wrong)))
moved = ['%d,%d on layer %d' % (x, y, layer) for layer in range(2) for y in range(len(before[layer]))
         for x in range(len(before[layer][y])) if (layer, x, y) not in covered and before[layer][y][x] != after[layer][y][x]]
print('ok' if not moved else 'bad', 'nothing outside the shapes changed' + ('' if not moved else ': ' + ', '.join(moved)))
PY
)
fi

# A stroke along an edge the tool has finished, and one across a finished wall,
# leave no undo step: a stroke that changes nothing is none.
b5_dump; depth=$(b5_json "d['undo']")
stroke 2 3  4 3  4 5  7 5  7 3  9 3  9 8  2 8  2 3
stroke 3 13  1 11  4 12  1 15  3 18  6 16  8 18  10 15  8 14  10 12  6 11  3 12
stroke 24 6  36 6
b5_dump
if [ "$(b5_json "d['undo']")" = "$depth" ]; then b5_ok "strokes over finished edges change nothing: undo $depth"
else b5_note "strokes over finished edges left undo at $(b5_json "d['undo']") where $depth was expected"; fi

b5_finish
