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

	// Puffergroesse in Bytes berechnen (1/4 Sekunde)
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
	if(playing) return;

	// Audioquelle holen
	sourceID = Sound::getFreeSource();
	setVolume(getVolume());
	setPitch(getPitch());

	// einen Puffer dekodieren und anhaengen
	stream(buffers[0]);

	// abspielen
	alSourcePlay(sourceID);

	finish = false;
	playing = true;
	startDecoderThread();
}

void StreamedSound::stop()
{
	if(!playing) return;

	// Erst den Thread einsammeln, dann die Quelle anhalten: pumpBuffers()
	// startet eine Quelle wieder, die es als AL_STOPPED vorfindet, koennte ein
	// alSourceStop davor also ueberholen.
	joinDecoderThread();
	playing = false;

	alSourceStop(sourceID);

	// Soundquelle loeschen
	alDeleteSources(1, &sourceID);

	// alle Puffer loeschen
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

#ifdef __EMSCRIPTEN__
	// Hier gibt es keinen Dekodier-Thread; die Warteschlange wird aus dem
	// Logiktakt heraus gefuellt.
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

// Ein Durchgang durch die OpenAL-Warteschlange: einsammeln, was abgespielt
// wurde, und wieder auffuellen. Unter Windows ruft der Dekodier-Thread das
// alle zehn Millisekunden auf; im Browser gibt es keine Threads, dort macht
// update() es einmal je Logiktakt.
void StreamedSound::pumpBuffers()
{
	// Irgendwelche Puffer fertig?
	int n = 0;
	alGetSourcei(sourceID, AL_BUFFERS_PROCESSED, &n);
	if(n > 0)
	{
		// Puffer holen
		uint* p_buffers = new uint[n];
		alSourceUnqueueBuffers(sourceID, n, p_buffers);

		// diese Puffer wieder auffuellen
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
		return;
	}

	// Laeuft die Warteschlange leer, haelt das die Quelle an, ohne sie zu
	// leeren - die Abfrage oben bekommt das also nie zu fassen: das Auffuellen
	// gibt der Quelle vier frische Puffer, AL_BUFFERS_QUEUED steht wieder auf
	// 4, und sie bleibt fuer den Rest der Sitzung AL_STOPPED. Die Musik ist
	// dann einfach weg. Im Browser ist das kein Sonderfall, sondern die normale
	// Folge eines Tab-Wechsels: die Hauptschleife haengt an
	// requestAnimationFrame, eine verborgene Seite bekommt keines, und in der
	// Warteschlange liegen vier Viertelsekunden. Wer laenger als eine Sekunde
	// weg ist, hoert nichts mehr.
	// Eine absichtlich angehaltene Quelle muss angehalten bleiben, deshalb
	// zaehlt nur AL_STOPPED als "bitte neu starten"; AL_PAUSED und AL_PLAYING
	// bleiben unangetastet.
	int state = AL_PLAYING;
	alGetSourcei(sourceID, AL_SOURCE_STATE, &state);
	if(state == AL_STOPPED) alSourcePlay(sourceID);
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

	// mit Daten fuellen
	alBufferData(bufferID, p_stream->getOpenALBufferFormat(), p_buffer, numSlicesRead * p_stream->getSliceSize(), p_stream->getSampleRate());

	// anhaengen
	alSourceQueueBuffers(sourceID, 1, &bufferID);
}

// Alles ab hier gibt es nur unter Windows. Im Browser bricht SDL_CreateThread
// ab und SDL_WaitThread ruft abort(); Semaphoren kennt dessen SDL gar nicht.
#ifdef __EMSCRIPTEN__

void StreamedSound::startDecoderThread()
{
	// Kein Thread: die uebrigen Puffer gleich hier fuellen, nachgelegt wird
	// dann aus update() heraus, einmal je Logiktakt.
	for(int i = 1; i < 4; i++) stream(buffers[i]);
}

void StreamedSound::joinDecoderThread()
{
}

#else

void StreamedSound::startDecoderThread()
{
	// Das Semaphor gehoert zu diesem einen Durchgang und wird zusammen mit dem
	// Thread angelegt und weggeraeumt. Eines, das den Sound ueberdauert,
	// brachte womoeglich einen Zaehlerstand aus der vorigen Runde mit - naemlich
	// dann, wenn der Thread schon am Dateiende von selbst ausgestiegen war und
	// stop() danach ins Leere gepostet hat -, und der naechste Thread wuerde
	// sofort wieder aussteigen.
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
	// die uebrigen Puffer fuellen
	for(int i = 1; i < 4; i++) stream(buffers[i]);

	// Die Wartezeit ist zugleich das Abbruchsignal: SDL_SemWaitTimeout kehrt
	// mit SDL_MUTEX_TIMEDOUT zurueck, wenn die zehn Millisekunden einfach
	// verstrichen sind, und mit 0, sobald joinDecoderThread() gepostet hat.
	// Alles andere (-1) ist ein Fehler und beendet den Thread ebenfalls.
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