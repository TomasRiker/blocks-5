#include "pch.h"
#include "streamedsound.h"
#include "sound.h"
#include "audiostream.h"
#include "engine.h"

StreamedSound::StreamedSound(const std::string& filename) : Resource(filename)
{
	p_stream = 0;
	p_buffer = 0;
	sourceID = 0;
	p_thread = 0;
	volume = pitch = 1.0;
	volumeSlideSpeed = 0.0;
	pitchSlideSpeed = 0.0;
	stopAtSlideEnd = false;
	loopBegin = 0.0;
	loopBeginInSlices = 0;

	p_stream = AudioStream::open(filename);
	if(!p_stream)
	{
		printfLog("+ ERROR: Could not create audio stream for audio file \"%s\".\n",
				  filename.c_str());
		error = 1;
		return;
	}

	// Puffergröße in Bytes berechnen (1/4 Sekunde)
	bufferSize = p_stream->getSampleRate() / 4 * p_stream->getSliceSize();

	if(!p_stream->getOpenALBufferFormat())
	{
		printfLog("+ ERROR: Format of audio file \"%s\" is not supported.\n",
				  filename.c_str());
		error = 2;
		return;
	}

	// Puffer reservieren
	p_buffer = new char[bufferSize];

	// OpenAL-Puffer erzeugen
	alGenBuffers(4, buffers);
}

StreamedSound::~StreamedSound()
{
	stop();
	delete p_stream;
	delete[] p_buffer;
}

void StreamedSound::play(bool loop)
{
	this->loop = loop;
	if(p_thread) return;

	// Audioquelle holen
	sourceID = Sound::getFreeSource();
	setVolume(getVolume());
	setPitch(getPitch());

	// einen Puffer dekodieren und anhängen
	stream(buffers[0]);

	// abspielen
	alSourcePlay(sourceID);

	// Thread erzeugen
	finish = false;
	p_thread = SDL_CreateThread(streamedSoundThreadProc, this);
}

void StreamedSound::stop()
{
	if(!p_thread) return;

	alSourceStop(sourceID);

	// Thread beenden
	finish = true;
	SDL_WaitThread(p_thread, 0);
	p_thread = 0;

	// Soundquelle löschen
	alDeleteSources(1, &sourceID);

	// alle Puffer löschen
	alDeleteBuffers(4, buffers);
}

void StreamedSound::pause()
{
	alSourcePause(sourceID);
}

void StreamedSound::resume()
{
	alSourcePlay(sourceID);
}

double StreamedSound::getVolume() const
{
	return volume;
}

void StreamedSound::setVolume(double volume)
{
	this->volume = volume;
	if(sourceID) alSourcef(sourceID, AL_GAIN, static_cast<float>(volume * Engine::inst().getMusicVolume()));
}

double StreamedSound::getPitch() const
{
	return pitch;
}

void StreamedSound::setPitch(double pitch)
{
	this->pitch = pitch;
	if(sourceID) alSourcef(sourceID, AL_PITCH, static_cast<float>(pitch));
}

double StreamedSound::getLoopBegin() const
{
	return loopBegin;
}

void StreamedSound::setLoopBegin(double loopBegin)
{
	this->loopBegin = loopBegin;
	loopBeginInSlices = secondsToSlices(loopBegin);
}

uint StreamedSound::secondsToSlices(double t) const
{
	return static_cast<uint>(t * p_stream->getSampleRate());
}

void StreamedSound::slideVolume(double targetVolume,
								double volumeSlideSpeed)
{
	if(targetVolume < 0.0)
	{
		// Danach anhalten!
		targetVolume = 0.0;
		stopAtSlideEnd = true;
	}
	else stopAtSlideEnd = false;

	this->targetVolume = targetVolume;
	this->volumeSlideSpeed = volumeSlideSpeed;
}

void StreamedSound::slidePitch(double targetPitch,
							   double pitchSlideSpeed)
{
	this->targetPitch = targetPitch;
	this->pitchSlideSpeed = pitchSlideSpeed;
}

bool StreamedSound::update()
{
	if(Engine::inst().wasVolumeChanged()) setVolume(getVolume());

	if(volumeSlideSpeed > 0.0)
	{
		double currentVolume = getVolume();
		double newVolume = currentVolume * (1.0 - volumeSlideSpeed) + targetVolume * volumeSlideSpeed;
		if(abs(targetVolume - newVolume) < 0.01)
		{
			newVolume = targetVolume;
			volumeSlideSpeed = 0.0;

			if(stopAtSlideEnd)
			{
				// Jetzt anhalten!
				return false;
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

	return true;
}

int StreamedSound::threadProc()
{
	// die übrigen Puffer füllen
	for(int i = 1; i < 4; i++) stream(buffers[i]);

	while(!finish)
	{
		// Irgendwelche Puffer fertig?
		int n = 0;
		alGetSourcei(sourceID, AL_BUFFERS_PROCESSED, &n);
		if(n > 0)
		{
			// Puffer holen
			uint* p_buffers = new uint[n];
			alSourceUnqueueBuffers(sourceID, n, p_buffers);

			// diese Puffer wieder auffüllen
			for(int i = 0; i < n; i++) stream(p_buffers[i]);

			delete[] p_buffers;
		}

		// Wie viele Puffer sind in der Warteschlange?
		n = 0;
		alGetSourcei(sourceID, AL_BUFFERS_QUEUED, &n);
		if(!n)
		{
			// Sound neu abspielen
			alSourcePlay(sourceID);
		}

		SDL_Delay(10);
	}

	return 0;
}

void StreamedSound::stream(uint bufferID)
{
	// lesen
	uint numSlices = bufferSize / p_stream->getSliceSize();
	uint numSlicesRead = p_stream->read(p_buffer, numSlices);
	if(numSlicesRead != numSlices)
	{
		if(loop)
		{
			// wieder von vorne anfangen
			p_stream->seek(loopBeginInSlices);
		}
		else finish = true;
	}

	// mit Daten füllen
	alBufferData(bufferID, p_stream->getOpenALBufferFormat(), p_buffer, numSlicesRead * p_stream->getSliceSize(), p_stream->getSampleRate());

	// anhängen
	alSourceQueueBuffers(sourceID, 1, &bufferID);
}

int streamedSoundThreadProc(void* p_param)
{
	StreamedSound* p_this = static_cast<StreamedSound*>(p_param);
	return p_this->threadProc();
}