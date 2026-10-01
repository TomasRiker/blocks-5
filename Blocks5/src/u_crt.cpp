#include "pch.h"
#include "u_crt.h"
#include "glextensions.h"

/* A monitor from the nineties, drawn in the present pass.

   No thresholds and no edge detection, only smooth functions of the source
   colour and the position, so moving the input by 1 moves the output by about
   1. An edge-directed filter's step() decisions flip when a semi-transparent
   dialog moves a colour by 1/255, and the picture flickers.

   WHICH MONITOR is one number, SCANLINE_PERIOD. Gaps between the lines belong
   to 240p: a console drew 240 lines into a 480-line raster and the phosphor
   between them stayed dark. A VGA monitor at 640x480 drew all 480 lines with
   overlapping beam profiles and had no gaps. 1.0 renders that monitor, the
   honest reference for a 640x480 Windows game; 2.0 pretends 240 lines arrive
   and gives the console look most people mean by "CRT". At period 1 and a 2x
   window both output rows of a line sit equally far from its centre, so no
   stripe shows while the crawl rests and the stripes pulse as it moves: only
   period 2 gives steady structure at 2x. The scan-line slider fades the
   effect in; the period decides which effect.

   RESOLUTION. The factor is rarely one to build on: the default window is 2x
   (1280x960) on 1080p and 1440p and 3x from 1600p, but fullscreen, where the
   Release build starts, fills a 1080p screen at 2.25x. So the mask sits in
   the output raster, not the source raster, as a real shadow mask belongs to
   the glass: MASK_PITCH is in output pixels and stays equally fine at every
   factor.

   DISTORTION. The mapping runs from output to source pixel, the direction a
   fragment shader asks in, with both in -1..1 from the picture's centre:

       src.x = out.x * (1 + a*out.y^2)
       src.y = out.y * (1 + b*out.x^2)

   Only the corners move out; the edge midpoints stay put. That is what
   getOverscan() is for: without it the picture reaches the last output row
   at the edge midpoints, and what the shader draws outside the picture - the
   raster's soft edge, the convergence fringe - has nowhere to go. At
   curvature 0 it is 0 and the picture covers exactly what the other filters
   cover (measured: zero shift in either axis against sharp-fit).

   warpToSource()/warpToOutput() repeat the mapping in C++ for the mouse, the
   inverse as a fixed-point iteration; change one and change the other.

   No #version: the source compiles as GLSL 110 on the desktop and as GLSL ES
   100, which has no array constants and no loops with a variable bound. */

/* Numbers the shader and C++ both need, written once: stringified into the
   shader and read as floats beside it, so the two cannot drift apart. No f
   suffix, since the same characters compile as GLSL, which has none. The
   curvature also warps the mouse (warpToSource/warpToOutput), the edge and
   the convergence size getOverscan(), and present() wraps its clock at the
   flicker cycle. CRT_CRAWL_SPEED is the exception: only present() reads it. */

#define CRT_CURVE_X 0.10
#define CRT_CURVE_Y 0.13
/* Half the width of the raster's soft edge, in source rows; rasterMask fades
   out over two of them. */
#define CRT_EDGE_ROWS 1.0
/* Convergence offset per beam at the edge of the picture, in source columns.
   Explained at the shader constant of the same name. */
#define CRT_CONVERGENCE_MAX 1.6
/* Scan-line crawl in line periods per second with the scan-flicker slider at
   full; see CRAWL_JITTER. */
#define CRT_CRAWL_SPEED 1.2
/* The flicker repeats exactly after this many seconds; present() wraps the
   clock at it. */
#define CRT_FLICKER_CYCLE 8.0
#define CRT_STR2(x) #x
#define CRT_STR(x) CRT_STR2(x)

static const float crtCurveX = static_cast<float>(CRT_CURVE_X);
static const float crtCurveY = static_cast<float>(CRT_CURVE_Y);
static const float crtEdgeRows = static_cast<float>(CRT_EDGE_ROWS);
static const float crtConvergenceMax = static_cast<float>(CRT_CONVERGENCE_MAX);
static const float crtCrawlSpeed = static_cast<float>(CRT_CRAWL_SPEED);

