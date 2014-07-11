#ifndef _SOUNDINSTANCE_H
#define _SOUNDINSTANCE_H

/*** Klasse für eine Sound-Instanz ***/

#include "sound.h"

class SoundInstance
{
	friend class Sound;

public:
	void play(bool loop);
	void stop();
	void pause();
	void resume();

	double getVolume() const;
	void setVolume(double volume);
	double getPitch() const;
	void setPitch(double pitch);

	void slideVolume(double targetVolume, double volumeSlideSpeed);
	void slidePitch(double targetPitch, double pitchSlideSpeed);

	int getPriority() const;
	void setPriority(int priority);

	uint onLoseSource();
	void update();
	bool toBeRemoved() const;

private:
	SoundInstance(Sound& sound);
	~SoundInstance();

	Sound& sound;
	uint sourceID;

	uint timestamp;
	int priority;
	bool looping;
	double volume;
	double pitch;
	double targetVolume;
	double targetPitch;
	double volumeSlideSpeed;
	double pitchSlideSpeed;
	bool pauseAtSlideEnd;
};

#endif