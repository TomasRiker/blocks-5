#ifndef _AUDIOCAPTURE_H
#define _AUDIOCAPTURE_H

/*** Klasse zum Mitschneiden des Systemklangs ***/

// Nimmt das auf, was das Standard-Wiedergabegerät gerade ausgibt, also Musik und
// Soundeffekte des Spiels - und nicht das Mikrofon. Unter Windows passiert das
// über den Loopback-Modus von WASAPI; das Gerät muss dafür nicht als Aufnahme-
// quelle eingerichtet sein ("Stereomix" o. Ä.).
//
// Die Samples kommen immer als 16 Bit, Stereo, interleaved heraus, unabhängig
// davon, in welchem Format das Gerät selbst arbeitet. Ein "Sample" ist dabei
// wie bei OpenAL ein Paar aus linkem und rechtem Kanal.
//
// Auf anderen Plattformen als Windows schlägt open() fehl; die aufgenommenen
// Videos bleiben dann stumm.

struct AudioCaptureImpl;

class AudioCapture
{
public:
	AudioCapture();
	~AudioCapture();

	// öffnet die Loopback-Aufnahme des Standard-Wiedergabegeräts
	bool open(uint sampleRate = 48000);

	// beendet die Aufnahme und gibt alles wieder frei
	void close();

	bool isOpen() const;

	// Name des Geräts, von dem aufgenommen wird (nur für die Logdatei)
	const std::string& getDeviceName() const;

	// beginnt bzw. beendet das Sammeln von Samples
	void start();
	void stop();

	// Anzahl der abholbereiten Samples
	int getNumSamplesReady();

	// holt numSamples Samples ab; was fehlt, wird mit Stille aufgefüllt
	void getSamples(short* p_buffer, int numSamples);

private:
	// nicht kopierbar - der Puffer und der Thread gehören genau einem Objekt
	AudioCapture(const AudioCapture&);
	AudioCapture& operator=(const AudioCapture&);

	AudioCaptureImpl* p_impl;
};

#endif