static const char* p_crtFragmentShader =
	"#ifdef GL_ES\n"
	"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
	"precision highp float;\n"
	"#else\n"
	"precision mediump float;\n"
	"#endif\n"
	"#endif\n"

	/* ------------------------------------------------------------------ */
	/* Tuning constants. Everything that gives this filter its character. */
	/* ------------------------------------------------------------------ */

	/* Which tube, see the top of the file: 1.0 a VGA monitor, no steady
	   stripes at 2x; 2.0 the 240-line console look. Values between look like
	   a fault. */
	"const float SCANLINE_PERIOD = 2.0;\n"

	/* Width of the electron beam, in line pitches. Smaller = narrower beam =
	   deeper and harder stripes. 0.5 is strong, 0.8 gentle. */
	"const float BEAM_WIDTH = 0.55;\n"

	/* The mask, in OUTPUT PIXELS per RGB triple: a stripe mask, one pixel
	   red, one green, one blue. At 2x it beats against the source raster
	   (period 2) into a pattern of period 6. The mask code below lights red
	   on stripe 0, green on 1 and blue on every other, so a pitch other than
	   3.0 tints the picture. */
	"const float MASK_PITCH = 3.0;\n"
	/* How dark the stripes that are not currently excited become. 0 = no mask,
	   1 = the other two channels fully off (far too much). 0.3 is plainly
	   visible without colours tipping over. */
	"const float MASK_STRENGTH = 0.30;\n"

	/* Halation: light scatters in the glass and comes back as a soft halo.
	   Only what is brighter than the threshold glows, so dark areas stay
	   sharp - the asymmetry that tells it from a blur. Radius in source
	   pixels. BLOOM_STRENGTH is the value with the Bloom slider at full; at 0
	   the whole block is compiled away. Measured off a bright spot: +23 grey
	   levels at the centre, +4 at 70 output pixels.

	   Cost as ratios of one present on a software rasterizer: nearest 1.0,
	   bilinear 1.3, sharp-fit 1.35, crt 7.8, and 4.2 with BLOOM_STRENGTH at
	   0 - the halation is about half. Noise on real hardware, but the browser
	   build can land on a software path. */
	"const float BLOOM_STRENGTH  = 0.38;\n"
	"const float BLOOM_THRESHOLD = 0.22;\n"
	"const float BLOOM_RADIUS    = 2.5;\n"   /* inner ring, source pixels */
	"const float BLOOM_OUTER     = 2.6;\n"   /* outer ring as a multiple of it */

	/* Horizontal bandwidth. The video signal ran analog and band-limited, which
	   made a tube softer across than down. In fractions of a source pixel;
	   0 = as sharp as sharp-fit. */
	"const float SOFTNESS = 0.35;\n"

	/* Convergence. A colour tube's three beams never meet the mask at exactly
	   the same place: converged in the centre, they drift apart toward the
	   rim, where the deflection is largest - a red and a blue fringe on
	   vertical edges, none in the middle. Not chromatic aberration, which
	   needs a lens: three pictures lie side by side. The value is each beam's
	   offset at the left and right edge with the slider at full, in source
	   pixels; red and blue move against each other, so the visible fringe is
	   twice as wide. A well-adjusted set stayed below that, a tired cheap one
	   reached one or two triads in the corners, one or two source pixels
	   here. At 0 the block is compiled away. */
	"const float CONVERGENCE_MAX = " CRT_STR(CRT_CONVERGENCE_MAX) ";\n"

	/* Measured at the slider's default, the outermost output column keeps
	   89% of its red; at full slider the outermost three keep 49%, 76% and
	   95%, with green and blue untouched - rasterMask evaluated at each
	   channel's own source point. How far red lags blue on a finished frame:
	   within a tenth of a pixel at 0; at full slider -5.3 output pixels at
	   the left edge, -0.3 in the middle and +4.4 at the right, the
	   antisymmetric ramp asked for. */

	/* Curvature with the slider at full. Lottes takes 1/32 and 1/24; this may
	   go further, since the slider rarely sits at the stop. From the
	   CRT_CURVE_* macros above, which the mouse conversion reads as well. */
	"const float CURVE_X = " CRT_STR(CRT_CURVE_X) ";\n"
	"const float CURVE_Y = " CRT_STR(CRT_CURVE_Y) ";\n"
	/* Half the width of the raster's soft edge, in source rows. From the macro
	   above, because getOverscan() needs the same value. */
	"const float EDGE_ROWS = " CRT_STR(CRT_EDGE_ROWS) ";\n"
	/* Rounded corners, in fractions of half the picture height.
	   0 = rectangular. */
	"const float CORNER_RADIUS = 0.10;\n"
	/* Vignette. 0 = off. */
	"const float VIGNETTE = 0.22;\n"

	/* Flicker, two effects with a slider each, since they are wanted apart.
	   Flicker shimmers the *brightness*: three oscillations at about 12, 19
	   and 29 Hz that never fall into a pattern, plus a much weaker mains hum,
	   a broad dark bar rolling slowly up the picture as the mains frequency
	   beats against the frame rate. ScanFlicker moves the *lines*: they drift
	   slowly down and shimmer, see CRAWL_JITTER. Every term has mean zero, so
	   costs no brightness, and none reads the previous frame's picture; all
	   but the drift hang off the clock alone, and the drift is the one value
	   present() keeps. Values with the slider at full; HUM_DEPTH 0 removes the
	   bar. Measured with both sliders at full, the terms give 1.7%
	   peak-to-peak between frames. */
	"const float FLICKER_DEPTH = 0.0367;\n"  /* fast brightness shimmer */
	"const float HUM_DEPTH     = 0.0147;\n"  /* depth of the rolling bar */
	"const float HUM_BARS      = 0.75;\n"    /* how many bars fit the picture */
	"const float HUM_ROLLS     = 3.0;\n"     /* runs per FLICKER_CYCLE */
	/* The flicker repeats exactly after this many seconds: every frequency
	   below is a whole number of runs per cycle, so the clock wraps without a
	   seam and Time stays small, where float keeps its precision. From the
	   macro above, because present() wraps the clock at the same value. */
	"const float FLICKER_CYCLE = " CRT_STR(CRT_FLICKER_CYCLE) ";\n"

	/* On a real tube the lines drift slowly down - the scan-line crawl, as
	   line and frame frequency never stand in an exact ratio - and shimmer as
	   they go; without that the stripes look painted on. The crawl
	   (CRT_CRAWL_SPEED) is the one term computed on the CPU, as ScanPhase:
	   its slope follows the slider, so a phase from the wrapped Time would
	   jump at every wrap. CRAWL_JITTER, the shimmer in periods, runs here,
	   since an oscillation of whole-number frequency wraps by itself. */
	"const float CRAWL_JITTER = 0.05;\n"

	/* Gamma. A tube had about 2.4; in between everything is computed in linear
	   light, or the halo turns into grey haze. */
	"const float GAMMA_IN  = 2.4;\n"
	"const float GAMMA_OUT = 2.2;\n"
	/* Purely a matter of taste: 1.0 is as bright as with no filter. The light
	   mask and stripes take away is given back below from the constants
	   (maskAvg and scanAvg), so changing them needs nothing here. Measured,
	   the scan-line slider moves a frame's mean brightness by 0.5% over its
	   whole travel. */
	"const float BRIGHTNESS = 1.0;\n"

	/* ------------------------------------------------------------------ */

	"uniform sampler2D decal;\n"
	"uniform vec2 TextureSize;\n"   /* whole texture, power of two */
	"uniform vec2 FrameSize;\n"     /* the part of it in use, 640x480 */
	"uniform vec2 Prescale;\n"      /* as with sharp-fit: whole-number, >= 1 */
	"uniform float Scanline;\n"     /* slider 0..1 */
	"uniform float Curvature;\n"    /* slider 0..1 */
	"uniform float Bloom;\n"        /* slider 0..1 */
	"uniform float Flicker;\n"      /* slider 0..1, brightness */
	"uniform float ScanFlicker;\n"  /* slider 0..1, position of the lines */
	"uniform float Convergence;\n"  /* slider 0..1, colour fringes at the edge */
	"uniform float Overscan;\n"     /* raster to glass edge, see getOverscan() */
	"uniform float Time;\n"         /* seconds, 0 .. FLICKER_CYCLE */
	"uniform float ScanPhase;\n"    /* scan-line crawl, 0..1 periods */
	"varying vec2 texCoord;\n"

	/* One texel through sharp-fit's piecewise linear remapping, softened by
	   SOFTNESS: sharp-fit's ramp at a pixel border is 1/N source pixels wide,
	   this one is wider, and that is the limited bandwidth. Vertically it
	   stays sharp - a tube ran soft across and sharp down. */
	"vec3 fetch(vec2 uv)\n"
	"{\n"
	"    vec2 texel = uv * FrameSize;\n"
	"    vec2 base  = floor(texel);\n"
	"    vec2 d     = (texel - base) - 0.5;\n"
	"    vec2 soft  = vec2(1.0 + SOFTNESS * Prescale.x, 1.0);\n"
	"    vec2 n     = max(Prescale / soft, vec2(1.0));\n"
	"    vec2 flat_ = 0.5 - 0.5 / n;\n"
	"    vec2 f     = (d - clamp(d, -flat_, flat_)) * n + 0.5;\n"
	"    vec2 p     = clamp(base + f, vec2(0.5), FrameSize - 0.5);\n"
	"    return texture2D(decal, p / TextureSize).rgb;\n"
	"}\n"

	"vec3 toLinear(vec3 c) { return pow(max(c, vec3(0.0)), vec3(GAMMA_IN)); }\n"

	/* The raster's edge as the distance function of a rounded rectangle: 1
	   inside, 0 outside, a transition 2 * edge wide between. wc is the source
	   point *this channel* reads, not the output point: a colour tube paints
	   three rasters, and where red's is narrower the red picture ends first,
	   which is how a misconverged set gives itself away. The rectangle is
	   edge larger than the picture, so the transition lies wholly outside it
	   and the picture is untouched; getOverscan() makes the room. */
	"float rasterMask(vec2 wc, float r, float edge)\n"
	"{\n"
	"    vec2  q  = abs(wc) - (1.0 - r + edge);\n"
	"    float sd = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;\n"
	"    return 1.0 - smoothstep(-edge, edge, sd);\n"
	"}\n"

	/* The halo's fetch. No sharp remapping as in fetch(): a soft image of a
	   soft image does not need it, which saves the ramp arithmetic eight
	   times over. */
	"vec3 fetchRaw(vec2 uv)\n"
	"{\n"
	"    vec2 p = clamp(uv * FrameSize, vec2(0.5), FrameSize - 0.5);\n"
	"    return texture2D(decal, p / TextureSize).rgb;\n"
	"}\n"

	"void main()\n"
	"{\n"
	/* texCoord runs only over the part of the texture in use; bring it to 0..1
	   over the picture here. */
	"    vec2 uv = texCoord * TextureSize / FrameSize;\n"

	/* --- curvature: output point -> source point ---------------------- */
	"    vec2 p = uv * 2.0 - 1.0;\n"
	"    float a = Curvature * CURVE_X;\n"
	"    float b = Curvature * CURVE_Y;\n"
	"    vec2 w = vec2(p.x * (1.0 + a * p.y * p.y),\n"
	"                  p.y * (1.0 + b * p.x * p.x));\n"
	/* One step further out, keeping the raster off the glass's edge. The
	   factor comes from getOverscan(); on a flat tube it is 0. */
	"    w *= 1.0 + Overscan;\n"
	"    vec2 suv = w * 0.5 + 0.5;\n"

	/* --- convergence, part one: how far red and blue are off ---------- */
	/* Up here because the edge needs it. cx is each beam's offset in suv, cw
	   the same in w; red reads cx further right, which puts its picture cx
	   further left. */
	"    float cx = Convergence * CONVERGENCE_MAX * w.x / FrameSize.x;\n"
	"    float cw = 2.0 * cx;\n"
	"    bool  converge = (CONVERGENCE_MAX > 0.0 && Convergence > 0.0);\n"

	/* --- edge: rounded rectangle, per channel ------------------------- */
	"    float r = CORNER_RADIUS * Curvature;\n"
	"    float edge = EDGE_ROWS * 2.0 / FrameSize.y;\n"
	"    vec3  vis = vec3(rasterMask(w, r, edge));\n"
	"    if(converge)\n"
	"    {\n"
	"        vis.r = rasterMask(vec2(w.x + cw, w.y), r, edge);\n"
	"        vis.b = rasterMask(vec2(w.x - cw, w.y), r, edge);\n"
	"    }\n"
	"    if(max(max(vis.r, vis.g), vis.b) <= 0.0)\n"
	"    { gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }\n"

	/* --- sample vertically: two rows, weighted by beam profile -------- */
	/* This is the resampling and it holds the brightness; the stripe is added
	   separately afterwards, letting the slider really start at 0. */
	"    float sy   = suv.y * FrameSize.y;\n"
	"    float rowc = floor(sy - 0.5) + 0.5;\n"
	"    float fy   = sy - rowc;\n"
	"    float g0   = exp(-(fy * fy) / (BEAM_WIDTH * BEAM_WIDTH));\n"
	"    float g1   = exp(-((1.0 - fy) * (1.0 - fy)) / (BEAM_WIDTH * BEAM_WIDTH));\n"
	"    float gs   = max(g0 + g1, 1e-4);\n"
	"    vec3 c0 = toLinear(fetch(vec2(suv.x, rowc / FrameSize.y)));\n"
	"    vec3 c1 = toLinear(fetch(vec2(suv.x, (rowc + 1.0) / FrameSize.y)));\n"
	"    vec3 col = (g0 * c0 + g1 * c1) / gs;\n"

	/* --- convergence, part two: red and blue off to the side ---------- */
	/* Green stays put as the reference a set was converged on; red and blue
	   shift against each other in proportion to w.x. The edge per channel is
	   set above; this adds the content. Horizontal only: a vertical offset
	   would need its own rows and beam profile per channel, eight fetches
	   instead of four, and the line structure all but hides it.

	   The four extra fetches triple the resampling, paid for because the
	   slider defaults to 0.5: measured on llvmpipe at 1280x960,
	   presentFrame() costs 30.6 instead of 25.3 ms, nothing on a real
	   graphics card. The if hands it all back at slider 0, since the
	   condition is the same for the whole draw call. */
	"    if(converge)\n"
	"    {\n"
	"        vec3 r0 = toLinear(fetch(vec2(suv.x + cx, rowc / FrameSize.y)));\n"
	"        vec3 r1 = toLinear(fetch(vec2(suv.x + cx, (rowc + 1.0) / FrameSize.y)));\n"
	"        vec3 b0 = toLinear(fetch(vec2(suv.x - cx, rowc / FrameSize.y)));\n"
	"        vec3 b1 = toLinear(fetch(vec2(suv.x - cx, (rowc + 1.0) / FrameSize.y)));\n"
	"        col.r = (g0 * r0.r + g1 * r1.r) / gs;\n"
	"        col.b = (g0 * b0.b + g1 * b1.b) / gs;\n"
	"    }\n"

	/* --- halation ----------------------------------------------------- */
	/* The dearest part of the shader, about half its cost, so it sits in an
	   if on a constant: BLOOM_STRENGTH 0 compiles the block away. */
	"    if(BLOOM_STRENGTH > 0.0 && Bloom > 0.0)\n"
	"    {\n"
	/* Eight taps on two rings, four axial on the inner and four diagonal on
	   the outer: eight on one radius make a hard-edged ring, two radii fall
	   off softly. Each tap goes to linear light before the average, where
	   light actually adds; averaged first, the ring round a bright spot is a
	   mix of bright and dark whose pow() falls far below the threshold, and
	   no halo shows. As x*x, gamma 2.0 rather than GAMMA_IN: meaningless for
	   a soft halo, and eight pow() calls would be the dearest item in the
	   shader. */
	"    vec2 br = BLOOM_RADIUS / FrameSize;\n"
	"    vec2 bo = br * BLOOM_OUTER;\n"
	"    vec3 t0 = fetchRaw(suv + vec2( br.x,  0.0));\n"
	"    vec3 t1 = fetchRaw(suv + vec2(-br.x,  0.0));\n"
	"    vec3 t2 = fetchRaw(suv + vec2( 0.0,  br.y));\n"
	"    vec3 t3 = fetchRaw(suv + vec2( 0.0, -br.y));\n"
	"    vec3 t4 = fetchRaw(suv + vec2( bo.x * 0.7,  bo.y * 0.7));\n"
	"    vec3 t5 = fetchRaw(suv + vec2(-bo.x * 0.7,  bo.y * 0.7));\n"
	"    vec3 t6 = fetchRaw(suv + vec2( bo.x * 0.7, -bo.y * 0.7));\n"
	"    vec3 t7 = fetchRaw(suv + vec2(-bo.x * 0.7, -bo.y * 0.7));\n"
	"    vec3 sum = t0*t0 + t1*t1 + t2*t2 + t3*t3\n"
	"             + t4*t4 + t5*t5 + t6*t6 + t7*t7;\n"
	"    vec3 halo = max(sum * 0.125 - vec3(BLOOM_THRESHOLD), vec3(0.0));\n"
	"    col += halo * BLOOM_STRENGTH * Bloom;\n"
	"    }\n"

	/* --- flicker, part 1: the terms ----------------------------------- */
	/* Needed twice, for the brightness below and the line position here.
	   Every frequency is a whole number of runs per FLICKER_CYCLE, so the
	   clock wraps seamlessly; 97, 151 and 233 per 8 s are about 12, 19 and
	   29 Hz, coprime so that their sum repeats no sooner. */
	"    float hum = 0.0;\n"
	"    float wob = 0.0;\n"
	"    if(Flicker > 0.0 || ScanFlicker > 0.0)\n"
	"    {\n"
	"        float w = 6.2831853 / FLICKER_CYCLE;\n"
	"        hum = sin((uv.y * HUM_BARS) * 6.2831853 - Time * w * HUM_ROLLS);\n"
	"        wob = sin(Time * w *  97.0) * 0.5\n"
	"            + sin(Time * w * 151.0) * 0.3\n"
	"            + sin(Time * w * 233.0) * 0.2;\n"
	"    }\n"

	/* --- line structure ----------------------------------------------- */
	/* dc is the distance to the nearest line centre, 0 on it and 1 midway
	   between two. At period 1 and 2x both output rows sit equally far off
	   while the crawl rests, and nothing shows but a pulse as it moves: the
	   reason for SCANLINE_PERIOD.
	   ScanPhase moves the pattern slowly down, CRAWL_JITTER makes it
	   shimmer; at Scanline 0 both drop out with the lines. */
	"    float ph = sy / SCANLINE_PERIOD + ScanPhase + ScanFlicker * CRAWL_JITTER * wob;\n"
	"    float dc = abs(fract(ph) - 0.5) * 2.0;\n"
	"    float k  = BEAM_WIDTH * BEAM_WIDTH * 4.0;\n"
	"    float beam = exp(-(dc * dc) / k);\n"
	/* The profile's mean over one period, from five samples. beam / scanAvg
	   then averages 1 and mix(1, ., Scanline) keeps that at every setting, so
	   the stripes cost no brightness however narrow the beam. The inputs are
	   constants, which the compiler folds. */
	"    float scanAvg = (exp(-0.01 / k) + exp(-0.09 / k) + exp(-0.25 / k)\n"
	"                   + exp(-0.49 / k) + exp(-0.81 / k)) * 0.2;\n"
	"    col *= mix(1.0, beam / max(scanAvg, 1e-3), Scanline);\n"

	/* --- mask, in the output pixel raster ----------------------------- */
	/* gl_FragCoord is in window pixels - exactly right, because a shadow mask
	   sits on the glass and not in the signal. */
	"    float mp = floor(mod(gl_FragCoord.x, MASK_PITCH));\n"
	"    vec3 mask = vec3(1.0 - MASK_STRENGTH);\n"
	"    if(mp < 0.5)      mask.r = 1.0;\n"
	"    else if(mp < 1.5) mask.g = 1.0;\n"
	"    else              mask.b = 1.0;\n"
	/* Per channel one stripe out of MASK_PITCH is bright and the rest dimmed;
	   this is the mean of that. Dividing by it makes the mask let through
	   exactly as much light as no mask at all. */
	"    float maskAvg = (1.0 + (MASK_PITCH - 1.0) * (1.0 - MASK_STRENGTH)) / MASK_PITCH;\n"
	"    col *= mask / maskAvg;\n"

	/* --- flicker, part 2: the brightness ------------------------------ */
	/* Both terms oscillate about zero; the mean brightness therefore stays
	   put. */
	"    col *= 1.0 + Flicker * (HUM_DEPTH * hum + FLICKER_DEPTH * wob);\n"

	/* --- give the light back, edge, gamma ----------------------------- */
	"    col *= BRIGHTNESS;\n"
	"    float vig = 1.0 - VIGNETTE * dot(w, w) * 0.5;\n"
	"    col *= max(vig, 0.0) * vis;\n"
	"    gl_FragColor = vec4(pow(max(col, vec3(0.0)), vec3(1.0 / GAMMA_OUT)), 1.0);\n"
	"}\n";

