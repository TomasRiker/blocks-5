#include "pch.h"
#include "engine.h"
#include "videorecorder.h"
#include "audiocapture.h"

#include "minih264e.h"
#include "minimp4.h"
extern "C"
{
#include "layer3.h"
}

namespace
{
	// Die Zeitbasis der Videospur, wie mp4_h26x_write_init sie anlegt.
	const uint k_videoTimeScale = 90000;

	// H.264 arbeitet in Makrobloecken von 16x16, und minih264 verlangt, dass Breite
	// und Hoehe glatt aufgehen. Das Spiel laeuft mit 640x480, also passt es; eine
	// andere Aufloesung wird auf das naechstkleinere Vielfache abgerundet.
	inline int roundDownTo16(int value)
	{
		return value & ~15;
	}

	int writeCallback(int64_t offset, const void* p_buffer, size_t size, void* p_token)
	{
		FILE* p_file = static_cast<FILE*>(p_token);
		if(fseek(p_file, static_cast<long>(offset), SEEK_SET)) return 1;
		return fwrite(p_buffer, 1, size, p_file) != size;
	}

}

struct VideoRecorderImpl
{
	VideoRecorderImpl()
		: error(false)
		, fps(30)
		, videoBitrate(0)
		, audioBitrate(0)
		, p_videoInputBuffer(0)
		, p_yuv(0)
		, p_encoder(0)
		, p_scratch(0)
		, p_file(0)
		, p_mux(0)
		, p_shine(0)
		, p_audioBuffer(0)
		, audioSamplesPerPass(0)
		, audioTrack(-1)
		, p_audioCapture(0)
		, p_heldFrame(0)
		, heldFrameSize(0)
		, heldFrameCapacity(0)
		, heldTimecode(0)
		, haveHeldFrame(false)
		, p_thread(0)
		, p_semaphore(0)
		, finish(false)
		, readyForNextFrame(true)
		, nextFrameTimecode(0)
	{
		memset(&h264Writer, 0, sizeof(h264Writer));
	}

	int threadProc();
	void convertFrame();
	void drainAudio();
	void flushHeldFrame(uint durationTicks);

	bool error;
	uint fps;
	uint videoBitrate;
	uint audioBitrate;

	Vec2i inputSize;      // was der Aufrufer liefert
	Vec2i encodedSize;    // auf Vielfache von 16 abgerundet

	uint8_t* p_videoInputBuffer;
	uint8_t* p_yuv;              // Y, dann U, dann V, alles am Stueck
	H264E_persist_t* p_encoder;
	H264E_scratch_t* p_scratch;

	FILE* p_file;
	MP4E_mux_t* p_mux;
	mp4_h26x_writer_t h264Writer;

	shine_t p_shine;
	short* p_audioBuffer;        // interleaved, audioSamplesPerPass Stereo-Samples
	int audioSamplesPerPass;
	int audioTrack;
	AudioCapture* p_audioCapture;

	// minih264 gibt einen Zeiger in seinen eigenen Puffer zurueck, der beim
	// naechsten Aufruf ungueltig wird. Ein Frame wird trotzdem zurueckgehalten,
	// weil seine Dauer erst feststeht, wenn das naechste eintrifft - deshalb die
	// Kopie.
	uint8_t* p_heldFrame;
	int heldFrameSize;
	int heldFrameCapacity;
	uint heldTimecode;
	bool haveHeldFrame;

	SDL_Thread* p_thread;
	SDL_sem* p_semaphore;
	volatile bool finish;
	volatile bool readyForNextFrame;
	uint nextFrameTimecode;
};

int videoRecorderThreadProc(void* p_param)
{
	return static_cast<VideoRecorderImpl*>(p_param)->threadProc();
}

// #define PROFILE_VIDEO_CONVERSION
// #define PROFILE_VIDEO_ENCODING

