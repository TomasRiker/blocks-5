#!/usr/bin/env python3
"""make_rewind_sound.py - erzeugt den Ruecklaufton fuer CF_Rewind.

    python3 Tools/make_rewind_sound.py Blocks5/data/rewind.wav
    ffmpeg -i Blocks5/data/rewind.wav -c:a libvorbis -q:a 4 Blocks5/data/rewind.ogg

Geschrieben wird eine WAV-Datei; ins Spiel gehoert sie als Ogg Vorbis, wie die
anderen 55 Klaenge auch, und dafuer ist ein Kodierer noetig, den die Standard-
bibliothek nicht mitbringt. Die zweite Zeile macht das. Nur die .ogg wird
eingecheckt - dieses Skript ist da, damit sich der Ton aendern laesst, ohne ihn
neu aufnehmen zu muessen.

Was ein Videorekorder beim Zurueckspulen von sich gibt, und woraus es hier
zusammengesetzt ist:

  * Das Laufwerk rastet ein - ein kurzer, tiefer Schlag mit etwas Geraeusch
    darin. Er sitzt ganz am Anfang und ist nach 60 ms vorbei.
  * Der Wickelmotor laeuft an. Das ist der traegt der Klang: ein Grundton mit
    seinen ersten Oberwellen, dessen Hoehe mit der Drehzahl steigt. Dazu ein
    langsames Wummern von etwa 7 Hz - der Wickel laeuft nicht ganz rund.
  * Der Wickel selbst pfeift, deutlich hoeher und ebenfalls mitsteigend.
  * Das Band rauscht durch die Fuehrungen. Breitbandiges Rauschen, oben herum
    gefiltert, und es ist der Teil, der das Tempo hoerbar macht.
  * Am Ende bremst das Laufwerk: die Hoehe faellt schnell, der Pegel geht
    zurueck, und ein zweiter, weicherer Schlag beendet es.

Die Laenge richtet sich nach der Ueberblendung, die 1,1 s dauert; der Ton ist
etwas laenger, damit das Ausklingen nicht abgeschnitten wirkt.

Alles hier ist Standardbibliothek: math, random, struct, wave.
"""

import math
import random
import struct
import sys
import wave

RATE = 44100
LENGTH = 1.30           # Sekunden, etwas mehr als die Ueberblendung
BRAKE_START = 0.80      # ab wann gebremst wird, als Anteil der Laenge
PEAK = 0.55             # Aussteuerung; der Rest der Mischung ist schon laut


def ease_out(x):
    """Schnell anlaufen, dann einlaufen."""
    x = max(0.0, min(1.0, x))
    return 1.0 - (1.0 - x) * (1.0 - x)


def spin(u):
    """Drehzahl von 0 bis 1 ueber den Verlauf u (0..1).

       Der Exponent ist der Unterschied zwischen "laeuft an" und "laeuft".
       Mit ease_out() steht die Drehzahl schon nach einem Fuenftel praktisch
       fest, und dann passiert eine Sekunde lang nichts mehr - der Ton wird
       eine Flaeche. Mit 0.55 steigt es anfangs schnell und danach immer noch,
       und genau dieses Weitersteigen ist das, was man als Aufspulen hoert."""
    if u < BRAKE_START:
        return (u / BRAKE_START) ** 0.55
    # Bremsen: von voller Drehzahl in kurzer Zeit auf null.
    return 1.0 - ease_out((u - BRAKE_START) / (1.0 - BRAKE_START))


def envelope(u):
    """Pegel ueber den Verlauf. Sehr schneller Einsatz, weiches Ende."""
    attack = min(1.0, u / 0.03)
    # Der Pegel haengt an der Drehzahl: schneller ist lauter.
    level = 0.5 + 0.5 * spin(u)
    if u < BRAKE_START:
        return attack * level
    return attack * level * (1.0 - ease_out((u - BRAKE_START) / (1.0 - BRAKE_START))) ** 0.7