namespace
{
	// Where every slider starts, and where one goes back to when config.xml
	// does not name it.
	const float SLIDER_DEFAULT = 0.5f;

	// clamp() lets a NaN through, both of its comparisons being false, and
	// config.xml can hold one: TinyXML reads "nan" as a float.
	float sliderValue(float value)
	{
		return isFiniteFloat(value) ? clamp(value, 0.0f, 1.0f) : SLIDER_DEFAULT;
	}
}

U_Crt::U_Crt()
{
	locScanline = -1;
	locCurvature = -1;
	locBloom = -1;
	locFlicker = -1;
	locScanFlicker = -1;
	locConvergence = -1;
	locOverscan = -1;
	locTime = -1;
	locScanPhase = -1;
	// The overscan depends on the frame size, 640x480 throughout the tree.
	// present() sets it every frame; this value serves the first logic tick,
	// which converts the mouse before any present().
	frameSize = Vec2i(640, 480);
	crawlPhase = 0.0f;
	crawlTicks = 0;
	scanline = curvature = bloom = flicker = scanFlicker = convergence = SLIDER_DEFAULT;
}

U_Crt::~U_Crt()
{
}

const char* U_Crt::getFragmentSource() const
{
	return p_crtFragmentShader;
}

bool U_Crt::createGL()
{
	if(!Upscaler::createGL()) return false;

	locScanline    = glExtGetUniformLocation(program.id, "Scanline");
	locCurvature   = glExtGetUniformLocation(program.id, "Curvature");
	locBloom       = glExtGetUniformLocation(program.id, "Bloom");
	locFlicker     = glExtGetUniformLocation(program.id, "Flicker");
	locScanFlicker = glExtGetUniformLocation(program.id, "ScanFlicker");
	locConvergence = glExtGetUniformLocation(program.id, "Convergence");
	locOverscan    = glExtGetUniformLocation(program.id, "Overscan");
	locTime        = glExtGetUniformLocation(program.id, "Time");
	locScanPhase   = glExtGetUniformLocation(program.id, "ScanPhase");
	return true;
}

