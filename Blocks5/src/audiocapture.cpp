#include "pch.h"
#include "audiocapture.h"

namespace
{
	// Size of the ring buffer in seconds. The recorder fetches the samples
	// only when a video frame is finished, that is every ~33 ms; a few
	// seconds of slack bridge longer stalls (a level change) too.
	const int k_ringBufferSeconds = 4;

	// How far the ring buffer may fall behind real time before it is padded
	// with silence. Must be well above the capture's packet rate (~10 ms), or
	// it pads while real data is about to arrive.
	const int k_silenceSlackMS = 60;

	// Insert at most this much silence in one go (in seconds)
	const int k_maxSilenceBurstSeconds = 1;
}

// ---------------------------------------------------------------------------
// The ring buffer. It stands ahead of the platform split because it is the
// same on both: what the capture delivers is 16-bit stereo samples either way,
// and only the path there differs - WASAPI's loopback mode under Windows, the
// monitor source of the default sink under Linux. The capture thread writes,
// the recorder reads, the mutex separates the two.
// ---------------------------------------------------------------------------

struct AudioRing
{
	AudioRing();
	~AudioRing();

	// Create buffer and mutex, or tear them down again.
	bool allocate(uint sampleRate);
	void release();

	// Throw away everything still in there from last time.
	void clearRing();

	// Append finished stereo samples; with no room left, the oldest give way.
	void push(const short* p_samples, int numSamples);

	// Append numSamples of silence.
	void pushSilence(int numSamples);

	// Fill the gap by the clock. While nothing is playing, the audio service
	// stops and delivers no packets at all; without this the audio track would
	// be shorter than the video. expected is the number of samples that should
	// have arrived since the start.
	void padToClock(long long expected);

	// The reader side, called by the recorder from another thread.
	int  available();
	void read(short* p_buffer, int numSamples);

	SDL_mutex* p_mutex;
	short* p_ring;
	int ringSize;   // in samples
	int ringRead;   // in samples
	int ringFill;   // in samples
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

	// more than the whole buffer at once: keep only the end
	if(numSamples > ringSize)
	{
		p_samples += 2 * (numSamples - ringSize);
		numSamples = ringSize;
	}

	// with no room left, the oldest samples give way
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

	// After a very long pause (a level change, a suspended process) the rest
	// is dropped rather than caught up endlessly.
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

	// not enough there: the rest goes silent
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
	// KSDATAFORMAT_SUBTYPE_PCM and KSDATAFORMAT_SUBTYPE_IEEE_FLOAT. They otherwise
	// live in <ksmedia.h>, which drags in the whole kernel-streaming apparatus; only
	// these two GUIDs are needed here.
	const GUID k_subformatPCM       = { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
	const GUID k_subformatIEEEFloat = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

	// PKEY_Device_FriendlyName, otherwise from <functiondiscoverykeys_devpkey.h>
	const PROPERTYKEY k_deviceFriendlyName = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };

	// requested length of the WASAPI buffer in 100 ns units (2 seconds)
	const REFERENCE_TIME k_wasapiBufferDuration = 20000000;

	// pause between two fetch rounds
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

	// Work out the device's format; false where it cannot be used
	bool setupSourceFormat(const WAVEFORMATEX* p_format);

	// reads one device frame and turns it into a stereo pair
	void readSourceFrame(const BYTE* p_frame, float& left, float& right) const;

	// converts numFrames device samples and pushes them into the ring buffer
	void convertAndPush(const BYTE* p_data, int numFrames, bool silent);

	std::string deviceName;
	long initResult;
	bool initOK;

	SDL_Thread* p_thread;
	SDL_sem* p_initSemaphore;

	volatile bool quit;
	volatile bool capturing;

	// device format
	int srcChannels;
	int srcRate;
	int srcBits;
	int srcBlockAlign;
	bool srcFloat;

	// resampler state (capture thread only)
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
		// The mixer almost always reports WAVE_FORMAT_EXTENSIBLE; only the SubFormat
		// GUID says whether the buffer holds floating point or integers.
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

	// With more than two channels (5.1, 7.1) the first two are front left and
	// right; those are enough for the recording.
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

		// interpolate linearly between the previous and the current device sample.
		// At an equal sample rate resampleStep is exactly 1.0 and every sample comes
		// through unchanged.
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
	// COM belongs to the thread that initializes it - every interface below is
	// therefore created, used and released here and nowhere else.
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

		// eRender, not eCapture: what is recorded is the output, not the input
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

		// In shared mode the mixer sets the format; the conversion happens here.
		if(SUCCEEDED(hr) && !setupSourceFormat(p_mixFormat)) hr = AUDCLNT_E_UNSUPPORTED_FORMAT;

