#ifndef _STREAMEDSOUND_H
#define _STREAMEDSOUND_H

/*** Class for streamed sounds (e.g. music) ***/

#include "resource.h"

class AudioStream;

class StreamedSound : public Resource<StreamedSound>
{
	friend class Manager<StreamedSound>;
	friend int streamedSoundThreadProc(void* p_param);

public:
	void play(bool loop = true);
	void stop();
	void pause();
	void resume();

	double getVolume() const;
	void setVolume(double volume);
	double getPitch() const;
	void setPitch(double pitch);
	double getLoopBegin() const;
	void setLoopBegin(double loopBegin);
	uint tellStream() const;
	void seekStream(uint position);

	uint secondsToSlices(double t) const;

	void slideVolume(double targetVolume, double volumeSlideSpeed);
	void slidePitch(double targetPitch, double pitchSlideSpeed);
	bool update();

private:
	StreamedSound(const std::string& filename);
	~StreamedSound();

	int threadProc();
	void startDecoderThread();   // both are nearly empty in the browser
	void joinDecoderThread();
	void pumpBuffers();   // one pass through the OpenAL queue
	void stream(uint bufferID);

	static bool forceReload() { return true; }

	AudioStream* p_stream;
	bool loop;
	uint sourceID;
	uint buffers[4];
	uint bufferSize;
	char* p_buffer;

	// p_thread is the decoder thread and nothing else, and always 0 in the
	// browser; whether this sound is running is what playing says.
	SDL_Thread* p_thread;
	bool playing;

#ifndef __EMSCRIPTEN__
	// Counted up when the decoder thread is to stop. SDL 1.2 has no atomic
	// types, and a volatile bool is not synchronisation; a semaphore is both
	// at once - the signal and the wait between two passes.
	// Under Windows/Linux a real kernel object sits behind it, not the loop
	// with 1 ms pauses that SDL_mutex.h warns about for other systems.
	SDL_sem* p_stopSignal;
#endif

	// Is the stream at its end? Only whatever fills the buffers writes and
	// reads this - the decoder thread under Windows/Linux, update() in the
	// browser. It never crosses a thread boundary; hence no volatile.
	bool finish;

	double volume;
	double pitch;
	double targetVolume;
	double targetPitch;
	double volumeSlideSpeed;
	double pitchSlideSpeed;
	bool stopAtSlideEnd;

	double loopBegin;
	uint loopBeginInSlices;
};

int streamedSoundThreadProc(void* p_param);

#endif