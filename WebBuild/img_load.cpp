// img_load.cpp - image decoding for the web build.
//
// The game loads every texture out of an encrypted zip through its own virtual
// filesystem, handing SDL_image a custom SDL_RWops backed by File::read/seek
// (see File::getRWOps in src/file.cpp). Emscripten's IMG_Load_RW cannot do that:
// it decodes via the browser and only accepts names of files that were
// preloaded, so it aborts on a synthesised RWops.
//
// So this build does not link Emscripten's SDL_image at all. Instead we supply
// IMG_Load_RW ourselves: pull the whole stream through the game's own RWops
// callbacks and decode it synchronously with stb_image. texture.cpp and
// engine.cpp are left completely untouched.
#include <SDL.h>
#include <cstdlib>
#include <cstring>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"

extern "C" {

// Emscripten's SDL declares these but leaves them as "TODO" aborts.
SDL_RWops* SDL_AllocRW(void)       { return (SDL_RWops*)calloc(1, sizeof(SDL_RWops)); }
void SDL_FreeRW(SDL_RWops* p_area) { free(p_area); }

SDL_Surface* IMG_Load_RW(SDL_RWops* p_src, int freeSrc)
{
    if (!p_src || !p_src->read) return 0;

    // Drain the stream through the game's own callbacks.
    std::vector<unsigned char> data;
    unsigned char chunk[16384];
    for (;;) {
        const size_t got = p_src->read(p_src, chunk, 1, sizeof(chunk));
        if (got == 0) break;
        data.insert(data.end(), chunk, chunk + got);
    }
    if (freeSrc && p_src->close) p_src->close(p_src);
    if (data.empty()) { SDL_SetError("IMG_Load_RW: empty stream"); return 0; }

    int w = 0, h = 0, channels = 0;
    unsigned char* p_pixels = stbi_load_from_memory(&data[0], (int)data.size(), &w, &h, &channels, 4);
    if (!p_pixels) { SDL_SetError("IMG_Load_RW: %s", stbi_failure_reason()); return 0; }

    // stb hands back tightly packed RGBA; hand SDL a surface that owns a copy,
    // because SDL_CreateRGBSurfaceFrom does not take ownership of the pixels.
    SDL_Surface* p_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32,
                                                  0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (!p_surface) { stbi_image_free(p_pixels); return 0; }
    // No SDL_LockSurface/UnlockSurface here: under Emscripten those synchronise
    // with a canvas and assert on a plain software surface. SDL_SWSURFACE pixels
    // are directly writable, which is all we need.
    for (int y = 0; y < h; ++y)
        memcpy((unsigned char*)p_surface->pixels + y * p_surface->pitch, p_pixels + y * w * 4, w * 4);
    stbi_image_free(p_pixels);
    return p_surface;
}

SDL_Surface* IMG_Load(const char* p_file)
{
    SDL_RWops* p_rw = SDL_RWFromFile(p_file, "rb");
    return p_rw ? IMG_Load_RW(p_rw, 1) : 0;
}

} // extern "C"
