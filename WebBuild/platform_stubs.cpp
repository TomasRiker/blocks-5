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
// Emscripten implements SDL_UnlockSurface as `assert(!SDL.GL)` - it refuses to
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


// --- SDL_GetKeyName ----------------------------------------------------------
// Emscripten's own (libsdl.js) answers only for a-z and 0-9 and returns an empty
// string for anything else, so every binding in the options read as a bare
// "Keyboard " - and not one of the defaults (arrows, shift, tab, return, F5 and
// the rest) is a letter or a digit, so that was all of them.
//
// The table is SDL 1.2.15's own, taken from
// libs/SDL-1.2.15/src/events/SDL_keyboard.c, which this tree vendors and the
// Windows build compiles - so both builds now name a key the same way. The
// SDLK_WORLD_0..95 entries are left out because Emscripten ships SDL2 headers
// with a 1.2 compatibility layer and does not declare them; they are the
// Latin-1 dead keys, which no binding here uses.
//
// Nothing depends on the text: config.xml stores a virtual key's index, never
// its name. This is what the options dialog prints, and nothing else.
static const char* keynames[SDLK_LAST];

static void initKeyNames(void)
{
	static int done = 0;
	if (done) return;
	done = 1;
	memset((void*)keynames, 0, sizeof(keynames));
	keynames[SDLK_BACKSPACE] = "backspace";
	keynames[SDLK_TAB] = "tab";
	keynames[SDLK_CLEAR] = "clear";
	keynames[SDLK_RETURN] = "return";
	keynames[SDLK_PAUSE] = "pause";
	keynames[SDLK_ESCAPE] = "escape";
	keynames[SDLK_SPACE] = "space";
	keynames[SDLK_EXCLAIM] = "!";
	keynames[SDLK_QUOTEDBL] = "\"";
	keynames[SDLK_HASH] = "#";
	keynames[SDLK_DOLLAR] = "$";
	keynames[SDLK_AMPERSAND] = "&";
	keynames[SDLK_QUOTE] = "'";
	keynames[SDLK_LEFTPAREN] = "(";
	keynames[SDLK_RIGHTPAREN] = ")";
	keynames[SDLK_ASTERISK] = "*";
	keynames[SDLK_PLUS] = "+";
	keynames[SDLK_COMMA] = ",";
	keynames[SDLK_MINUS] = "-";
	keynames[SDLK_PERIOD] = ".";
	keynames[SDLK_SLASH] = "/";
	keynames[SDLK_0] = "0";
	keynames[SDLK_1] = "1";
	keynames[SDLK_2] = "2";
	keynames[SDLK_3] = "3";
	keynames[SDLK_4] = "4";
	keynames[SDLK_5] = "5";
	keynames[SDLK_6] = "6";
	keynames[SDLK_7] = "7";
	keynames[SDLK_8] = "8";
	keynames[SDLK_9] = "9";
	keynames[SDLK_COLON] = ":";
	keynames[SDLK_SEMICOLON] = ";";
	keynames[SDLK_LESS] = "<";
	keynames[SDLK_EQUALS] = "=";
	keynames[SDLK_GREATER] = ">";
	keynames[SDLK_QUESTION] = "?";
	keynames[SDLK_AT] = "@";
	keynames[SDLK_LEFTBRACKET] = "[";
	keynames[SDLK_BACKSLASH] = "\\";
	keynames[SDLK_RIGHTBRACKET] = "]";
	keynames[SDLK_CARET] = "^";
	keynames[SDLK_UNDERSCORE] = "_";
	keynames[SDLK_BACKQUOTE] = "`";
	keynames[SDLK_a] = "a";
	keynames[SDLK_b] = "b";
	keynames[SDLK_c] = "c";
	keynames[SDLK_d] = "d";
	keynames[SDLK_e] = "e";
	keynames[SDLK_f] = "f";
	keynames[SDLK_g] = "g";
	keynames[SDLK_h] = "h";
	keynames[SDLK_i] = "i";
	keynames[SDLK_j] = "j";
	keynames[SDLK_k] = "k";
	keynames[SDLK_l] = "l";
	keynames[SDLK_m] = "m";
	keynames[SDLK_n] = "n";
	keynames[SDLK_o] = "o";
	keynames[SDLK_p] = "p";
	keynames[SDLK_q] = "q";
	keynames[SDLK_r] = "r";
	keynames[SDLK_s] = "s";
	keynames[SDLK_t] = "t";
	keynames[SDLK_u] = "u";
	keynames[SDLK_v] = "v";
	keynames[SDLK_w] = "w";
	keynames[SDLK_x] = "x";
	keynames[SDLK_y] = "y";
	keynames[SDLK_z] = "z";
	keynames[SDLK_DELETE] = "delete";
	keynames[SDLK_KP0] = "[0]";
	keynames[SDLK_KP1] = "[1]";
	keynames[SDLK_KP2] = "[2]";
	keynames[SDLK_KP3] = "[3]";
	keynames[SDLK_KP4] = "[4]";
	keynames[SDLK_KP5] = "[5]";
	keynames[SDLK_KP6] = "[6]";
	keynames[SDLK_KP7] = "[7]";
	keynames[SDLK_KP8] = "[8]";
	keynames[SDLK_KP9] = "[9]";
	keynames[SDLK_KP_PERIOD] = "[.]";
	keynames[SDLK_KP_DIVIDE] = "[/]";
	keynames[SDLK_KP_MULTIPLY] = "[*]";
	keynames[SDLK_KP_MINUS] = "[-]";
	keynames[SDLK_KP_PLUS] = "[+]";
	keynames[SDLK_KP_ENTER] = "enter";
	keynames[SDLK_KP_EQUALS] = "equals";
	keynames[SDLK_UP] = "up";
	keynames[SDLK_DOWN] = "down";
	keynames[SDLK_RIGHT] = "right";
	keynames[SDLK_LEFT] = "left";
	keynames[SDLK_DOWN] = "down";
	keynames[SDLK_INSERT] = "insert";
	keynames[SDLK_HOME] = "home";
	keynames[SDLK_END] = "end";
	keynames[SDLK_PAGEUP] = "page up";
	keynames[SDLK_PAGEDOWN] = "page down";
	keynames[SDLK_F1] = "f1";
	keynames[SDLK_F2] = "f2";
	keynames[SDLK_F3] = "f3";
	keynames[SDLK_F4] = "f4";
	keynames[SDLK_F5] = "f5";
	keynames[SDLK_F6] = "f6";
	keynames[SDLK_F7] = "f7";
	keynames[SDLK_F8] = "f8";
	keynames[SDLK_F9] = "f9";
	keynames[SDLK_F10] = "f10";
	keynames[SDLK_F11] = "f11";
	keynames[SDLK_F12] = "f12";
	keynames[SDLK_F13] = "f13";
	keynames[SDLK_F14] = "f14";
	keynames[SDLK_F15] = "f15";
	keynames[SDLK_NUMLOCK] = "numlock";
	keynames[SDLK_CAPSLOCK] = "caps lock";
	keynames[SDLK_SCROLLOCK] = "scroll lock";
	keynames[SDLK_RSHIFT] = "right shift";
	keynames[SDLK_LSHIFT] = "left shift";
	keynames[SDLK_RCTRL] = "right ctrl";
	keynames[SDLK_LCTRL] = "left ctrl";
	keynames[SDLK_RALT] = "right alt";
	keynames[SDLK_LALT] = "left alt";
	keynames[SDLK_RMETA] = "right meta";
	keynames[SDLK_LMETA] = "left meta";
	keynames[SDLK_LSUPER] = "left super";
	keynames[SDLK_RSUPER] = "right super";
	keynames[SDLK_MODE] = "alt gr";
	keynames[SDLK_COMPOSE] = "compose";
	keynames[SDLK_HELP] = "help";
	keynames[SDLK_PRINT] = "print screen";
	keynames[SDLK_SYSREQ] = "sys req";
	keynames[SDLK_BREAK] = "break";
	keynames[SDLK_MENU] = "menu";
	keynames[SDLK_POWER] = "power";
	keynames[SDLK_EURO] = "euro";
	keynames[SDLK_UNDO] = "undo";
}

