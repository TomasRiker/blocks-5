#ifndef _AS_Wav_H
#define _AS_Wav_H

/*** Klasse fuer WAV-Audiodateien ***/

#include "audiostream.h"

class File;

class AS_Wav : public AudioStream
{
public:
	AS_Wav(const std::string& filename);
	~AS_Wav();

	uint read(void* p_dest, uint numSlices);
	uint tell();
	void seek(uint position);
	bool isEOS();
	uint getError();
	uint getSampleRate();
	uint getNumBitsPerSample();
	uint getNumChannels();
	uint getLength();

private:
	struct FormatData
	{
		ushort compression;
		ushort numChannels;
		uint sampleRate;
		uint numBytesPerSecond;
		ushort blockAlign;
		ushort numBitsPerSample;
	};

	File* p_file;
	uint dataOffset;
	uint dataSize;
	uint sliceSize;

	bool eos;
	int error;
	uint sampleRate;
	uint numBitsPerSample;
	uint numChannels;
	uint length;
};

#endif