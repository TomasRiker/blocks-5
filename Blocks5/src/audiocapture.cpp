#include "pch.h"
#include "audiocapture.h"

// A loopback capture, because OpenAL's alcCaptureOpenDevice opens an *input*
// device and would put the microphone into every video: WASAPI loopback on
// the default render endpoint under Windows, the monitor source of the
// default sink under Linux. Both deliver 16-bit stereo at the rate prepare()
// was given (48 kHz, what videorecorder.cpp wants), so the ring buffer, its
// clock, the silence padding and the reader side are shared and only the
// capture side differs. In the browser prepare() always fails; nothing
// records video there.
//
// The device is opened by start() and closed by stop(), on a thread that
// lives as long as the recording. Opening takes a moment - on PulseAudio,
// measured with the game's own output on the sink, the stream is there in
// 2 ms and its first samples 8 ms after that - and that moment goes into the
// recording as silence, by the clock, so that the sound stays with the
// picture. A stream opened per recording also starts empty, where one left
// open and unread between recordings would have the server's backlog to
// deliver first.

namespace
{
	// Ring buffer size in seconds. The recorder drains it once per video
	// frame (~33 ms); the slack covers longer stalls such as a level change.
	const int k_ringBufferSeconds = 4;

	// How far the ring buffer may fall behind real time before it is padded
	// with silence. Must be well above the capture's packet interval (~10 ms),
	// or it pads while real data is about to arrive.
	const int k_silenceSlackMS = 60;

	// Insert at most this much silence in one go (in seconds)
	const int k_maxSilenceBurstSeconds = 1;
}

// ---------------------------------------------------------------------------
// The ring buffer, shared by every platform. The capture thread writes, the
// recorder's thread reads, the mutex separates the two.
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

	// A recording begins: the ring empty, nothing written, the clock at 0.
	// Called before the capture thread is made, which therefore sees it all.
	void begin();

	// How many samples should have arrived since begin(), by the clock.
	long long samplesByClock() const;

	// Append finished stereo samples; with no room left, the oldest give way.
	void push(const short* p_samples, int numSamples);

	// Append numSamples of silence.
	void pushSilence(int numSamples);

	// Pad with silence up to expected, the number of samples that should have
	// arrived since the start by the clock: while nothing plays, no packets
	// come at all, and the audio track would fall short of the video.
	void padToClock(long long expected);

	// Silence up to expected, exactly: what the device took to open, in front
	// of its first samples. padToClock() leaves a gap under its slack alone
	// and caps a long one, both right for a gap between packets and wrong
	// for this one, which is nothing but the time the device took.
	void padExactly(long long expected);

	// The reader side, called by the recorder from another thread.
	int  available();
	void read(short* p_buffer, int numSamples);

	SDL_mutex* p_mutex;
	short* p_ring;
	int ringSize;   // in samples
	int ringRead;   // in samples
	int ringFill;   // in samples
	uint sampleRate;
	bool ready;
	long long samplesWritten;
	uint64 clockStart;
};

AudioRing::AudioRing()
	: p_mutex(0)
	, p_ring(0)
	, ringSize(0)
	, ringRead(0)
	, ringFill(0)
	, sampleRate(48000)
	, ready(false)
	, samplesWritten(0)
	, clockStart(0)
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
	p_mutex = SDL_CreateMutex();
	ready = p_mutex != 0;
	return ready;
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
	ready = false;
}

void AudioRing::clearRing()
{
	if(!p_mutex) return;
	SDL_LockMutex(p_mutex);
	ringRead = 0;
	ringFill = 0;
	SDL_UnlockMutex(p_mutex);
}

void AudioRing::begin()
{
	clearRing();
	samplesWritten = 0;
	clockStart = getExactTimeUS();
}

