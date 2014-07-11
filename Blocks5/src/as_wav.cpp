#include "pch.h"
#include "as_wav.h"
#include "filesystem.h"

AS_WAV::AS_WAV(const std::string& filename)
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

	// Datei öffnen
	p_file = FileSystem::inst().openFile(filename);
	if(!p_file)
	{
		printfLog("+ ERROR: Could not open file \"%s\".\n",
				  filename.c_str());
		error = 1;
		return;
	}

	// RIFF-Header lesen
	uint riffHeader[3] = {0};
	p_file->read(&riffHeader, 12);

	// Header überprüfen
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

	// Datei nach Format- und Daten-Chunk durchsuchen
	while(!p_file->isEOF())
	{
		// Chunk-Header lesen
		uint chunkHeader[2] = {0};
		if(!p_file->read(&chunkHeader, 8)) break;

		uint chunkDataOffset = p_file->tell();

		// Was für ein Chunk ist es?
		switch(chunkHeader[0])
		{
		case 0x20746D66:	// "fmt "
			{
				// Formatdaten lesen
				FormatData fmt;
				p_file->read(&fmt, sizeof(fmt));

				// Nur PCM wird unterstützt!
				if(fmt.compression != 1)
				{
					printfLog("+ ERROR: WAV file \"%s\" is not in PCM format.\n",
							  filename.c_str());
					error = 2;
					return;
				}

				// Daten kopieren
				sampleRate = fmt.sampleRate;
				numBitsPerSample = fmt.numBitsPerSample;
				numChannels = fmt.numChannels;
				sliceSize = numBitsPerSample / 8 * numChannels;

				// den Rest dieses Chunks überspringen
				p_file->seek(chunkDataOffset + chunkHeader[1]);

				fmtChunkFound = true;
			}
			break;

		case 0x61746164:	// "data"
			{
				// die Position der Daten und ihre Länge (in Bytes) merken
				dataOffset = chunkDataOffset;
				dataSize = chunkHeader[1];

				// den Rest dieses Chunks überspringen
				p_file->seek(chunkDataOffset + chunkHeader[1]);

				dataChunkFound = true;
			}
			break;

		default:
			// Dieser Chunk interessiert uns nicht.
			p_file->seek(p_file->tell() + chunkHeader[1]);
			break;
		}

		if(fmtChunkFound && dataChunkFound) break;
	}

	// Wurden Format- und Daten-Chunk gefunden, und sind alle Werte ausgefüllt?
	if(!fmtChunkFound || !dataChunkFound ||
	   !dataOffset || !sampleRate || !numBitsPerSample ||!numChannels)
	{
		printfLog("+ ERROR: WAV file \"%s\" is incomplete.\n",
				  filename.c_str());
		error = 3;
		return;
	}

	// Länge (in Slices) berechnen
	length = dataSize / sliceSize;

	// Datei an den Beginn der Daten spulen
	p_file->seek(dataOffset);
}

AS_WAV::~AS_WAV()
{
	// Datei schließen
	FileSystem::inst().closeFile(p_file);
}

uint AS_WAV::read(void* p_dest,
				  uint numSlices)
{
	uint readPointer = tell();

	// Passt das?
	if(readPointer + numSlices > length)
	{
		// Nein! Anpassen!
		numSlices = length - readPointer;
		eos = true;
		error = 0;
	}

	// lesen
	uint numSlicesRead = p_file->read(p_dest, sliceSize * numSlices) / sliceSize;
	if(numSlicesRead != numSlices) error = 1;

	return numSlicesRead;
}

uint AS_WAV::tell()
{
	uint currentOffset = p_file->tell() - dataOffset;
	return currentOffset / sliceSize;
}

void AS_WAV::seek(uint position)
{
	if(position > length) return;
	p_file->seek(dataOffset + position * sliceSize);
	eos = false;
}

bool AS_WAV::isEOS()
{
	return eos;
}

uint AS_WAV::getError()
{
	return error;
}

uint AS_WAV::getSampleRate()
{
	return sampleRate;
}

uint AS_WAV::getNumBitsPerSample()
{
	return numBitsPerSample;
}

uint AS_WAV::getNumChannels()
{
	return numChannels;
}

uint AS_WAV::getLength()
{
	return length;
}