#include "pch.h"
#include "audiocapture.h"

namespace
{
	// Groesse des Ringpuffers in Sekunden. Der Rekorder holt die Samples nur
	// dann ab, wenn ein Videoframe fertig ist, also alle ~33 ms; ein paar
	// Sekunden Reserve ueberbruecken auch laengere Haenger (Levelwechsel).
	const int k_ringBufferSeconds = 4;

	// So weit darf der Ringpuffer hinter der Echtzeit zurueckfallen, bevor mit
	// Stille aufgefuellt wird. Muss deutlich groesser sein als die Paketrate der
	// Aufnahme (~10 ms), sonst wird gepolstert, obwohl gleich noch echte Daten
	// kommen.
	const int k_silenceSlackMS = 60;

	// Hoechstens so viel Stille am Stueck einfuegen (in Sekunden)
	const int k_maxSilenceBurstSeconds = 1;
}

// ---------------------------------------------------------------------------
// Der Ringpuffer. Er steht vor der Fallunterscheidung, weil er auf beiden
// Plattformen derselbe ist: was die Aufnahme liefert, sind hier wie dort
// 16-Bit-Stereo-Samples, und verschieden ist nur der Weg dorthin - der
// Loopback-Modus von WASAPI unter Windows, der Monitor der Standardsenke unter
// Linux. Der Aufnahmefaden schreibt, der Rekorder liest, der Mutex trennt die
// beiden.
// ---------------------------------------------------------------------------

struct AudioRing
{
	AudioRing();
	~AudioRing();

	// Puffer und Mutex anlegen bzw. wieder abraeumen.
	bool allocate(uint sampleRate);
	void release();

	// Alles wegwerfen, was noch vom letzten Mal darin liegt.
	void clearRing();

	// Fertige Stereo-Samples anhaengen; ist kein Platz mehr, weichen die
	// aeltesten.
	void push(const short* p_samples, int numSamples);

	// numSamples Stille anhaengen.
	void pushSilence(int numSamples);

	// Die Luecke nach der Uhr auffuellen. Solange nichts abgespielt wird, haelt
	// der Audiodienst an und liefert gar keine Pakete mehr; ohne das hier waere
	// die Tonspur kuerzer als das Video. expected ist die Zahl der Samples, die
	// seit dem Start haetten kommen muessen.
	void padToClock(long long expected);

	// Die Leserseite, vom Rekorder aus einem anderen Faden gerufen.
	int  available();
	void read(short* p_buffer, int numSamples);

	SDL_mutex* p_mutex;
	short* p_ring;
	int ringSize;   // in Samples
	int ringRead;   // in Samples
	int ringFill;   // in Samples
	uint sampleRate;
	bool opened;
	bool overflowed;
	long long samplesWritten;
};

AudioRing::AudioRing()
	: p_mutex(0)
	, p_ring(0)
	, ringSize(0)
	, ringRead(0)
	, ringFill(0)
	, sampleRate(48000)
	, opened(false)
	, overflowed(false)
	, samplesWritten(0)
{
}

AudioRing::~AudioRing()
{
	release();
}

bool AudioRing::allocate(uint sampleRate)
{
	this->sampleRate = sampleRate;
	ringSize = (int)sampleRate * k_ringBufferSeconds;
	p_ring = new short[ringSize * 2];
	ringRead = 0;
	ringFill = 0;
	samplesWritten = 0;
	overflowed = false;
	p_mutex = SDL_CreateMutex();
	return p_mutex != 0;
}

void AudioRing::release()
{
	if(p_mutex)
	{
		SDL_DestroyMutex(p_mutex);
		p_mutex = 0;
	}
	delete[] p_ring;
	p_ring = 0;
	ringSize = 0;
	ringRead = 0;
	ringFill = 0;
	opened = false;
}

void AudioRing::clearRing()
{
	SDL_LockMutex(p_mutex);
	ringRead = 0;
	ringFill = 0;
	SDL_UnlockMutex(p_mutex);
}

