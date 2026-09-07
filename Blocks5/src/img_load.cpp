#include "pch.h"
#include "img_load.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"

extern "C"
{

#ifdef __EMSCRIPTEN__
// Emscripten's SDL declares these two but does not implement them -
// File::getRWOps needs them to build its own RWops.
SDL_RWops* SDL_AllocRW(void)       { return (SDL_RWops*)calloc(1, sizeof(SDL_RWops)); }
void SDL_FreeRW(SDL_RWops* p_area) { free(p_area); }
#endif

SDL_Surface* IMG_Load_RW(SDL_RWops* p_src, int freeSrc)
{
	if(!p_src || !p_src->read) return 0;

	// Read the stream to the end through the game's own callbacks. stb_image
	// wants the data in memory in one piece.
	std::vector<unsigned char> data;
	unsigned char chunk[16384];
	for(;;)
	{
		const size_t got = p_src->read(p_src, chunk, 1, sizeof(chunk));
		if(got == 0) break;
		data.insert(data.end(), chunk, chunk + got);
	}
	if(freeSrc && p_src->close) p_src->close(p_src);
	if(data.empty())
	{
		SDL_SetError("IMG_Load_RW: empty stream");
		return 0;
	}

	int width = 0, height = 0, channels = 0;
	unsigned char* p_pixels = stbi_load_from_memory(&data[0], (int)data.size(), &width, &height, &channels, 4);
	if(!p_pixels)
	{
		SDL_SetError("IMG_Load_RW: %s", stbi_failure_reason());
		return 0;
	}

	// stb delivers tightly packed RGBA. SDL_CreateRGBSurfaceFrom would not take
	// ownership of the buffer; the pixels are copied into a surface of our own.
	SDL_Surface* p_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32,
												  0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	if(!p_surface)
	{
		stbi_image_free(p_pixels);
		return 0;
	}

	// No SDL_LockSurface: under Emscripten lock and unlock synchronise with the
	// canvas and bail out on a pure software surface. Its pixels are directly
	// writable anyway, in real SDL too.
	for(int y = 0; y < height; y++)
		memcpy((unsigned char*)p_surface->pixels + y * p_surface->pitch, p_pixels + y * width * 4, width * 4);

	stbi_image_free(p_pixels);
	return p_surface;
}

SDL_Surface* IMG_Load(const char* p_filename)
{
	SDL_RWops* p_rwOps = SDL_RWFromFile(p_filename, "rb");
	return p_rwOps ? IMG_Load_RW(p_rwOps, 1) : 0;
}

}
