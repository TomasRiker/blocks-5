#ifndef _PCH_H
#define _PCH_H

// std::find und Verwandte. MSVC und libc++ ziehen <algorithm> ueber die
// Containerkoepfe mit herein, libstdc++ nicht - panel.cpp, e_pulsepanel.cpp
// und teleporter.cpp liessen sich deshalb ausserhalb von MSVC nicht
// uebersetzen, und level.cpp nur durch Zufall.
#include <algorithm>
#include <cmath>
#include <string>
#include <iostream>
#include <vector>
#include <list>
#include <stack>
#include <unordered_map>
#include <set>
#include <queue>
#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_opengl.h>
#ifdef __EMSCRIPTEN__
#include <GL/glu.h>
#endif
#include "img_load.h"
#include <al.h>
#include <alc.h>
#include <vorbis/vorbisfile.h>
#include <tinyxml.h>
#include <sigslot.h>
#include <MersenneTwister.h>

#include "singleton.h"
#include "vec.h"
#include "typedefs.h"
#include "util.h"
#include "manager.h"

#endif