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

// "01 - Title", the way it stands in the game and in the level select above
// the picture. The title comes out of the level file unexamined (level.cpp
// reads the attribute exactly as it stands) and out of an input field with no
// length limit - it is therefore arbitrarily long and must never run through a
// fixed buffer. Only the number goes through one here, and that is a number.
std::string formatLevelCaption(int number, const std::string& title);

// A single level carries its filename instead of the number: three levels
// somebody sent you that are all called "Unnamed" are otherwise
// indistinguishable. Inside a campaign it would be pointless - there every
// other one would be called "level_2.xml".
std::string formatSingleLevelCaption(const std::string& title, const std::string& filename);

// Turns any filename - including one imported from outside - into a safe name
// stem: the base name only, only [A-Za-z0-9_-], no dots, never empty, at most
// 64 characters. The result can never escape its directory; the caller decides
// the extension.
std::string sanitizeFilenameStem(const std::string& untrusted,
                                 const std::string& fallback = "imported");

// Checks whether a string may be used unchanged as a filename or as the name
// of an archive member. Anything that could redirect FileSystem::convertPath
// or evalRelativePath is refused - separators, the drive colon, the archive
// markers < > [ ], "..", a leading dot or tilde, control characters - and so
// are the four Windows reserves beside them, " | ? *, and anything longer than
// a hundred characters. Umlauts and spaces stay allowed, or legitimate names
// would silently vanish.
bool isSafeMemberName(const std::string& name);

// Case-insensitive comparison, over ASCII only. By hand and neither through
// _stricmp - only MSVC knows that one - nor through strcasecmp or tolower:
// those two hang off the configured locale, and in the Turkish one 'I' is not
// the upper-case form of 'i'. What is compared here are filenames and
// command-line switches, and those are the same in every locale.
bool equalsNoCase(const char* p_a, const char* p_b);
int randomInt();
int random(int min, int max);
float random(float min, float max);

// Seed the one generator all four of those draw from. Only the test build
// calls it, and only where B5_SEED asks: a shipped game wants MTRand's own
// seeding from the clock. It is here so that a frame can be made
// byte-reproducible, which is what lets a change that should move no pixel be
// proved rather than asserted - see LinuxBuild/test/frames.sh.
void seedRandom(uint seed);
Vec2i numberToDir(int dir);
void generatePrimes(uint* p_out, uint maxNum);
// Reads 7 base-62 digits; the caller has checked they are there.
uint fromBase62(const char* p_in);
bool decryptPassword(const std::string& in, std::string& out, const uint* p_primes);
void clearLog();
void printfLog(const char* p_format, ...);
std::string localizeString(const std::string& text);
std::string loadString(const std::string& id);
std::vector<Vec2i> bresenham(const Vec2i& p1, const Vec2i& p2);

// Microseconds since the first call, as an integer. The value keeps growing
// for as long as the session lasts while what is read off it is the
// difference of two readings, a few thousand of these - and a count cannot
// drift at all, where a float of seconds steps by 244 us an hour in and
// 7.8 ms after a day, more than a whole frame, and a double only pushes that
// out instead of removing it. 2^64 microseconds is 584 thousand years.
uint64 getExactTimeUS();
uint getExactTimeMS();

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
// Open an address in the web browser. It exists only here: Windows takes a
// .url shortcut next to the application, the same one that sits in the start
// menu, and the browser opens a second tab - both can do it already, and both
// do it where they stand.
void openURL(const std::string& url);
#endif
void writeProfileLine(const std::string& name, float dt, float avgTime);

// The two keys a player means by "Enter": the big one and the one on the
// numeric keypad. A keyboard has two and nothing in this game distinguishes
// them, so everything that answers to one has to answer to the other -
// confirming a dialog, playing the selected level, leaving the credits,
// toggling the fullscreen with Alt. The named actions get this for free,
// since a binding has a primary and a secondary; these are the places that
// read the SDL key itself.
inline bool isReturnKey(int key)
{
	return key == SDLK_RETURN || key == SDLK_KP_ENTER;
}

// Everything the game animates by is rate * clock + base, a straight line in a
// counter that is an exact integer - Level::time and GS_Menu::time in
// milliseconds, Lava::anim and SDL_GetTicks in ticks. The three below form
// that line; only two of them reduce it, and which is which is the whole
// point.
//
// All of it is float, and the limit that puts on it was measured rather than
// guessed. The clouds scroll one texel a tick: that step comes out exactly
// 1.0000 for as long as twelve hours in a single level, is 0.5 to 1.5 after a
// day and collapses to a stutter of 0 to 2 after three. Level::time starts
// again at every level, and nobody plays one for a day - so the float costs
// nothing a player can reach, and a double here would buy only a number that
// looks tidier in a probe.
// The argument of an animation's sine or cosine: rate * ticks + base, and
// deliberately NOT reduced to a turn on the way out.
//
// Reducing it here is the obvious thing and it is wrong. sinf and cosf do
// their own argument reduction, against the real pi to as many bits as it
// takes, and come out 3e-08 from the true sine at every clock value this game
// can reach. A fmodf by a float 2*pi beforehand reduces against a constant
// that is itself 1.7e-07 off, and the error grows with the number of turns
// thrown away: measured against the exact sine of the same float, 3.9e-05 one
// minute into a level, 2.0e-03 after an hour, 0.14 after three days. The
// library is better at this than the caller, so let it do it.
//
// What a float does cost here is the product: it goes coarse when the clock
// gets big, which no reduction afterwards can undo. That is a twelve-hour
// problem in a single level - see scrollOffset - and Level::time starts again
// at every level.
inline float clockPhase(uint ticks, float perTick, float base)
{
	return perTick * static_cast<float>(ticks) + base;
}

// The same for a quantity whose period is not a turn: a scrolling texture
// offset in texels, or the CRT filter's flicker, which repeats every eight
// seconds and whose crawl repeats every one. Exact for a texture under
// GL_REPEAT, since whole periods move the finished coordinate by a whole
// number and it samples the same texel. It is needed there because a texture
// coordinate reaches the fragment shader as a varying at that shader's float
// precision - highp where the browser has it, mediump on a phone that has not,
// ten mantissa bits. The step it quantizes to is offset/2048 texels, so the
// drift turns visibly steppy about a minute in and gets worse from there.
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

// To the nearest whole pixel, both signs alike. A plain conversion to Vec2i
// cuts towards zero, so adding 0.5 first - the obvious rounding - rounds up
// above zero and down below it, and a symmetric movement comes out a pixel
// short on one side.
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