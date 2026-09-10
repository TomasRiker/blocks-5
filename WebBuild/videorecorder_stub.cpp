// videorecorder_stub.cpp - an inert VideoRecorder for the web build.
//
// The real videorecorder.cpp would be portable as far as its libraries go -
// minih264, shine and minimp4 are all plain C - but it encodes in a thread of
// its own, and there is none here: SDL_CreateThread aborts under Emscripten,
// SDL_WaitThread calls abort(), and its SDL has no semaphores at all (see
// streamedsound.cpp). On top of that comes the audio, which audiocapture.cpp
// does not deliver in the browser.
//
// Both are solvable, and the way there probably does not lead through here at
// all: the browser can encode by itself. See ROADMAP item 28.
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