void AudioRing::push(const short* p_samples, int numSamples)
{
	if(numSamples <= 0 || !p_ring) return;

	SDL_LockMutex(p_mutex);

	// mehr als der ganze Puffer auf einmal: nur das Ende behalten
	if(numSamples > ringSize)
	{
		p_samples += 2 * (numSamples - ringSize);
		numSamples = ringSize;
	}

	// ist kein Platz mehr, weichen die aeltesten Samples
	const int overflow = ringFill + numSamples - ringSize;
	if(overflow > 0)
	{
		ringRead = (ringRead + overflow) % ringSize;
		ringFill -= overflow;
		overflowed = true;
	}

	int write = (ringRead + ringFill) % ringSize;
	int left = numSamples;
	while(left > 0)
	{
		const int chunk = left < (ringSize - write) ? left : (ringSize - write);
		memcpy(p_ring + 2 * write, p_samples, chunk * 2 * sizeof(short));
		p_samples += 2 * chunk;
		write = (write + chunk) % ringSize;
		left -= chunk;
	}
	ringFill += numSamples;

	SDL_UnlockMutex(p_mutex);
}

void AudioRing::pushSilence(int numSamples)
{
	const int k_scratchSamples = 1024;
	short scratch[k_scratchSamples * 2];
	memset(scratch, 0, sizeof(scratch));

	while(numSamples > 0)
	{
		const int chunk = numSamples < k_scratchSamples ? numSamples : k_scratchSamples;
		push(scratch, chunk);
		samplesWritten += chunk;
		numSamples -= chunk;
	}
}

void AudioRing::padToClock(long long expected)
{
	const long long slack = (long long)sampleRate * k_silenceSlackMS / 1000;
	long long missing = expected - samplesWritten;
	if(missing <= slack) return;

	// War die Pause sehr lang (Levelwechsel, angehaltener Prozess), wird der
	// Rest verworfen statt endlos aufgeholt.
	const long long maxBurst = (long long)sampleRate * k_maxSilenceBurstSeconds;
	if(missing > maxBurst)
	{
		samplesWritten += missing - maxBurst;
		missing = maxBurst;
	}
	pushSilence((int)missing);
}

int AudioRing::available()
{
	if(!opened || !p_mutex) return 0;
	SDL_LockMutex(p_mutex);
	const int numSamples = ringFill;
	SDL_UnlockMutex(p_mutex);
	return numSamples;
}

void AudioRing::read(short* p_buffer, int numSamples)
{
	if(numSamples <= 0) return;
	if(!opened || !p_ring)
	{
		memset(p_buffer, 0, numSamples * 2 * sizeof(short));
		return;
	}

	SDL_LockMutex(p_mutex);

	const int numAvailable = numSamples < ringFill ? numSamples : ringFill;
	short* p_out = p_buffer;
	int left = numAvailable;
	while(left > 0)
	{
		const int chunk = left < (ringSize - ringRead) ? left : (ringSize - ringRead);
		memcpy(p_out, p_ring + 2 * ringRead, chunk * 2 * sizeof(short));
		p_out += 2 * chunk;
		ringRead = (ringRead + chunk) % ringSize;
		left -= chunk;
	}
	ringFill -= numAvailable;

	SDL_UnlockMutex(p_mutex);

	// war nicht genug da, wird der Rest stumm
	if(numAvailable < numSamples) memset(p_buffer + 2 * numAvailable, 0, (numSamples - numAvailable) * 2 * sizeof(short));
}

#ifdef _WIN32

#include <windows.h>
#include <objbase.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

namespace
{
	// KSDATAFORMAT_SUBTYPE_PCM und KSDATAFORMAT_SUBTYPE_IEEE_FLOAT. Die stehen sonst
	// in <ksmedia.h>, das aber den ganzen Kernel-Streaming-Kram nachzieht; hier werden
	// nur diese beiden GUIDs gebraucht.
	const GUID k_subformatPCM       = { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
	const GUID k_subformatIEEEFloat = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

	// PKEY_Device_FriendlyName, sonst aus <functiondiscoverykeys_devpkey.h>
	const PROPERTYKEY k_deviceFriendlyName = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };

