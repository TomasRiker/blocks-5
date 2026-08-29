// compat.h - non-MSVC compatibility layer for Blocks 5.
// Force-included ahead of everything so no game source needs editing.
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

// Win32 scalar types that leak into otherwise-portable headers (e.g. src/hq2x.h).
typedef unsigned long  DWORD;
typedef unsigned short WORD;
typedef unsigned char  BYTE;

#endif // !_MSC_VER
#endif
