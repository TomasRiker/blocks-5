// compat.h - Ausgleichsschicht fuer Uebersetzer ausser MSVC. Wird allem
// vorangestellt, damit keine Quelldatei des Spiels angefasst werden muss.
#ifndef BLOCKS5_COMPAT_H
#define BLOCKS5_COMPAT_H
#ifndef _MSC_VER

#include <cstring>
#include <cstdio>
#include <cstdlib>

// MSVC CRT spellings used by the game.
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#ifndef _strnicmp
#define _strnicmp strncasecmp
#endif

#endif // !_MSC_VER
#endif
