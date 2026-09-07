#ifndef _AUDIOCAPTURE_H
#define _AUDIOCAPTURE_H

/*** Class for capturing the system audio ***/

// Records what the default playback device is putting out, i.e. the game's
// music and sound effects - and not the microphone. Under Windows that goes
// through WASAPI's loopback mode; for that the device does not have to be set
// up as a recording source ("Stereo Mix" or similar). Under Linux through the
// monitor of PulseAudio's default sink, which is the same thing and which
// PipeWire with pipewire-pulse serves just as well.
//
// The samples always come out as 16 bit stereo interleaved, whatever format
// the device itself works in. A "sample" here, as in OpenAL, is a pair of a
// left and a right channel.
//
// Where there is nothing to listen in on - in the browser, or under Linux
// without PulseAudio - open() fails and the videos stay silent.

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
