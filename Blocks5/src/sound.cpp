#include "pch.h"
#include "sound.h"
#include "soundinstance.h"
#include "audiostream.h"

std::set<SoundInstance*> Sound::allInstances;

Sound::Sound(const std::string& filename) : Resource(filename)
{
	lastInstanceCreatedAt = 0;

	AudioStream* p_stream = AudioStream::open(filename);
	if(!p_stream)
	{
		printfLog("+ ERROR: Could not create audio stream for audio file \"%s\".\n",
				  filename.c_str());
		error = 2;
		return;
	}

	// OpenAL-Buffer erzeugen
	alGetError();
	alGenBuffers(1, &bufferID);
	ALenum err = alGetError();
	if(err)
	{
		printfLog("+ ERROR: Could not create audio buffer for audio file \"%s\" (Error: %d).\n",
				  filename.c_str(),
				  err);
		error = 2;
		return;
	}

	// Format holen
	ALenum format = p_stream->getOpenALBufferFormat();
	if(!format)
	{
		printfLog("+ ERROR: Format of audio file \"%s\" is not supported.\n",
				  filename.c_str());
		error = 3;
		return;
	}

	// Daten lesen
	uint length = p_stream->getLength();
	uint size = length * p_stream->getSliceSize();
	char* p_data = new char[size];
	if(p_stream->read(p_data, length) != length)
	{
		printfLog("+ ERROR: Could not read from audio stream for audio file \"%s\".\n",
				  filename.c_str());
		error = 4;
		delete[] p_data;
		return;
	}

	// Buffer mit Daten füllen
	alGetError();
	alBufferData(bufferID, format, p_data, size, p_stream->getSampleRate());
	err = alGetError();
	if(err)
	{
		printfLog("+ ERROR: Could not fill audio buffer for audio file \"%s\" (Error: %d).\n",
				  filename.c_str(),
				  err);
		error = 5;
		delete[] p_data;
		return;
	}

	// Daten wieder freigeben
	delete[] p_data;
	delete p_stream;
}

Sound::~Sound()
{
	// alle Instanzen löschen
	for(std::set<SoundInstance*>::const_iterator i = instances.begin(); i != instances.end(); ++i) delete *i;

	// Sound freigeben
	alDeleteBuffers(1, &bufferID);
}

SoundInstance* Sound::createInstance(bool forceCreation)
{
	if(!forceCreation)
	{
		uint t = SDL_GetTicks();
		uint dt = t - lastInstanceCreatedAt;
		if(dt < 10) return 0;
	}

	SoundInstance* p_inst = new SoundInstance(*this);
	instances.insert(p_inst);
	allInstances.insert(p_inst);
	lastInstanceCreatedAt = SDL_GetTicks();

	return p_inst;
}

void Sound::update()
{
	std::set<SoundInstance*> garbage;

	// alle Instanzen durchgehen, aktualisieren und bei Bedarf löschen
	for(std::set<SoundInstance*>::const_iterator i = instances.begin(); i != instances.end(); ++i)
	{
		SoundInstance* p_inst = *i;
		p_inst->update();
		if(p_inst->toBeRemoved()) garbage.insert(p_inst);
	}

	// Müll löschen
	for(std::set<SoundInstance*>::const_iterator i = garbage.begin(); i != garbage.end(); ++i)
	{
		SoundInstance* p_inst = *i;
		delete p_inst;
		instances.erase(p_inst);
		allInstances.erase(p_inst);
	}
}

const std::set<SoundInstance*>& Sound::getInstances() const
{
	return instances;
}

uint Sound::getFreeSource()
{
	uint sourceID = 0;

	alGenSources(1, &sourceID);
	if(sourceID) return sourceID;
	else
	{
		alGetError();

		// die Instanz mit der niedrigsten Priorität, die schon am längsten spielt, suchen
		int lowestPriority = 0x7FFFFFFF;
		uint oldestTimestamp = ~0;
		SoundInstance* p_oldestInstance = 0;
		for(std::set<SoundInstance*>::const_iterator i = allInstances.begin(); i != allInstances.end(); ++i)
		{
			if(!(*i)->looping)
			{
				if((*i)->priority < lowestPriority ||
				   ((*i)->priority <= lowestPriority &&
					(*i)->timestamp < oldestTimestamp))
				{
					lowestPriority = (*i)->priority;
					oldestTimestamp = (*i)->timestamp;
					p_oldestInstance = *i;
				}
			}
		}

		if(p_oldestInstance)
		{
			// dieser Instanz die Audioquelle entziehen
			sourceID = p_oldestInstance->onLoseSource();
		}

		return sourceID;
	}
}