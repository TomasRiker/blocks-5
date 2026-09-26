#ifndef _IMG_LOAD_H
#define _IMG_LOAD_H

/*** Loading images ***/

// Stands in for SDL_image, of which the game needs exactly one function,
// IMG_Load_RW: every texture is read through the game's own SDL_RWops
// (File::getRWOps), which reach into the encrypted archives. stb_image
// (libs/stb, one header) decodes, the same code in all three builds. PNG and
// JPEG are enabled; every image the game ships is a PNG.
//
// The returned surface is always 32 bit RGBA, SDL_SWSURFACE.

extern "C"
{
	SDL_Surface* IMG_Load_RW(SDL_RWops* p_src, int freeSrc);
}

#endif
