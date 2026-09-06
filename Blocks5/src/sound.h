#ifndef _SOUND_H
#define _SOUND_H

/*** Klasse fuer einen Sound ***/

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

	// Wie laut dieser Klang gegenueber seiner Datei gespielt wird - 1.0, wenn
	// er nicht in data/sounds.xml steht. Einmal beim Anlegen nachgeschlagen und
	// nicht bei jedem Abspielen, denn die Tabelle aendert sich nicht mehr.
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