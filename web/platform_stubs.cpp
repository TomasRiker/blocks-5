// platform_stubs.cpp - symbols the web build references but cannot provide.
#include <SDL.h>
#include <cstddef>

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

// --- hq2x upscaler ----------------------------------------------------------
// The real implementation is hand-written x86 assembly shipped as a prebuilt
// .obj (libs/bin/hq2x32.obj), which cannot target wasm. InitLUTs() returning 0
// makes Engine::init report the -hq2x mode as unavailable and fall back to
// normal scaling, so hq2x_32 is never reached.
void hq2x_32(unsigned char*, unsigned char*, unsigned long, unsigned long, unsigned long) {}

} // extern "C"

int InitLUTs(void) { return 0; }