class OnePole(object):
    """Ein Tiefpass erster Ordnung, so einfach wie er nur sein kann."""

    def __init__(self, cutoff):
        # a aus der Grenzfrequenz, ueber die Zeitkonstante.
        self.a = 1.0 - math.exp(-2.0 * math.pi * cutoff / RATE)
        self.y = 0.0

    def __call__(self, x):
        self.y += self.a * (x - self.y)
        return self.y


def build(seed):
    """Einen Kanal bauen. Der Startwert macht die beiden Seiten verschieden -
       zwei unabhaengige Rauschquellen sind das ganze Stereobild, das es
       braucht."""
    rnd = random.Random(seed)
    n = int(RATE * LENGTH)

    # Drei Tiefpaesse hintereinander statt einem: ein einzelner Pol faellt mit
    # 6 dB je Oktave, und damit steht bei 20 kHz immer noch so viel Rauschen,
    # dass es das Ganze in weisses Zischen ertraenkt statt nach Band zu klingen.
    hiss_lo = [OnePole(4800.0), OnePole(4800.0), OnePole(4800.0)]
    hiss_hi = OnePole(1100.0)     # und unten heraus, ueber die Differenz
    clunk_lo = OnePole(220.0)     # der Schlag ist dumpf

    # Zwei Phasenakkumulatoren, damit die Hoehe gleiten kann, ohne zu knacken:
    # der Sprung muesste sonst in der Phase landen.
    motor_phase = 0.0
    reel_phase = 0.0
    out = []

    for i in range(n):
        t = i / float(RATE)
        u = t / LENGTH
        s = spin(u)
        env = envelope(u)

        # --- Wickelmotor: Grundton und die ersten beiden Oberwellen ---------
        wobble = 1.0 + 0.03 * math.sin(2.0 * math.pi * 7.3 * t)
        motor_freq = (82.0 + 178.0 * s) * wobble
        motor_phase += 2.0 * math.pi * motor_freq / RATE
        motor = (math.sin(motor_phase)
                 + 0.45 * math.sin(2.0 * motor_phase)
                 + 0.22 * math.sin(3.0 * motor_phase))

        # --- Das Pfeifen des Wickels ----------------------------------------
        reel_freq = (520.0 + 2100.0 * s) * wobble
        reel_phase += 2.0 * math.pi * reel_freq / RATE
        reel = math.sin(reel_phase) + 0.3 * math.sin(2.0 * reel_phase)

        # --- Das Band in den Fuehrungen -------------------------------------
        hiss = rnd.uniform(-1.0, 1.0)
        for stage in hiss_lo: hiss = stage(hiss)
        hiss -= hiss_hi(hiss)
        hiss *= 3.0               # was die drei Pole an Pegel gekostet haben

        # --- Das Einrasten des Laufwerks ------------------------------------
        clunk = 0.0
        if t < 0.08:
            decay = math.exp(-t / 0.018)
            clunk = clunk_lo(rnd.uniform(-1.0, 1.0)) * decay
            clunk += 0.8 * math.sin(2.0 * math.pi * 68.0 * t) * decay

        # --- Und das Abstellen am Ende --------------------------------------
        stop = 0.0
        stop_at = LENGTH * 0.94
        if t > stop_at:
            decay = math.exp(-(t - stop_at) / 0.02)
            stop = 0.5 * math.sin(2.0 * math.pi * 55.0 * (t - stop_at)) * decay

        sample = env * (0.38 * motor
                        + 0.13 * s * reel
                        + 0.20 * (0.25 + 0.75 * s) * hiss) \
               + 0.55 * clunk + stop
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

    frames = bytearray()
    for a, b in zip(left, right):
        frames += struct.pack('<hh',
                              int(max(-1.0, min(1.0, a * gain)) * 32767),
                              int(max(-1.0, min(1.0, b * gain)) * 32767))

    handle = wave.open(path, 'wb')
    handle.setnchannels(2)
    handle.setsampwidth(2)
    handle.setframerate(RATE)
    handle.writeframes(bytes(frames))
    handle.close()

    print('%s: %.2f s, %d Bilder, Spitze %.2f vor der Anpassung'
          % (path, LENGTH, len(left), peak))
    return 0


if __name__ == '__main__':
    sys.exit(main())
