#include "pch.h"
#include "sound.h"
#include "soundinstance.h"
#include "audiostream.h"
#include "engine.h"

std::set<SoundInstance*> Sound::allInstances;

Sound::Sound(const std::string& filename, int) : Resource(filename)
{
	bufferID = 0;
	lastInstanceCreatedAt = 0;
	volumeFactor = Engine::inst().getSoundVolumeFactor(filename);

	AudioStream* p_stream = AudioStream::open(filename);
	if(!p_stream)
	{
		printfLog("+ ERROR: Could not create audio stream for audio file \"%s\".\n",
				  filename.c_str());
		error = 2;
		return;
	}

	// create the OpenAL buffer
	alGetError();
	alGenBuffers(1, &bufferID);
	ALenum err = alGetError();
	if(err)
	{
		printfLog("+ ERROR: Could not create audio buffer for audio file \"%s\" (Error: %d).\n",
				  filename.c_str(),
				  err);
		error = 2;
		delete p_stream;
		return;
	}

	// get the format
	ALenum format = p_stream->getOpenALBufferFormat();
	if(!format)
	{
		printfLog("+ ERROR: Format of audio file \"%s\" is not supported.\n",
				  filename.c_str());
		error = 3;
		delete p_stream;
		return;
	}

	// read the data
	uint length = p_stream->getLength();
	uint size = length * p_stream->getSliceSize();
	char* p_data = new char[size];
	if(p_stream->read(p_data, length) != length)
	{
		printfLog("+ ERROR: Could not read from audio stream for audio file \"%s\".\n",
				  filename.c_str());
		error = 4;
		delete[] p_data;
		delete p_stream;
		return;
	}

	// fill the buffer with the data
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
		delete p_stream;
		return;
	}

	// free the data again
	delete[] p_data;
	delete p_stream;
}

Sound::~Sound()
{
	// Delete all instances and take them out of allInstances as well:
	// getFreeSource() dereferences every entry there once the source pool
	// runs dry.
	for(std::set<SoundInstance*>::const_iterator i = instances.begin(); i != instances.end(); ++i)
	{
		allInstances.erase(*i);
		delete *i;
	}

	// free the sound
	if(bufferID) alDeleteBuffers(1, &bufferID);
}

SoundInstance* Sound::createInstance(bool forceCreation)
{
	// The same sound twice within 10 ms is one too many: a dozen blocks landing
	// in one tick make the same impact a dozen times, and stacked up that is
	// not louder but broken. The one-shots from Engine::playSound() are what
	// the lockout is for, and they can live with the 0.
	//
	// The looping sounds must ask with forceCreation. Each holds its instance
	// in a static that the last object of a level empties and the first
	// object of the next refills, with only the building of the new level in
	// between: 12 and 13 ms measured for toxic and mask, which every level
	// has, against the 10 ms lockout. On a faster machine they would get a 0,
	// and nothing refills the static until the last object is gone.
	if(!forceCreation)
	{
		uint t = SDL_GetTicks();
		uint dt = t - lastInstanceCreatedAt;
		if(dt < 10) return 0;
	}

	SoundInstance* p_inst = new SoundInstance(*this);
	if(!p_inst->sourceID)
	{
		// No source, and no one-shot to take one from. An instance without
		// one would be reaped at the next update() under an object still
		// holding the pointer, so the caller gets a 0 instead.
		delete p_inst;
		return 0;
	}

	instances.insert(p_inst);
	allInstances.insert(p_inst);
	lastInstanceCreatedAt = SDL_GetTicks();

	return p_inst;
}

void Sound::update()
{
	std::set<SoundInstance*> garbage;

	// go through all instances, update them and delete them where needed
	for(std::set<SoundInstance*>::const_iterator i = instances.begin(); i != instances.end(); ++i)
	{
		SoundInstance* p_inst = *i;
		p_inst->update();
		if(p_inst->toBeRemoved()) garbage.insert(p_inst);
	}

	// delete the garbage
	for(std::set<SoundInstance*>::const_iterator i = garbage.begin(); i != garbage.end(); ++i)
	{
		SoundInstance* p_inst = *i;
		delete p_inst;
		instances.erase(p_inst);
		allInstances.erase(p_inst);
	}
}

float Sound::getVolumeFactor() const
{
	return volumeFactor;
}

bool Sound::isLiveInstance(SoundInstance* p_instance, uint serial)
{
	// The serial is read only once the set has vouched for the pointer.
	return p_instance && allInstances.find(p_instance) != allInstances.end() &&
		   p_instance->getSerial() == serial;
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

		// find the instance with the lowest priority that has been playing longest
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
			// take the audio source away from that instance
			sourceID = p_oldestInstance->onLoseSource();
		}

		return sourceID;
	}
}