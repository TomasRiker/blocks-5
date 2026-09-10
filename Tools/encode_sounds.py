#!/usr/bin/env python3
# Produces the shipped .ogg from the .wav beside it - one to one, with no
# level change. Where a sound should play quieter, that factor lives in
# Blocks5/data/sounds.xml and is applied at playback; the .wav stays the
# unaltered source at full resolution.
#
#     python3 Tools/encode_sounds.py            everything out of date
#     python3 Tools/encode_sounds.py ricochet   only this one
#     python3 Tools/encode_sounds.py --force    all of them, out of date or not
#
# Only what is older than its .wav is re-encoded. Two runs over the same source
# do not deliver the same file: the Ogg pages carry a random stream id, and
# with it the checksums of the page headers change - twenty-four bytes of nine
# thousand, for the same audio. Without that check every run would rewrite
# fifty-five binary files.
#
# 96 kbit/s is what the greater part of the stock carries. Less is a trap for
# short effects: at 45 kbit/s the encoder smears a transient far enough that
# the decoder is decibels out. But libvorbis does not accept 96 at every
# sample rate - at 11025 Hz mono it stops at 48 - hence the search downward
# instead of a fixed number.

import os, subprocess, sys, glob

RATES = [96, 80, 64, 56, 48, 40, 32]
DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Blocks5', 'data')

def encode(wav, ogg):
    for kbit in RATES:
        p = subprocess.run(['ffmpeg', '-y', '-v', 'error', '-i', wav,
                            '-c:a', 'libvorbis', '-b:a', '%dk' % kbit,
                            '-map_metadata', '-1', ogg], capture_output=True)
        if p.returncode == 0:
            return kbit
    sys.stderr.write(p.stderr.decode('utf-8', 'replace'))
    return 0

def main(argv):
    force = '--force' in argv
    names = [a for a in argv[1:] if not a.startswith('--')]
    if names:
        wavs = [os.path.join(DATA, n if n.endswith('.wav') else n + '.wav') for n in names]
    else:
        # Only what already has an .ogg: enemy2_laugh_original.wav is a source
        # that has been set aside, and music3.wav belongs to the campaign in
        # blocks.zip.
        wavs = [w for w in sorted(glob.glob(os.path.join(DATA, '*.wav')))
                if os.path.exists(w[:-4] + '.ogg')]

    bad = 0
    done = 0
    for wav in wavs:
        if not os.path.exists(wav):
            print('%s missing' % wav); bad += 1; continue
        ogg = wav[:-4] + '.ogg'
        if not force and not names and os.path.exists(ogg) \
           and os.path.getmtime(ogg) >= os.path.getmtime(wav):
            continue
        done += 1
        kbit = encode(wav, ogg)
        if not kbit:
            print('%s: no bit rate was accepted' % os.path.basename(wav)); bad += 1
        elif kbit != 96:
            print('%-24s %d kbit/s' % (os.path.basename(wav)[:-4], kbit))
    print('%d of %d file(s) encoded%s'
          % (done - bad, len(wavs), ', %d error(s)' % bad if bad else ''))
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
