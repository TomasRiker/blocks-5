#ifndef _AS_OGG_H
#define _AS_OGG_H

/*** Klasse für OGG-Audiodateien ***/

#include "audiostream.h"

class AS_OGG : public AudioStream
{
public:
	AS_OGG(const std::string& filename);
	~AS_OGG();

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
	FILE* p_file;
	OggVorbis_File vorbisFile;
	uint sliceSize;

	bool eos;
	int error;
	uint sampleRate;
	uint numBitsPerSample;
	uint numChannels;
	uint length;
};

#endif