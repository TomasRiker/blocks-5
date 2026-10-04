#!/bin/bash
# record.sh - the sound of a recorded video: the loopback capture is open for
# as long as a recording runs and at no other time (ROADMAP 46), and what it
# records starts with the video and runs on without a gap.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/record.sh
#
# It brings a PulseAudio server of its own, whose one output is a null sink,
# so nothing reaches the speakers and a server the developer runs is left
# alone: the game, the tone below and pactl all find this one through
# PULSE_SERVER. Needs pulseaudio, pactl and pacat, and ffmpeg to read the
# videos back.
set -u
B5_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for tool in pulseaudio pactl pacat ffmpeg ffprobe; do
	command -v "$tool" >/dev/null 2>&1 || { echo "record.sh needs $tool"; exit 2; }
done

# A private home, as drag.sh keeps one: the videos land in it, and the
# developer's own are neither counted nor added to.
export XDG_DATA_HOME="${B5_RECORD_XDG:-/tmp/blocks5-record-xdg}"
B5_PRIVATE_HOME="$XDG_DATA_HOME/blocks5"
rm -rf "$XDG_DATA_HOME"
mkdir -p "$B5_PRIVATE_HOME"
printf 1.2.0 > "$B5_PRIVATE_HOME/.initialized"

# The server, daemonized rather than started with & - b5_stop ends in a bare
# wait, which would wait for a job of this shell for ever. Its runtime folder
# is its own, where it keeps its pid, and anybody may connect to its socket:
# the game is one client among others and has no cookie to show.
PULSE_DIR="${B5_RECORD_PULSE:-/tmp/blocks5-record-pulse}"
rm -rf "$PULSE_DIR"
mkdir -p "$PULSE_DIR"
XDG_RUNTIME_DIR="$PULSE_DIR" pulseaudio -D -n --exit-idle-time=-1 --disallow-exit \
	--log-target=file:"$PULSE_DIR/server.log" \
	--load="module-native-protocol-unix socket=$PULSE_DIR/native auth-anonymous=1" \
	--load="module-null-sink sink_name=b5record rate=48000" >/dev/null 2>&1
export PULSE_SERVER="unix:$PULSE_DIR/native"
for i in $(seq 1 20); do pactl info >/dev/null 2>&1 && break; sleep 0.25; done
pactl info >/dev/null 2>&1 || { echo "FAILED: no PulseAudio server - see $PULSE_DIR/server.log"; exit 2; }
pactl set-default-sink b5record

# A tone into the sink for the whole run, so that any moment of a recording
# has something to hear: 440 Hz at a quarter of full scale, beside the menu's
# music. A low latency, as the game's own output asks for one - a sink driven
# by a player that asks for two seconds renders in two-second blocks, and a
# capture opened then waits for the next one.
cat > "$PULSE_DIR/tone.py" <<'PY'
import math, struct, sys
rate = 48000
step = 2 * math.pi * 440 / rate
# 4800 samples is 0.1 s and 44 whole periods, so the blocks join without a seam.
block = b''.join(struct.pack('<hh', int(8000 * math.sin(step * i)), int(8000 * math.sin(step * i)))
                 for i in range(4800))
out = sys.stdout.buffer
while True:
    out.write(block)
PY
( python3 "$PULSE_DIR/tone.py" | pacat --raw --rate=48000 --channels=2 --format=s16le \
	--latency-msec=20 --client-name=b5record-tone >/dev/null 2>&1 & )

b5_stopSound()
{
	pkill -f "$PULSE_DIR/tone.py" 2>/dev/null
	pkill -f "client-name=b5record-tone" 2>/dev/null
	[ -f "$PULSE_DIR/pulse/pid" ] && kill "$(cat "$PULSE_DIR/pulse/pid")" 2>/dev/null
	return 0
}

source "$B5_HERE/harness.sh"
trap 'b5_stop; b5_stopSound' EXIT

# The game's capture streams, by the name audiocapture.cpp gives pa_simple_new.
b5_captures() { pactl list source-outputs 2>/dev/null | grep -c 'application.name = "Blocks 5"'; }

