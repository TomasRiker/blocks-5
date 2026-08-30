#ifndef _AUDIOCAPTURE_H
#define _AUDIOCAPTURE_H

/*** Klasse zum Mitschneiden des Systemklangs ***/

// Nimmt das auf, was das Standard-Wiedergabegeraet gerade ausgibt, also Musik und
// Soundeffekte des Spiels - und nicht das Mikrofon. Unter Windows passiert das
// ueber den Loopback-Modus von WASAPI; das Geraet muss dafuer nicht als Aufnahme-
// quelle eingerichtet sein ("Stereomix" o. Ae.).
//
// Die Samples kommen immer als 16 Bit, Stereo, interleaved heraus, unabhaengig
// davon, in welchem Format das Geraet selbst arbeitet. Ein "Sample" ist dabei
// wie bei OpenAL ein Paar aus linkem und rechtem Kanal.
//
// Auf anderen Plattformen als Windows schlaegt open() fehl; die aufgenommenen
// Videos bleiben dann stumm.

struct AudioCaptureImpl;

class AudioCapture
{
public:
	AudioCapture();
	~AudioCapture();

	// oeffnet die Loopback-Aufnahme des Standard-Wiedergabegeraets
	bool open(uint sampleRate = 48000);

	// beendet die Aufnahme und gibt alles wieder frei
	void close();

	bool isOpen() const;

	// Name des Geraets, von dem aufgenommen wird (nur fuer die Logdatei)
	const std::string& getDeviceName() const;

	// beginnt bzw. beendet das Sammeln von Samples
	void start();
	void stop();

	// Anzahl der abholbereiten Samples
	int getNumSamplesReady();

	// holt numSamples Samples ab; was fehlt, wird mit Stille aufgefuellt
	void getSamples(short* p_buffer, int numSamples);

private:
	// nicht kopierbar - der Puffer und der Thread gehoeren genau einem Objekt
	AudioCapture(const AudioCapture&);
	AudioCapture& operator=(const AudioCapture&);

	AudioCaptureImpl* p_impl;
};

#endif