	// gewuenschte Laenge des WASAPI-Puffers in 100-ns-Einheiten (2 Sekunden)
	const REFERENCE_TIME k_wasapiBufferDuration = 20000000;

	// Pause zwischen zwei Abholrunden
	const int k_pollDelayMS = 5;

	inline short floatToShort(float value)
	{
		int i = (int)(value * 32767.0f + (value >= 0.0f ? 0.5f : -0.5f));
		if(i >  32767) i =  32767;
		if(i < -32768) i = -32768;
		return (short)i;
	}
}

struct AudioCaptureImpl : public AudioRing
{
	AudioCaptureImpl();

	int threadProc();

	// Format des Geraets auswerten; false, wenn wir damit nichts anfangen koennen
	bool setupSourceFormat(const WAVEFORMATEX* p_format);

	// liest ein Frame des Geraets und macht daraus ein Stereo-Paar
	void readSourceFrame(const BYTE* p_frame, float& left, float& right) const;

	// rechnet numFrames Geraetesamples um und schiebt sie in den Ringpuffer
	void convertAndPush(const BYTE* p_data, int numFrames, bool silent);

	std::string deviceName;
	long initResult;
	bool initOK;

	SDL_Thread* p_thread;
	SDL_sem* p_initSemaphore;

	volatile bool quit;
	volatile bool capturing;

	// Format des Geraets
	int srcChannels;
	int srcRate;
	int srcBits;
	int srcBlockAlign;
	bool srcFloat;

	// Zustand des Resamplers (nur im Capture-Thread)
	double resampleStep;
	double resamplePos;
	float prevLeft;
	float prevRight;
	bool havePrev;
};

int audioCaptureThreadProc(void* p_param);

AudioCaptureImpl::AudioCaptureImpl()
	: deviceName("(unknown)")
	, initResult(0)
	, initOK(false)
	, p_thread(0)
	, p_initSemaphore(0)
	, quit(false)
	, capturing(false)
	, srcChannels(2)
	, srcRate(48000)
	, srcBits(32)
	, srcBlockAlign(8)
	, srcFloat(true)
	, resampleStep(1.0)
	, resamplePos(0.0)
	, prevLeft(0.0f)
	, prevRight(0.0f)
	, havePrev(false)
{
}

bool AudioCaptureImpl::setupSourceFormat(const WAVEFORMATEX* p_format)
{
	srcChannels = p_format->nChannels;
	srcRate = (int)p_format->nSamplesPerSec;
	srcBits = p_format->wBitsPerSample;
	srcBlockAlign = p_format->nBlockAlign;
	srcFloat = false;

	if(p_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) srcFloat = true;
	else if(p_format->wFormatTag == WAVE_FORMAT_PCM) srcFloat = false;
	else if(p_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && p_format->cbSize >= 22)
	{
		// Der Mischer meldet fast immer WAVE_FORMAT_EXTENSIBLE; erst die SubFormat-GUID
		// sagt, ob Fliesskomma oder Ganzzahl im Puffer steht.
		const WAVEFORMATEXTENSIBLE* p_extensible = (const WAVEFORMATEXTENSIBLE*)p_format;
		if(IsEqualGUID(p_extensible->SubFormat, k_subformatIEEEFloat)) srcFloat = true;
		else if(IsEqualGUID(p_extensible->SubFormat, k_subformatPCM)) srcFloat = false;
		else return false;
	}
	else return false;

	if(srcChannels < 1 || srcRate < 1) return false;
	if(srcFloat && srcBits != 32) return false;
	if(!srcFloat && srcBits != 16 && srcBits != 24 && srcBits != 32) return false;
	if(srcBlockAlign < srcChannels * (srcBits / 8)) return false;

	resampleStep = (double)srcRate / (double)sampleRate;
	return true;
}

