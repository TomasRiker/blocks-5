// platform_stubs.cpp - Symbole, die der Web-Build braucht, aber nicht bekommt.
#include <SDL.h>
#include <cstddef>
#include <cstring>
#include <algorithm>

extern "C" {

// --- SDL-1.2-Mauszeiger ---------------------------------------------------------
// Emscriptens SDL deklariert diese Funktionen, setzt aber keine davon um: der
// Zeiger gehoert dem Browser. Was Engine::setupCursor baut, wird hier angenommen
// und verworfen. Den Zeiger, den die GUI selbst zeichnet, betrifft das nicht.
SDL_Cursor* SDL_CreateCursor(const Uint8*, const Uint8*, int, int, int, int) { return NULL; }
void        SDL_SetCursor(SDL_Cursor*)  {}
SDL_Cursor* SDL_GetCursor(void)         { return NULL; }
void        SDL_FreeCursor(SDL_Cursor*) {}

// --- Sperren von SDL-Flaechen ---------------------------------------------------
// Emscripten setzt SDL_UnlockSurface als `assert(!SDL.GL)` um und verweigert damit
// genau den Modus, den dieses Spiel als einzigen benutzt. Jede hier gesperrte
// Flaeche ist eine selbst angelegte SDL_SWSURFACE, deren `pixels` immer gueltig
// ist - das Sperren ist also eine Formsache, die auch das echte SDL leer laesst.
// Hier definiert, loest wasm-ld gegen diese Fassungen auf und zieht die
// JS-Fassungen nie herein.
int  SDL_LockSurface(SDL_Surface*)   { return 0; }
void SDL_UnlockSurface(SDL_Surface*) {}

// --- Kopieren von Flaechen ------------------------------------------------------
// Emscripten setzt SDL_BlitSurface auf einer 2D-Leinwand um: drawImage von der
// Quellleinwand, dann zurueckgelesen in den Pixelpuffer. Das geht nur fuer
// Flaechen, die die SDL-Schicht selbst aus einem Bild angelegt hat, denn nur die
// haben eine Leinwand dahinter.
//
// Jede Flaeche, die dieses Spiel kopiert, wurde geradewegs im Speicher
// geschrieben - von stb_image in img_load.cpp oder von SDL_CreateRGBSurface -,
// die Quellleinwand ist also leer und das Kopieren tut stillschweigend nichts.
// Jede Textur kaeme voellig durchsichtig an, was aussieht wie "Texturen gehen
// nicht".
//
// Alle vier Aufrufstellen kopieren 32-Bit-RGBA nach 32-Bit-RGBA und schalten
// vorher mit SDL_SetAlpha(s, 0, 0) die Flaechendeckkraft ab, was in SDL 1.2
// "nicht mischen, Pixel kopieren" heisst. Ein einfaches zeilenweises Kopieren
// ist damit die richtige Bedeutung und keine Naeherung davon.
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


// --- SDL_GetKeyName -------------------------------------------------------------
// Emscriptens eigenes antwortet nur fuer a-z und 0-9 und liefert sonst eine leere
// Zeichenkette. Keine der Vorgabebelegungen - Pfeile, Shift, Tab, Return, F5 - ist
// ein Buchstabe oder eine Ziffer, es traf also alle.
//
// Die Tabelle ist die von SDL 1.2.15 selbst, aus
// libs/SDL-1.2.15/src/events/SDL_keyboard.c, das dieser Baum mitliefert und der
// Windows-Build uebersetzt - beide Builds benennen eine Taste also gleich. Die
// Eintraege SDLK_WORLD_0..95 fehlen, weil Emscripten SDL2-Koepfe mit einer
// 1.2-Ausgleichsschicht mitbringt und sie nicht deklariert; es sind die
// Latin-1-Totentasten, die hier keine Belegung benutzt.
//
// Am Text haengt nichts: die config.xml speichert Kennungen, nie Namen. Das hier
// ist, was der Optionsdialog anzeigt, und sonst nichts.
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

// Emscripten liefert die SDL2-Deklaration - const char*, SDL_Keycode - und nicht
// die von 1.2, die Definition muss dieser also entsprechen.
const char* SDL_GetKeyName(SDL_Keycode key)
{
	const char* name;

	initKeyNames();
	name = (key >= 0 && key < SDLK_LAST) ? keynames[key] : NULL;
	return name ? name : "unknown key";
}

} // extern "C"

// --- SDL_PixelFormat vervollstaendigen ------------------------------------------
// SDL.makeSurface aus libsdl.js legt das SDL_PixelFormat an und schreibt nur acht
// seiner Felder: format, palette, BitsPerPixel, BytesPerPixel und die vier Masken.
// Rloss..Aloss und Rshift..Ashift bleiben ungeschrieben und halten, was der
// Vormieter des Speicherblocks hinterlassen hat - null auf einem frischen Heap,
// gemessen aber 171 und 120, sobald dlmalloc anfaengt, Bloecke wiederzuverwenden.
// libsdl.js liest diese Felder nie, sie hier zu fuellen kann die JS-Schicht also
// nicht stoeren.
//
// Texture::getPixel ist die Quelle jeder Truemmerfarbe. Als null gelesen, kommt
// Gruen 256-fach und Blau 65536-fach zu gross heraus und beides klemmt - die
// Truemmer waeren ein cyanweisser Schleier. Als Muell gelesen, maskiert wasm die
// Schiebeweite modulo 32, und die Truemmer kommen schwarz heraus.
//
// --wrap von wasm-ld erledigt alle fuenf Aufrufstellen ohne eine Aenderung am
// Spielcode: Verweise auf SDL_CreateRGBSurface gehen an
// __wrap_SDL_CreateRGBSurface, __real_ zurueck ans Original. Das Modul zieht
// also weiterhin Emscriptens JS-Umsetzung, und die Flaeche bleibt in SDL.surfaces
// angemeldet - ein handgeschriebener Ersatz braeche SDL_FreeSurface, das dort
// nachschlaegt.
//
// BRAUCHT -Wl,--wrap=SDL_CreateRGBSurface auf der Linkzeile von build.sh und
// build_asan.sh. Ohne das sammelt wasm-ld diese Funktion stillschweigend als
// unbenutzt ein, und der Fehler ist ohne Warnung wieder da.
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

    // Das Spiel uebergibt immer 0x000000ff/0x0000ff00/0x00ff0000/0xff000000,
    // daraus werden die Schiebeweiten 0/8/16/24 und die Verluste 0/0/0/0.
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
