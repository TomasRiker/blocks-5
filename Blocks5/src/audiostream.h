#ifndef _AUDIOSTREAM_H
#define _AUDIOSTREAM_H

/*** Klasse für einen Audio-Stream ***/

class AudioStream
{
public:
	AudioStream();
	virtual ~AudioStream();

	virtual uint read(void* p_dest, uint numSlices) = 0;
	virtual uint tell() = 0;
	virtual void seek(uint position) = 0;
	virtual bool isEOS() = 0;
	virtual uint getError() = 0;
	virtual uint getSampleRate() = 0;
	virtual uint getNumBitsPerSample() = 0;
	virtual uint getNumChannels() = 0;
	virtual uint getLength() = 0;

	uint getSliceSize();
	ALenum getOpenALBufferFormat();

	static AudioStream* open(const std::string& filename);
};

#endif