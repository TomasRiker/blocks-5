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

	// Chunk sizes come from the file, and a campaign brings the music its
	// levels name, so the file may be anybody's. A chunk's end is therefore
	// computed in 64 bits and held to the file: in 32 a size near 4 GB wraps
	// to before the chunk, and the walk reads the same header for ever.
	const uint64 fileSize = p_file->getSize();

	// search the file for the format and the data chunk
	while(!p_file->isEOF())
	{
		// read the chunk header
		uint chunkHeader[2] = {0};
		if(p_file->read(&chunkHeader, 8) != 8) break;

		const uint chunkDataOffset = p_file->tell();
		const uint64 chunkEnd = static_cast<uint64>(chunkDataOffset) + chunkHeader[1] + (chunkHeader[1] & 1);
		const uint nextChunk = static_cast<uint>(chunkEnd < fileSize ? chunkEnd : fileSize);

		// What kind of chunk is it?
		switch(chunkHeader[0])
		{
		case 0x20746D66:	// "fmt "
			{
				// read the format data
				FormatData fmt;
				memset(&fmt, 0, sizeof(fmt));
				p_file->read(&fmt, sizeof(fmt));

				// Only PCM is supported!
				if(fmt.compression != 1)
				{
					printfLog("+ ERROR: WAV file \"%s\" is not in PCM format.\n",
							  filename.c_str());
					error = 2;
					return;
				}

				// And only what OpenAL plays: 8 or 16 bits, mono or stereo.
				// Anything else is refused here rather than later - a slice of
				// fewer than 8 bits is zero bytes, and the length below is
				// divided by it.
				if((fmt.numBitsPerSample != 8 && fmt.numBitsPerSample != 16) ||
				   (fmt.numChannels != 1 && fmt.numChannels != 2))
				{
					printfLog("+ ERROR: WAV file \"%s\" has %u bits and %u channels; only 8 or 16 bits, mono or stereo, is supported.\n",
							  filename.c_str(),
							  static_cast<uint>(fmt.numBitsPerSample),
							  static_cast<uint>(fmt.numChannels));
					error = 2;
					return;
				}

				// copy the data
				sampleRate = fmt.sampleRate;
				numBitsPerSample = fmt.numBitsPerSample;
				numChannels = fmt.numChannels;
				sliceSize = numBitsPerSample / 8 * numChannels;

				// Skip the rest of this chunk. RIFF pads an odd-sized chunk
				// to an even length, and the pad byte is not in the size.
				p_file->seek(nextChunk);

				fmtChunkFound = true;
			}
			break;

		case 0x61746164:	// "data"
			{
				// remember where the data sits and how long it is (in bytes) -
				// no longer than what the file actually holds
				dataOffset = chunkDataOffset;
				dataSize = static_cast<uint>(min(static_cast<uint64>(chunkHeader[1]), fileSize - chunkDataOffset));

				// skip the rest of this chunk, pad byte included
				p_file->seek(nextChunk);

				dataChunkFound = true;
			}
			break;

		default:
			// This chunk is of no interest.
			p_file->seek(nextChunk);
			break;
		}

		if(fmtChunkFound && dataChunkFound) break;

		// a chunk that reaches the end of the file is the last one
		if(nextChunk >= fileSize) break;
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