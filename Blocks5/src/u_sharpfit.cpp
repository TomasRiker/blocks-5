#include "pch.h"
#include "u_sharpfit.h"

/* "SharpFit" - the nearest look at a fractional scale.

   In effect: nearest-upscale the 640x480 frame by the smallest integer N that
   makes it at least as large as the destination, then resample that down to
   the actual size. Nearest alone needs an integer ratio or it doubles some
   source pixels and not others; here the fractional remainder falls into the
   second step, where it makes a soft edge about a pixel wide instead of
   uneven stroke widths.

   One fetch does both. Bilinear over a nearest-upscaled image is piecewise
   linear: constant within a source pixel, with a ramp exactly 1/N source
   pixels wide at every border. Sampling the original texture bilinearly
   after putting the coordinate through that same function gives exactly
   that. In one dimension, with s the position within the source pixel
   (0..1):

       d    = s - 0.5                       distance from the pixel centre
       flat = 0.5 - 0.5 / N                 half the width of the flat part
       f    = (d - clamp(d, -flat, flat)) * N + 0.5

   For |d| <= flat, f is 0.5, the texel centre, as sharp as nearest; beyond
   that f runs linearly to 0 or 1, the ramp. At N = 1 the flat part vanishes
   and plain bilinear is left. Checked against a real two-pass version:
   pixel-identical at integer scales, and at fractional ones at most 1 apart
   in a channel - the 8-bit rounding of the intermediate image, which only
   the two-pass version has.

   It MUST sample with GL_LINEAR: the hardware interpolation is the second
   half of the filter. With GL_NEAREST the remapped coordinate lands back on
   the texel it started from and the result is plain nearest.

   Known in emulator circles as "sharp bilinear" (Themaister, libretro);
   derived afresh here, no code borrowed.

   No #version: the source compiles as GLSL 110 on the desktop and as GLSL ES
   100. */
static const char* p_sharpFitFragmentShader =
	"#ifdef GL_ES\n"
	"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
	"precision highp float;\n"
	"#else\n"
	"precision mediump float;\n"
	"#endif\n"
	"#endif\n"
	"uniform sampler2D decal;\n"
	"uniform vec2 TextureSize;\n"   /* the whole texture, power of two */
	"uniform vec2 FrameSize;\n"     /* the part of it in use, 640x480 */
	"uniform vec2 Prescale;\n"      /* N, integer, >= 1 */
	"varying vec2 texCoord;\n"
	"void main()\n"
	"{\n"
	"    vec2 texel = texCoord * TextureSize;\n"
	"    vec2 base  = floor(texel);\n"
	"    vec2 d     = (texel - base) - 0.5;\n"
	"    vec2 flat_ = 0.5 - 0.5 / Prescale;\n"
	"    vec2 f     = (d - clamp(d, -flat_, flat_)) * Prescale + 0.5;\n"
	/* At the right and top edge the ramp must not reach into the unused part
	   of the power-of-two texture. */
	"    vec2 p     = clamp(base + f, vec2(0.5), FrameSize - 0.5);\n"
	"    gl_FragColor = vec4(texture2D(decal, p / TextureSize).rgb, 1.0);\n"
	"}\n";

U_SharpFit::U_SharpFit()
{
}

U_SharpFit::~U_SharpFit()
{
}

const char* U_SharpFit::getFragmentSource() const
{
	return p_sharpFitFragmentShader;
}
