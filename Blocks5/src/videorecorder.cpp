#include "pch.h"
#include "engine.h"
#include "videorecorder.h"

VideoRecorder::VideoRecorder(const std::string& videoFilename,
							 const Vec2i& inputFrameSize,
							 const Vec2i& outputFrameSize,
							 uint videoBitrate,
							 uint audioBitrate,
							 uint fps)
	: error(false)
	, inputFrameSize(inputFrameSize)
	, outputFrameSize(outputFrameSize)
	, videoBitrate(videoBitrate)
	, audioBitrate(audioBitrate)
	, fps(fps)
	, p_videoInputBuffer(0)
	, p_audioInputBuffer(0)
	, p_avFormatContext(0)
	, p_frameYUV(0)
	, p_videoOutputBuffer(0)
	, p_audioOutputBuffer(0)
	, p_swScaleContext(0)
	, p_thread(0)
	, finish(false)
	, readyForNextFrame(true)
{
	// Ausgabeformat "raten"
	AVOutputFormat* p_avOutputFormat = av_guess_format(0, videoFilename.c_str(), 0);
	if(!p_avOutputFormat)
	{
		printfLog("+ ERROR: Could not guess AV format!\n");
		error = true;
		return;
	}

	// Formatkontext erzeugen
	p_avFormatContext = avformat_alloc_context();
	p_avFormatContext->oformat = p_avOutputFormat;

	// Video-Stream hinzufügen
	AVStream* p_videoStream = av_new_stream(p_avFormatContext, 0);
	if(!p_videoStream)
	{
		printfLog("+ ERROR: Could not create video stream!\n");
		error = true;
		return;
	}

	// Video-Codec konfigurieren
	AVCodecContext* p_videoCodecContext = p_videoStream->codec;
	p_videoCodecContext->codec_id = p_avOutputFormat->video_codec;
	p_videoCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
	p_videoCodecContext->bit_rate = videoBitrate;
	p_videoCodecContext->width = outputFrameSize.x;
	p_videoCodecContext->height = outputFrameSize.y;
	p_videoCodecContext->time_base.num = 1;
	p_videoCodecContext->time_base.den = fps;
	p_videoCodecContext->gop_size = 12;
	p_videoCodecContext->pix_fmt = PIX_FMT_YUV420P;

	AVStream* p_audioStream = 0;
	AVCodecContext* p_audioCodecContext = 0;
	if(audioBitrate)
	{
		// Audio-Stream hinzufügen
		p_audioStream = av_new_stream(p_avFormatContext, 1);
		if(!p_audioStream)
		{
			printfLog("+ ERROR: Could not create audio stream!\n");
			error = true;
			return;
		}

		// Audio-Codec konfigurieren
		p_audioCodecContext = p_audioStream->codec;
		p_audioCodecContext->codec_id = p_avOutputFormat->audio_codec;
		p_audioCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
		p_audioCodecContext->bit_rate = audioBitrate;
		p_audioCodecContext->channels = 2;
		p_audioCodecContext->sample_rate = 48000;
		p_audioCodecContext->sample_fmt = SAMPLE_FMT_S16;
	}

	// Parameter setzen (???)
	if(av_set_parameters(p_avFormatContext, 0) < 0)
	{
		printfLog("+ ERROR: Invalid AV output parameters!\n");
		error = true;
		return;
	}

	// Video-Codec öffnen
	AVCodec* p_videoCodec = avcodec_find_encoder(p_videoCodecContext->codec_id);
	if(!p_videoCodec || avcodec_open(p_videoCodecContext, p_videoCodec) < 0)
	{
		printfLog("+ ERROR: Could not find/open video codec!\n");
		error = true;
		return;
	}

	AVCodec* p_audioCodec = 0;
	if(audioBitrate)
	{
		// Audio-Codec öffnen
		p_audioCodec = avcodec_find_encoder(p_audioCodecContext->codec_id);
		if(!p_audioCodec || avcodec_open(p_audioCodecContext, p_audioCodec) < 0)
		{
			printfLog("+ ERROR: Could not find/open audio codec!\n");
			error = true;
			return;
		}
	}

	// Datei öffnen
	if(url_fopen(&p_avFormatContext->pb, videoFilename.c_str(), URL_WRONLY) < 0)
	{
		printfLog("+ ERROR: Could not create video file: \"%s\"!\n", videoFilename.c_str());
		error = true;
		return;
	}

	// Header schreiben
	av_write_header(p_avFormatContext);

	// Videopuffer reservieren
	p_videoInputBuffer = new uint8_t[inputFrameSize.x * inputFrameSize.y * 4];
	p_frameYUV = avcodec_alloc_frame();
	const uint frameSize = outputFrameSize.x * outputFrameSize.y;
	const uint yuvSize = (frameSize * 3) / 2;
	p_frameYUV->data[0] = new uint8_t[yuvSize];
	p_frameYUV->data[1] = p_frameYUV->data[0] + frameSize;
	p_frameYUV->data[2] = p_frameYUV->data[1] + frameSize / 4;
	p_frameYUV->linesize[0] = outputFrameSize.x;
	p_frameYUV->linesize[1] = outputFrameSize.x / 2;
	p_frameYUV->linesize[2] = outputFrameSize.x / 2;
	p_videoOutputBuffer = new uint8_t[outputFrameSize.x * outputFrameSize.y * 3];

	if(audioBitrate)
	{
		// Audiopuffer reservieren
		p_audioInputBuffer = new short[p_audioCodecContext->frame_size * 2];
		audioOutputBufferSize = std::max(FF_MIN_BUFFER_SIZE, p_audioCodecContext->frame_size * 2 * 2);
		p_audioOutputBuffer = new uint8_t[audioOutputBufferSize];
	}

	// Skalierung/Konvertierung initialisieren
	if(!sws_isSupportedInput(PIX_FMT_RGBA) || !sws_isSupportedOutput(p_videoCodecContext->pix_fmt))
	{
		printfLog("+ ERROR: Input/output format not supported!\n");
		error = true;
		return;
	}

	p_swScaleContext = sws_getContext(inputFrameSize.x, inputFrameSize.y, PIX_FMT_RGBA,
									  outputFrameSize.x, outputFrameSize.y, p_videoCodecContext->pix_fmt,
									  outputFrameSize == inputFrameSize ? SWS_POINT : SWS_AREA, 0, 0, 0);
	if(!p_swScaleContext)
	{
		printfLog("+ ERROR: Could not get scaler context!\n");
		error = true;
		return;
	}

	// Synchronisierungsprimitive initialisieren und Thread starten
	p_semaphore = SDL_CreateSemaphore(0);
	p_thread = SDL_CreateThread(videoRecorderThreadProc, this);
}

