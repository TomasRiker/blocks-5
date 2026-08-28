// compat.h - non-MSVC compatibility layer for Blocks 5.
// Force-included ahead of everything so no game source needs editing.
#ifndef BLOCKS5_COMPAT_H
#define BLOCKS5_COMPAT_H
#ifndef _MSC_VER

#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include <cstdio>
#include <cstdlib>

// POSIX declares `long random(void)` in <stdlib.h>; the game declares its own
// `int random()` in util.h, which MSVC never saw because its CRT has no such
// function. stdlib.h is pulled in above FIRST, so this rename only ever hits
// the game's own declaration, definition and call sites.
#define random blocks5_random

// The game uses MSVC's stdext::hash_map/hash_multimap in 11 files (41 uses),
// always in the two-argument form, so plain aliases to the standard containers
// are behaviour-compatible here: every use is order-independent, and the one
// place that needs ordered iteration (Engine's action list) keeps its own vector.
namespace stdext {
    template <class K, class V> using hash_map      = std::unordered_map<K, V>;
    template <class K, class V> using hash_multimap = std::unordered_multimap<K, V>;
    template <class K>          using hash_set      = std::unordered_set<K>;
}

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
