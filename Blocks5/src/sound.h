#ifndef _SOUND_H
#define _SOUND_H

/*** Klasse für einen Sound ***/

#include "resource.h"

class AudioStream;

class Sound : public Resource<Sound>
{
	friend class Manager<Sound>;
	friend class SoundInstance;

public:
	SoundInstance* createInstance(bool forceCreation = false);
	void update();

	const std::set<SoundInstance*>& getInstances() const;

	static uint getFreeSource();

private:
	Sound(const std::string& filename);
	~Sound();

	static bool forceReload() { return false; }

	uint bufferID;
	std::set<SoundInstance*> instances;
	uint lastInstanceCreatedAt;
	static std::set<SoundInstance*> allInstances;
};

#endif