		if(SUCCEEDED(hr)) hr = p_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
														 k_wasapiBufferDuration, 0, p_mixFormat, 0);
		if(SUCCEEDED(hr)) hr = p_audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&p_captureClient);
	}

	initResult = hr;
	initOK = comInitialized && SUCCEEDED(hr) && p_captureClient != 0;

	// The main thread waits for this signal and reads initOK. printfLog must not
	// be used here: it has a static buffer and is not thread-safe, which is why
	// only the result is left behind.
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
			// started, not running: a failed Start() must not be retried every
			// few milliseconds
			if(capturing && !started)
			{
				started = true;

				// throw away everything still lying around from last time
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

			// fetch every packet that is ready
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

				// With AUDCLNT_BUFFERFLAGS_SILENT p_data points at nothing, but
				// the samples still have to be counted.
				convertAndPush(p_data, (int)numFrames, (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0);
				p_captureClient->ReleaseBuffer(numFrames);
			}

			// While nothing is playing, the audio engine stops and delivers no
			// packets at all; padToClock() fills the gap.
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

	// wait for the result of the WASAPI initialization
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

#elif !defined(__EMSCRIPTEN__)

// ---------------------------------------------------------------------------
// Linux. What WASAPI calls loopback mode is a monitor in PulseAudio: every
// output sink has a source of the same name that listens in on what is going
// out. "@DEFAULT_MONITOR@" is the default sink's, which saves enumerating
// anything. PipeWire brings the same interface along with pipewire-pulse, and
// the one path therefore covers both.
//
// libpulse is loaded at runtime rather than linked against: the game still
// starts where PulseAudio is absent - the videos are then silent. For the same
// reason the handful of declarations needed here are written out by hand
// instead of taken from <pulse/simple.h>: otherwise the build would need
// libpulse-dev.
//
// pa_simple is told which format to deliver - S16LE, stereo, 48 kHz - and the
// server converts. Neither the format conversion nor the resampler of the
// Windows side exists here.
// ---------------------------------------------------------------------------

#include <dlfcn.h>

namespace
{
	// The slice of the libpulse ABI this file needs. pa_simple_new takes NULL
	// for the channel map and the buffer attributes, which leaves the format
	// spec as the only struct.
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

			// The version in the name is deliberate: the same name without the
			// number belongs to the development package and is not on a
			// player's machine.
			p_library = dlopen("libpulse-simple.so.0", RTLD_LAZY | RTLD_LOCAL);
			if(!p_library) return false;

			simple_new  = (pa_simple_new_t) dlsym(p_library, "pa_simple_new");
			simple_read = (pa_simple_read_t)dlsym(p_library, "pa_simple_read");
			simple_free = (pa_simple_free_t)dlsym(p_library, "pa_simple_free");
			// pa_strerror lives in libpulse itself, not in libpulse-simple.
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

	// How many samples are fetched at once. pa_simple_read waits until that
	// many are there, and it must not be too many - the thread would otherwise
	// hang too long on shutdown. 10 ms is what WASAPI delivers per packet too.
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

	// The server resolves "@DEFAULT_MONITOR@" to the monitor of the currently
	// selected default sink - exactly what the player hears.
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
			// throw away everything still lying around from last time
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

		// Reading has to continue even while nothing is being recorded:
		// otherwise the server's buffer overflows and the next recording begins
		// with music seconds old.
		if(!started) continue;

		push(buffer, k_readSamples);
		samplesWritten += k_readSamples;

		// A suspended sink delivers nothing - module-suspend-on-idle is loaded
		// by default - and pa_simple_read then waits. Fill the gap by the
		// clock, keeping the audio track as long as the video.
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

	// wait for the result of pa_simple_new
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

#else

// ---------------------------------------------------------------------------
// Browser. A stub, because nothing is recorded there:
// $A_TOGGLE_CAPTURE_VIDEO does not exist in the web build at all. The ring
// buffer stays unopened, and the shared reader side below then delivers
// silence.
//
// Not because a page could not listen in on its own output - it can. In
// Emscripten's OpenAL every source hangs off AL.currentCtx.gain and that in
// turn off ctx.destination; one extra connection from that sum node to a
// createMediaStreamDestination() delivers exactly the finished mix, the same
// thing WASAPI loopback under Windows and the monitor source under Linux are
// needed for. What would remain is pushing the blocks from there into the
// AudioRing as 16 bit stereo 48 kHz, out of an AudioWorklet. See ROADMAP,
// item 28.
// ---------------------------------------------------------------------------

struct AudioCaptureImpl : public AudioRing
{
	std::string deviceName;
};

AudioCapture::AudioCapture()
{
	p_impl = new AudioCaptureImpl;
	p_impl->deviceName = "(none)";
}

AudioCapture::~AudioCapture()
{
	delete p_impl;
}

bool AudioCapture::open(uint sampleRate)
{
	(void)sampleRate;
	return false;
}

void AudioCapture::close()
{
}

bool AudioCapture::isOpen() const
{
	return false;
}

const std::string& AudioCapture::getDeviceName() const
{
	return p_impl->deviceName;
}

void AudioCapture::start()
{
}

void AudioCapture::stop()
{
}

#endif

// ---------------------------------------------------------------------------
// The reader side belongs to both: it only reads the ring buffer.
// ---------------------------------------------------------------------------

int AudioCapture::getNumSamplesReady()
{
	return p_impl->available();
}

void AudioCapture::getSamples(short* p_buffer, int numSamples)
{
	p_impl->read(p_buffer, numSamples);
}
