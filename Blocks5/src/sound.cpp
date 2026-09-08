#include "pch.h"
#include "sound.h"
#include "soundinstance.h"
#include "audiostream.h"
#include "engine.h"

std::set<SoundInstance*> Sound::allInstances;

Sound::Sound(const std::string& filename) : Resource(filename)
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
	// delete all instances, and take them out of the list of all of them:
	// getFreeSource() walks that one and dereferences every entry, so a sound
	// released while its instances were still listed would leave it reading
	// freed memory the next time the source pool ran dry.
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
	// The same sound twice within 10 ms is one too many: a dozen falling blocks
	// trigger the same impact a dozen times in one tick, and laid on top of
	// each other that is not louder, it is broken. The lockout applies to the
	// one-shots from Engine::playSound(), which can take a 0 as well - one
	// impact out of twelve simply drops out.
	//
	// The looping sounds ask with forceCreation, and that is not a luxury but
	// the difference between running and crashing. They hold their one instance
	// in a static that goes to 0 with the last object and is refilled by the
	// first object of the next level - and between those two moments lies
	// nothing but the building of the new level. Measured on toxic and mask,
	// the only two that sit in every level: 12 and 13 ms. The lockout stands at
	// 10. Two milliseconds of slack on this machine, none on a faster one, and
	// behind it waits a null pointer with setVolume() on it in the next line.
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

double Sound::getVolumeFactor() const
{
	return volumeFactor;
}

bool Sound::isLiveInstance(SoundInstance* p_instance)
{
	return p_instance && allInstances.find(p_instance) != allInstances.end();
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