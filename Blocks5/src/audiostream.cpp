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

	// WAV or OGG? In any case: a track called Theme.OGG is as good as one
	// called theme.ogg.
	std::string ext = getFilenameExtension(filename);
	if(equalsNoCase(ext.c_str(), "wav"))
	{
		p_stream = new AS_Wav(filename);
	}
	else if(equalsNoCase(ext.c_str(), "ogg"))
	{
		p_stream = new AS_Ogg(filename);
	}
	else
	{
		// Unknown file type!
		printfLog("+ ERROR: Unknown file extension \"%s\" for audio files.\n",
				  ext.c_str());
		return 0;
	}

	// Loaded successfully?
	if(p_stream->getError())
	{
		printfLog("+ ERROR: Could not create audio stream for file \"%s\".\n",
				  filename.c_str());
		delete p_stream;
		return 0;
	}

	// The rate is the file's word, and music comes with levels and campaigns
	// from anybody: StreamedSound sizes its buffer as a quarter second of it,
	// which a rate near 2^32 turns into gigabytes. 192 kHz is the highest
	// rate in common use; anything above is refused rather than trusted.
	const uint rate = p_stream->getSampleRate();
	if(rate < 1000 || rate > 192000)
	{
		printfLog("+ ERROR: Audio file \"%s\" claims a sample rate of %u Hz.\n",
				  filename.c_str(), rate);
		delete p_stream;
		return 0;
	}

	return p_stream;
}