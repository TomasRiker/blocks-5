#include "pch.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#elif !defined(_WIN32)
#include <ctime>
#endif
#include "util.h"
#include "engine.h"
#include "filesystem.h"

MTRand mt;
bool writingCrashLog = false;

bool equalsNoCase(const char* p_a, const char* p_b)
{
	for(; *p_a && *p_b; p_a++, p_b++)
	{
		char x = *p_a, y = *p_b;
		if(x >= 'A' && x <= 'Z') x += 'a' - 'A';
		if(y >= 'A' && y <= 'Z') y += 'a' - 'A';
		if(x != y) return false;
	}
	return !*p_a && !*p_b;
}

int nextPow2(int x)
{
	--x;

	int n = 0;
	while(x)
	{
		x >>= 1;
		n++;
	}

	return 1 << n;
}

std::string getFilenameExtension(const std::string& filename)
{
	// look for the last dot
	size_t index = filename.find_last_of('.');
	if(index == std::string::npos)
	{
		// No extension!
		return "";
	}
	else
	{
		return std::string(filename.begin() + index + 1, filename.end());
	}
}

std::string formatLevelCaption(int number,
							   const std::string& title)
{
	char prefix[16] = "";
	sprintf(prefix, "%02d - ", number);
	return prefix + title;
}

std::string formatSingleLevelCaption(const std::string& title,
									 const std::string& filename)
{
	if(filename.empty()) return title;
	return title + " (" + filename + ")";
}

std::string setFilenameExtension(const std::string& filename,
								 const std::string& extension)
{
	// look for the last dot
	size_t index = filename.find_last_of('.');
	if(index == std::string::npos)
	{
		// No extension!
		return filename + "." + extension;
	}
	else
	{
		// replace the existing extension
		return std::string(filename.begin(), filename.begin() + index) + "." + extension;
	}
}

int randomInt()
{
	return mt.randInt(0x7FFFFFFF);
}

std::string sanitizeFilenameStem(const std::string& untrusted,
								 const std::string& fallback)
{
	// look at the base name only
	std::string name(untrusted);
	const size_t cut = name.find_last_of("/\\:");
	if(cut != std::string::npos) name = name.substr(cut + 1);

	// cut the extension off - the caller decides that, not the file
	const size_t dot = name.find_last_of('.');
	if(dot != std::string::npos) name = name.substr(0, dot);

	std::string result;
	for(size_t i = 0; i < name.length() && result.length() < 64; i++)
	{
		const char c = name[i];
		const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
						(c >= '0' && c <= '9') || c == '_' || c == '-';
		result += ok ? c : '_';
	}

	while(!result.empty() && result[0] == '_') result.erase(0, 1);
	while(!result.empty() && result[result.length() - 1] == '_') result.resize(result.length() - 1);

	return result.empty() ? fallback : result;
}

void seedRandom(uint seed)
{
	mt.seed(seed);
}

int random(int min,
		   int max)
{
	// MTRand::randInt() takes a uint32, where a negative max - min would
	// become four billion and the result would leave [min, max].
	if(max <= min) return min;
	return min + mt.randInt(max - min);
}

float random(float min,
			 float max)
{
	return min + static_cast<float>(mt.rand(max - min));
}

Vec2i numberToDir(int dir)
{
	switch(dir % 4)
	{
	case 0:
		return Vec2i(0, -1);
	case 1:
		return Vec2i(1, 0);
	case 2:
		return Vec2i(0, 1);
	case 3:
		return Vec2i(-1, 0);
	}

	return Vec2i(0, 0);
}

void generatePrimes(uint* p_out,
					uint maxNum)
{
	if(!maxNum) return;

	p_out[0] = 2;

	uint n = 3;
	uint numPrimes = 1;
	while(numPrimes < maxNum)
	{
		// Is n a prime number?
		bool isPrime = true;
		for(uint i = 0; i < numPrimes; i++)
		{
			if(!(n % p_out[i]))
			{
				isPrime = false;
				break;
			}
		}

		if(isPrime)
		{
			p_out[numPrimes] = n;
			numPrimes++;
		}

		n++;
	}
}

uint fromBase62(const char* p_in)
{
	const uint base[7] = {916132832, 14776336, 238328, 63844, 3844, 62, 1};
	uint n = 0;
	for(uint i = 0; i < 7; i++)
	{
		// decoding
		if(p_in[i] >= 'a') n += (36 + p_in[i] - 'a') * base[i];
		else if(p_in[i] >= 'A') n += (10 + p_in[i] - 'A') * base[i];
		else n += (p_in[i] - '0') * base[i];
	}

	return n;
}

