#include "pch.h"
#include "img_save.h"
#include <zlib.h>

namespace
{
	const uchar SIGNATURE[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

	void putUint(std::vector<uchar>* p_out, uint value)
	{
		p_out->push_back(static_cast<uchar>(value >> 24));
		p_out->push_back(static_cast<uchar>(value >> 16));
		p_out->push_back(static_cast<uchar>(value >> 8));
		p_out->push_back(static_cast<uchar>(value));
	}

	// Length, type, content, checksum. The CRC32 runs over type *and* content;
	// the length itself does not count.
	void putChunk(std::vector<uchar>* p_out, const char* p_type,
				  const uchar* p_data, uint numBytes)
	{
		putUint(p_out, numBytes);
		const size_t crcStart = p_out->size();
		p_out->insert(p_out->end(), p_type, p_type + 4);
		if(numBytes) p_out->insert(p_out->end(), p_data, p_data + numBytes);
		putUint(p_out, static_cast<uint>(
			crc32(0, &(*p_out)[crcStart], static_cast<uInt>(p_out->size() - crcStart))));
	}

	// Predict a row and write the deviation. a is the left neighbour, b the one
	// above, c the one above left; outside the image all three are 0. Type 4 is
	// the Paeth predictor: of the three it takes the one that comes closest to
	// a+b-c.
	void applyFilter(int type, const uchar* p_row, const uchar* p_prev,
					 uint bpp, uint stride, uchar* p_dest)
	{
		for(uint i = 0; i < stride; i++)
		{
			const int a = (i >= bpp) ? p_row[i - bpp] : 0;
			const int b = p_prev[i];
			const int c = (i >= bpp) ? p_prev[i - bpp] : 0;

			int prediction = 0;
			switch(type)
			{
				case 1: prediction = a; break;
				case 2: prediction = b; break;
				case 3: prediction = (a + b) / 2; break;
				case 4:
				{
					const int p = a + b - c;
					const int pa = abs(p - a);
					const int pb = abs(p - b);
					const int pc = abs(p - c);
					prediction = (pa <= pb && pa <= pc) ? a : ((pb <= pc) ? b : c);
					break;
				}
			}

			p_dest[i] = static_cast<uchar>(p_row[i] - prediction);
		}
	}

	// Which filter is best for this row is decided by the sum of magnitudes over
	// the filtered bytes read as signed numbers. That is the rule of thumb from
	// libpng, and it is worth its twenty lines: measured on a real screenshot,
	// 239 instead of 283 KB.
	uint filterScore(const uchar* p_line, uint stride)
	{
		uint sum = 0;
		for(uint i = 0; i < stride; i++)
		{
			const uint v = p_line[i];
			sum += (v < 128) ? v : 256 - v;
		}
		return sum;
	}
}

bool encodePNG(const uchar* p_pixels, const Vec2i& size,
			   int srcChannels, int dstChannels, bool bottomUp,
			   std::vector<uchar>* p_out)
{
	if(!p_pixels || !p_out) return false;
	if(size.x <= 0 || size.y <= 0) return false;
	if(srcChannels != 3 && srcChannels != 4) return false;
	if(dstChannels != 3 && dstChannels != 4) return false;
	if(dstChannels > srcChannels) return false;

	const uint width = static_cast<uint>(size.x);
	const uint height = static_cast<uint>(size.y);
	const uint bpp = static_cast<uint>(dstChannels);
	const uint srcBpp = static_cast<uint>(srcChannels);
	const uint stride = width * bpp;

	// Every row gets its filter byte in front of it; that is the stream that
	// gets compressed.
	std::vector<uchar> raw(static_cast<size_t>(stride + 1) * height);
	std::vector<uchar> candidate(stride);
	std::vector<uchar> best(stride);
	std::vector<uchar> previous(stride, 0);
	std::vector<uchar> narrowed(srcBpp == bpp ? 0 : stride);

	uchar* p_dest = &raw[0];
	for(uint y = 0; y < height; y++)
	{
		const uchar* p_source =
			p_pixels + static_cast<size_t>(bottomUp ? (height - 1 - y) : y) * width * srcBpp;

		// Surplus channels are dropped here, before the filtering - afterwards
		// the prediction would already have run at the wrong spacing.
		const uchar* p_row = p_source;
		if(srcBpp != bpp)
		{
			for(uint x = 0; x < width; x++)
				memcpy(&narrowed[x * bpp], p_source + x * srcBpp, bpp);
			p_row = &narrowed[0];
		}

		int bestType = 0;
		uint bestScore = 0;
		for(int type = 0; type < 5; type++)
		{
			applyFilter(type, p_row, &previous[0], bpp, stride, &candidate[0]);
			const uint score = filterScore(&candidate[0], stride);
			if(!type || score < bestScore)
			{
				bestScore = score;
				bestType = type;
				best.swap(candidate);
			}
		}

		*p_dest++ = static_cast<uchar>(bestType);
		memcpy(p_dest, &best[0], stride);
		p_dest += stride;
		memcpy(&previous[0], p_row, stride);
	}

	uLongf compressedSize = compressBound(static_cast<uLong>(raw.size()));
	std::vector<uchar> compressed(compressedSize);
	if(compress2(&compressed[0], &compressedSize, &raw[0],
				 static_cast<uLong>(raw.size()), 9) != Z_OK) return false;

	uchar header[13];
	header[0] = static_cast<uchar>(width >> 24);
	header[1] = static_cast<uchar>(width >> 16);
	header[2] = static_cast<uchar>(width >> 8);
	header[3] = static_cast<uchar>(width);
	header[4] = static_cast<uchar>(height >> 24);
	header[5] = static_cast<uchar>(height >> 16);
	header[6] = static_cast<uchar>(height >> 8);
	header[7] = static_cast<uchar>(height);
	header[8] = 8;                                        // bits per channel
	header[9] = (dstChannels == 4) ? 6 : 2;               // with or without alpha
	header[10] = 0;                                       // Deflate
	header[11] = 0;                                       // per-row filter
	header[12] = 0;                                       // no interlacing

	p_out->clear();
	p_out->reserve(compressedSize + 64);
	p_out->insert(p_out->end(), SIGNATURE, SIGNATURE + 8);
	putChunk(p_out, "IHDR", header, 13);
	putChunk(p_out, "IDAT", &compressed[0], static_cast<uint>(compressedSize));
	putChunk(p_out, "IEND", 0, 0);
	return true;
}