void AudioCaptureImpl::readSourceFrame(const BYTE* p_frame, float& left, float& right) const
{
	float channel[2] = { 0.0f, 0.0f };
	const int numChannels = srcChannels < 2 ? 1 : 2;
	const int bytesPerChannel = srcBits / 8;

	// Bei mehr als zwei Kanaelen (5.1, 7.1) sind die ersten beiden vorne links und
	// rechts; die reichen fuer den Mitschnitt.
	for(int c = 0; c < numChannels; c++)
	{
		const BYTE* p_sample = p_frame + c * bytesPerChannel;
		if(srcFloat) channel[c] = *(const float*)p_sample;
		else if(srcBits == 16) channel[c] = (float)(*(const short*)p_sample) * (1.0f / 32768.0f);
		else if(srcBits == 24)
		{
			const unsigned int raw = ((unsigned int)p_sample[0] << 8) | ((unsigned int)p_sample[1] << 16) | ((unsigned int)p_sample[2] << 24);
			channel[c] = (float)((int)raw >> 8) * (1.0f / 8388608.0f);
		}
		else channel[c] = (float)(*(const int*)p_sample) * (1.0f / 2147483648.0f);
	}

	if(numChannels == 1) { left = channel[0]; right = channel[0]; }
	else { left = channel[0]; right = channel[1]; }
}

void AudioCaptureImpl::convertAndPush(const BYTE* p_data, int numFrames, bool silent)
{
	const int k_scratchSamples = 1024;
	short scratch[k_scratchSamples * 2];
	int numInScratch = 0;

	for(int f = 0; f < numFrames; f++)
	{
		float left = 0.0f, right = 0.0f;
		if(!silent) readSourceFrame(p_data + f * srcBlockAlign, left, right);

		if(!havePrev)
		{
			prevLeft = left;
			prevRight = right;
			resamplePos = 0.0;
			havePrev = true;
		}

		// linear zwischen dem vorigen und dem aktuellen Geraetesample interpolieren.
		// Bei gleicher Abtastrate ist resampleStep genau 1.0, dann kommt jedes Sample
		// unveraendert durch.
		while(resamplePos < 1.0)
		{
			const float t = (float)resamplePos;
			scratch[2 * numInScratch    ] = floatToShort(prevLeft  + (left  - prevLeft ) * t);
			scratch[2 * numInScratch + 1] = floatToShort(prevRight + (right - prevRight) * t);
			numInScratch++;
			if(numInScratch == k_scratchSamples)
			{
				push(scratch, numInScratch);
				samplesWritten += numInScratch;
				numInScratch = 0;
			}
			resamplePos += resampleStep;
		}
		resamplePos -= 1.0;
		prevLeft = left;
		prevRight = right;
	}

	if(numInScratch)
	{
		push(scratch, numInScratch);
		samplesWritten += numInScratch;
	}
}

