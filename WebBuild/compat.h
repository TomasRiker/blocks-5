// compat.h - compatibility layer for compilers other than MSVC. Prepended to
// everything, which is why no source file of the game has to be touched.
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
