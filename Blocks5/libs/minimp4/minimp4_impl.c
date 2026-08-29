/* The one translation unit that instantiates minimp4, and the place where it is
   bent into shape without editing it.

   THE PROBLEM
   minimp4 writes the 'esds' descriptor of an audio track with the
   objectTypeIndication byte hardcoded to AAC, ignoring the
   object_type_indication field the caller filled into MP4E_track_t. That byte
   is the only thing that tells a demuxer whether an 'mp4a' track holds AAC or
   MP3 - Microsoft's documentation for the MPEG-4 File Source lists that one
   sample entry as meaning "AAC or MP3" - so an MP3 track comes back out
   declared as AAC and will not decode anywhere.

   THE FIX, WITHOUT TOUCHING THE FILE
   The constant it writes is a macro, and minimp4's declarations and its
   implementation sit on opposite sides of its include guard in the same file
   (#endif //MINIMP4_H, then the MINIMP4_IMPLEMENTATION block). So the header can
   be included twice: once for the declarations, which is what defines the
   macro, and again for the implementation with the macro redefined in between.
   The replacement reads the field the caller actually set.

   The alternative was a patch to minimp4.h, which is worse: it survives no
   upgrade unless somebody remembers it, and there is nothing to remind them.

   WHAT STILL HAS TO BE DONE BY THE CALLER
   Upstream emits the descriptor at all only when a decoder-specific info blob
   has been set, which AAC has and MP3 does not. videorecorder.cpp therefore
   calls MP4E_set_dsi(mux, track, "", 0) on the audio track; a zero-length blob
   is enough to get the descriptor written.

   DETECTING THAT THIS STILL WORKS
   Three layers, because no single one covers everything:
     - the #if checks below fail the build if the macro is gone, renamed, or no
       longer 0x40;
     - the replacement dereferences `tr`, so if a future version uses the macro
       anywhere that variable is not in scope, the build fails there;
     - the counter is incremented on every expansion, and videorecorder.cpp
       checks it moved while closing a file that has an audio track. That is the
       one case the preprocessor cannot see: a version that simply stops using
       the macro here, leaving the override silently doing nothing. */

#include "minimp4.h"

#if !defined(MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3)
#  error "minimp4 no longer defines MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3. See the comment above: the audio objectTypeIndication has to be overridden somehow, or minimp4.h has to be patched."
#endif
#if MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3 != 0x40
#  error "MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3 is no longer 0x40. Check what minimp4 now writes into the esds before trusting the override below."
#endif
#if defined(MINIMP4_IMPLEMENTATION_GUARD)
#  error "minimp4's implementation was already instantiated before this file, so the override below would come too late. It must be instantiated here and nowhere else."
#endif

/* Bumped once per expansion; read by videorecorder.cpp. */
int g_minimp4_audioObjectTypeHookHits = 0;

#undef  MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3
#define MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3 \
    (++g_minimp4_audioObjectTypeHookHits, tr->info.object_type_indication)

#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"