// Emscripten ships SDL2's declaration - const char*, SDL_Keycode - rather than
// 1.2's char*/SDLKey, so the definition has to match that one to override it.
const char* SDL_GetKeyName(SDL_Keycode key)
{
	const char* name;

	initKeyNames();
	name = (key >= 0 && key < SDLK_LAST) ? keynames[key] : NULL;
	return name ? name : "unknown key";
}

} // extern "C"

// --- SDL_PixelFormat completion ---------------------------------------------
// SDL.makeSurface (emsdk src/lib/libsdl.js:352) _malloc()s the SDL_PixelFormat
// and writes only 8 of its members: format, palette, BitsPerPixel,
// BytesPerPixel and the four masks (libsdl.js:385-393). Rloss..Aloss (byte
// offsets 28-31) and Rshift..Ashift (32-35) are never written and hold whatever
// the allocator's previous tenant left - 0/0/0/0 on a pristine heap, but
// measured as 171/171/171/171 and 120/120/120/120 once dlmalloc starts
// recycling blocks. Nothing in libsdl.js ever reads those fields, so completing
// them here cannot disturb the JS layer.
//
// Texture::getPixel (texture.cpp:281-284) is the source of every debris colour,
// via DebrisColorDB's alpha-weighted mean of a sprite's 16x16 cell. Read as 0,
// green comes out 256x and blue 65536x too large and both clamp, so debris is a
// cyan-white wash whose only variation is the red level. Read as garbage,
// wasm's i32.shr_u masks the shift count mod 32 (120 & 31 == 24) and debris
// comes out solid black.
//
// wasm-ld's --wrap fixes all five call sites - texture.cpp:66/181/227,
// engine.cpp:259, img_load.cpp:51 - with no game-code edit: references to
// SDL_CreateRGBSurface are redirected to __wrap_SDL_CreateRGBSurface, and
// __real_ back to the original, so the module still imports Emscripten's JS
// implementation and the surface stays registered in SDL.surfaces. A
// hand-rolled C++ replacement would break SDL_FreeSurface, which dereferences
// SDL.surfaces[surf] (libsdl.js:484).
//
// REQUIRES -Wl,--wrap=SDL_CreateRGBSurface on the link line of BOTH build.sh
// and build_asan.sh. Without it wasm-ld silently garbage-collects this function
// as unreferenced and the bug returns with no error and no warning.
extern "C" SDL_Surface* __real_SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                                    Uint32 rMask, Uint32 gMask, Uint32 bMask, Uint32 aMask);