void U_Crt::present(const PresentContext& context)
{
	program.use(context);

	PresentProgram::setUniform(locScanline, scanline);
	PresentProgram::setUniform(locCurvature, curvature);
	PresentProgram::setUniform(locBloom, bloom);
	PresentProgram::setUniform(locFlicker, flicker);
	PresentProgram::setUniform(locScanFlicker, scanFlicker);
	PresentProgram::setUniform(locConvergence, convergence);

	// Fill in the frame size first, then ask for it.
	frameSize = context.frameSize;
	PresentProgram::setUniform(locOverscan, getOverscan());

	// The wall clock, not Engine::getTime(), which counts logic ticks and
	// stops with them; a screen flickers anyway. Wrapped at
	// CRT_FLICKER_CYCLE, a whole number of runs of every frequency in the
	// shader, so nothing jumps at the wrap and the shader's sin() arguments
	// stay small - and wrapped as an integer, before it becomes a float, or
	// after four and a half hours of running a float second no longer holds
	// the millisecond and the 29 Hz term starts to step.
	const uint cycleMs = static_cast<uint>(CRT_FLICKER_CYCLE * 1000.0f);
	PresentProgram::setUniform(locTime, 0.001f * static_cast<float>(SDL_GetTicks() % cycleMs));

	// The scan-line crawl is a ramp whose slope follows the slider, so it
	// cannot come from the wrapped Time without jumping at every wrap. It is
	// kept, and moved by each present's share of the clock, rather than
	// formed as slope times clock: that product is a float which loses the
	// millisecond after four and a half hours of running, and one step of
	// the slider would throw the lines by a random part of a period. The
	// unsigned difference is right across the clock's wrap. With the slider
	// at 0 the lines rest where the design puts them, at phase 0, and not
	// wherever the crawl had got to.
	const uint now = SDL_GetTicks();
	if(scanFlicker > 0.0f)
		crawlPhase = fmodf(crawlPhase + 0.001f * crtCrawlSpeed * scanFlicker * static_cast<float>(now - crawlTicks), 1.0f);
	else crawlPhase = 0.0f;
	crawlTicks = now;
	PresentProgram::setUniform(locScanPhase, crawlPhase);

	program.drawQuad(context);
}

