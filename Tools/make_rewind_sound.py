#!/usr/bin/env python3
"""make_rewind_sound.py - erzeugt den Ruecklaufton fuer CF_Rewind.

    python3 Tools/make_rewind_sound.py Blocks5/data/rewind.wav
    ffmpeg -i Blocks5/data/rewind.wav -c:a libvorbis -q:a 4 Blocks5/data/rewind.ogg

Geschrieben wird eine WAV; ins Spiel gehoert sie als Ogg Vorbis wie die anderen
55 Klaenge, und dafuer ist ein Kodierer noetig, den die Standardbibliothek nicht
mitbringt. Nur die .ogg wird eingecheckt - dieses Skript ist da, damit sich der
Ton aendern laesst, ohne ihn neu aufnehmen zu muessen.

Was hier nachgebaut wird, steht nicht in einem Lehrbuch, sondern ist an einer
Aufnahme eines echten Rekorders ausgemessen worden. Drei Zahlen daraus tragen
das Ganze:

  * Das Spektrum hat ZWEI Berge und dazwischen ein Loch: einen um 250 Hz und
    einen breiten um 2 bis 4 kHz, und bei 500 bis 1000 Hz liegt es 13 dB
    tiefer. Der untere ist das Chassis, der obere das Band an Kopf und
    Fuehrungen. Wer nur den unteren nimmt, bekommt ein Brummen; wer nur den
    oberen nimmt, ein Zischen. Beide zusammen sind ein Videorekorder.
  * Die Huellkurve ist nicht glatt, sie klappert - mit 46,5 Hz und dessen
    Oberwelle, dazu langsamer mit 12 Hz. Das sind die umlaufenden Teile, und
    dieses Klappern ist der Unterschied zwischen "Rauschen" und "Maschine".
  * Der Scheitelfaktor betraegt 14 dB. So impulsiv wird es nur durch dieses
    Klappern; ein gefiltertes Rauschen allein liegt bei etwa 11 dB.

Was NICHT drin ist: ein Sinuston. Der erste Versuch war um einen Motorgrundton
mit Oberwellen und ein Wickelpfeifen herum gebaut, beides als saubere Sinus -
im Spektrogramm sind das schmale Linien, und genau das hoert man dann auch: ein
Summen. Ein Laufwerk hat keine Tonhoehe, es hat ein Band und ein Klappern.

Alles hier ist Standardbibliothek: math, random, struct, wave.
"""

import math
import random
import struct
import sys
import wave

RATE = 44100

# Der Ton darf die Ueberblendung ueberdauern - das Laufwerk kommt zur Ruhe,
# nachdem das Bild schon wieder steht.
LENGTH = 1.75
RUN_UP = 0.45           # Sekunden bis zur vollen Drehzahl
BRAKE_AT = 1.26         # Sekunde, ab der gebremst wird

# Die Drehzahl steigt waehrend des Laufs noch etwas: beim Zurueckspulen waechst
# der Wickel, auf den gespult wird, und damit die Bandgeschwindigkeit.
DRIFT = 0.10

# Klappern: Grundfrequenz und langsame Unwucht, beide an die Drehzahl gekoppelt.
CLATTER_HZ = 46.5
FLUTTER_HZ = 12.0
CLATTER_DEPTH = 0.55
FLUTTER_DEPTH = 0.30

PEAK = 0.50             # Aussteuerung; der Rest der Mischung ist schon laut


class OnePole(object):
    """Ein Tiefpass erster Ordnung; mehrere davon hintereinander machen die
       Flanke steil genug."""

    def __init__(self, cutoff):
        self.a = 1.0 - math.exp(-2.0 * math.pi * cutoff / RATE)
        self.y = 0.0

    def __call__(self, x):
        self.y += self.a * (x - self.y)
        return self.y


class Biquad(object):
    """Bandpass nach dem ueblichen Kochbuch. Zwei Pole reichen: gebraucht wird
       eine Beule an einer Stelle, kein sauberer Trennfilter."""

    def __init__(self, freq, q, gain):
        w = 2.0 * math.pi * freq / RATE
        alpha = math.sin(w) / (2.0 * q)
        cw = math.cos(w)
        a0 = 1.0 + alpha
        self.b0 = alpha / a0
        self.b1 = 0.0
        self.b2 = -alpha / a0
        self.a1 = -2.0 * cw / a0
        self.a2 = (1.0 - alpha) / a0
        self.gain = gain
        self.x1 = self.x2 = self.y1 = self.y2 = 0.0

    def __call__(self, x):
        y = (self.b0 * x + self.b1 * self.x1 + self.b2 * self.x2
             - self.a1 * self.y1 - self.a2 * self.y2)
        self.x2, self.x1 = self.x1, x
        self.y2, self.y1 = self.y1, y
        return y * self.gain


# Die zwei Beulen und der Tiefpass darueber. Die Zahlen sind nicht geraten: es
# ist der beste Satz aus einer Suche gegen das gemessene Terzspektrum, und er
# trifft es im Mittel auf 1,3 dB genau, im schlechtesten Band auf 6 dB.
#
# Bemerkenswert ist, was dabei uebrigblieb. Angefangen hatte es mit fuenf
# Beulen; die Anpassung hat drei davon auf null gezogen. Ein Videorekorder
# braucht genau zwei: eine schmale bei 250 Hz - das Gehaeuse - und eine breite
# bei 2,6 kHz - das Band. Die Guete von 3,2 ist der Grund, warum der erste
# Versuch nicht stimmte: mit 1,4 ist die untere Beule so breit, dass sie das
# Loch bei 500 Hz zuschuettet, und dann klingt es nach Brummen statt nach
# Maschine.
BANDS = ((250.0, 3.2, 1.00),     # das Gehaeuse: schmal
         (2600.0, 0.8, 0.42))    # das Band: breit

