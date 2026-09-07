#ifndef _PCH_H
#define _PCH_H

// std::find and relatives. MSVC and libc++ pull <algorithm> in through the
// container headers, libstdc++ does not - without it panel.cpp,
// e_pulsepanel.cpp and teleporter.cpp do not compile outside MSVC, and
// level.cpp compiles only by chance.
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
#ifndef _WIN32
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