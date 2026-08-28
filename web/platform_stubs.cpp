// platform_stubs.cpp - symbols the web build references but cannot provide.
#include <SDL.h>
#include <cstddef>
#include <cstring>
#include <algorithm>

extern "C" {

// --- SDL 1.2 cursor API -----------------------------------------------------
// Emscripten's SDL declares these but implements none of them: the cursor is
// the browser's. Engine::setupCursor builds a 32x32 cursor from the game's
// artwork; here it is accepted and dropped, leaving the default pointer.
// The in-game cursor that GUI draws itself is unaffected.
SDL_Cursor* SDL_CreateCursor(const Uint8*, const Uint8*, int, int, int, int) { return NULL; }
void        SDL_SetCursor(SDL_Cursor*)  {}
SDL_Cursor* SDL_GetCursor(void)         { return NULL; }
void        SDL_FreeCursor(SDL_Cursor*) {}

// --- SDL surface locking -----------------------------------------------------
// Emscripten implements SDL_UnlockSurface as `assert(!SDL.GL)` — it refuses to
// run at all in GL mode, which is the only mode this game uses. Every locked
// surface here is an offscreen SDL_SWSURFACE the game allocated itself
// (Texture::p_rgba and the window-icon surface), whose `pixels` pointer is
// always valid, so locking is a formality that real SDL also treats as a no-op.
// Defining them here means wasm-ld resolves to these and never pulls in the JS
// versions. Note texture.cpp:187 deliberately unlocks before a blit without a
// matching lock, so any counting implementation would be wrong anyway.
int  SDL_LockSurface(SDL_Surface*)   { return 0; }
void SDL_UnlockSurface(SDL_Surface*) {}

// --- Surface blitting --------------------------------------------------------
// Emscripten implements SDL_BlitSurface (SDL_UpperBlit) on a 2D canvas:
// dstData.ctx.drawImage(srcData.canvas, ...) followed by a read-back into the
// surface's pixel buffer. That only works for surfaces the SDL layer itself
// created from an image, because only those have a backing canvas.
//
// Every surface this game blits was written directly in memory - decoded by
// stb_image in img_load.cpp, or produced by SDL_CreateRGBSurface and filled by
// the game - so the source canvas is empty and the blit silently copies
// nothing. The result is that every texture uploads fully transparent, which
// looks exactly like "textures do not work" while untextured geometry still
// draws normally.
//
// All four call sites (texture.cpp:74/191/248, engine.cpp:261) are 32-bit RGBA
// to 32-bit RGBA and disable per-surface alpha first with SDL_SetAlpha(s, 0, 0),
// which in SDL 1.2 means "no blending, copy the pixels". So a straight row copy
// is the correct semantics, not an approximation of one.
extern "C" int SDL_UpperBlit(SDL_Surface* p_src, const SDL_Rect* p_srcRect,
                             SDL_Surface* p_dst, SDL_Rect* p_dstRect)
{
    if (!p_src || !p_dst || !p_src->pixels || !p_dst->pixels) return -1;
    if (p_src->format->BytesPerPixel != 4 || p_dst->format->BytesPerPixel != 4) return -1;

    int sx = p_srcRect ? p_srcRect->x : 0;
    int sy = p_srcRect ? p_srcRect->y : 0;
    int w  = p_srcRect ? p_srcRect->w : p_src->w;
    int h  = p_srcRect ? p_srcRect->h : p_src->h;
    int dx = p_dstRect ? p_dstRect->x : 0;
    int dy = p_dstRect ? p_dstRect->y : 0;

    // Clip against both surfaces, keeping source and destination in step.
    if (sx < 0) { w += sx; dx -= sx; sx = 0; }
    if (sy < 0) { h += sy; dy -= sy; sy = 0; }
    if (dx < 0) { w += dx; sx -= dx; dx = 0; }
    if (dy < 0) { h += dy; sy -= dy; dy = 0; }
    w = std::min(w, std::min(p_src->w - sx, p_dst->w - dx));
    h = std::min(h, std::min(p_src->h - sy, p_dst->h - dy));
    if (w <= 0 || h <= 0)
    {
        if (p_dstRect) { p_dstRect->w = 0; p_dstRect->h = 0; }
        return 0;
    }

    const unsigned char* p_srcPixels = static_cast<const unsigned char*>(p_src->pixels);
    unsigned char* p_dstPixels = static_cast<unsigned char*>(p_dst->pixels);
    for (int row = 0; row < h; ++row)
        memcpy(p_dstPixels + (dy + row) * p_dst->pitch + dx * 4,
               p_srcPixels + (sy + row) * p_src->pitch + sx * 4,
               (size_t)w * 4);

    if (p_dstRect) { p_dstRect->w = w; p_dstRect->h = h; }
    return 0;
}

// --- hq2x upscaler ----------------------------------------------------------
// The real implementation is hand-written x86 assembly shipped as a prebuilt
// .obj (libs/bin/hq2x32.obj), which cannot target wasm. InitLUTs() returning 0
// makes Engine::init report the -hq2x mode as unavailable and fall back to
// normal scaling, so hq2x_32 is never reached.
void hq2x_32(unsigned char*, unsigned char*, unsigned long, unsigned long, unsigned long) {}

} // extern "C"

int InitLUTs(void) { return 0; }
