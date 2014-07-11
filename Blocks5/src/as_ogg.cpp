#include "pch.h"
#include "as_ogg.h"
#include "filesystem.h"

AS_OGG::AS_OGG(const std::string& filename)
{
	sliceSize = 0;

	eos = false;
	error = 0;
	sampleRate = 0;
	numBitsPerSample = 0;
	numChannels = 0;
	length = 0;

	File* p_file = FileSystem::inst().openFile(filename.c_str());
	if(!p_file)
	{
		printfLog("+ ERROR: Could not open file \"%s\".\n",
				  filename.c_str());
		error = 1;
		return;
	}

	// OGG-Vorbis-Handle erzeugen
	int r = ov_open_callbacks(p_file, &vorbisFile, 0, 0, p_file->getOVCallbacks());
	if(r)
	{
		printfLog("+ ERROR: Could not open OGG file \"%s\" (Error: %d).\n",
				  filename.c_str(),
				  r);
		error = 2;
		return;
	}

	// Informationen eintragen
	vorbis_info* p_info = ov_info(&vorbisFile, -1);
	sampleRate = p_info->rate;
	numBitsPerSample = 16;
	numChannels = p_info->channels;
	sliceSize = numBitsPerSample / 8 * numChannels;
	length = static_cast<uint>(ov_pcm_total(&vorbisFile, -1));
}

AS_OGG::~AS_OGG()
{
	// Datei schlieﬂen
	ov_clear(&vorbisFile);
}

uint AS_OGG::read(void* p_dest,
				  uint numSlices)
{
	uint numBytesRead = 0;
	uint numBytesLeft = numSlices * sliceSize;
	char* p_cursor = static_cast<char*>(p_dest);
	while(numBytesLeft)
	{
		int currentStream = 0;
		int n = ov_read(&vorbisFile, p_cursor, numBytesLeft, 0, 2, 1, &currentStream);
		if(!n)
		{
			// Ende der Datei!
			eos = true;
			error = 0;
			break;
		}
		else if(n < 0)
		{
			// Fehler!
			error = 1;
			break;
		}
		else
		{
			numBytesRead += n;
			numBytesLeft -= n;
			p_cursor += n;
		}
	}

	return numBytesRead / sliceSize;
}

uint AS_OGG::tell()
{
	return static_cast<uint>(ov_pcm_tell(&vorbisFile));
}

void AS_OGG::seek(uint position)
{
	if(position > length) return;
	ov_pcm_seek(&vorbisFile, position);
	eos = false;
}

bool AS_OGG::isEOS()
{
	return eos;
}

uint AS_OGG::getError()
{
	return error;
}

uint AS_OGG::getSampleRate()
{
	return sampleRate;
}

uint AS_OGG::getNumBitsPerSample()
{
	return numBitsPerSample;
}

uint AS_OGG::getNumChannels()
{
	return numChannels;
}

uint AS_OGG::getLength()
{
	return length;
}