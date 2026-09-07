#include "pch.h"
#include "u_sharpfit.h"

/* "SharpFit" - the nearest look at a fractional scale.

   Conceptually: nearest-upscale the 640x480 frame by the smallest integer
   factor N that makes it at least as large as the destination rectangle, then
   resample that result down to the actual size. Nearest on its own needs an
   integer ratio, or it doubles some source pixels and not others; here the
   fractional remainder falls into the second step, where all it produces is a
   soft edge about a pixel wide instead of uneven stroke widths.

   Two passes are not needed. Bilinear over a nearest-upscaled image is
   piecewise linear: constant within a source pixel, and at every pixel border
   a ramp exactly 1/N source pixels wide. The hardware delivers precisely that
   if the original texture is sampled bilinearly and the texture coordinate is
   first put through the same piecewise linear function - one fetch instead of
   two passes, and exactly the same result.

   The remapping, one-dimensional, with s = the position within the source
   pixel (0..1):

       d    = s - 0.5                       distance from the pixel centre
       flat = 0.5 - 0.5 / N                 half the width of the flat part
       f    = (d - clamp(d, -flat, flat)) * N + 0.5

   For |d| <= flat, f is 0.5, exactly the pixel centre: the hardware delivers
   the texel unchanged, as sharp as nearest. Outside that, f runs linearly to 0
   or to 1, which is the ramp at the pixel border. At N = 1 the flat part
   disappears and ordinary bilinear is left.

   The same arithmetic is known in emulator circles as "sharp bilinear"
   (Themaister, libretro); it is derived afresh here, and none of the code
   is borrowed.

   No #version: 110 on the desktop, 100 on the embedded shading language, and
   the source compiles as both. */
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

bool U_SharpFit::createGL()
{
	return program.create(p_sharpFitFragmentShader, "sharp-fit fragment");
}

void U_SharpFit::destroyGL()
{
	program.destroy();
}

void U_SharpFit::present(const PresentContext& context)
{
	program.use(context);
	program.drawQuad(context);
}