int AudioCaptureImpl::threadProc()
{
	// COM gehoert dem Thread, der es initialisiert - alle Schnittstellen unten werden
	// deshalb nur hier angelegt, benutzt und wieder freigegeben.
	long hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	const bool comInitialized = SUCCEEDED(hr);

	IMMDeviceEnumerator* p_enumerator = 0;
	IMMDevice* p_device = 0;
	IAudioClient* p_audioClient = 0;
	IAudioCaptureClient* p_captureClient = 0;
	WAVEFORMATEX* p_mixFormat = 0;

	if(comInitialized)
	{
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL,
							  __uuidof(IMMDeviceEnumerator), (void**)&p_enumerator);

		// eRender statt eCapture: aufgenommen wird der Ausgang, nicht der Eingang
		if(SUCCEEDED(hr)) hr = p_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &p_device);

		if(SUCCEEDED(hr))
		{
			IPropertyStore* p_properties = 0;
			if(SUCCEEDED(p_device->OpenPropertyStore(STGM_READ, &p_properties)))
			{
				PROPVARIANT name;
				PropVariantInit(&name);
				if(SUCCEEDED(p_properties->GetValue(k_deviceFriendlyName, &name)) && name.vt == VT_LPWSTR && name.pwszVal)
				{
					char buffer[256];
					const int length = WideCharToMultiByte(CP_ACP, 0, name.pwszVal, -1, buffer, sizeof(buffer), 0, 0);
					if(length > 0) deviceName = buffer;
				}
				PropVariantClear(&name);
				p_properties->Release();
			}
		}

		if(SUCCEEDED(hr)) hr = p_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, 0, (void**)&p_audioClient);
		if(SUCCEEDED(hr)) hr = p_audioClient->GetMixFormat(&p_mixFormat);

		// Im Shared Mode gibt der Mischer das Format vor, umgerechnet wird hier.
		if(SUCCEEDED(hr) && !setupSourceFormat(p_mixFormat)) hr = AUDCLNT_E_UNSUPPORTED_FORMAT;

		if(SUCCEEDED(hr)) hr = p_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
														 k_wasapiBufferDuration, 0, p_mixFormat, 0);
		if(SUCCEEDED(hr)) hr = p_audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&p_captureClient);
	}

	initResult = hr;
	initOK = comInitialized && SUCCEEDED(hr) && p_captureClient != 0;

	// Der Hauptthread wartet auf dieses Signal und wertet initOK aus. printfLog darf
	// hier nicht benutzt werden, es hat einen statischen Puffer und ist nicht
	// threadsicher - deshalb wird nur das Ergebnis hinterlegt.
	SDL_SemPost(p_initSemaphore);

	if(initOK)
	{
		LARGE_INTEGER qpcFrequency;
		QueryPerformanceFrequency(&qpcFrequency);
		LARGE_INTEGER captureStart;
		captureStart.QuadPart = 0;

		bool started = false;
		bool running = false;
		bool deviceLost = false;

		while(!quit)
		{
			// started statt running, damit ein fehlgeschlagenes Start() nicht alle
			// paar Millisekunden neu versucht wird
			if(capturing && !started)
			{
				started = true;

				// alles wegwerfen, was noch vom letzten Mal herumliegt
				clearRing();
				havePrev = false;
				resamplePos = 0.0;
				samplesWritten = 0;
				overflowed = false;
				p_audioClient->Reset();
				QueryPerformanceCounter(&captureStart);
				running = SUCCEEDED(p_audioClient->Start());
				deviceLost = !running;
			}
			else if(!capturing && started)
			{
				if(running) p_audioClient->Stop();
				running = false;
				started = false;
			}

			if(!capturing)
			{
				SDL_Delay(20);
				continue;
			}

			// alle bereitliegenden Pakete abholen
			while(running && !deviceLost)
			{
				UINT32 packetFrames = 0;
				hr = p_captureClient->GetNextPacketSize(&packetFrames);
				if(FAILED(hr)) { deviceLost = true; break; }
				if(!packetFrames) break;

				BYTE* p_data = 0;
				UINT32 numFrames = 0;
				DWORD flags = 0;
				hr = p_captureClient->GetBuffer(&p_data, &numFrames, &flags, 0, 0);
				if(hr == AUDCLNT_S_BUFFER_EMPTY) break;
				if(FAILED(hr)) { deviceLost = true; break; }

				// Bei AUDCLNT_BUFFERFLAGS_SILENT zeigt p_data ins Leere, die Samples
				// muessen aber trotzdem gezaehlt werden.
				convertAndPush(p_data, (int)numFrames, (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0);
				p_captureClient->ReleaseBuffer(numFrames);
			}

			// Solange nichts abgespielt wird, haelt die Audio-Engine an und liefert gar
			// keine Pakete mehr; padToClock() fuellt die Luecke.
			LARGE_INTEGER now;
			QueryPerformanceCounter(&now);
			padToClock((now.QuadPart - captureStart.QuadPart) * (LONGLONG)sampleRate / qpcFrequency.QuadPart);

			SDL_Delay(k_pollDelayMS);
		}

		if(running) p_audioClient->Stop();
	}

	if(p_mixFormat) CoTaskMemFree(p_mixFormat);
	if(p_captureClient) p_captureClient->Release();
	if(p_audioClient) p_audioClient->Release();
	if(p_device) p_device->Release();
	if(p_enumerator) p_enumerator->Release();
	if(comInitialized) CoUninitialize();

	return 0;
}

