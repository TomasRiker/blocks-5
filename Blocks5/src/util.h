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

// "01 - Titel", wie es im Spiel und in der Levelauswahl ueber dem Bild steht.
// Der Titel kommt unbesehen aus der Leveldatei (level.cpp liest das Attribut
// so, wie es dasteht) und aus einem Eingabefeld ohne Laengengrenze - er ist
// also beliebig lang und darf nie durch einen festen Puffer laufen. Nur die
// Nummer geht hier durch einen, und die ist eine Zahl.
std::string formatLevelCaption(int number, const std::string& title);

// Ein einzelner Level traegt statt der Nummer seinen Dateinamen: wer drei
// Level geschickt bekommt, die alle "Unbenannt" heissen, kann sie sonst nicht
// auseinanderhalten. In einer Kampagne waere es sinnlos - dort hiesse jeder
// zweite "level_2.xml".
std::string formatSingleLevelCaption(const std::string& title, const std::string& filename);

// Macht aus einem beliebigen - auch von aussen eingeschleusten - Dateinamen
// einen sicheren Namensteil: nur der Basisname, nur [A-Za-z0-9_-], keine
// Punkte, nie leer, hoechstens 64 Zeichen. Das Ergebnis kann nie aus seinem
// Verzeichnis ausbrechen; die Erweiterung legt der Aufrufer fest.
std::string sanitizeFilenameStem(const std::string& untrusted,
                                 const std::string& fallback = "imported");

// Prueft, ob ein String unveraendert als Dateiname oder als Name eines
// Archivmitglieds benutzt werden darf. Abgelehnt wird alles, womit
// FileSystem::convertPath oder evalRelativePath umgelenkt werden koennten -
// Trenner, Laufwerksdoppelpunkt, die Archivmarken < > [ ], "..", ein
// fuehrender Punkt oder eine Tilde, Steuerzeichen. Umlaute und Leerzeichen
// bleiben erlaubt, damit legitime Namen nicht stillschweigend verschwinden.
bool isSafeMemberName(const std::string& name);

// Vergleich ohne Ruecksicht auf Gross- und Kleinschreibung, nur ueber ASCII.
// Von Hand und weder ueber _stricmp - das kennt nur MSVC - noch ueber
// strcasecmp oder tolower: die beiden haengen an der eingestellten Locale, und
// in der tuerkischen ist 'I' nicht die Grossform von 'i'. Verglichen werden
// hier Dateinamen und Schalter der Befehlszeile, und die sind in jeder Locale
// dieselben.
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
// Eine Adresse im Webbrowser oeffnen. Es gibt sie nur hier: Windows nimmt eine
// .url-Verknuepfung neben der Anwendung, die zugleich im Startmenue steht, und
// der Browser oeffnet einen zweiten Tab - beide koennen das schon, und beide
// tun es an Ort und Stelle.
void openURL(const std::string& url);
#endif
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