long long AudioRing::samplesByClock() const
{
	return static_cast<long long>(getExactTimeUS() - clockStart) * sampleRate / 1000000;
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

void AudioRing::padExactly(long long expected)
{
	const long long missing = expected - samplesWritten;
	if(missing > 0) pushSilence((int)missing);
}

int AudioRing::available()
{
	if(!ready || !p_mutex) return 0;
	SDL_LockMutex(p_mutex);
	const int numSamples = ringFill;
	SDL_UnlockMutex(p_mutex);
	return numSamples;
}

void AudioRing::read(short* p_buffer, int numSamples)
{
	if(numSamples <= 0) return;
	if(!ready || !p_ring)
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
	// KSDATAFORMAT_SUBTYPE_PCM and KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, written out
	// rather than taken from <ksmedia.h> and the kernel-streaming headers it
	// drags in.
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

	// What became of the device, left for stop() to log once the thread is
	// gone: printfLog is not thread-safe.
	std::string deviceName;
	long initResult;
	bool initOK;
	bool deviceLost;

	SDL_Thread* p_thread;
	bool threadFailed;
	volatile bool quit;

	// device format
	int srcChannels;
	int srcRate;
	int srcBits;
	int srcBlockAlign;
	bool srcFloat;

	// resampler state (capture thread only)
	float resampleStep;
	float resamplePos;
	float prevLeft;
	float prevRight;
	bool havePrev;
};

int audioCaptureThreadProc(void* p_param);

AudioCaptureImpl::AudioCaptureImpl()
	: deviceName("(unknown)")
	, initResult(0)
	, initOK(false)
	, deviceLost(false)
	, p_thread(0)
	, threadFailed(false)
	, quit(false)
	, srcChannels(2)
	, srcRate(48000)
	, srcBits(32)
	, srcBlockAlign(8)
	, srcFloat(true)
	, resampleStep(1.0f)
	, resamplePos(0.0f)
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

	resampleStep = (float)srcRate / (float)sampleRate;
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
			resamplePos = 0.0f;
			havePrev = true;
		}

		// interpolate linearly between the previous and the current device sample.
		// At an equal sample rate resampleStep is exactly 1.0 and every sample comes
		// through unchanged.
		while(resamplePos < 1.0f)
		{
			const float t = resamplePos;
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
		resamplePos -= 1.0f;
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
		if(SUCCEEDED(hr)) hr = p_audioClient->Start();
	}

	initResult = hr;
	initOK = comInitialized && SUCCEEDED(hr) && p_captureClient != 0;
	deviceLost = false;
	havePrev = false;
	resamplePos = 0.0f;

	// What opening took goes in front of the first packet, which holds what
	// played from Start() on.
	padExactly(samplesByClock());

	while(!quit)
	{
		// fetch every packet that is ready
		while(initOK && !deviceLost)
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

			// With AUDCLNT_BUFFERFLAGS_SILENT the packet's data is to be
			// ignored as silence, but its frames still count.
			convertAndPush(p_data, (int)numFrames, (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0);
			p_captureClient->ReleaseBuffer(numFrames);
		}

		// While nothing is playing, the audio engine stops and delivers no
		// packets at all; padToClock() fills the gap - as it fills the
		// whole recording where the device would not open or was lost, so
		// that the sound is still as long as the picture.
		padToClock(samplesByClock());

		SDL_Delay(k_pollDelayMS);
	}

	if(initOK) p_audioClient->Stop();

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
	stop();
	p_impl->release();
	delete p_impl;
}

bool AudioCapture::prepare(uint sampleRate)
{
	// WASAPI is part of every Windows the game runs on; whether the device
	// opens is the recording's question.
	if(p_impl->ready) return true;
	if(!p_impl->allocate(sampleRate))
	{
		printfLog("+ WARNING: Could not create the audio capture's buffer.\n");
		p_impl->release();
		return false;
	}
	return true;
}

void AudioCapture::start()
{
	if(!p_impl->ready || p_impl->p_thread) return;

	p_impl->begin();
	p_impl->quit = false;
	p_impl->p_thread = SDL_CreateThread(audioCaptureThreadProc, p_impl);
	p_impl->threadFailed = !p_impl->p_thread;
}

void AudioCapture::stop()
{
	if(p_impl->threadFailed)
	{
		printfLog("+ WARNING: Could not start the audio capture thread; the video has no sound.\n");
		p_impl->threadFailed = false;
	}
	if(!p_impl->p_thread) return;

	p_impl->quit = true;
	SDL_WaitThread(p_impl->p_thread, 0);
	p_impl->p_thread = 0;

	if(!p_impl->initOK)
		printfLog("+ WARNING: Could not open loopback capture (HRESULT 0x%08X); the video's sound is silent.\n", (uint)p_impl->initResult);
	else if(p_impl->deviceLost)
		printfLog("+ WARNING: The loopback capture of %s was lost during the video.\n", p_impl->deviceName.c_str());
	else
		printfLog("Recorded the video's sound from: %s (loopback)\n", p_impl->deviceName.c_str());
}

#elif !defined(__EMSCRIPTEN__)

// ---------------------------------------------------------------------------
// Linux. What WASAPI calls loopback is a PulseAudio sink's monitor source, and
// "@DEFAULT_MONITOR@" names the default sink's, so nothing is enumerated.
// pipewire-pulse serves the same interface.
//
// libpulse is dlopen'd, and the few declarations this file needs are written
// out rather than taken from <pulse/simple.h>: the game still starts without
// PulseAudio (videos are then silent), and the build needs no libpulse-dev.
//
// pa_simple is asked for S16LE stereo at the ring's rate and the server
// converts, so there is no format conversion or resampler on this side.
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

	// Samples per read: 10 ms at 48 kHz, a WASAPI packet's worth.
	// pa_simple_read blocks until that many are there, so a larger read would
	// hold up stop(), which waits for the thread.
	const int k_readSamples = 480;
}

struct AudioCaptureImpl : public AudioRing
{
	AudioCaptureImpl();

	int threadProc();

	PulseAPI pulse;

	SDL_Thread* p_thread;
	bool threadFailed;
	volatile bool quit;

	// What became of the stream, left for stop() to log once the thread is
	// gone: printfLog is not thread-safe. 0 where nothing went wrong.
	int openError;
	int readError;
};

int audioCaptureThreadProc(void* p_param);

AudioCaptureImpl::AudioCaptureImpl()
	: p_thread(0)
	, threadFailed(false)
	, quit(false)
	, openError(0)
	, readError(0)
{
}

