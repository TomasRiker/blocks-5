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
	playing = false;
	finish = false;
#ifndef __EMSCRIPTEN__
	p_stopSignal = 0;
#endif
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

	// compute the buffer size in bytes (1/4 second)
	bufferSize = p_stream->getSampleRate() / 4 * p_stream->getSliceSize();

	if(!p_stream->getOpenALBufferFormat())
	{
		printfLog("+ ERROR: Format of audio file \"%s\" is not supported.\n",
				  filename.c_str());
		error = 2;
		return;
	}

	// allocate the buffer
	p_buffer = new char[bufferSize];

	// create the OpenAL buffers
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
	if(playing) return;

	// get an audio source
	sourceID = Sound::getFreeSource();
	setVolume(getVolume());
	setPitch(getPitch());

	// decode one buffer and queue it
	stream(buffers[0]);

	// play
	alSourcePlay(sourceID);

	finish = false;
	playing = true;
	startDecoderThread();
}

void StreamedSound::stop()
{
	if(!playing) return;

	// Join the thread first, then stop the source: pumpBuffers() restarts a
	// source it finds as AL_STOPPED and could therefore overtake an
	// alSourceStop placed before it.
	joinDecoderThread();
	playing = false;

	alSourceStop(sourceID);

	// delete the sound source
	alDeleteSources(1, &sourceID);

	// delete all buffers
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
		// Stop afterwards!
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

#ifdef __EMSCRIPTEN__
	// There is no decoder thread here; the queue is filled from the logic
	// tick.
	if(playing && !finish) pumpBuffers();
#endif

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
				// Stop now!
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

// One pass through the OpenAL queue: collect what has been played and refill
// it. Under Windows the decoder thread calls this every ten milliseconds; in
// the browser there are no threads, and update() does it once per logic tick.
void StreamedSound::pumpBuffers()
{
	// any buffers finished?
	int n = 0;
	alGetSourcei(sourceID, AL_BUFFERS_PROCESSED, &n);
	if(n > 0)
	{
		// get the buffers
		uint* p_buffers = new uint[n];
		alSourceUnqueueBuffers(sourceID, n, p_buffers);

		// refill these buffers
		for(int i = 0; i < n; i++) stream(p_buffers[i]);

		delete[] p_buffers;
	}

	// how many buffers are in the queue?
	n = 0;
	alGetSourcei(sourceID, AL_BUFFERS_QUEUED, &n);
	if(!n)
	{
		// play the sound again
		alSourcePlay(sourceID);
		return;
	}

	// If the queue runs dry, that stops the source without emptying it - the
	// refill gives it four fresh buffers, and it stays AL_STOPPED for the rest
	// of the session. In the browser that is the normal consequence of a tab
	// switch: a hidden page gets no requestAnimationFrame, and the queue holds
	// four quarter-seconds. A source halted on purpose must stay halted, which
	// is why only AL_STOPPED counts as "please restart".
	int state = AL_PLAYING;
	alGetSourcei(sourceID, AL_SOURCE_STATE, &state);
	if(state == AL_STOPPED) alSourcePlay(sourceID);
}

void StreamedSound::stream(uint bufferID)
{
	// read
	uint numSlices = bufferSize / p_stream->getSliceSize();
	uint numSlicesRead = p_stream->read(p_buffer, numSlices);
	if(numSlicesRead != numSlices)
	{
		if(loop)
		{
			// start again from the beginning
			p_stream->seek(loopBeginInSlices);
		}
		else finish = true;
	}

	// fill with data
	alBufferData(bufferID, p_stream->getOpenALBufferFormat(), p_buffer, numSlicesRead * p_stream->getSliceSize(), p_stream->getSampleRate());

	// queue it
	alSourceQueueBuffers(sourceID, 1, &bufferID);
}

// Everything from here on exists only under Windows. In the browser
// SDL_CreateThread gives up and SDL_WaitThread calls abort(); its SDL does not
// know semaphores at all.
#ifdef __EMSCRIPTEN__

void StreamedSound::startDecoderThread()
{
	// No thread: fill the remaining buffers right here, and topping up then
	// happens from update(), once per logic tick.
	for(int i = 1; i < 4; i++) stream(buffers[i]);
}

void StreamedSound::joinDecoderThread()
{
}

#else

void StreamedSound::startDecoderThread()
{
	// The semaphore belongs to this one run and is created and cleaned up
	// together with the thread. One that outlived the sound could carry a
	// count over from the previous round, and the next thread would bail out
	// immediately.
	p_stopSignal = SDL_CreateSemaphore(0);
	p_thread = SDL_CreateThread(streamedSoundThreadProc, this);
}

void StreamedSound::joinDecoderThread()
{
	if(!p_thread) return;

	SDL_SemPost(p_stopSignal);
	SDL_WaitThread(p_thread, 0);
	p_thread = 0;

	SDL_DestroySemaphore(p_stopSignal);
	p_stopSignal = 0;
}

int StreamedSound::threadProc()
{
	// fill the remaining buffers
	for(int i = 1; i < 4; i++) stream(buffers[i]);

	// The wait doubles as the stop signal: SDL_SemWaitTimeout returns
	// SDL_MUTEX_TIMEDOUT once the ten milliseconds have passed, and 0 as soon
	// as joinDecoderThread() has posted. Anything else (-1) is an error and
	// ends the thread as well.
	while(!finish)
	{
		pumpBuffers();
		if(SDL_SemWaitTimeout(p_stopSignal, 10) != SDL_MUTEX_TIMEDOUT) break;
	}

	return 0;
}

int streamedSoundThreadProc(void* p_param)
{
	StreamedSound* p_this = static_cast<StreamedSound*>(p_param);
	return p_this->threadProc();
}

#endif