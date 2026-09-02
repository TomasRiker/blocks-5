#ifndef _VIDEORECORDER_H
#define _VIDEORECORDER_H

/*** Klasse zum Aufnehmen von Videos ***/

// Schreibt H.264 (Baseline) und MP3 in eine MP4-Datei. Das kodiert minih264,
// den Ton shine, und zusammengesetzt wird das Ganze von minimp4 - alle drei
// als Quelltext in libs/, keine DLL. Windows spielt diese Kombination seit
// Windows 7 ohne Zusatzcodec ab, und ein Linux-Build koennte dieselben drei
// Bibliotheken benutzen; das war der Grund, sie ffmpeg vorzuziehen.
//
// Der Aufrufer schreibt jedes Bild als 32-Bit-RGBX in den Puffer von
// getInputFrameBuffer() und meldet es mit encodeNextFrame() an; kodiert wird
// in einem eigenen Thread.

struct VideoRecorderImpl;

class VideoRecorder
{
public:
	VideoRecorder(const std::string& videoFilename, const Vec2i& inputFrameSize, const Vec2i& outputFrameSize, uint videoBitrate, uint audioBitrate, uint fps);
	~VideoRecorder();

	// fragt ab, ob der Rekorder bereit fuer das naechste Frame ist
	bool isReadyForNextFrame() const;

	// liefert den Puffer, in den das naechste aufzunehmende Frame kopiert werden muss (32-Bit RGBX, zeilenweise ohne Pitch)
	void* getInputFrameBuffer();

	// startet mit der Kodierung des naechsten Frames
	void encodeNextFrame(uint timecode);

	uint getFPS() const;

	bool getError() const;

private:
	// nicht kopierbar - Thread und Datei gehoeren genau einem Objekt
	VideoRecorder(const VideoRecorder&);
	VideoRecorder& operator=(const VideoRecorder&);

	VideoRecorderImpl* p_impl;
};

#endif