float U_Crt::getOverscan() const
{
	// How far the raster stands back from the glass's edge, in fractions of
	// half the picture width. Zero on a flat tube, where the picture sits
	// point for point where every other filter puts it. Otherwise it makes
	// room for what the shader draws outside the picture: the raster's soft
	// edge, fading over twice EDGE_ROWS source rows, and one beam's
	// convergence offset, by which blue at the right edge and red at the left
	// reach past green. The curvature leaves the edge midpoints in
	// place, and there both would be cut off - measured without it: 0 black
	// rows above the picture at the top centre, 20 at nine tenths out; with
	// it, 2 rising to 26, the picture fading in over the next four. At the
	// right edge with convergence at full, red's raster dies at output
	// column 1274, green's at 1277, and blue's still burns at 1279.
	//
	// One value for both axes, or the pixels would stop being square: the
	// sum is needed across, the fade alone down, the rest is black surround.
	// It is a step: the moment the slider leaves 0, a 2x window's picture
	// steps back by six output pixels top and bottom and eight at the sides.
	if(curvature <= 0.0f) return 0.0f;

	const float fade   = 2.0f * (crtEdgeRows * 2.0f / frameSize.y);
	const float fringe = 2.0f * crtConvergenceMax / frameSize.x;
	return fade + fringe;
}

