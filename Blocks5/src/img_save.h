#ifndef _IMG_SAVE_H
#define _IMG_SAVE_H

/*** Saving images ***/

// PNG encoder, the counterpart to img_load.h - only without a third-party
// library. Decoding is hard, which is why stb_image sits in libs/stb; encoding
// is not, because zlib does the work and is compiled into all three builds
// anyway: compress2() returns exactly the zlib datastream an IDAT chunk
// consists of, and crc32() is the checksum every chunk needs.
//
// srcChannels is the layout in memory, dstChannels what is to end up in the
// file; 3 means RGB, 4 RGBA, always 8 bit per channel. The two may differ, and
// the one caller does need that: glReadPixels must read RGBA because WebGL 1
// allows nothing else, but a channel of nothing but 255 still costs a quarter
// of the file - it does not collapse in on itself but pushes every prediction
// one byte apart. Measured on a screenshot: 330 instead of 247 KB.
//
// bottomUp flips the rows while encoding, which comes from glReadPixels as
// well: OpenGL delivers the bottom row first, PNG wants the top one.
bool encodePNG(const uchar* p_pixels, const Vec2i& size,
			   int srcChannels, int dstChannels, bool bottomUp,
			   std::vector<uchar>* p_out);

#endif