VideoRecorder::~VideoRecorder()
{
	if(p_thread)
	{
		// Thread beenden
		finish = true;
		SDL_WaitThread(p_thread, 0);
	}

	if(p_semaphore) SDL_DestroySemaphore(p_semaphore);

	delete[] p_videoInputBuffer;
	delete[] p_videoOutputBuffer;

	if(audioBitrate)
	{
		delete[] p_audioInputBuffer;
		delete[] p_audioOutputBuffer;
	}

	if(p_frameYUV)
	{
		delete[] p_frameYUV->data[0];
		av_free(p_frameYUV);
	}

	if(p_swScaleContext) sws_freeContext(p_swScaleContext);

	if(p_avFormatContext)
	{
		for(uint i = 0; i < p_avFormatContext->nb_streams; ++i)
		{
			// Codec schließen
			avcodec_close(p_avFormatContext->streams[i]->codec);
		}

		// Ausgabeformatkontext selbst freigeben
		av_free(p_avFormatContext);
	}
}

bool VideoRecorder::isReadyForNextFrame() const
{
	return readyForNextFrame;
}

void* VideoRecorder::getInputFrameBuffer()
{
	return p_videoInputBuffer;
}

void VideoRecorder::encodeNextFrame(uint timecode)
{
	if(!readyForNextFrame) return;

	readyForNextFrame = false;
	nextFrameTimecode = timecode;

	// Thread benachrichtigen, dass ein neues Frame da ist
	SDL_SemPost(p_semaphore);
}

uint VideoRecorder::getFPS() const
{
	return fps;
}

bool VideoRecorder::getError() const
{
	return error;
}

// #define PROFILE_VIDEO_CONVERSION
// #define PROFILE_VIDEO_ENCODING

