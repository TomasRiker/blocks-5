/* Not upstream: minih264 is a single-header library in the stb style, and this
   is the one translation unit that instantiates it. It has to be separate from
   minimp4's, because both headers define a bitstream type called bs_t in their
   implementation halves and the two collide in one file. */
#define MINIH264_IMPLEMENTATION
#include "minih264e.h"
