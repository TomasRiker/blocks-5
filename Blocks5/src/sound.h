#ifndef _SOUND_H
#define _SOUND_H

/*** Class for a sound ***/

#include "resource.h"

class AudioStream;
class SoundInstance;

class Sound : public Resource<Sound>
{
	friend class Manager<Sound>;
	friend class SoundInstance;

public:
	SoundInstance* createInstance(bool forceCreation = false);
	void update();

	const std::set<SoundInstance*>& getInstances() const;

	// Is this still a live instance? An instance is deleted the moment its
	// sound has played out, so anything that keeps the pointer across ticks -
	// the diamond machine holds the one its conversion started, to slide it
	// down when the conversion falls through - has to ask before it touches
	// it again.
	static bool isLiveInstance(SoundInstance* p_instance);

	// How loud this sound plays relative to its file - 1.0 if it is not in
	// data/sounds.xml. Looked up once at construction and not on every
	// playback, since the table never changes again.
	double getVolumeFactor() const;

	static uint getFreeSource();

private:
	Sound(const std::string& filename);
	~Sound();

	static bool forceReload() { return false; }

	uint bufferID;
	double volumeFactor;
	std::set<SoundInstance*> instances;
	uint lastInstanceCreatedAt;
	static std::set<SoundInstance*> allInstances;
};

#endif