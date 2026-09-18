#include "pch.h"
#include "soundinstance.h"
#include "engine.h"

SoundInstance::SoundInstance(Sound& sound) : sound(sound)
{
	// Every member before the source is asked for, so that an instance that
	// gets none is still a whole object for the delete that follows.
	timestamp = ~0;
	priority = 0;
	looping = false;
	volume = targetVolume = 1.0f;
	pitch = targetPitch = 1.0f;
	volumeSlideSpeed = 0.0f;
	pitchSlideSpeed = 0.0f;
	pauseAtSlideEnd = false;

	// create the audio source
	sourceID = Sound::getFreeSource();
	if(sourceID)
	{
		// plug in the audio buffer
		alSourcei(sourceID, AL_BUFFER, sound.bufferID);

		setVolume(1.0f);
		setPitch(1.0f);

		timestamp = Engine::inst().getTime();
	}
}

SoundInstance::~SoundInstance()
{
	if(!sourceID) return;

	// stop and free the audio source
	stop();
	alDeleteSources(1, &sourceID);
}

void SoundInstance::play(bool loop)
{
	if(!sourceID) return;

	// set the parameters
	alSourcei(sourceID, AL_LOOPING, loop ? 1 : 0);
	this->looping = loop;

	// play it
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

float SoundInstance::getVolume() const
{
	return volume;
}

void SoundInstance::setVolume(float volume)
{
	if(!sourceID) return;

	this->volume = volume;
	// The sound's own factor comes in here and not into volume: this is the one
	// place where a volume reaches OpenAL, which makes it apply to
	// slideVolume() and to every caller that sets volume itself.
	alSourcef(sourceID, AL_GAIN,
		volume * sound.getVolumeFactor() * Engine::inst().getEffectiveSoundVolume());
}

float SoundInstance::getPitch() const
{
	return pitch;
}

void SoundInstance::setPitch(float pitch)
{
	if(!sourceID) return;

	this->pitch = pitch;
	alSourcef(sourceID, AL_PITCH, pitch);
}

void SoundInstance::slideVolume(float targetVolume,
								float volumeSlideSpeed)
{
	if(targetVolume < 0.0f)
	{
		// Pause afterwards!
		targetVolume = 0.0f;
		pauseAtSlideEnd = true;
	}
	else pauseAtSlideEnd = false;

	this->targetVolume = targetVolume;
	this->volumeSlideSpeed = volumeSlideSpeed;
}

void SoundInstance::slidePitch(float targetPitch,
							   float pitchSlideSpeed)
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

	if(volumeSlideSpeed > 0.0f)
	{
		float currentVolume = getVolume();
		float newVolume = currentVolume * (1.0f - volumeSlideSpeed) + targetVolume * volumeSlideSpeed;
		if(abs(targetVolume - newVolume) < 0.01f)
		{
			newVolume = targetVolume;
			volumeSlideSpeed = 0.0f;

			if(pauseAtSlideEnd)
			{
				// Pause now!
				pause();
				pauseAtSlideEnd = false;
			}
		}

		setVolume(newVolume);
	}

	if(pitchSlideSpeed > 0.0f)
	{
		float currentPitch = getPitch();
		float newPitch = currentPitch * (1.0f - pitchSlideSpeed) + targetPitch * pitchSlideSpeed;
		if(abs(targetPitch - newPitch) < 0.01f)
		{
			newPitch = targetPitch;
			pitchSlideSpeed = 0.0f;
		}

		setPitch(newPitch);
	}
}

bool SoundInstance::toBeRemoved() const
{
	if(!sourceID) return true;

	// Is the sound finished?
	ALint state = 0;
	alGetSourcei(sourceID, AL_SOURCE_STATE, &state);
	return state == AL_STOPPED;
}