#ifndef _VIDEORECORDER_H
#define _VIDEORECORDER_H

/*** Class for recording videos ***/

// Writes H.264 (Baseline) and MP3 into an MP4 file. minih264 encodes the
// video, shine the audio, and minimp4 puts the whole thing together - all
// three as source in libs/, no DLL. Windows has played that combination
// without an extra codec since Windows 7, and a Linux build could use the
// same three libraries; that is why they were chosen over ffmpeg.
//
// The caller writes each frame as 32-bit RGBX into the buffer from
// getInputFrameBuffer() and announces it with encodeNextFrame(); encoding runs
// in a thread of its own.

struct VideoRecorderImpl;

class VideoRecorder
{
public:
	VideoRecorder(const std::string& videoFilename, const Vec2i& inputFrameSize, const Vec2i& outputFrameSize, uint videoBitrate, uint audioBitrate, uint fps);
	~VideoRecorder();

	// asks whether the recorder is ready for the next frame
	bool isReadyForNextFrame() const;

	// returns the buffer the next frame to be recorded must be copied into (32-bit RGBX, row by row with no pitch)
	void* getInputFrameBuffer();

	// starts encoding the next frame
	void encodeNextFrame(uint timecode);

	uint getFPS() const;

	bool getError() const;

private:
	// not copyable - thread and file belong to exactly one object
	VideoRecorder(const VideoRecorder&);
	VideoRecorder& operator=(const VideoRecorder&);

	VideoRecorderImpl* p_impl;
};

#endif
