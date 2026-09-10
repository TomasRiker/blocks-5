#include "pch.h"
#include "as_wav.h"
#include "filesystem.h"

AS_Wav::AS_Wav(const std::string& filename)
{
	dataOffset = 0;
	dataSize = 0;
	sliceSize = 0;

	eos = false;
	error = 0;
	sampleRate = 0;
	numBitsPerSample = 0;
	numChannels = 0;
	length = 0;

	// open the file
	p_file = FileSystem::inst().openFile(filename);
	if(!p_file)
	{
		printfLog("+ ERROR: Could not open file \"%s\".\n",
				  filename.c_str());
		error = 1;
		return;
	}

	// read the RIFF header
	uint riffHeader[3] = {0};
	p_file->read(&riffHeader, 12);

	// check the header
	if(riffHeader[0] != 0x46464952 ||	// "RIFF"
	   riffHeader[2] != 0x45564157)		// "WAVE"
	{
		printfLog("+ ERROR: File \"%s\" is not a valid WAV file.\n",
				  filename.c_str());
		error = 2;
		return;
	}

	bool fmtChunkFound = false;
	bool dataChunkFound = false;

	// search the file for the format and the data chunk
	while(!p_file->isEOF())
	{
		// read the chunk header
		uint chunkHeader[2] = {0};
		if(!p_file->read(&chunkHeader, 8)) break;

		uint chunkDataOffset = p_file->tell();

		// What kind of chunk is it?
		switch(chunkHeader[0])
		{
		case 0x20746D66:	// "fmt "
			{
				// read the format data
				FormatData fmt;
				p_file->read(&fmt, sizeof(fmt));

				// Only PCM is supported!
				if(fmt.compression != 1)
				{
					printfLog("+ ERROR: WAV file \"%s\" is not in PCM format.\n",
							  filename.c_str());
					error = 2;
					return;
				}

				// copy the data
				sampleRate = fmt.sampleRate;
				numBitsPerSample = fmt.numBitsPerSample;
				numChannels = fmt.numChannels;
				sliceSize = numBitsPerSample / 8 * numChannels;

				// skip the rest of this chunk
				p_file->seek(chunkDataOffset + chunkHeader[1]);

				fmtChunkFound = true;
			}
			break;

		case 0x61746164:	// "data"
			{
				// remember where the data sits and how long it is (in bytes)
				dataOffset = chunkDataOffset;
				dataSize = chunkHeader[1];

				// skip the rest of this chunk
				p_file->seek(chunkDataOffset + chunkHeader[1]);

				dataChunkFound = true;
			}
			break;

		default:
			// This chunk is of no interest.
			p_file->seek(p_file->tell() + chunkHeader[1]);
			break;
		}

		if(fmtChunkFound && dataChunkFound) break;
	}

	// Were the format and the data chunk found, and are all values filled in?
	if(!fmtChunkFound || !dataChunkFound ||
	   !dataOffset || !sampleRate || !numBitsPerSample ||!numChannels)
	{
		printfLog("+ ERROR: WAV file \"%s\" is incomplete.\n",
				  filename.c_str());
		error = 3;
		return;
	}

	// compute the length (in slices)
	length = dataSize / sliceSize;

	// wind the file to the start of the data
	p_file->seek(dataOffset);
}

AS_Wav::~AS_Wav()
{
	// close the file
	FileSystem::inst().closeFile(p_file);
}

uint AS_Wav::read(void* p_dest,
				  uint numSlices)
{
	uint readPointer = tell();

	// Does that fit?
	if(readPointer + numSlices > length)
	{
		// No! Adjust!
		numSlices = length - readPointer;
		eos = true;
		error = 0;
	}

	// read
	uint numSlicesRead = p_file->read(p_dest, sliceSize * numSlices) / sliceSize;
	if(numSlicesRead != numSlices) error = 1;

	return numSlicesRead;
}

uint AS_Wav::tell()
{
	uint currentOffset = p_file->tell() - dataOffset;
	return currentOffset / sliceSize;
}

void AS_Wav::seek(uint position)
{
	if(position > length) return;
	p_file->seek(dataOffset + position * sliceSize);
	eos = false;
}

bool AS_Wav::isEOS()
{
	return eos;
}

uint AS_Wav::getError()
{
	return error;
}

uint AS_Wav::getSampleRate()
{
	return sampleRate;
}

uint AS_Wav::getNumBitsPerSample()
{
	return numBitsPerSample;
}

uint AS_Wav::getNumChannels()
{
	return numChannels;
}

uint AS_Wav::getLength()
{
	return length;
}