# Und darueber faellt es steil ab. Ein einzelner Pol reicht dafuer nicht - bei
# 16 kHz stuenden sonst noch 25 dB zu viel.
LOWPASS_HZ = 4800.0
LOWPASS_POLES = 3


def speed(t):
    """Drehzahl von 0 bis 1 ueber die Zeit in Sekunden."""
    if t < RUN_UP:
        # Kein ease_out: mit einem laeuft die Drehzahl nach einem Fuenftel
        # praktisch fest, und der Rest ist eine Flaeche.
        return (t / RUN_UP) ** 0.6
    if t < BRAKE_AT:
        return 1.0 + DRIFT * (t - RUN_UP) / (BRAKE_AT - RUN_UP)
    u = (t - BRAKE_AT) / (LENGTH - BRAKE_AT)
    return max(0.0, (1.0 + DRIFT) * (1.0 - u) ** 1.6)


def pulse(phase, sharpness):
    """Eine Schwingung, die nicht rund ist, sondern anschlaegt. Mittelwertfrei,
       damit sie nur moduliert und nichts zur Lautstaerke beitraegt."""
    c = 0.5 + 0.5 * math.cos(2.0 * math.pi * phase)
    return c ** sharpness - 1.0 / (sharpness + 1.0)


def build(seed):
    """Einen Kanal bauen. Der Startwert macht die beiden Seiten verschieden -
       zwei unabhaengige Rauschquellen sind das ganze Stereobild, das es
       braucht."""
    rnd = random.Random(seed)
    n = int(RATE * LENGTH)
    bands = [Biquad(f, q, g) for f, q, g in BANDS]
    lowpass = [OnePole(LOWPASS_HZ) for _ in range(LOWPASS_POLES)]

    clatter_phase = 0.0
    flutter_phase = 0.0
    out = []

    for i in range(n):
        t = i / float(RATE)
        s = speed(t)

        # --- Das Laufgeraeusch: Rauschen durch die gemessenen Beulen ---------
        white = rnd.uniform(-1.0, 1.0)
        # Die obere Beule kommt erst mit dem Tempo - ein langsam laufendes Band
        # rauscht nicht. Das Gehaeuse rumpelt dagegen von Anfang an.
        weights = (1.0, 0.20 + 0.80 * s)
        noise = 0.0
        for band, weight in zip(bands, weights):
            noise += band(white) * weight
        for stage in lowpass:
            noise = stage(noise)

        # --- Das Klappern ----------------------------------------------------
        # Die Frequenzen haengen an der Drehzahl; ein wenig Zittern in der
        # Phase, damit daraus kein sauberer Ton wird.
        clatter_phase += (CLATTER_HZ * s + rnd.uniform(-1.5, 1.5)) / RATE
        flutter_phase += (FLUTTER_HZ * s) / RATE
        modulation = (1.0
                      + CLATTER_DEPTH * s * pulse(clatter_phase, 3.0)
                      + FLUTTER_DEPTH * s * pulse(flutter_phase, 1.5))

        # --- Der Pegel -------------------------------------------------------
        level = 0.25 + 0.75 * s

        sample = noise * modulation * level

        # --- Das Einrasten des Laufwerks --------------------------------------
        if t < 0.09:
            decay = math.exp(-t / 0.020)
            sample += 0.55 * rnd.uniform(-1.0, 1.0) * decay
            sample += 0.45 * math.sin(2.0 * math.pi * 78.0 * t) * decay

        # --- Und die Schlaege beim Abstellen ----------------------------------
        for when, strength, freq in ((BRAKE_AT + 0.02, 0.75, 96.0),
                                     (BRAKE_AT + 0.14, 0.45, 72.0),
                                     (BRAKE_AT + 0.27, 0.30, 58.0)):
            if when <= t < when + 0.12:
                decay = math.exp(-(t - when) / 0.028)
                sample += strength * (0.6 * math.sin(2.0 * math.pi * freq * (t - when))
                                      + 0.4 * rnd.uniform(-1.0, 1.0)) * decay

        out.append(sample)

    return out


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    path = sys.argv[1]

    left = build(20260905)
    right = build(776941)

    peak = max(max(abs(v) for v in left), max(abs(v) for v in right))
    gain = (PEAK / peak) if peak > 0.0 else 1.0

    # Ein weicher Einsatz und ein weiches Ende, damit an den Raendern nichts
    # knackt.
    n = len(left)
    fade_in = int(RATE * 0.004)
    fade_out = int(RATE * 0.030)

    frames = bytearray()
    for i, (a, b) in enumerate(zip(left, right)):
        f = 1.0
        if i < fade_in: f = i / float(fade_in)
        elif i > n - fade_out: f = (n - i) / float(fade_out)
        frames += struct.pack('<hh',
                              int(max(-1.0, min(1.0, a * gain * f)) * 32767),
                              int(max(-1.0, min(1.0, b * gain * f)) * 32767))

    handle = wave.open(path, 'wb')
    handle.setnchannels(2)
    handle.setsampwidth(2)
    handle.setframerate(RATE)
    handle.writeframes(bytes(frames))
    handle.close()

    print('%s: %.2f s, %d Bilder, Spitze %.2f vor der Anpassung'
          % (path, LENGTH, n, peak))
    return 0


if __name__ == '__main__':
    sys.exit(main())
