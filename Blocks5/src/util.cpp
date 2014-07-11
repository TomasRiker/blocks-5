#include "pch.h"
#include "util.h"
#include "engine.h"
#include "filesystem.h"

MTRand mt;
bool writingCrashLog = false;

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
	// letzten Punkt suchen
	size_t index = filename.find_last_of('.');
	if(index == std::string::npos)
	{
		// Keine Erweiterung!
		return "";
	}
	else
	{
		return std::string(filename.begin() + index + 1, filename.end());
	}
}

std::string setFilenameExtension(const std::string& filename,
								 const std::string& extension)
{
	// letzten Punkt suchen
	size_t index = filename.find_last_of('.');
	if(index == std::string::npos)
	{
		// Keine Erweiterung!
		return filename + "." + extension;
	}
	else
	{
		// die vorhandene Erweiterung ersetzen
		return std::string(filename.begin(), filename.begin() + index) + "." + extension;
	}
}

int random()
{
	return mt.randInt(0x7FFFFFFF);
}

int random(int min,
		   int max)
{
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
		// Ist n eine Primzahl?
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
		// Dekodierung
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
	uint length1 = static_cast<uint>(strlen(p_in) / 7) * 4;
	for(uint i = 0, shift = 0; i < strlen(p_in); i += 7, shift++)
	{
		// immer 7 Zeichen zur Basis 62 in einen 32-Bit-Integer umwandeln
		uint n = fromBase62(&p_in[i]);

		// entschlüsseln
		uint pattern = (0x958B47A6 << (shift % 31)) ^ (0x8D4BA2D4 >> (shift % 17));
		n ^= pattern;

		// schreiben
		*(reinterpret_cast<uint*>(&step1[shift * 4])) = n;
	}

	char step2[256] = "";

	uint indexIn = 0, indexOut = 0;
	while(true)
	{
		// Anzahl der Terme lesen und entschlüsseln
		unsigned char numTerms = step1[indexIn++] ^ 0xB6;
		if(!numTerms) break;

		// Primzahlen und ihre Potenzen lesen und entschlüsseln
		uint c = 1;
		for(uint i = 0; i < numTerms; i++)
		{
			unsigned char prime = step1[indexIn++] ^ 0x4D;
			unsigned char power = step1[indexIn++] ^ 0xE9;

			// Potenz einmultiplizieren
			for(uint j = 0; j < power; j++) c *= p_primes[prime];
		}

		// Buchstabe entschlüsseln und schreiben
		c -= indexOut * 7;
		step2[indexOut++] = static_cast<char>(c);
	}

	step2[indexOut] = 0;
	strcpy(p_out, step2);
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

	// Parameterliste anfertigen und den String erstellen
	va_start(vaList, p_format);
	vsprintf(text, p_format, vaList);
	va_end(vaList);

	char datetime[32];
	time_t t;
	time(&t);
	strftime(datetime, 32, "%H:%M:%S", localtime(&t));
	std::string finalLogText(std::string(datetime) + " // " + text);

	// ausgeben
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
#else
#error NOT IMPLEMENTED
#endif
}

uint getExactTimeMS()
{
	return static_cast<uint>(getExactTime() * 1000.0);
}

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