char keyLetter(const SDL_keysym& keysym)
{
	const uint u = keysym.unicode;

	// A control code counts only from a letter key: Tab, Backspace and Return
	// send codes in the same range - 9, 8 and 13 - without being letters.
	const bool letterKey = keysym.sym >= SDLK_a && keysym.sym <= SDLK_z;
	if(u >= 1 && u <= 26) return letterKey ? static_cast<char>('a' + u - 1) : 0;
	if(u >= 'a' && u <= 'z') return static_cast<char>(u);
	if(u >= 'A' && u <= 'Z') return static_cast<char>(u - 'A' + 'a');

	// the layout made something else of the key: no letter
	if(u) return 0;

	return letterKey ? static_cast<char>(keysym.sym) : 0;
}

namespace
{
	bool isBase62Digit(char c)
	{
		return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
	}
}

bool decryptPassword(const std::string& in,
					 std::string& out,
					 const uint* p_primes)
{
	// The text between an archive path's brackets, as trustworthy as the file
	// it came from: a skin carries its own as password.txt, which
	// Level::getSkinFilename reads. Only whole blocks of seven base-62 digits
	// holding complete records are a password at all.
	out.clear();
	if(in.empty() || in.length() % 7) return false;
	for(size_t i = 0; i < in.length(); i++)
	{
		if(!isBase62Digit(in[i])) return false;
	}

	// Seven digits make one 32-bit number, xored with a pattern that moves
	// with the block. Its bytes come lowest first: PWEncrypt reads the four
	// as one number straight out of memory, on a little-endian machine.
	std::vector<unsigned char> bytes;
	bytes.reserve(in.length() / 7 * 4);
	for(uint i = 0, shift = 0; i < in.length(); i += 7, shift++)
	{
		uint n = fromBase62(&in[i]);
		n ^= (0x958B47A6 << (shift % 31)) ^ (0x8D4BA2D4 >> (shift % 17));
		for(uint b = 0; b < 4; b++) bytes.push_back(static_cast<unsigned char>(n >> (8 * b)));
	}

	// One record per letter: a count, then that many pairs of a prime's index
	// and its power, and a count of zero at the end. The index is a byte and
	// p_primes holds 256, so it cannot leave the table. PWEncrypt cannot write
	// more than 85 letters, and 255 is where a made-up password stops, since
	// each letter can ask for 65025 multiplications.
	size_t at = 0;
	for(uint letter = 0; ; letter++)
	{
		if(at >= bytes.size()) break;
		const uint numTerms = bytes[at++] ^ 0xB6;
		if(!numTerms) return true;
		if(letter >= 255 || bytes.size() - at < 2 * static_cast<size_t>(numTerms)) break;

		uint c = 1;
		for(uint t = 0; t < numTerms; t++)
		{
			const uint prime = bytes[at++] ^ 0x4D;
			const uint power = bytes[at++] ^ 0xE9;
			for(uint j = 0; j < power; j++) c *= p_primes[prime];
		}

		c -= letter * 7;
		out += static_cast<char>(c);
	}

	out.clear();
	return false;
}

bool isSafeMemberName(const std::string& name)
{
	if(name.empty() || name.length() > 100) return false;
	if(name[0] == '.' || name[0] == '~') return false;
	if(name.find("..") != std::string::npos) return false;

	for(size_t i = 0; i < name.length(); i++)
	{
		// Unsigned: these names come out of ISO-8859-1 XML, and as a signed
		// char every umlaut would be negative and fail the control test.
		const unsigned char c = static_cast<unsigned char>(name[i]);
		if(c < 0x20 || c == 0x7F) return false;
		if(strchr("/\\:<>[]\"|?*", c)) return false;
	}

	return true;
}

void clearLog()
{
	const std::string logFilename(FileSystem::inst().getAppHomeDirectory() + "log.txt");
	FILE* p_file = fopen(logFilename.c_str(), "wt");
	if(p_file) fclose(p_file);
}

void printfLog(const char* p_format,
			   ...)
{
	static char text[1024];
	text[0] = 0;
	va_list vaList;

	// build the parameter list and the string
	va_start(vaList, p_format);
	vsnprintf(text, sizeof(text), p_format, vaList);
	va_end(vaList);

	char datetime[32];
	time_t t;
	time(&t);
	strftime(datetime, 32, "%H:%M:%S", localtime(&t));
	std::string finalLogText(std::string(datetime) + " // " + text);

	// Flushed at once: stdout is a file or a pipe here (SDL 1.2 points it at
	// stdout.txt under Windows, the harnesses redirect it), so it is block
	// buffered, and a run that is killed would lose the lines saying what it
	// was doing. Not setvbuf(stdout, 0, _IOLBF, 0) at startup: the MSVC
	// runtime ends the process over a size of 0, and SDL has written to the
	// stream before main() anyway.
	printf("%s", finalLogText.c_str());
	fflush(stdout);
	const std::string logFilename(FileSystem::inst().getAppHomeDirectory() + "log.txt");
	FILE* p_file = fopen(logFilename.c_str(), "at");
	if(p_file)
	{
		fprintf(p_file, "%s", finalLogText.c_str());
#ifdef _DEBUG
		fflush(p_file);
#endif
		fclose(p_file);
	}

	if(writingCrashLog)
	{
		const std::string logFilename(FileSystem::inst().getAppHomeDirectory() + "crash_log.txt");
		FILE* p_file = fopen(logFilename.c_str(), "at");
		if(p_file)
		{
			fprintf(p_file, "%s", finalLogText.c_str());
#ifdef _DEBUG
			fflush(p_file);
#endif
			fclose(p_file);
		}
	}
}

