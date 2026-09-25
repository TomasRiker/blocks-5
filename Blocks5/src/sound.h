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

	// Is this still the live instance it was? One is deleted once its sound
	// has played out or its source was taken, so whatever keeps the pointer
	// across ticks (the diamond machine and the hint, to slide their sound
	// down) has to ask before touching it again - with the serial it took
	// along, since a newer instance can be allocated at the same address.
	static bool isLiveInstance(SoundInstance* p_instance, uint serial);

	// How loud this sound plays relative to its file, 1.0 if it is not in
	// data/sounds.xml. Looked up once at construction; the table never
	// changes.
	float getVolumeFactor() const;

	static uint getFreeSource();

private:
	Sound(const std::string& filename, int options);
	~Sound();

	static bool forceReload() { return false; }

	uint bufferID;
	float volumeFactor;
	std::set<SoundInstance*> instances;
	uint lastInstanceCreatedAt;
	static std::set<SoundInstance*> allInstances;
};

#endif