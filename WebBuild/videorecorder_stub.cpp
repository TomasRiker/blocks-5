// videorecorder_stub.cpp - inert VideoRecorder for the web build.
//
// The real videorecorder.cpp is portable now - minih264, shine and minimp4 are
// all plain C - so this build could in principle record too. It does not,
// because nothing here captures audio (audiocapture.cpp is a stub outside
// Windows) and the browser has no obvious place to put the file. Engine's F12
// handler tears the recorder straight back down when it reports an error, which
// is the intended behaviour when recording cannot start.
#include "pch.h"
#include "videorecorder.h"

struct VideoRecorderImpl { uint fps; };

VideoRecorder::VideoRecorder(const std::string&, const Vec2i&, const Vec2i&, uint, uint, uint fps)
{
    p_impl = new VideoRecorderImpl;
    p_impl->fps = fps ? fps : 30;
    printfLog("+ Video recording is not available in the web build.\n");
}

VideoRecorder::~VideoRecorder()                  { delete p_impl; }
bool  VideoRecorder::isReadyForNextFrame() const { return false; }
void* VideoRecorder::getInputFrameBuffer()       { return 0; }
void  VideoRecorder::encodeNextFrame(uint)       {}
uint  VideoRecorder::getFPS() const              { return p_impl->fps; }
bool  VideoRecorder::getError() const            { return true; }