void VideoRecorderImpl::convertFrame()
{
	// RGBX nach YUV420 planar. Das Bild kommt von glReadPixels und steht auf dem
	// Kopf, deshalb wird die Quellzeile von unten gezaehlt. Ist das Bild breiter
	// oder hoeher als das Vielfache von 16, wird mittig beschnitten.
	const int w = encodedSize.x;
	const int h = encodedSize.y;
	const int offsetX = (inputSize.x - w) / 2;
	const int offsetY = (inputSize.y - h) / 2;
	const int srcPitch = inputSize.x * 4;

	uint8_t* p_y = p_yuv;
	uint8_t* p_u = p_y + w * h;
	uint8_t* p_v = p_u + (w * h) / 4;

	for(int j = 0; j < h; j++)
	{
		const uint8_t* p_row = p_videoInputBuffer + (size_t)(inputSize.y - 1 - (j + offsetY)) * srcPitch + offsetX * 4;
		uint8_t* p_dst = p_y + (size_t)j * w;
		for(int i = 0; i < w; i++)
		{
			const int r = p_row[i * 4 + 0], g = p_row[i * 4 + 1], b = p_row[i * 4 + 2];
			p_dst[i] = static_cast<uint8_t>(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
		}
	}

	// Chroma mit 2x2-Mittelwert
	for(int j = 0; j < h / 2; j++)
	{
		for(int i = 0; i < w / 2; i++)
		{
			int r = 0, g = 0, b = 0;
			for(int dy = 0; dy < 2; dy++)
			{
				const uint8_t* p_row = p_videoInputBuffer + (size_t)(inputSize.y - 1 - (2 * j + dy + offsetY)) * srcPitch + offsetX * 4;
				for(int dx = 0; dx < 2; dx++)
				{
					r += p_row[(2 * i + dx) * 4 + 0];
					g += p_row[(2 * i + dx) * 4 + 1];
					b += p_row[(2 * i + dx) * 4 + 2];
				}
			}
			r >>= 2; g >>= 2; b >>= 2;
			p_u[j * (w / 2) + i] = static_cast<uint8_t>(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
			p_v[j * (w / 2) + i] = static_cast<uint8_t>((( 112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
		}
	}
}

void VideoRecorderImpl::drainAudio()
{
	if(!p_shine || !p_audioCapture) return;

	while(p_audioCapture->getNumSamplesReady() >= audioSamplesPerPass)
	{
		p_audioCapture->getSamples(p_audioBuffer, audioSamplesPerPass);

		int numBytes = 0;
		unsigned char* p_mp3 = shine_encode_buffer_interleaved(p_shine, p_audioBuffer, &numBytes);
		if(numBytes > 0)
		{
			MP4E_put_sample(p_mux, audioTrack, p_mp3, numBytes, audioSamplesPerPass, MP4E_SAMPLE_RANDOM_ACCESS);
		}
	}
}


void VideoRecorderImpl::flushHeldFrame(uint durationTicks)
{
	if(!haveHeldFrame) return;
	mp4_h26x_write_nal(&h264Writer, p_heldFrame, heldFrameSize, durationTicks);
	haveHeldFrame = false;
}

int VideoRecorderImpl::threadProc()
{
	if(audioBitrate && p_audioCapture) p_audioCapture->start();

	while(!finish)
	{
		if(SDL_SemWaitTimeout(p_semaphore, 10)) continue;

		// Ton zuerst: der Ringpuffer der Aufnahme soll nicht ueberlaufen.
		drainAudio();

#ifdef PROFILE_VIDEO_CONVERSION
		BEGIN_PROFILE(videoConversion)
#endif
		convertFrame();
#ifdef PROFILE_VIDEO_CONVERSION
		END_PROFILE(videoConversion)
#endif

#ifdef PROFILE_VIDEO_ENCODING
		BEGIN_PROFILE(videoEncoding)
#endif
		H264E_io_yuv_t yuv;
		yuv.yuv[0] = p_yuv;
		yuv.yuv[1] = p_yuv + encodedSize.x * encodedSize.y;
		yuv.yuv[2] = yuv.yuv[1] + (encodedSize.x * encodedSize.y) / 4;
		yuv.stride[0] = encodedSize.x;
		yuv.stride[1] = encodedSize.x / 2;
		yuv.stride[2] = encodedSize.x / 2;

		H264E_run_param_t runParam;
		memset(&runParam, 0, sizeof(runParam));
		runParam.qp_min = 16;
		runParam.qp_max = 42;
		runParam.desired_frame_bytes = videoBitrate / 8 / (fps ? fps : 30);

		uint8_t* p_nal = 0;
		int nalSize = 0;
		const int encodeError = H264E_encode(p_encoder, p_scratch, &runParam, &yuv, &p_nal, &nalSize);
#ifdef PROFILE_VIDEO_ENCODING
		END_PROFILE(videoEncoding)
#endif

		if(!encodeError && nalSize > 0)
		{
			// Das vorige Frame lief bis jetzt - erst dadurch steht seine Dauer fest.
			if(haveHeldFrame)
			{
				uint elapsed = nextFrameTimecode - heldTimecode;
				if(elapsed < 1) elapsed = 1;
				flushHeldFrame(elapsed * k_videoTimeScale / (fps ? fps : 30));
			}

			if(nalSize > heldFrameCapacity)
			{
				delete[] p_heldFrame;
				heldFrameCapacity = nalSize * 2;
				p_heldFrame = new uint8_t[heldFrameCapacity];
			}
			memcpy(p_heldFrame, p_nal, nalSize);
			heldFrameSize = nalSize;
			heldTimecode = nextFrameTimecode;
			haveHeldFrame = true;
		}

		readyForNextFrame = true;
	}

	// Das letzte Frame bekommt eine ganz normale Framedauer.
	flushHeldFrame(k_videoTimeScale / (fps ? fps : 30));

	if(audioBitrate && p_audioCapture)
	{
		drainAudio();
		p_audioCapture->stop();
	}

	if(p_shine)
	{
		int numBytes = 0;
		unsigned char* p_mp3 = shine_flush(p_shine, &numBytes);
		if(numBytes > 0) MP4E_put_sample(p_mux, audioTrack, p_mp3, numBytes, audioSamplesPerPass, MP4E_SAMPLE_RANDOM_ACCESS);
	}

	// Erst hier entstehen die Indextabellen der Datei - und damit auch der esds.
	mp4_h26x_write_close(&h264Writer);
	if(p_mux) MP4E_close(p_mux);

	if(p_file) fclose(p_file);
	p_mux = 0;
	p_file = 0;

	return 0;
}

VideoRecorder::VideoRecorder(const std::string& videoFilename,
							 const Vec2i& inputFrameSize,
							 const Vec2i& outputFrameSize,
							 uint videoBitrate,
							 uint audioBitrate,
							 uint fps)
{
	p_impl = new VideoRecorderImpl;
	p_impl->fps = fps ? fps : 30;
	p_impl->videoBitrate = videoBitrate;
	p_impl->audioBitrate = audioBitrate;
	p_impl->inputSize = inputFrameSize;
	p_impl->encodedSize = Vec2i(roundDownTo16(outputFrameSize.x), roundDownTo16(outputFrameSize.y));

	if(p_impl->encodedSize.x < 16 || p_impl->encodedSize.y < 16)
	{
		printfLog("+ ERROR: Frame size %dx%d is too small to encode.\n", outputFrameSize.x, outputFrameSize.y);
		p_impl->error = true;
		return;
	}
	if(p_impl->encodedSize != outputFrameSize)
	{
		printfLog("- WARNING: Recording %dx%d instead of %dx%d; H.264 needs a multiple of 16.\n",
				  p_impl->encodedSize.x, p_impl->encodedSize.y, outputFrameSize.x, outputFrameSize.y);
	}

	// Videokodierer
	H264E_create_param_t createParam;
	memset(&createParam, 0, sizeof(createParam));
	createParam.width = p_impl->encodedSize.x;
	createParam.height = p_impl->encodedSize.y;
	createParam.gop = p_impl->fps * 2;    // alle zwei Sekunden ein Schluesselbild
	createParam.num_layers = 1;
	createParam.max_threads = 0;

	int persistSize = 0, scratchSize = 0;
	if(H264E_sizeof(&createParam, &persistSize, &scratchSize))
	{
		printfLog("+ ERROR: Could not size the H.264 encoder.\n");
		p_impl->error = true;
		return;
	}
	p_impl->p_encoder = static_cast<H264E_persist_t*>(::operator new(persistSize));
	p_impl->p_scratch = static_cast<H264E_scratch_t*>(::operator new(scratchSize));
	if(H264E_init(p_impl->p_encoder, &createParam))
	{
		printfLog("+ ERROR: Could not initialize the H.264 encoder.\n");
		p_impl->error = true;
		return;
	}

	p_impl->p_videoInputBuffer = new uint8_t[inputFrameSize.x * inputFrameSize.y * 4];
	p_impl->p_yuv = new uint8_t[(p_impl->encodedSize.x * p_impl->encodedSize.y * 3) / 2];

	// Datei und Container
	p_impl->p_file = fopen(videoFilename.c_str(), "wb");
	if(!p_impl->p_file)
	{
		printfLog("+ ERROR: Could not create video file: \"%s\"!\n", videoFilename.c_str());
		p_impl->error = true;
		return;
	}

	// Nicht fragmentiert: der MP4-Leser von Windows kennt 'moof' erst ab Windows 8.
	p_impl->p_mux = MP4E_open(0, 0, p_impl->p_file, writeCallback);
	if(!p_impl->p_mux ||
	   MP4E_STATUS_OK != mp4_h26x_write_init(&p_impl->h264Writer, p_impl->p_mux,
											 p_impl->encodedSize.x, p_impl->encodedSize.y, 0))
	{
		printfLog("+ ERROR: Could not start the MP4 writer.\n");
		p_impl->error = true;
		return;
	}

	// Tonspur
	if(audioBitrate)
	{
		p_impl->p_audioCapture = Engine::inst().getAudioCapture();
		if(p_impl->p_audioCapture)
		{
			const int sampleRate = 48000;
			const int kbps = audioBitrate / 1000;
			if(shine_check_config(sampleRate, kbps) < 0)
			{
				printfLog("- WARNING: %d kbit/s at %d Hz is not an MP3 bitrate; recording without sound.\n", kbps, sampleRate);
				p_impl->p_audioCapture = 0;
				p_impl->audioBitrate = 0;
			}
			else
			{
				shine_config_t shineConfig;
				shine_set_config_mpeg_defaults(&shineConfig.mpeg);
				shineConfig.wave.channels = PCM_STEREO;
				shineConfig.wave.samplerate = sampleRate;
				shineConfig.mpeg.mode = STEREO;
				shineConfig.mpeg.bitr = kbps;
				p_impl->p_shine = shine_initialise(&shineConfig);
			}
		}

		if(p_impl->p_shine)
		{
			p_impl->audioSamplesPerPass = shine_samples_per_pass(p_impl->p_shine);
			p_impl->p_audioBuffer = new short[p_impl->audioSamplesPerPass * 2];

			MP4E_track_t audioTrackInfo;
			memset(&audioTrackInfo, 0, sizeof(audioTrackInfo));
			audioTrackInfo.object_type_indication = 0x6B;   // MPEG-1 Layer III
			memcpy(audioTrackInfo.language, "und", 4);
			audioTrackInfo.track_media_kind = e_audio;
			audioTrackInfo.time_scale = 48000;
			audioTrackInfo.u.a.channelcount = 2;
			p_impl->audioTrack = MP4E_add_track(p_impl->p_mux, &audioTrackInfo);

			if(p_impl->audioTrack < 0)
			{
				printfLog("- WARNING: Could not add the audio track; recording without sound.\n");
				shine_close(p_impl->p_shine);
				p_impl->p_shine = 0;
				p_impl->p_audioCapture = 0;
				p_impl->audioBitrate = 0;
			}
		}
	}

	p_impl->p_semaphore = SDL_CreateSemaphore(0);
	p_impl->p_thread = SDL_CreateThread(videoRecorderThreadProc, p_impl);
	if(!p_impl->p_thread)
	{
		printfLog("+ ERROR: Could not start the video encoding thread.\n");
		p_impl->error = true;
	}
}

VideoRecorder::~VideoRecorder()
{
	if(p_impl->p_thread)
	{
		p_impl->finish = true;
		SDL_WaitThread(p_impl->p_thread, 0);
		p_impl->p_thread = 0;
	}
	else if(p_impl->p_mux)
	{
		// Der Thread ist nie gelaufen, die Datei muss trotzdem geschlossen werden.
		mp4_h26x_write_close(&p_impl->h264Writer);
		MP4E_close(p_impl->p_mux);
		p_impl->p_mux = 0;
	}
	if(p_impl->p_file) { fclose(p_impl->p_file); p_impl->p_file = 0; }

	if(p_impl->p_semaphore) SDL_DestroySemaphore(p_impl->p_semaphore);
	if(p_impl->p_shine) shine_close(p_impl->p_shine);

	::operator delete(p_impl->p_encoder);
	::operator delete(p_impl->p_scratch);
	delete[] p_impl->p_videoInputBuffer;
	delete[] p_impl->p_yuv;
	delete[] p_impl->p_audioBuffer;
	delete[] p_impl->p_heldFrame;
	delete p_impl;
}

bool VideoRecorder::isReadyForNextFrame() const
{
	return p_impl->readyForNextFrame && !p_impl->error;
}

void* VideoRecorder::getInputFrameBuffer()
{
	return p_impl->p_videoInputBuffer;
}

void VideoRecorder::encodeNextFrame(uint timecode)
{
	if(!p_impl->readyForNextFrame || p_impl->error) return;

	p_impl->readyForNextFrame = false;
	p_impl->nextFrameTimecode = timecode;

	// Thread benachrichtigen, dass ein neues Frame da ist
	SDL_SemPost(p_impl->p_semaphore);
}

uint VideoRecorder::getFPS() const
{
	return p_impl->fps;
}

bool VideoRecorder::getError() const
{
	return p_impl->error;
}
