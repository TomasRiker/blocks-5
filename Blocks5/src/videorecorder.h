#ifndef _VIDEORECORDER_H
#define _VIDEORECORDER_H

/*** Klasse zum Aufnehmen von Videos ***/

class VideoRecorder
{
	friend int videoRecorderThreadProc(void* p_param);

public:
	VideoRecorder(const std::string& videoFilename, const Vec2i& inputFrameSize, const Vec2i& outputFrameSize, uint videoBitrate, uint audioBitrate, uint fps);
	~VideoRecorder();

	// fragt ab, ob der Rekorder bereit für das nächste Frame ist
	bool isReadyForNextFrame() const;

	// liefert den Puffer, in den das nächste aufzunehmende Frame kopiert werden muss (32-Bit RGBX, zeilenweise ohne Pitch)
	void* getInputFrameBuffer();

	// startet mit der Kodierung des nächsten Frames
	void encodeNextFrame(uint timecode);

	uint getFPS() const;

	bool getError() const;

private:
	int threadProc();

	bool error;
	const Vec2i inputFrameSize;
	const Vec2i outputFrameSize;
	const uint videoBitrate;
	const uint audioBitrate;
	const uint fps;
	uint8_t* p_videoInputBuffer;
	short* p_audioInputBuffer;
	AVFormatContext* p_avFormatContext;
	AVFrame* p_frameYUV;
	SwsContext* p_swScaleContext;
	uint8_t* p_videoOutputBuffer;
	uint8_t* p_audioOutputBuffer;
	uint audioOutputBufferSize;
	SDL_Thread* p_thread;
	volatile bool finish;
	volatile bool readyForNextFrame;
	uint nextFrameTimecode;
	SDL_sem* p_semaphore;
};

int videoRecorderThreadProc(void* p_param);

#endif