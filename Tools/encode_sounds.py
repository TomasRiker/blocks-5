#!/usr/bin/env python3
# Erzeugt die ausgelieferten .ogg aus den .wav daneben - eins zu eins, ohne
# Pegelaenderung. Wo ein Klang leiser sein soll, steht das in
# Blocks5/data/sounds.xml und wird beim Abspielen angewandt; die .wav bleibt
# die unveraenderte Quelle in voller Aufloesung.
#
#     python3 Tools/encode_sounds.py            alle veralteten
#     python3 Tools/encode_sounds.py ricochet   nur diese
#     python3 Tools/encode_sounds.py --force    alle, ob veraltet oder nicht
#
# Neu kodiert wird nur, was aelter ist als seine .wav. Zwei Laeufe ueber
# dieselbe Quelle liefern naemlich nicht dieselbe Datei: die Ogg-Seiten tragen
# eine zufaellige Stromkennung, und mit ihr aendern sich die Pruefsummen der
# Seitenkoepfe - vierundzwanzig Byte von neuntausend, bei gleichem Ton. Ohne
# diese Pruefung schriebe jeder Lauf fuenfundfuenfzig Binaerdateien um.
#
# 96 kbit/s ist, was der groesste Teil des Bestandes traegt. Weniger ist fuer
# kurze Effekte eine Falle: bei 45 kbit/s verschmiert der Kodierer eine
# Transiente so weit, dass der Dekoder um Dezibel danebenliegt. libvorbis nimmt
# 96 aber nicht bei jeder Abtastrate an - bei 11025 Hz mono ist bei 48 Schluss -,
# deshalb wird nach unten gesucht statt eine Zahl vorzugeben.

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
        # Nur was schon eine .ogg hat: enemy2_laugh_original.wav ist eine
        # abgelegte Quelle, und music3.wav gehoert zur Kampagne in blocks.zip.
        wavs = [w for w in sorted(glob.glob(os.path.join(DATA, '*.wav')))
                if os.path.exists(w[:-4] + '.ogg')]

    bad = 0
    done = 0
    for wav in wavs:
        if not os.path.exists(wav):
            print('%s fehlt' % wav); bad += 1; continue
        ogg = wav[:-4] + '.ogg'
        if not force and not names and os.path.exists(ogg) \
           and os.path.getmtime(ogg) >= os.path.getmtime(wav):
            continue
        done += 1
        kbit = encode(wav, ogg)
        if not kbit:
            print('%s: kein Bitratenwert ging durch' % os.path.basename(wav)); bad += 1
        elif kbit != 96:
            print('%-24s %d kbit/s' % (os.path.basename(wav)[:-4], kbit))
    print('%d von %d Datei(en) kodiert%s'
          % (done - bad, len(wavs), ', %d Fehler' % bad if bad else ''))
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
