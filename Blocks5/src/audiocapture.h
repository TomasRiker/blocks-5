#ifndef _AUDIOCAPTURE_H
#define _AUDIOCAPTURE_H

/*** Class for capturing the system audio ***/

// Records what the default playback device plays - the game's music and
// effects, not the microphone: WASAPI loopback under Windows (no "Stereo Mix"
// source needed), the monitor of PulseAudio's default sink under Linux (which
// pipewire-pulse serves as well). Samples always come out as 16-bit
// interleaved stereo whatever the device uses; a "sample" here, as in OpenAL,
// is one left/right pair.
//
// Where there is nothing to listen in on, open() fails: under Linux without
// PulseAudio the videos are then silent, and the browser records none.

struct AudioCaptureImpl;

class AudioCapture
{
public:
	AudioCapture();
	~AudioCapture();

	// opens the loopback recording of the default playback device
	bool open(uint sampleRate = 48000);

	// ends the recording and frees everything again
	void close();

	bool isOpen() const;

	// name of the device being recorded from (for the log only)
	const std::string& getDeviceName() const;

	// starts and stops collecting samples
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
