#ifndef _UTIL_H
#define _UTIL_H

template<typename T> const T& min(const T& a, const T& b)
{
	return a < b ? a : b;
}

template<typename T> const T& max(const T& a, const T& b)
{
	return a > b ? a : b;
}

template<typename T> const T& clamp(const T& value,
									const T& minValue,
									const T& maxValue)
{
	if(value < minValue) return minValue;
	else if(value > maxValue) return maxValue;
	else return value;
}

int nextPow2(int x);
std::string getFilenameExtension(const std::string& filename);
std::string setFilenameExtension(const std::string& filename, const std::string& extension);

// "01 - Title", as the game and the level select show it. The title comes
// unexamined from a level file or an input field and can be any length, so
// only the number goes through a fixed buffer.
std::string formatLevelCaption(int number, const std::string& title);

// A single level shows its filename instead of a number: three levels all
// called "Unnamed" are otherwise indistinguishable. In a campaign the
// filename would say nothing, since every level there is "level_N.xml".
std::string formatSingleLevelCaption(const std::string& title, const std::string& filename);

// Turns any filename - including one imported from outside - into a safe name
// stem: the base name only, only [A-Za-z0-9_-], no dots, never empty, at most
// 64 characters. The result can never escape its directory; the caller decides
// the extension.
std::string sanitizeFilenameStem(const std::string& untrusted,
                                 const std::string& fallback = "imported");

// Whether a string may be used unchanged as a filename or archive member
// name. Refused: anything that could redirect FileSystem::convertPath or
// evalRelativePath (separators, the drive colon, the archive markers < > [ ],
// "..", a leading dot or tilde, control characters), the Windows reserves
// " | ? *, and more than 100 characters. Umlauts and spaces pass, or
// legitimate names would silently vanish.
bool isSafeMemberName(const std::string& name);

// Case-insensitive comparison over ASCII only, by hand: _stricmp is MSVC's
// alone, and strcasecmp and tolower follow the locale, in which the Turkish
// 'I' is not the upper case of 'i'. Filenames, command-line switches and
// upscaler names are the same in every locale.
bool equalsNoCase(const char* p_a, const char* p_b);
int randomInt();
int random(int min, int max);
float random(float min, float max);

// Seeds the generator the three functions above draw from. Only a test-hooks
// build calls it, where B5_SEED asks, so that a frame is byte-reproducible
// (LinuxBuild/test/frames.sh); a shipped game keeps MTRand's own seeding.
void seedRandom(uint seed);
Vec2i numberToDir(int dir);
// A value that goes round - a direction, a rail's shape - reduced to 0 to
// count - 1, as the editor's rotation reduces it. For what a level file
// brings, which may be anybody's: a negative value makes "% count" negative
// too, and then it matches no case.
inline int wrapIndex(int value, int count) { return ((value % count) + count) % count; }
void generatePrimes(uint* p_out, uint maxNum);
// Reads 7 base-62 digits; the caller has checked they are there.
uint fromBase62(const char* p_in);
// The letter on the key's label, 'a' to 'z', or 0 where it is no letter. The
// keysym cannot always say it: SDL 1.2 on Windows maps keys through the US
// layout, so a German Z arrives as SDLK_y. Where SDL translates the key,
// unicode carries the layout's answer (under Ctrl the control code on
// Windows, Ctrl+Z as 26, and the letter itself under X11); where it does not,
// as for a Ctrl combination in the browser, the keysym follows the layout.
char keyLetter(const SDL_keysym& keysym);
// The character a key press types into a text box, or 0. Printable Latin-1
// only, which is what the font draws, and nothing under Ctrl without Alt:
// X11 hands Ctrl+S over with the s as its unicode, while Ctrl with Alt is
// how Windows reports AltGr, which types the @ and the braces.
char typedCharacter(const SDL_keysym& keysym);
bool decryptPassword(const std::string& in, std::string& out, const uint* p_primes);
void clearLog();
void printfLog(const char* p_format, ...);
std::string localizeString(const std::string& text);
std::string loadString(const std::string& id);
std::vector<Vec2i> bresenham(const Vec2i& p1, const Vec2i& p2);

