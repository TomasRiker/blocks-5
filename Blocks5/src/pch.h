#ifndef _PCH_H
#define _PCH_H

#include <cmath>
#include <string>
#include <iostream>
#include <vector>
#include <list>
#include <stack>
#include <hash_map>
#include <set>
#include <queue>
#include <SDL.h>
#include <SDL_Thread.h>
#include <SDL_OpenGL.h>
#include <SDL_image.h>
#include <al.h>
#include <alc.h>
#include <vorbis/vorbisfile.h>
#include <tinyxml.h>
#include <sigslot.h>

#define __STDC_CONSTANT_MACROS

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include "singleton.h"
#include "manager.h"
#include "vec.h"
#include "typedefs.h"
#include "util.h"
#include "mtrand.h"

#endif