// videorecorder_stub.cpp - untaetiger VideoRecorder fuer den Web-Build.
//
// Das eigentliche videorecorder.cpp waere von seinen Bibliotheken her portabel -
// minih264, shine und minimp4 sind alle reines C -, kodiert aber in einem
// eigenen Thread, und den gibt es hier nicht: SDL_CreateThread bricht unter
// Emscripten ab, SDL_WaitThread ruft abort(), und Semaphoren kennt dessen SDL
// gar nicht (siehe streamedsound.cpp). Dazu kommt der Ton, den
// audiocapture.cpp im Browser nicht liefert.
//
// Beides ist loesbar, und der Weg dorthin fuehrt vermutlich gar nicht hier
// entlang: der Browser kann selbst kodieren. Siehe ROADMAP, Punkt 28.
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