// Microseconds since the first call (since the page loaded, in the browser),
// as an integer: only differences are read off it, and a count loses no
// precision as it grows, where a float of seconds steps by 244 us an hour in
// and 7.8 ms after a day. 2^64 us is 584 thousand years.
uint64 getExactTimeUS();
uint getExactTimeMS();

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
// Opens an address in the web browser. Linux only: Windows opens the .url
// shortcut beside the program, and the browser build opens a tab itself.
void openURL(const std::string& url);
#endif
void writeProfileLine(const std::string& name, float dt, float avgTime);

// Either Enter key, the main one or the keypad's: nothing in the game tells
// them apart. The named actions get both through their two bindings; this is
// for code that reads the SDL key itself.
inline bool isReturnKey(int key)
{
	return key == SDLK_RETURN || key == SDLK_KP_ENTER;
}

// Animation runs on rate * clock + base, a line in an exact integer counter:
// Level::time, GS_Menu::time and SDL_GetTicks in milliseconds, Lava::anim in
// ticks. clockPhase and scrollOffset form it in float, which is enough:
// measured, a scroll of one texel a tick still steps exactly 1.0 after twelve
// hours in one level, and Level::time starts again with every level.

// The argument of an animation's sine or cosine, deliberately NOT reduced to
// a turn. sinf and cosf reduce against the exact pi and stay 3e-08 from the
// true sine at any clock this game reaches; a fmodf by a float 2*pi, itself
// 1.7e-07 off, errs by 2.0e-03 an hour into a level, and more with every
// turn it throws away.
inline float clockPhase(uint ticks, float perTick, float base)
{
	return perTick * static_cast<float>(ticks) + base;
}

// The same line reduced to one period, for a quantity whose period is not a
// turn: a scrolling texture offset in texels, or the CRT filter's flicker
// (eight seconds) and crawl (one). Exact for a repeating texture, since whole
// periods sample the same texel, and needed there: a phone without highp
// gives the fragment shader a mediump varying, ten mantissa bits, and an
// unreduced offset turns visibly steppy within a minute.
inline float scrollOffset(uint ticks, float perTick, float base, float period)
{
	const float value = perTick * static_cast<float>(ticks) + base;
	if(period <= 0.0f) return value;
	return fmodf(value, period);
}

// The same reduction for an offset that is already small: what scrollOffset
// returned, plus a wobble bounded by its own sine. Whole periods are still
// whole periods, and nothing here has grown.
inline float wrapTextureOffset(float offset, int period)
{
	if(period <= 0) return offset;
	return fmodf(offset, static_cast<float>(period));
}

// Whether a float is a number and not a NaN or an infinity, read off its
// bits: the Windows release build compiles /fp:fast, under which a comparison
// with a NaN need not come out false. For values out of a file.
inline bool isFiniteFloat(float value)
{
	uint bits;
	memcpy(&bits, &value, sizeof(bits));
	return (bits & 0x7F800000u) != 0x7F800000u;
}

// To the nearest whole pixel, both signs alike. Adding 0.5 and converting is
// right only above zero, since the conversion cuts towards zero, and a
// symmetric movement would come out a pixel short on one side.
inline Vec2i roundToVec2i(const Vec2f& v)
{
	return Vec2i(static_cast<int>(floor(v.x + 0.5f)),
				 static_cast<int>(floor(v.y + 0.5f)));
}

extern bool writingCrashLog;

#define BEGIN_PROFILE(NAME) \
	static float profile_accumTime_##NAME = 0.0f; \
	static uint profile_numMeasurements_##NAME = 0; \
	const uint64 profile_t0_##NAME = getExactTimeUS();

#define END_PROFILE(NAME) \
	{ \
		const float profile_dt_##NAME = 1.0e-6f * static_cast<float>(getExactTimeUS() - profile_t0_##NAME); \
		profile_accumTime_##NAME += profile_dt_##NAME; \
		++profile_numMeasurements_##NAME; \
		writeProfileLine(#NAME, profile_dt_##NAME, profile_accumTime_##NAME / static_cast<float>(profile_numMeasurements_##NAME)); \
	}

#endif