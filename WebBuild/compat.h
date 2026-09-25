// compat.h - compatibility layer for compilers other than MSVC. build.sh
// force-includes it into every C++ source of the web build.
#ifndef BLOCKS5_COMPAT_H
#define BLOCKS5_COMPAT_H
#ifndef _MSC_VER

#include <cstring>
#include <cstdio>
#include <cstdlib>

// MSVC CRT spellings, unused by the game's own sources (see util.h's
// equalsNoCase).
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#ifndef _strnicmp
#define _strnicmp strncasecmp
#endif

#endif // !_MSC_VER
#endif
