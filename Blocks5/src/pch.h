#ifndef _PCH_H
#define _PCH_H

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

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef BLOCKS5_NO_FFMPEG
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#endif

#include "singleton.h"
#include "vec.h"
#include "typedefs.h"
#include "util.h"
#include "manager.h"

#endif