#ifndef _STREAMEDSOUND_H
#define _STREAMEDSOUND_H

/*** Klasse fuer gestreamte Sounds (z.B. Musik) ***/

#include "resource.h"

class AudioStream;

class StreamedSound : public Resource<StreamedSound>
{
	friend class Manager<StreamedSound>;
	friend int streamedSoundThreadProc(void* p_param);

public:
	void play(bool loop = true);
	void stop();
	void pause();
	void resume();

	double getVolume() const;
	void setVolume(double volume);
	double getPitch() const;
	void setPitch(double pitch);
	double getLoopBegin() const;
	void setLoopBegin(double loopBegin);

	uint secondsToSlices(double t) const;

	void slideVolume(double targetVolume, double volumeSlideSpeed);
	void slidePitch(double targetPitch, double pitchSlideSpeed);
	bool update();

private:
	StreamedSound(const std::string& filename);
	~StreamedSound();

	int threadProc();
	void startDecoderThread();   // beide sind im Browser fast leer
	void joinDecoderThread();
	void pumpBuffers();   // ein Durchgang durch die OpenAL-Warteschlange
	void stream(uint bufferID);

	static bool forceReload() { return true; }

	AudioStream* p_stream;
	bool loop;
	uint sourceID;
	uint buffers[4];
	uint bufferSize;
	char* p_buffer;

	// p_thread ist ausschliesslich der Dekodier-Thread und im Browser immer 0;
	// ob dieser Sound laeuft, sagt playing. Frueher trug p_thread beides, und
	// die Browser-Fassung brauchte deshalb einen Zeiger, der keiner war.
	SDL_Thread* p_thread;
	bool playing;

#ifndef __EMSCRIPTEN__
	// Wird hochgezaehlt, wenn der Dekodier-Thread aufhoeren soll. SDL 1.2 hat
	// keine atomaren Typen, und ein volatile bool ist keine Synchronisierung;
	// ein Semaphor ist dagegen beides zugleich - das Signal und die Wartezeit
	// zwischen zwei Durchgaengen, die vorher ein SDL_Delay war. Unter Windows
	// steht ein echtes Kernel-Objekt dahinter (WaitForSingleObject), nicht die
	// Schleife mit 1-ms-Pausen, vor der SDL_mutex.h fuer andere Systeme warnt.
	SDL_sem* p_stopSignal;
#endif

	// Ist der Datenstrom zu Ende? Das schreibt und liest ausschliesslich, wer
	// die Puffer fuellt - unter Windows der Dekodier-Thread, im Browser
	// update(). Es geht nie ueber eine Thread-Grenze, also kein volatile.
	bool finish;

	double volume;
	double pitch;
	double targetVolume;
	double targetPitch;
	double volumeSlideSpeed;
	double pitchSlideSpeed;
	bool stopAtSlideEnd;

	double loopBegin;
	uint loopBeginInSlices;
};

int streamedSoundThreadProc(void* p_param);

#endif