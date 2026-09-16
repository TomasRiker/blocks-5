---
paths:
  - "Blocks5/src/{audiocapture,videorecorder,sound,soundinstance,streamedsound,audiostream,as_ogg,as_wav}.{cpp,h}"
  - "Tools/encode_sounds.py"
  - "Blocks5/data/sounds.xml"
  - "WebBuild/web_audio.{cpp,h}"
  - "WebBuild/videorecorder_stub.cpp"
  - "Blocks5/libs/{minih264,minimp4,shine,openal-soft-1.25.2}/**"
---

# Audio and video: OpenAL, the mix, the sound files and recording

**Video recording** writes H.264 Baseline video and MP3 audio into an MP4, with no DLL involved:
`libs/minih264` encodes video, `libs/shine` audio, `libs/minimp4` writes the container, all vendored source.
Windows has decoded that combination natively since Windows 7 — container and H.264 since 7, the MP3 decoder
since Vista, and the MPEG-4 File Source documents its `'mp4a'` sample entry as meaning "AAC or MP3" — so a
recording plays on a clean install, which the old ffmpeg AVI did not. All three are plain C and were chosen
so an eventual Linux build can use the same ones. `videorecorder.cpp` does its own RGBX→YUV420 conversion
(the frame arrives from `glReadPixels` upside down) and holds each encoded frame back by one, because a
frame's duration is only known when the next arrives. minih264 needs the frame size to be a multiple of 16;
640x480 is.

**Recorded audio is a loopback capture of what the machine is playing**, not OpenAL — `audiocapture.cpp`
says why and how the two platforms differ. One thing about it is a build fact rather than an audio one:
libpulse is `dlopen`'d with its declarations written out by hand, so the build needs no libpulse-dev and the
game still starts where PulseAudio is absent.

**The mix is turned down, and that is not a taste setting.** A dozen effects and the music at full volume
summed above the ceiling and were clipped by OpenAL Soft — audible as distortion, in the game and in a
recorded video alike. `MASTER_HEADROOM` at the top of `engine.cpp` scales the finished mix before that
clamp, and the comment there carries the measurement and the two standards that pick the number. It belongs
in the source rather than the options because it is a property of the mixture, not a preference — the
player's own sliders are untouched and still read 100%.

**The sound files are repaired sources, and the mix decisions are not in them.** `Blocks5/data` holds a WAV
beside every shipped OGG, and `Tools/encode_sounds.py` produces one from the other **one to one** — 96 kbit/s
where libvorbis accepts it, stepping down where it does not (11025 Hz mono tops out at 48). Where a sound
should play quieter than its file, that factor lives in `data/sounds.xml` and is applied at playback: `Sound`
looks itself up once at construction and `SoundInstance` multiplies it into the single `alSourcef(…,
AL_GAIN, …)` call, so it covers `slideVolume` and every caller that sets a volume itself.

That split exists because the alternative had already failed silently. Eight OGGs had been exported at a
reduced level while the WAV beside them kept the loud original, so the intent lived only in the compressed
file: re-encoding from the source would have made `ricochet` 6.8 dB louder, `push` 5.1, `thunder` 4.6.
Measuring it back out needs the right comparison — the shipped OGG against a *freshly encoded* one from the
same WAV, since WAV-against-OGG folds in the encoder's own frequency-dependent loss, the same order as the
smallest of these factors (`syringe` at 0.914).

Three things belong in the WAV instead: no DC offset, endpoints on zero, nothing clipped. A 20 Hz high-pass
takes the first — measured, it costs at most 0.8 dB of BS.1770 loudness while removing up to 6.6 dB of RMS,
because what it removes is inaudible. Half-cosine fades of 5 ms take the second, **except on the eight looping
sounds** (`conveyorbelt`, `elevator`, `gas`, `laser`, `mask`, `rain`, `thunderstorm`, `toxic`), where the end
*is* the beginning. Those also need the high-pass convolved **circularly** rather than linearly: a looping
sound is periodic, and the filter's transient otherwise droops both ends and made the seam 10–12 dB worse. The
third cannot be repaired — clipped peaks are gone, and getting back under full scale means lowering the level.