int audioCaptureThreadProc(void* p_param)
{
	return ((AudioCaptureImpl*)p_param)->threadProc();
}

AudioCapture::AudioCapture()
{
	p_impl = new AudioCaptureImpl;
}

AudioCapture::~AudioCapture()
{
	close();
	delete p_impl;
}

bool AudioCapture::open(uint sampleRate)
{
	if(p_impl->opened) return true;

	p_impl->quit = false;
	p_impl->capturing = false;

	p_impl->p_initSemaphore = SDL_CreateSemaphore(0);
	if(!p_impl->allocate(sampleRate) || !p_impl->p_initSemaphore)
	{
		printfLog("+ WARNING: Could not create audio capture thread objects.\n");
		close();
		return false;
	}

	p_impl->p_thread = SDL_CreateThread(audioCaptureThreadProc, p_impl);
	if(!p_impl->p_thread)
	{
		printfLog("+ WARNING: Could not create audio capture thread.\n");
		close();
		return false;
	}

	// auf das Ergebnis der WASAPI-Initialisierung warten
	SDL_SemWait(p_impl->p_initSemaphore);
	if(!p_impl->initOK)
	{
		printfLog("+ WARNING: Could not open loopback capture (HRESULT 0x%08X).\n", (uint)p_impl->initResult);
		close();
		return false;
	}

	p_impl->opened = true;
	return true;
}

void AudioCapture::close()
{
	if(p_impl->p_thread)
	{
		p_impl->capturing = false;
		p_impl->quit = true;
		SDL_WaitThread(p_impl->p_thread, 0);
		p_impl->p_thread = 0;
	}
	if(p_impl->p_initSemaphore)
	{
		SDL_DestroySemaphore(p_impl->p_initSemaphore);
		p_impl->p_initSemaphore = 0;
	}
	p_impl->release();
}

bool AudioCapture::isOpen() const
{
	return p_impl->opened;
}

const std::string& AudioCapture::getDeviceName() const
{
	return p_impl->deviceName;
}

void AudioCapture::start()
{
	if(p_impl->opened) p_impl->capturing = true;
}

void AudioCapture::stop()
{
	p_impl->capturing = false;
}

#else // _WIN32

// ---------------------------------------------------------------------------
// Linux. Was WASAPI den Loopback-Modus nennt, heisst bei PulseAudio Monitor:
// zu jeder Ausgabesenke gibt es eine gleichnamige Quelle, die mithoert, was
// gerade hinausgeht. "@DEFAULT_MONITOR@" ist die der Standardsenke, so dass
// nichts aufgezaehlt werden muss. PipeWire bringt mit pipewire-pulse dieselbe
// Schnittstelle mit, also deckt der eine Weg beide ab.
//
// libpulse wird zur Laufzeit geladen und nicht dazugebunden: das Spiel soll
// auch dort starten, wo kein PulseAudio liegt - dann bleiben die Videos stumm,
// wie bisher auf jeder Nicht-Windows-Plattform. Aus demselben Grund sind die
// paar gebrauchten Deklarationen hier von Hand aufgeschrieben statt aus
// <pulse/simple.h> geholt: sonst braeuchte der Build libpulse-dev.
//
// pa_simple bekommt gesagt, welches Format es liefern soll - S16LE, stereo,
// 48 kHz -, und der Server rechnet um. Deshalb gibt es hier weder die
// Formatumrechnung noch den Resampler der Windows-Seite.
// ---------------------------------------------------------------------------

#include <dlfcn.h>

namespace
{
	// Der Ausschnitt der libpulse-ABI, den diese Datei braucht. pa_simple_new
	// nimmt fuer Kanalzuordnung und Pufferwuensche NULL, damit bleiben von den
	// Strukturen nur die Formatangabe uebrig.
	enum { PA_SAMPLE_S16LE = 3 };
	enum { PA_STREAM_RECORD = 2 };

