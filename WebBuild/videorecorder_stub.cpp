// videorecorder_stub.cpp - untaetiger VideoRecorder fuer den Web-Build.
//
// Das eigentliche videorecorder.cpp ist inzwischen portabel - minih264, shine und
// minimp4 sind alle reines C -, aber hier nimmt nichts den Ton auf
// (audiocapture.cpp ist ausserhalb von Windows ein Rumpf), und der Browser hat
// keinen naheliegenden Platz fuer die Datei.
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
