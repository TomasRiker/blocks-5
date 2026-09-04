#ifndef _AS_Ogg_H
#define _AS_Ogg_H

/*** Klasse fuer OGG-Audiodateien ***/

#include "audiostream.h"

class AS_Ogg : public AudioStream
{
public:
	AS_Ogg(const std::string& filename);
	~AS_Ogg();

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