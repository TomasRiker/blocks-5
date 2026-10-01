#ifndef _IMG_SAVE_H
#define _IMG_SAVE_H

/*** Saving images ***/

// PNG encoder, the counterpart to img_load.h but with no library of its own:
// zlib is compiled into all three builds anyway, compress2() returns exactly
// the zlib stream an IDAT chunk holds and crc32() is every chunk's checksum.
//
// srcChannels is the layout in memory, dstChannels what goes into the file;
// 3 means RGB, 4 RGBA, 8 bits per channel. The screenshot needs them to
// differ: glReadPixels must read RGBA, all that WebGL 1 allows, but a channel
// of nothing but 255 does not collapse in the deflate, it pushes every
// prediction one byte apart - 330 instead of 247 KB on a screenshot.
//
// bottomUp flips the rows: glReadPixels delivers the bottom row first, PNG
// wants the top one.
bool encodePNG(const uchar* p_pixels, const Vec2i& size,
			   int srcChannels, int dstChannels, bool bottomUp,
			   std::vector<uchar>* p_out);

#endif
