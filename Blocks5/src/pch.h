#ifndef _PCH_H
#define _PCH_H

// std::find and relatives: MSVC and libc++ pull <algorithm> in through the
// container headers, libstdc++ does not.
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

// After vec.h and SDL_opengl.h, whose types it names. Here rather than per
// file because every source that draws reaches Renderer, the one way to put
// a pixel on the screen; verify.py's raw_gl check keeps every gl* call to the
// few files that own raw GL.
#include "renderer.h"

#endif