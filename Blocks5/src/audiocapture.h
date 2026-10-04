#ifndef _AUDIOCAPTURE_H
#define _AUDIOCAPTURE_H

/*** Class for capturing the system audio ***/

// Records what the default playback device plays - the game's music and
// effects, not the microphone: WASAPI loopback under Windows (no "Stereo Mix"
// source needed), the monitor of PulseAudio's default sink under Linux (which
// pipewire-pulse serves as well). Samples always come out as 16-bit
// interleaved stereo at the rate prepare() was given, whatever the device
// uses; a "sample" here, as in OpenAL, is one left/right pair.
//
// The device is open only while a video is being made: start() opens it and
// stop() closes it, so a desktop that shows a recording indicator shows one
// for the recording and not for the whole session. Where there is nothing to
// listen in on, prepare() fails: under Linux without PulseAudio the videos
// are then silent, and the browser records none.

struct AudioCaptureImpl;

class AudioCapture
{
public:
	AudioCapture();
	~AudioCapture();

	// Whether a loopback capture can be made here at all; opens nothing.
	bool prepare(uint sampleRate = 48000);

	// Open the device on a thread of its own and collect its samples from now
	// on; stop() closes it again. Both are the video recorder's: start()
	// returns at once, so the game never waits for a device to open, and
	// stop() once the device is closed, logging what became of it.
	void start();
	void stop();

	// number of samples ready to be fetched
	int getNumSamplesReady();

	// fetches numSamples samples; what is missing is padded with silence
	void getSamples(short* p_buffer, int numSamples);

private:
	// not copyable - the buffer and the thread belong to exactly one object
	AudioCapture(const AudioCapture&);
	AudioCapture& operator=(const AudioCapture&);

	AudioCaptureImpl* p_impl;
};

#endif