std::string localizeString(const std::string& text)
{
	return Engine::inst().localizeString(text);
}

std::string loadString(const std::string& id)
{
	return Engine::inst().loadString(id);
}

std::vector<Vec2i> bresenham(const Vec2i& p1,
							 const Vec2i& p2)
{
	std::vector<Vec2i> points;
	Vec2i c = p1;

	int dx = abs(p1.x - p2.x);
	int dy = abs(p1.y - p2.y);
	int ix = p1.x < p2.x ? 1 : -1;
	int iy = p1.y < p2.y ? 1 : -1;

	if(dx > dy)
	{
		int dpr = dy << 1;
		int dpu = dpr - (dx << 1);
		int p = dpr - dx;

		for(; dx >= 0; dx--)
		{
			points.push_back(c);

			if(p > 0)
			{
				c.x += ix;
				c.y += iy;
				p += dpu;
			}
			else
			{
				c.x += ix;
				p += dpr;
			}
		}
	}
	else
	{
		int dpr = dx << 1;
		int dpu = dpr - (dy << 1);
		int p =	dpr - dy;

		for(; dy >= 0; dy--)
		{
			points.push_back(c);

			if(p > 0)
			{
				c.x += ix;
				c.y += iy;
				p += dpu;
			}
			else
			{
				c.y += iy;
				p += dpr;
			}
		}
	}

	return points;
}

uint64 getExactTimeUS()
{
#ifdef _WIN32
	static bool initialized = false;
	static LARGE_INTEGER startTime;
	static LONGLONG frequency;

	if(!initialized)
	{
		LARGE_INTEGER f;
		QueryPerformanceFrequency(&f);
		frequency = f.QuadPart;
		QueryPerformanceCounter(&startTime);
		initialized = true;
	}

	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	const LONGLONG ticks = t.QuadPart - startTime.QuadPart;
	// Whole seconds and remainder apart: at the 10 MHz current Windows counts
	// in, ticks * 1000000 overflows a signed 64-bit integer after about eleven
	// days, and the remainder, below one second, cannot.
	return static_cast<uint64>(ticks / frequency) * 1000000
	     + static_cast<uint64>((ticks % frequency) * 1000000 / frequency);
#elif defined(__EMSCRIPTEN__)
	// The one clock that passes through floating point, because
	// emscripten_get_now() is a JavaScript number: a double, in milliseconds.
	return static_cast<uint64>(emscripten_get_now() * 1000.0);
#else
	// CLOCK_MONOTONIC and not CLOCK_REALTIME: what is measured are intervals,
	// and those must not change because somebody sets the clock.
	static bool initialized = false;
	static struct timespec startTime;

	if(!initialized)
	{
		clock_gettime(CLOCK_MONOTONIC, &startTime);
		initialized = true;
	}

	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	// Signed, and summed before the cast: the nanoseconds of the later
	// reading are regularly the smaller of the two, and that difference is
	// negative while the whole is not.
	const long long seconds = static_cast<long long>(t.tv_sec) - startTime.tv_sec;
	const long long nanoseconds = static_cast<long long>(t.tv_nsec) - startTime.tv_nsec;
	return static_cast<uint64>(seconds * 1000000 + nanoseconds / 1000);
#endif
}

uint getExactTimeMS()
{
	return static_cast<uint>(getExactTimeUS() / 1000);
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
void openURL(const std::string& url)
{
	// xdg-open reaches the configured browser on every desktop. It runs
	// through a shell, where an apostrophe would end the quoted argument, so
	// one is refused although only the program's own addresses arrive here.
	if(url.find('\'') != std::string::npos)
	{
		printfLog("Refusing to open a URL containing a quote: %s\n", url.c_str());
		return;
	}

	const std::string command = "xdg-open '" + url + "' >/dev/null 2>&1 &";
	if(::system(command.c_str()) != 0) printfLog("Could not open %s\n", url.c_str());
}
#endif

void writeProfileLine(const std::string& name,
					  float dt,
					  float avgTime)

{
	std::string line(name + ": ");
	line += std::string(30 - line.length(), ' ');
	char temp[32];
	// %f and not %lf: a float promotes to double in a variadic call, which is
	// what both of these conversions read.
	sprintf(temp, "dt: %f ms", dt * 1000.0f);
	line += temp;
	line += std::string(50 - line.length(), ' ');
	sprintf(temp, "avg: %f ms", avgTime * 1000.0f);
	line += temp;
	printfLog("%s\n", line.c_str());
}