	struct pa_sample_spec
	{
		int format;
		uint32_t rate;
		uint8_t channels;
	};

	typedef struct pa_simple pa_simple;

	typedef pa_simple* (*pa_simple_new_t)(const char*, const char*, int, const char*,
										  const char*, const pa_sample_spec*,
										  const void*, const void*, int*);
	typedef int   (*pa_simple_read_t)(pa_simple*, void*, size_t, int*);
	typedef void  (*pa_simple_free_t)(pa_simple*);
	typedef const char* (*pa_strerror_t)(int);

	struct PulseAPI
	{
		void* p_library;
		pa_simple_new_t  simple_new;
		pa_simple_read_t simple_read;
		pa_simple_free_t simple_free;
		pa_strerror_t    strerror;

		PulseAPI() : p_library(0), simple_new(0), simple_read(0), simple_free(0), strerror(0) {}

		bool load()
		{
			if(p_library) return true;

			// Die Version im Namen ist Absicht: libpulse-simple.so ohne Nummer
			// gehoert zum Entwicklungspaket und liegt auf einem Spielrechner
			// nicht.
			p_library = dlopen("libpulse-simple.so.0", RTLD_LAZY | RTLD_LOCAL);
			if(!p_library) return false;

			simple_new  = (pa_simple_new_t) dlsym(p_library, "pa_simple_new");
			simple_read = (pa_simple_read_t)dlsym(p_library, "pa_simple_read");
			simple_free = (pa_simple_free_t)dlsym(p_library, "pa_simple_free");
			// pa_strerror steht in libpulse selbst, nicht in libpulse-simple.
			strerror    = (pa_strerror_t)   dlsym(p_library, "pa_strerror");

			if(!simple_new || !simple_read || !simple_free)
			{
				dlclose(p_library);
				p_library = 0;
				return false;
			}
			return true;
		}

		const char* errorText(int error) const
		{
			return strerror ? strerror(error) : "unknown error";
		}
	};

	// Wie viele Samples auf einmal geholt werden. pa_simple_read wartet, bis so
	// viele da sind, also darf es nicht zu viel sein - sonst haengt der Faden
	// beim Beenden zu lange. 10 ms sind auch das, was WASAPI je Paket liefert.
	const int k_readSamples = 480;
}

struct AudioCaptureImpl : public AudioRing
{
	AudioCaptureImpl();

	int threadProc();

	std::string deviceName;
	PulseAPI pulse;
	pa_simple* p_stream;

	SDL_Thread* p_thread;
	SDL_sem* p_initSemaphore;
	bool initOK;
	int initError;

	volatile bool quit;
	volatile bool capturing;
};

int audioCaptureThreadProc(void* p_param);

AudioCaptureImpl::AudioCaptureImpl()
	: deviceName("(unknown)")
	, p_stream(0)
	, p_thread(0)
	, p_initSemaphore(0)
	, initOK(false)
	, initError(0)
	, quit(false)
	, capturing(false)
{
}