Vec2f U_Crt::warpToSource(const Vec2f& p) const
{
	// Exactly the formula from the shader above. p and the return value run
	// from -1 to 1, measured from the centre of the picture.
	if(curvature <= 0.0f) return p;

	const float a = curvature * crtCurveX;
	const float b = curvature * crtCurveY;
	const float k = 1.0f + getOverscan();
	return Vec2f(p.x * (1.0f + a * p.y * p.y) * k,
				 p.y * (1.0f + b * p.x * p.x) * k);
}

Vec2f U_Crt::warpToOutput(const Vec2f& s) const
{
	if(curvature <= 0.0f) return s;

	const float a = curvature * crtCurveX;
	const float b = curvature * crtCurveY;

	// The inverse. The two equations are coupled and have no closed form; as
	// a fixed point
	//
	//     x <- u / (1 + a*y^2)      y <- v / (1 + b*x^2)
	//
	// it contracts fast: after eight rounds the error is under 2.3e-4 pixels
	// at an absurd curvature and under 1e-5 anywhere the slider reaches.
	// Engine's cursor mapping takes pixel centres and floors in both
	// directions, so get(set(g)) == g exactly: measured, 0 of 34240 positions
	// off at curvature 0, 0.25, 0.5 and 1.0 and three window sizes. The one
	// exception is exactly 1x, the minimum window, where 640 window pixels
	// and 640 game pixels cannot both hold a non-identity warp and 0.5% of
	// positions land on a neighbouring tile. The overscan is a plain factor
	// and comes off first.
	const float k = 1.0f + getOverscan();
	const float u = s.x / k;
	const float v = s.y / k;

	float x = u;
	float y = v;
	for(int i = 0; i < 8; i++)
	{
		x = u / (1.0f + a * y * y);
		y = v / (1.0f + b * x * x);
	}
	return Vec2f(x, y);
}

