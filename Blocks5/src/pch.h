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

// After vec.h and SDL_opengl.h, whose types it names. It is here rather than
// per file because every source that draws anything reaches GL:: - a raw
// glBindTexture or GL_TEXTURE_2D enable is what verify.py's gl_state check
// bans - and a missing include is then the one way to get a raw call past it.
#include "glstate.h"

#endif