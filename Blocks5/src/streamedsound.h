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

	float getVolume() const;
	void setVolume(float volume);
	float getPitch() const;
	void setPitch(float pitch);
	float getLoopBegin() const;
	void setLoopBegin(float loopBegin);
	uint tellStream() const;
	void seekStream(uint position);

	uint secondsToSlices(float t) const;

	void slideVolume(float targetVolume, float volumeSlideSpeed);
	void slidePitch(float targetPitch, float pitchSlideSpeed);
	bool update();

private:
	StreamedSound(const std::string& filename, int options);
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

	// The decoder thread, always 0 in the browser; whether the sound is
	// running is what playing says.
	SDL_Thread* p_thread;
	bool playing;

#ifndef __EMSCRIPTEN__
	// Posted to stop the decoder thread, and the thread's wait between two
	// passes: SDL 1.2 has no atomics, and a volatile bool is no
	// synchronisation. A kernel semaphore under Windows and Linux, not the
	// 1 ms polling loop SDL_mutex.h warns of on other systems.
	SDL_sem* p_stopSignal;
#endif

	// Held around every use of p_stream from the time the decoder thread may
	// run: tellStream() is asked on the main thread while the thread reads
	// and seeks the same stream, and libvorbis keeps no lock of its own. In
	// the browser, which has no such thread, Emscripten's SDL makes it a
	// no-op.
	SDL_mutex* p_streamLock;

	// Is the stream at its end? Only whatever fills the buffers touches it -
	// the decoder thread natively (play() sets it before starting the
	// thread), update() in the browser - so it needs no synchronisation.
	bool finish;

	float volume;
	float pitch;
	float targetVolume;
	float targetPitch;
	float volumeSlideSpeed;
	float pitchSlideSpeed;
	bool stopAtSlideEnd;

	float loopBegin;
	uint loopBeginInSlices;
};

int streamedSoundThreadProc(void* p_param);

#endif