void U_Crt::setScanline(float value)    { scanline = sliderValue(value); }
void U_Crt::setCurvature(float value)   { curvature = sliderValue(value); }
void U_Crt::setBloom(float value)       { bloom = sliderValue(value); }
void U_Crt::setFlicker(float value)     { flicker = sliderValue(value); }
void U_Crt::setScanFlicker(float value) { scanFlicker = sliderValue(value); }
void U_Crt::setConvergence(float value) { convergence = sliderValue(value); }

void U_Crt::loadConfig(TiXmlElement* p_config)
{
	// The defaults for what the file does not say, and for everything where
	// there is no file (p_config 0): the options dialog's Cancel reloads the
	// configuration to take back what the sliders changed.
	scanline = curvature = bloom = flicker = scanFlicker = convergence = SLIDER_DEFAULT;
	TiXmlElement* p_crt = p_config ? p_config->FirstChildElement("CrtUpscaler") : 0;
	if(!p_crt) return;

	float value = 0.0f;
	if(p_crt->QueryFloatAttribute("scanline", &value) == TIXML_SUCCESS)    setScanline(value);
	if(p_crt->QueryFloatAttribute("curvature", &value) == TIXML_SUCCESS)   setCurvature(value);
	if(p_crt->QueryFloatAttribute("bloom", &value) == TIXML_SUCCESS)       setBloom(value);
	if(p_crt->QueryFloatAttribute("flicker", &value) == TIXML_SUCCESS)     setFlicker(value);
	if(p_crt->QueryFloatAttribute("scanFlicker", &value) == TIXML_SUCCESS) setScanFlicker(value);
	if(p_crt->QueryFloatAttribute("convergence", &value) == TIXML_SUCCESS) setConvergence(value);
}

void U_Crt::saveConfig(TiXmlElement* p_config)
{
	TiXmlElement* p_crt = new TiXmlElement("CrtUpscaler");
	p_crt->SetDoubleAttribute("scanline", scanline);
	p_crt->SetDoubleAttribute("curvature", curvature);
	p_crt->SetDoubleAttribute("bloom", bloom);
	p_crt->SetDoubleAttribute("flicker", flicker);
	p_crt->SetDoubleAttribute("scanFlicker", scanFlicker);
	p_crt->SetDoubleAttribute("convergence", convergence);
	p_config->LinkEndChild(p_crt);
}
