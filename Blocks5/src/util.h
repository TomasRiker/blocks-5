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

// A colour on its way into a vertex array, cut off where GL would have cut it
// off. The game hands out colours above 1 on purpose and lets the hardware do
// it: Level::renderShine takes deathCountDown * 5.0 from an exploding bomb, the
// teleport swirl ramps its red to 2.1 and the two spark bursts to 1.5. Desktop
// GL clamps a primitive colour before it multiplies the texel; Emscripten's
// emulation clamps only gl_FragColor, after the multiply, so an over-bright
// colour eats the texture's falloff and a soft glow comes out a hard-edged
// blob. Hence the browser build alone, and hence a function rather than a clamp
// written out at each site - see ROADMAP item 42 for how to stop paying for it
// there as well.
//
// The two callers are the only places a colour reaches GL without being cut off
// on the way: a colour array. Every other path either sets glColor inside a
// glBegin block, which the emulation quantises to a byte and so clamps, or
// carries a value that cannot leave [0,1] to begin with.
#ifdef __EMSCRIPTEN__
template<typename T, int DIM> Vec<T, DIM> clampColor(const Vec<T, DIM>& color)
{
	Vec<T, DIM> out;
	for(int i = 0; i < DIM; i++)
		out.value[i] = clamp(color.value[i], static_cast<T>(0), static_cast<T>(1));
	return out;
}
#else
template<typename T, int DIM> const Vec<T, DIM>& clampColor(const Vec<T, DIM>& color)
{
	return color;
}
#endif

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
// markers < > [ ], "..", a leading dot or tilde, control characters. Umlauts
// and spaces stay allowed, or legitimate names would silently vanish.
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
double random(double min, double max);
Vec2i numberToDir(int dir);
void generatePrimes(uint* p_out, uint maxNum);
uint fromBase62(const char* p_in);
void decryptPassword(const char* p_in, char* p_out, const uint* p_primes);
void clearLog();
void printfLog(const char* p_format, ...);
std::string localizeString(const std::string& text);
std::string loadString(const std::string& id);
std::vector<Vec2i> bresenham(const Vec2i& p1, const Vec2i& p2);
double getExactTime();
uint getExactTimeMS();

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
// Open an address in the web browser. It exists only here: Windows takes a
// .url shortcut next to the application, the same one that sits in the start
// menu, and the browser opens a second tab - both can do it already, and both
// do it where they stand.
void openURL(const std::string& url);
#endif
void writeProfileLine(const std::string& name, double dt, double avgTime);

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

// Reduces a scrolling texture offset, in texels, to one period of the texture.
// Exact under GL_REPEAT: whole periods move the finished coordinate by a whole
// number and it samples the same texel. It is needed because the weather and
// the title clouds scroll by an offset that has been growing since the level
// began, and a texture coordinate reaches the fragment shader as a varying -
// which Emscripten's GL emulation declares under "precision mediump float",
// ten mantissa bits on a phone. The step it quantizes to is offset/2048 texels,
// so the drift turns visibly steppy about a minute in and gets worse from
// there. Keeping the offset inside one period keeps the precision constant.
inline double wrapTextureOffset(double offset, int period)
{
	if(period <= 0) return offset;
	return fmod(offset, static_cast<double>(period));
}

extern bool writingCrashLog;

#define BEGIN_PROFILE(NAME) \
	static double profile_accumTime_##NAME = 0.0; \
	static uint profile_numMeasurements_##NAME = 0; \
	const double profile_t0_##NAME = getExactTime();

#define END_PROFILE(NAME) \
	{ \
		const double profile_dt_##NAME = getExactTime() - profile_t0_##NAME; \
		profile_accumTime_##NAME += profile_dt_##NAME; \
		++profile_numMeasurements_##NAME; \
		writeProfileLine(#NAME, profile_dt_##NAME, profile_accumTime_##NAME / profile_numMeasurements_##NAME); \
	}

#endif