int AudioCaptureImpl::threadProc()
{
	pa_sample_spec spec;
	spec.format   = PA_SAMPLE_S16LE;
	spec.rate     = sampleRate;
	spec.channels = 2;

	// The server resolves "@DEFAULT_MONITOR@" to the monitor of the currently
	// selected default sink - exactly what the player hears. No buffer
	// attributes: the record stream's fragments then follow the sink, which
	// the game's own output keeps at a low latency (measured: 10 ms reads
	// that arrive in real time), and the capture asks the sound card for no
	// lower one than the game already does.
	int error = 0;
	pa_simple* p_stream = pulse.simple_new(0, "Blocks 5", PA_STREAM_RECORD, "@DEFAULT_MONITOR@",
										   "video capture", &spec, 0, 0, &error);
	if(!p_stream) openError = error ? error : -1;

	short buffer[k_readSamples * 2];
	bool first = true;

	while(!quit)
	{
		// No stream, or none any more: the sound is still as long as the
		// picture, silent from here on.
		if(!p_stream)
		{
			padToClock(samplesByClock());
			SDL_Delay(20);
			continue;
		}

		if(pulse.simple_read(p_stream, buffer, sizeof(buffer), &error) < 0)
		{
			readError = error ? error : -1;
			pulse.simple_free(p_stream);
			p_stream = 0;
			continue;
		}

		// What the stream took to open and to deliver goes in front of its
		// first samples, so that they sit where they were heard: the clock
		// now, less the read's own length.
		if(first)
		{
			padExactly(samplesByClock() - k_readSamples);
			first = false;
		}

		push(buffer, k_readSamples);
		samplesWritten += k_readSamples;

		// A sink that delivers nothing leaves pa_simple_read waiting - one
		// suspended by hand or gone, since this stream itself keeps it from
		// suspending on idle; pad the gap by the clock so the audio track
		// stays as long as the video.
		padToClock(samplesByClock());
	}

	if(p_stream) pulse.simple_free(p_stream);
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
	stop();
	p_impl->release();
	delete p_impl;
}

bool AudioCapture::prepare(uint sampleRate)
{
	// Whether the server is there is the recording's question: a player may
	// start one after the game.
	if(p_impl->ready) return true;

	if(!p_impl->pulse.load())
	{
		printfLog("+ WARNING: libpulse-simple.so.0 is not available.\n");
		return false;
	}

	if(!p_impl->allocate(sampleRate))
	{
		printfLog("+ WARNING: Could not create the audio capture's buffer.\n");
		p_impl->release();
		return false;
	}
	return true;
}

void AudioCapture::start()
{
	if(!p_impl->ready || p_impl->p_thread) return;

	p_impl->begin();
	p_impl->quit = false;
	p_impl->openError = 0;
	p_impl->readError = 0;
	p_impl->p_thread = SDL_CreateThread(audioCaptureThreadProc, p_impl);
	p_impl->threadFailed = !p_impl->p_thread;
}

void AudioCapture::stop()
{
	if(p_impl->threadFailed)
	{
		printfLog("+ WARNING: Could not start the audio capture thread; the video has no sound.\n");
		p_impl->threadFailed = false;
	}
	if(!p_impl->p_thread) return;

	// Within a fragment of the sink's: pa_simple_read returns no later.
	p_impl->quit = true;
	SDL_WaitThread(p_impl->p_thread, 0);
	p_impl->p_thread = 0;

	if(p_impl->openError)
		printfLog("+ WARNING: Could not open the monitor of the default sink (%s); the video's sound is silent.\n",
				  p_impl->pulse.errorText(p_impl->openError));
	else if(p_impl->readError)
		printfLog("+ WARNING: Audio capture read failed (%s); the video's sound is silent from there on.\n",
				  p_impl->pulse.errorText(p_impl->readError));
	else
		printfLog("Recorded the video's sound from: @DEFAULT_MONITOR@ (loopback)\n");
}

#else

// ---------------------------------------------------------------------------
// Browser: a stub, since the web build records no video
// ($A_TOGGLE_CAPTURE_VIDEO is not registered there). The ring is never made
// and the shared reader side delivers silence. A page could hear its own mix -
// every OpenAL source there hangs off AL.currentCtx.gain - which is the route
// ROADMAP item 28 describes.
// ---------------------------------------------------------------------------

struct AudioCaptureImpl : public AudioRing
{
};

AudioCapture::AudioCapture()
{
	p_impl = new AudioCaptureImpl;
}

AudioCapture::~AudioCapture()
{
	delete p_impl;
}

bool AudioCapture::prepare(uint sampleRate)
{
	(void)sampleRate;
	return false;
}

void AudioCapture::start()
{
}

void AudioCapture::stop()
{
}

#endif

// ---------------------------------------------------------------------------
// The reader side, shared by every platform: it only reads the ring buffer.
// ---------------------------------------------------------------------------

int AudioCapture::getNumSamplesReady()
{
	return p_impl->available();
}

void AudioCapture::getSamples(short* p_buffer, int numSamples)
{
	p_impl->read(p_buffer, numSamples);
}
