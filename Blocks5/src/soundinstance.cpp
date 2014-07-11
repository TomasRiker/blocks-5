#include "pch.h"
#include "soundinstance.h"
#include "engine.h"

SoundInstance::SoundInstance(Sound& sound) : sound(sound)
{
	// Audioquelle erzeugen
	sourceID = Sound::getFreeSource();
	if(sourceID)
	{
		// Audio-Buffer einsetzen
		alSourcei(sourceID, AL_BUFFER, sound.bufferID);

		setVolume(1.0);
		setPitch(1.0);

		timestamp = Engine::inst().getTime();
		priority = 0;
		looping = false;
		volumeSlideSpeed = 0.0;
		pitchSlideSpeed = 0.0;
		pauseAtSlideEnd = false;
	}
	else
	{
		timestamp = ~0;
		priority = 0;
		looping = false;
	}
}

SoundInstance::~SoundInstance()
{
	if(!sourceID) return;

	// Audioquelle stoppen und freigeben
	stop();
	alDeleteSources(1, &sourceID);
}

void SoundInstance::play(bool loop)
{
	if(!sourceID) return;

	// Parameter setzen
	alSourcei(sourceID, AL_LOOPING, loop ? 1 : 0);
	this->looping = loop;

	// abspielen
	alSourcePlay(sourceID);
}

void SoundInstance::stop()
{
	if(!sourceID) return;
	alSourceStop(sourceID);
}

void SoundInstance::pause()
{
	if(!sourceID) return;
	alSourcePause(sourceID);
}

void SoundInstance::resume()
{
	if(!sourceID) return;
	alSourcePlay(sourceID);
}

double SoundInstance::getVolume() const
{
	return volume;
}

void SoundInstance::setVolume(double volume)
{
	if(!sourceID) return;

	this->volume = volume;
	alSourcef(sourceID, AL_GAIN, static_cast<float>(volume * Engine::inst().getSoundVolume()));
}

double SoundInstance::getPitch() const
{
	return pitch;
}

void SoundInstance::setPitch(double pitch)
{
	if(!sourceID) return;

	this->pitch = pitch;
	alSourcef(sourceID, AL_PITCH, static_cast<float>(pitch));
}

void SoundInstance::slideVolume(double targetVolume,
								double volumeSlideSpeed)
{
	if(targetVolume < 0.0)
	{
		// Danach anhalten!
		targetVolume = 0.0;
		pauseAtSlideEnd = true;
	}
	else pauseAtSlideEnd = false;

	this->targetVolume = targetVolume;
	this->volumeSlideSpeed = volumeSlideSpeed;
}

void SoundInstance::slidePitch(double targetPitch,
							   double pitchSlideSpeed)
{
	this->targetPitch = targetPitch;
	this->pitchSlideSpeed = pitchSlideSpeed;
}

int SoundInstance::getPriority() const
{
	return priority;
}

void SoundInstance::setPriority(int priority)
{
	this->priority = priority;
}

uint SoundInstance::onLoseSource()
{
	if(!sourceID) return 0;

	alSourceStop(sourceID);
	uint r = sourceID;
	sourceID = 0;
	priority = 0;
	timestamp = ~0;

	return r;
}

void SoundInstance::update()
{
	if(!sourceID) return;

	if(Engine::inst().wasVolumeChanged()) setVolume(getVolume());

	if(volumeSlideSpeed > 0.0)
	{
		double currentVolume = getVolume();
		double newVolume = currentVolume * (1.0 - volumeSlideSpeed) + targetVolume * volumeSlideSpeed;
		if(abs(targetVolume - newVolume) < 0.01)
		{
			newVolume = targetVolume;
			volumeSlideSpeed = 0.0;

			if(pauseAtSlideEnd)
			{
				// Jetzt anhalten!
				pause();
				pauseAtSlideEnd = false;
			}
		}

		setVolume(newVolume);
	}

	if(pitchSlideSpeed > 0.0)
	{
		double currentPitch = getPitch();
		double newPitch = currentPitch * (1.0 - pitchSlideSpeed) + targetPitch * pitchSlideSpeed;
		if(abs(targetPitch - newPitch) < 0.01)
		{
			newPitch = targetPitch;
			pitchSlideSpeed = 0.0;
		}

		setPitch(newPitch);
	}
}

bool SoundInstance::toBeRemoved() const
{
	if(!sourceID) return true;

	// Ist der Sound fertig?
	ALint state = 0;
	alGetSourcei(sourceID, AL_SOURCE_STATE, &state);
	return state == AL_STOPPED;
}