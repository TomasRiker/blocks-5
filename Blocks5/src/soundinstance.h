#ifndef _SOUNDINSTANCE_H
#define _SOUNDINSTANCE_H

/*** Class for a sound instance ***/

#include "sound.h"

class SoundInstance
{
	friend class Sound;

public:
	void play(bool loop);
	void stop();
	void pause();
	void resume();

	float getVolume() const;
	void setVolume(float volume);
	float getPitch() const;
	void setPitch(float pitch);

	void slideVolume(float targetVolume, float volumeSlideSpeed);
	void slidePitch(float targetPitch, float pitchSlideSpeed);

	int getPriority() const;
	void setPriority(int priority);

	uint onLoseSource();
	void update();
	bool toBeRemoved() const;

	// Different for every instance ever created; see Sound::isLiveInstance().
	uint getSerial() const { return serial; }

private:
	SoundInstance(Sound& sound);
	~SoundInstance();

	Sound& sound;
	uint sourceID;
	uint serial;

	uint timestamp;
	int priority;
	bool looping;
	float volume;
	float pitch;
	float targetVolume;
	float targetPitch;
	float volumeSlideSpeed;
	float pitchSlideSpeed;
	bool pauseAtSlideEnd;
};

#endif