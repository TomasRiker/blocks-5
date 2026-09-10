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

int random(int min,
		   int max)
{
	// An empty span is empty and not huge: MTRand::randInt() takes a uint32,
	// and a negative max - min would become four billion there, with an
	// arbitrary number far outside [min, max] coming out.
	if(max <= min) return min;
	return min + mt.randInt(max - min);
}

double random(double min,
			  double max)
{
	return min + mt.rand(max - min);
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

std::string prepareForTinyXML(const std::string& text)
{
	return text;
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

void decryptPassword(const char* p_in,
					 char* p_out,
					 const uint* p_primes)
{
	char step1[1024] = "";
	for(uint i = 0, shift = 0; i < strlen(p_in); i += 7, shift++)
	{
		// always turn 7 base-62 characters into one 32-bit integer
		uint n = fromBase62(&p_in[i]);

		// decrypt
		uint pattern = (0x958B47A6 << (shift % 31)) ^ (0x8D4BA2D4 >> (shift % 17));
		n ^= pattern;

		// write it
		*(reinterpret_cast<uint*>(&step1[shift * 4])) = n;
	}

	char step2[256] = "";

	uint indexIn = 0, indexOut = 0;
	while(true)
	{
		// read the number of terms and decrypt it
		unsigned char numTerms = step1[indexIn++] ^ 0xB6;
		if(!numTerms) break;

		// read the primes and their powers and decrypt them
		uint c = 1;
		for(uint i = 0; i < numTerms; i++)
		{
			unsigned char prime = step1[indexIn++] ^ 0x4D;
			unsigned char power = step1[indexIn++] ^ 0xE9;

			// multiply the power in
			for(uint j = 0; j < power; j++) c *= p_primes[prime];
		}

		// decrypt the letter and write it
		c -= indexOut * 7;
		step2[indexOut++] = static_cast<char>(c);
	}

	step2[indexOut] = 0;
	strcpy(p_out, step2);
}

bool isSafeMemberName(const std::string& name)
{
	if(name.empty() || name.length() > 100) return false;
	if(name[0] == '.' || name[0] == '~') return false;
	if(name.find("..") != std::string::npos) return false;

	for(size_t i = 0; i < name.length(); i++)
	{
		// Source files are ISO-8859-1: without the reinterpretation as
		// unsigned, every umlaut would be negative and would fail the control
		// character test.
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

	// output
	printf("%s", finalLogText.c_str());
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
		fprintf(p_file, "%s", finalLogText.c_str());
#ifdef _DEBUG
		fflush(p_file);
#endif
		fclose(p_file);
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

double getExactTime()
{
#ifdef _WIN32
	static bool initialized = false;
	static LARGE_INTEGER startTime;
	static double invFrequency;

	if(!initialized)
	{
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		invFrequency = 1.0 / frequency.QuadPart;
		QueryPerformanceCounter(&startTime);
		initialized = true;
	}

	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return (t.QuadPart - startTime.QuadPart) * invFrequency;
#elif defined(__EMSCRIPTEN__)
	return emscripten_get_now() * 0.001;
#else
	// CLOCK_MONOTONIC and not CLOCK_REALTIME: what is measured are intervals,
	// and those must not change because somebody sets the clock.
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec * 1.0e-9;
#endif
}

uint getExactTimeMS()
{
	return static_cast<uint>(getExactTime() * 1000.0);
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
void openURL(const std::string& url)
{
	// xdg-open is what every desktop environment brings along and what points
	// at the configured browser. Only addresses from the program arrive here,
	// but the call goes through a shell, and an apostrophe in one would end
	// the argument - do not let one through in the first place.
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
					  double dt,
					  double avgTime)

{
	std::string line(name + ": ");
	line += std::string(30 - line.length(), ' ');
	char temp[32];
	sprintf(temp, "dt: %lf ms", dt * 1000.0);
	line += temp;
	line += std::string(50 - line.length(), ' ');
	sprintf(temp, "avg: %lf ms", avgTime * 1000.0);
	line += temp;
	printfLog("%s\n", line.c_str());
}