# F12, the action: held, as an action key must be. Start, rest, stop, and the
# capture streams counted while it runs and after it ends.
b5_record()   # $1 seconds; sets during, after and video
{
	b5_hold F12
	sleep 0.5
	during=$(b5_captures)
	sleep "$1"
	b5_hold F12
	sleep 1
	after=$(b5_captures)
	video=$(ls -t "$B5_PRIVATE_HOME/videos"/*.mp4 2>/dev/null | head -1)
}

# What the video's sound says: the lengths of both tracks, where the tone
# first sounds and every stretch after that in which it does not. Decoded to
# mono at 48 kHz and judged by the RMS of 5 ms windows; the tone alone is
# about 5700, silence padded in is 0.
b5_audio()   # $1 the video; prints JSON
{
	python3 - "$1" <<'PY'
import json, math, struct, subprocess, sys
path = sys.argv[1]
probe = json.loads(subprocess.check_output(
    ['ffprobe', '-v', 'error', '-show_streams', '-of', 'json', path]))
kinds = {s['codec_type']: float(s.get('duration', 0)) for s in probe['streams']}
result = {'video': kinds.get('video', 0.0), 'audio': kinds.get('audio', -1.0)}
if 'audio' in kinds:
    pcm = subprocess.check_output(['ffmpeg', '-v', 'error', '-i', path, '-map', '0:a:0',
                                   '-f', 's16le', '-ac', '1', '-ar', '48000', '-'])
    samples = struct.unpack('<%dh' % (len(pcm) // 2), pcm)
    window = 240
    rms = [math.sqrt(sum(v * v for v in samples[i:i + window]) / window)
           for i in range(0, len(samples) - window, window)]
    onset = next((i for i, r in enumerate(rms) if r > 1500), None)
    result['onsetMs'] = None if onset is None else onset * 5
    gaps = []
    if onset is not None:
        # The last 100 ms are left out: the encoder pads its last frame.
        quiet = [i for i in range(onset, len(rms) - 20) if rms[i] < 500]
        for i in quiet:
            if gaps and gaps[-1][1] == i - 1: gaps[-1][1] = i
            else: gaps.append([i, i])
    result['gaps'] = [[a * 5, (b - a + 1) * 5] for a, b in gaps]
print(json.dumps(result))
PY
}

export B5_ALSOFT_DRIVERS=pulse
b5_start
b5_waitForState GS_Menu
sleep 2

idle=$(b5_captures)
if [ "$idle" = 0 ]; then
	b5_ok "with no video being made, the game records nothing"
else
	b5_note "with no video being made, $idle capture stream(s) of the game's are open"
fi

for n in 1 2; do
	b5_record 3
	if [ "$during" = 1 ] && [ "$after" = 0 ]; then
		b5_ok "recording $n opened one capture stream and closed it again"
	else
		b5_note "recording $n: $during capture stream(s) while it ran, $after after it ended"
	fi
	if [ -z "$video" ]; then
		b5_note "recording $n wrote no video"
		continue
	fi
	audio=$(b5_audio "$video")
	echo "  ($(basename "$video"): $audio)"
	json() { python3 -c "import json,sys; d=json.loads(sys.argv[1]); print($1)" "$audio"; }
	# The tone is there from the start: what is in front of it is the time the
	# capture took to open and the MP3 codec's own delay, some 50 ms together.
	if [ "$(json "d.get('onsetMs') is not None and d['onsetMs'] <= 150")" = True ]; then
		b5_ok "recording $n has sound from its first $(json "d['onsetMs']") ms on"
	else
		b5_note "recording $n: the sound begins at $(json "d.get('onsetMs')") ms"
	fi
	if [ "$(json "len(d.get('gaps', [None])) == 0")" = True ]; then
		b5_ok "and it runs on without a gap"
	else
		b5_note "recording $n has gaps in its sound (start and length in ms): $(json "d.get('gaps')")"
	fi
	if [ "$(json "abs(d['audio'] - d['video']) < 0.25")" = True ]; then
		b5_ok "its sound is as long as its picture, $(json "round(d['audio'], 2)") s"
	else
		b5_note "recording $n: $(json "d['audio']") s of sound against $(json "d['video']") s of picture"
	fi
	sleep 1
done

idle=$(b5_captures)
[ "$idle" = 0 ] && b5_ok "and after both, nothing records" || b5_note "after both recordings, $idle capture stream(s) are still open"

b5_finish
