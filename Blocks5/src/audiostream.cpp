#include "pch.h"
#include "audiostream.h"
#include "as_wav.h"
#include "as_ogg.h"

AudioStream::AudioStream()
{
}

AudioStream::~AudioStream()
{
}

uint AudioStream::getSliceSize()
{
	return getNumBitsPerSample() / 8 * getNumChannels();
}

ALenum AudioStream::getOpenALBufferFormat()
{
	ALenum format = 0;
	uint bps = getNumBitsPerSample();
	uint channels = getNumChannels();
	if(bps == 8)
	{
		if(channels == 1) format = AL_FORMAT_MONO8;
		else if(channels == 2) format = AL_FORMAT_STEREO8;
	}
	else if(bps == 16)
	{
		if(channels == 1) format = AL_FORMAT_MONO16;
		else if(channels == 2) format = AL_FORMAT_STEREO16;
	}

	return format;
}

AudioStream* AudioStream::open(const std::string& filename)
{
	AudioStream* p_stream = 0;

	// WAV oder OGG?
	std::string ext = getFilenameExtension(filename);
	if(ext == "wav")
	{
		p_stream = new AS_WAV(filename);
	}
	else if(ext == "ogg")
	{
		p_stream = new AS_OGG(filename);
	}
	else
	{
		// Unbekannter Dateityp!
		printfLog("+ ERROR: Unknown file extension \"%s\" for audio files.\n",
				  ext.c_str());
		return 0;
	}

	// Laden erfolgreich?
	if(p_stream->getError())
	{
		printfLog("+ ERROR: Could not create audio stream for file \"%s\".\n",
				  filename.c_str());
		return 0;
	}

	return p_stream;
}