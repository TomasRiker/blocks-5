// videorecorder_stub.cpp - an inert VideoRecorder for the web build.
//
// The real videorecorder.cpp's libraries are plain C and would port, but it
// encodes on a thread of its own and Emscripten's SDL has none: its
// SDL_CreateThread aborts and it has no semaphores (see streamedsound.cpp).
// Nor does audiocapture.cpp deliver any audio in the browser. ROADMAP item 28
// has the likelier route: let the browser encode.
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
