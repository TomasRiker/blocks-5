#!/bin/bash
# particles.sh - how many particles are alive at once, level by level.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/particles.sh
#
# The select screen's preview runs the level for real - rain, fire, lava,
# conveyor belts and all - so walking it with the right arrow is a tour of
# every level's particle load without playing any of them. The test hook
# reports the largest number of particles one system has had to draw since it
# was last asked, and asking clears it, so a dump at the start of a dwell and
# one at the end give the peak over exactly that stretch with no spike lost
# between two polls.
#
# That number is what sizes VERTEX_BUFFER_SIZE in ParticleSystem: above it,
# render() splits the frame into more than one glDrawArrays.
#
# The single levels come last, and their first entry is particle_stress.xml -
# nine bombs each standing in a fire, in a field of destructible brick. Its
# title begins with "!!" so that the title sort puts it first and Home reaches
# it without anybody having to read the screen.
set -u
B5_SHOTS="${B5_SHOTS:-/tmp/blocks5-particles}"
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/harness.sh"

DWELL="${B5_DWELL:-5}"
LEVELS="${B5_LEVELS:-42}"

# Details decides the particle density - a third at low, two thirds at medium,
# everything at high - so the measurement is worth nothing without it. Written
# before the game starts, because that is when config.xml is read; the game
# writes the file back on exit, which is why it is written whole rather than
# edited.
B5_HOME="${XDG_DATA_HOME:-$HOME/.local/share}/blocks5"
mkdir -p "$B5_HOME/levels"
cat > "$B5_HOME/config.xml" <<'XML'
<?xml version="1.0" ?>
<Config>
    <Language>en</Language>
    <Details>2</Details>
    <SoundVolume>0.000000</SoundVolume>
    <MusicVolume>0.000000</MusicVolume>
</Config>
XML
cp "$B5_HERE/particle_stress.xml" "$B5_HOME/levels/"

trap b5_stop EXIT
b5_start
b5_waitForState GS_Menu

if [ "$(b5_json "el('Menu.CrtPane.Crt.NoThanks')['shown']")" = "True" ]; then
	b5_click Menu.CrtPane.Crt.NoThanks
fi

b5_click Menu.StartGame
b5_waitForState GS_SelectLevel

# Shift+F7 unlocks the whole campaign. Without it the walk stops at the first
# level nobody has solved, because the arrow buttons grey out with it.
xdotool keydown --clearmodifiers shift; sleep 0.1
xdotool key --clearmodifiers F7; sleep 0.1
xdotool keyup --clearmodifiers shift; sleep 1.5

# The peak over one level, and the order is what makes it mean anything: clear
# first, then load the level, then dwell. A level that throws its whole load in
# the first tick - the stress test does, its bombs stand in fire - has peaked
# and gone before a clearing dump placed after the load would run, and the
# reading that follows sees only the embers.
b5_peakAfter()
{
	b5_dump >/dev/null || return 1
	xdotool keydown --clearmodifiers "$1"; sleep 0.06; xdotool keyup --clearmodifiers "$1"
	sleep "$DWELL"
	b5_dump >/dev/null || return 1
	b5_json "d['particlePeak']"
}

b5_walk()
{
	local label="$1" count="$2" i peak best=0 bestAt=1
	for i in $(seq 1 "$count"); do
		# Home for the first, one step right for each after it.
		if [ "$i" = 1 ]; then peak="$(b5_peakAfter Home)"; else peak="$(b5_peakAfter Right)"; fi
		[ -n "$peak" ] || peak=0
		printf '  %-16s %3d  peak %6d\n' "$label" "$i" "$peak"
		if [ "$peak" -gt "$best" ]; then best="$peak"; bestAt="$i"; fi
	done
	echo "  -> $label: highest $best, at number $bestAt"
	B5_BEST="$best"
}

echo
echo "Campaign, $DWELL s in each level's preview:"
b5_walk "campaign level" "$LEVELS"
campaignBest="$B5_BEST"

echo
echo "Single levels, starting at the stress test:"
b5_key Down
b5_walk "single level" "${B5_SINGLE:-1}"
stressBest="$B5_BEST"
b5_shot stress-level

echo
echo "Highest count in one particle system:"
echo "  campaign preview   $campaignBest"
echo "  stress test        $stressBest"
echo "  VERTEX_BUFFER_SIZE $(( $(grep -o 'VERTEX_BUFFER_SIZE = [0-9]*' "$B5_GAME/src/particlesystem.h" | grep -o '[0-9]*') / 4 )) particles"
b5_finish
