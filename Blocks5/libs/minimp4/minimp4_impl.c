/* The one translation unit that instantiates minimp4.

   It has to be separate from minih264's for the same reason that one is
   separate: both headers define a bitstream struct called bs_t inside their
   implementation halves, and the two collide if both are instantiated in a
   single file.

   minimp4.h carries local changes to the esds descriptor it writes; they are
   documented in PROVENANCE.txt next to it. Nothing has to be done here. */

#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"
