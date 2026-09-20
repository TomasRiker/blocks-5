#ifndef _IMG_LOAD_H
#define _IMG_LOAD_H

/*** Loading images ***/

// Replaces SDL_image. The game needs exactly one function from it, IMG_Load_RW,
// and reads every texture through its own SDL_RWops out of the encrypted
// data.zip (see File::getRWOps). Decoding is done by stb_image, a single header
// in libs/stb - which drops sdl_image.dll, libpng15-15.dll and zlib1.dll, and
// lets the Emscripten build use exactly the same code as the Windows build.
//
// PNG and JPEG are supported, and every image the game ships is a PNG. The
// packing scripts - zip_data.bat, zip_skins.bat and pack.sh - carry ogg, dat,
// xml and txt as well, so it is not that they pack only pictures; it is that
// no other image format ever gets into one.
//
// The returned surface is always 32 bit RGBA, SDL_SWSURFACE.

extern "C"
{
	SDL_Surface* IMG_Load_RW(SDL_RWops* p_src, int freeSrc);
}

#endif