int VideoRecorder::threadProc()
{
	ALCdevice* p_openALCaptureDevice = 0;
	int numSamplesReady = 0;
	if(audioBitrate)
	{
		// alle noch im Puffer befindlichen Samples wegwerfen
		p_openALCaptureDevice = Engine::inst().getOpenALCaptureDevice();
		alcGetIntegerv(p_openALCaptureDevice, ALC_CAPTURE_SAMPLES, 1, &numSamplesReady);
		static short tempBuffer[4800][2];
		while(numSamplesReady > 0)
		{
			alcCaptureSamples(p_openALCaptureDevice, tempBuffer, std::min(numSamplesReady, 4800));
			numSamplesReady -= 4800;
		}

		// Audioaufnahme beginnen
		alcCaptureStart(p_openALCaptureDevice);
	}

	while(!finish)
	{
		if(!SDL_SemWaitTimeout(p_semaphore, 10))
		{
#ifdef PROFILE_VIDEO_CONVERSION
			BEGIN_PROFILE(videoConversion)
#endif

			if(audioBitrate)
			{
				// Anzahl verfügbarer Audio-Samples abfragen
				numSamplesReady = 0;
				alcGetIntegerv(p_openALCaptureDevice, ALC_CAPTURE_SAMPLES, 1, &numSamplesReady);

				// Audio-Pakete kodieren und schreiben
				AVCodecContext* p_audioCodecContext = p_avFormatContext->streams[1]->codec;
				while(numSamplesReady >= p_audioCodecContext->frame_size)
				{
					// Samples holen und kodieren
					alcCaptureSamples(p_openALCaptureDevice, p_audioInputBuffer, p_audioCodecContext->frame_size);
					numSamplesReady -= p_audioCodecContext->frame_size;
					int audioOutSize = avcodec_encode_audio(p_audioCodecContext, p_audioOutputBuffer, audioOutputBufferSize, p_audioInputBuffer);

					// Audiopaket bauen
					AVPacket audioPacket;
					av_init_packet(&audioPacket);
					audioPacket.stream_index = 1;
					audioPacket.data = p_audioOutputBuffer;
					audioPacket.size = audioOutSize;

					// Audiopaket schreiben
					av_interleaved_write_frame(p_avFormatContext, &audioPacket);
				}
			}

			// Ein neues Frame ist da!
			// Frame mit libswscale konvertieren.
			const uint8_t* const p_srcLastLine = p_videoInputBuffer + 4 * inputFrameSize.x * (inputFrameSize.y - 1);
			const uint8_t* const p_srcPlanes[] = { p_srcLastLine, p_srcLastLine + 1, p_srcLastLine + 2, p_srcLastLine + 3 };
			const int srcStride = 4 * inputFrameSize.x;
			const int srcStrides[] = { -srcStride, -srcStride, -srcStride, -srcStride };
			sws_scale(p_swScaleContext, p_srcPlanes, srcStrides, 0, inputFrameSize.y, p_frameYUV->data, p_frameYUV->linesize);

#ifdef PROFILE_VIDEO_CONVERSION
			END_PROFILE(videoConversion)
#endif

#ifdef PROFILE_VIDEO_ENCODING
			BEGIN_PROFILE(videoEncoding)
#endif

			// Frame kodieren
			AVCodecContext* p_videoCodecContext = p_avFormatContext->streams[0]->codec;
			int videoOutSize = avcodec_encode_video(p_videoCodecContext, p_videoOutputBuffer,
													outputFrameSize.x * outputFrameSize.y * 3, p_frameYUV);

#ifdef PROFILE_VIDEO_ENCODING
			END_PROFILE(videoEncoding)
#endif

			// in die Datei schreiben
			AVPacket videoPacket;
			av_init_packet(&videoPacket);
			if(p_videoCodecContext->coded_frame->key_frame) videoPacket.flags |= AV_PKT_FLAG_KEY;
			videoPacket.stream_index = 0;
			videoPacket.pts = nextFrameTimecode;
			videoPacket.data = p_videoOutputBuffer;
			videoPacket.size = videoOutSize;

			// Frame schreiben
			av_interleaved_write_frame(p_avFormatContext, &videoPacket);

			readyForNextFrame = true;
		}
	}

	// TODO: verzögerte Frames schreiben
	/*
	while(true)
	{
		const int outSize = avcodec_encode_video(p_codecContext, p_outputBuffer, outputFrameSize.x * outputFrameSize.y * 3, 0);
		if(!outSize) break;
		fwrite(p_outputBuffer, 1, outSize, p_outputFile);
	}
	*/

	if(audioBitrate)
	{
		// Audioaufnahme stoppen
		alcCaptureStop(p_openALCaptureDevice);
	}

	// Trailer schreiben
	av_write_trailer(p_avFormatContext);

	// Datei schließen
	url_fclose(p_avFormatContext->pb);

	return 0;
}

int videoRecorderThreadProc(void* p_param)
{
	VideoRecorder* p_this = static_cast<VideoRecorder*>(p_param);
	return p_this->threadProc();
}