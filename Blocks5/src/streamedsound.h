#ifndef _STREAMEDSOUND_H
#define _STREAMEDSOUND_H

/*** Klasse für gestreamte Sounds (z.B. Musik) ***/

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

	uint secondsToSlices(double t) const;

	void slideVolume(double targetVolume, double volumeSlideSpeed);
	void slidePitch(double targetPitch, double pitchSlideSpeed);
	bool update();

private:
	StreamedSound(const std::string& filename);
	~StreamedSound();

	int threadProc();
	void stream(uint bufferID);

	static bool forceReload() { return true; }

	AudioStream* p_stream;
	bool loop;
	uint sourceID;
	uint buffers[4];
	uint bufferSize;
	char* p_buffer;
	SDL_Thread* p_thread;
	volatile bool finish;

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