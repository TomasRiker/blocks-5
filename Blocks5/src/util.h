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
int random();
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
void writeProfileLine(const std::string& name, double dt, double avgTime);

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