static Uint8 maskShift(Uint32 mask)
{
    if (!mask) return 0;
    Uint8 shift = 0;
    while (!(mask & 1u)) { mask >>= 1; ++shift; }
    return shift;
}

static Uint8 maskLoss(Uint32 mask)
{
    if (!mask) return 8;               // real SDL leaves loss at 8 for an absent channel
    mask >>= maskShift(mask);
    Uint8 bits = 0;
    while (mask & 1u) { mask >>= 1; ++bits; }
    return bits >= 8 ? 0 : (Uint8)(8 - bits);
}

extern "C" SDL_Surface* __wrap_SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                                    Uint32 rMask, Uint32 gMask, Uint32 bMask, Uint32 aMask)
{
    SDL_Surface* p_surface = __real_SDL_CreateRGBSurface(flags, width, height, depth,
                                                         rMask, gMask, bMask, aMask);
    if (!p_surface || !p_surface->format) return p_surface;

    // The game always passes 0x000000ff/0x0000ff00/0x00ff0000/0xff000000,
    // so this yields shifts 0/8/16/24 and losses 0/0/0/0.
    SDL_PixelFormat* p_format = p_surface->format;
    p_format->Rshift = maskShift(p_format->Rmask);
    p_format->Gshift = maskShift(p_format->Gmask);
    p_format->Bshift = maskShift(p_format->Bmask);
    p_format->Ashift = maskShift(p_format->Amask);
    p_format->Rloss  = maskLoss(p_format->Rmask);
    p_format->Gloss  = maskLoss(p_format->Gmask);
    p_format->Bloss  = maskLoss(p_format->Bmask);
    p_format->Aloss  = maskLoss(p_format->Amask);
    p_format->refcount = 1;            // makeSurface leaves these two uninitialised
    p_format->next     = 0;
    return p_surface;
}
