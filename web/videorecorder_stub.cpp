// videorecorder_stub.cpp - inert VideoRecorder for the web build.
//
// The real videorecorder.cpp encodes gameplay to AVI through ffmpeg
// (avcodec/avformat/swscale). Building ffmpeg for wasm to support a capture
// feature the browser cannot usefully expose is not worth it, so the class is
// replaced with one that reports an error and never accepts a frame. Engine's
// F12 handler then tears it straight back down, which is the intended
// behaviour when recording cannot start.
#include "pch.h"
#include "videorecorder.h"

VideoRecorder::VideoRecorder(const std::string&, const Vec2i& inputFrameSize, const Vec2i& outputFrameSize,
                             uint videoBitrate, uint audioBitrate, uint fps)
    : inputFrameSize(inputFrameSize), outputFrameSize(outputFrameSize),
      videoBitrate(videoBitrate), audioBitrate(audioBitrate), fps(fps)
{
    error = true;
    p_videoInputBuffer = 0;
    p_audioInputBuffer = 0;
    p_videoOutputBuffer = 0;
    p_audioOutputBuffer = 0;
    audioOutputBufferSize = 0;
    p_thread = 0;
    finish = false;
    readyForNextFrame = false;
    nextFrameTimecode = 0;
    p_semaphore = 0;
    printfLog("+ Video recording is not available in the web build.\n");
}

VideoRecorder::~VideoRecorder() {}
bool  VideoRecorder::isReadyForNextFrame() const { return false; }
void* VideoRecorder::getInputFrameBuffer()       { return 0; }
void  VideoRecorder::encodeNextFrame(uint)       {}
uint  VideoRecorder::getFPS() const              { return fps; }
bool  VideoRecorder::getError() const            { return error; }
int   VideoRecorder::threadProc()                { return 0; }
int videoRecorderThreadProc(void*)               { return 0; }