int AudioCaptureImpl::threadProc()
{
	pa_sample_spec spec;
	spec.format   = PA_SAMPLE_S16LE;
	spec.rate     = sampleRate;
	spec.channels = 2;

	// "@DEFAULT_MONITOR@" loest der Server auf den Monitor der gerade
	// eingestellten Standardsenke auf - genau das, was der Spieler hoert.
	p_stream = pulse.simple_new(0, "Blocks 5", PA_STREAM_RECORD, "@DEFAULT_MONITOR@",
								"video capture", &spec, 0, 0, &initError);
	initOK = p_stream != 0;
	if(initOK) deviceName = "@DEFAULT_MONITOR@";
	SDL_SemPost(p_initSemaphore);
	if(!initOK) return 0;

	bool started = false;
	double captureStart = 0.0;
	short buffer[k_readSamples * 2];

	while(!quit)
	{
		if(capturing && !started)
		{
			started = true;
			// alles wegwerfen, was noch vom letzten Mal herumliegt
			clearRing();
			samplesWritten = 0;
			overflowed = false;
			captureStart = getExactTime();
		}
		else if(!capturing && started)
		{
			started = false;
		}

		int error = 0;
		if(pulse.simple_read(p_stream, buffer, sizeof(buffer), &error) < 0)
		{
			printfLog("+ WARNING: Audio capture read failed (%s).\n", pulse.errorText(error));
			break;
		}

		// Auch wenn gerade nicht aufgenommen wird, muss weitergelesen werden:
		// sonst laeuft der Puffer des Servers ueber und die naechste Aufnahme
		// beginnt mit Sekunden alter Musik.
		if(!started) continue;

		push(buffer, k_readSamples);
		samplesWritten += k_readSamples;

		// Eine ruhende Senke liefert nichts mehr - module-suspend-on-idle ist
		// voreingestellt geladen -, und pa_simple_read wartet dann. Die Luecke
		// nach der Uhr auffuellen, damit die Tonspur so lang wird wie das Video.
		padToClock((long long)((getExactTime() - captureStart) * sampleRate));
	}

	return 0;
}

int audioCaptureThreadProc(void* p_param)
{
	return static_cast<AudioCaptureImpl*>(p_param)->threadProc();
}

AudioCapture::AudioCapture()
{
	p_impl = new AudioCaptureImpl;
}

AudioCapture::~AudioCapture()
{
	close();
	delete p_impl;
}

bool AudioCapture::open(uint sampleRate)
{
	if(p_impl->opened) return true;

	if(!p_impl->pulse.load())
	{
		printfLog("+ WARNING: libpulse-simple.so.0 is not available.\n");
		return false;
	}

	p_impl->quit = false;
	p_impl->capturing = false;

	p_impl->p_initSemaphore = SDL_CreateSemaphore(0);
	if(!p_impl->allocate(sampleRate) || !p_impl->p_initSemaphore)
	{
		printfLog("+ WARNING: Could not create audio capture thread objects.\n");
		close();
		return false;
	}

	p_impl->p_thread = SDL_CreateThread(audioCaptureThreadProc, p_impl);
	if(!p_impl->p_thread)
	{
		printfLog("+ WARNING: Could not create audio capture thread.\n");
		close();
		return false;
	}

	// auf das Ergebnis von pa_simple_new warten
	SDL_SemWait(p_impl->p_initSemaphore);
	if(!p_impl->initOK)
	{
		printfLog("+ WARNING: Could not open the monitor of the default sink (%s).\n",
				  p_impl->pulse.errorText(p_impl->initError));
		close();
		return false;
	}

	p_impl->opened = true;
	return true;
}

void AudioCapture::close()
{
	if(p_impl->p_thread)
	{
		p_impl->capturing = false;
		p_impl->quit = true;
		SDL_WaitThread(p_impl->p_thread, 0);
		p_impl->p_thread = 0;
	}
	if(p_impl->p_stream)
	{
		p_impl->pulse.simple_free(p_impl->p_stream);
		p_impl->p_stream = 0;
	}
	if(p_impl->p_initSemaphore)
	{
		SDL_DestroySemaphore(p_impl->p_initSemaphore);
		p_impl->p_initSemaphore = 0;
	}
	p_impl->release();
}

bool AudioCapture::isOpen() const
{
	return p_impl->opened;
}

const std::string& AudioCapture::getDeviceName() const
{
	return p_impl->deviceName;
}

void AudioCapture::start()
{
	p_impl->capturing = true;
}

void AudioCapture::stop()
{
	p_impl->capturing = false;
}

#endif // _WIN32

// ---------------------------------------------------------------------------
// Die Leserseite gehoert beiden: sie liest nur den Ringpuffer.
// ---------------------------------------------------------------------------

int AudioCapture::getNumSamplesReady()
{
	return p_impl->available();
}

void AudioCapture::getSamples(short* p_buffer, int numSamples)
{
	p_impl->read(p_buffer, numSamples);
}
