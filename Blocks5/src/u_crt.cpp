#include "pch.h"
#include "u_crt.h"
#include "glextensions.h"

/* A monitor from the nineties, reimplemented in the present pass.

   The filter reconstructs nothing. It lays a rendering over the frame as
   drawn, and is honest about what it does. It is stable too: an edge-directed
   filter like xBR is a set of step() decisions against thresholds, and a
   colour difference of 1/255 under a semi-transparent dialog flips them -
   measured, 1% of the pixels moved by up to 154. Here there is no threshold
   and no edge detection, only smooth functions of the source colour and the
   position: move the input by 1 and the output moves by about 1.

   WHICH MONITOR? That is the real question, and it sits in exactly one number,
   SCANLINE_PERIOD.

   Visible gaps between the lines are an artifact of 240p: a console drew 240
   lines into a 480-line raster, and in between the phosphor stayed dark. A VGA
   monitor at 640x480 drew all 480 lines and their beam profiles overlapped -
   there were no gaps. Blocks 5 is a 640x480 Windows game; its honest reference
   is the beige 15-inch monitor, not the television with the console attached.
   SCANLINE_PERIOD = 1.0 renders that. 2.0 pretends 240 lines arrive and gives
   the console look most people today mean by "CRT".

   That is no small matter of appearance: at period 1 and a window of double
   the size, both output rows sit equally far from the line centre, leaving no
   stripe visible at all - physically right and useless as an effect. Only
   period 2 produces a visible structure at 2x. The scan-line slider fades the
   effect in; the period decides which effect it fades into.

   RESOLUTION. Almost everyone plays at exactly 2x (1280x960): on 1080p and
   1440p getDefaultWindowSize() gives 2x, and only 1600p gets 3x. Two output
   pixels per source pixel is not much. The mask therefore sits in the *output*
   raster and not in the source raster - a real shadow mask belongs to the
   glass and does not change when you switch resolution. MASK_PITCH is given in
   output pixels and stays equally fine at every factor.

   DISTORTION. The glass of a tube curved outward. The mapping goes from output
   pixel to source pixel, because that is exactly the direction a fragment
   shader asks in:

       src.x = out.x * (1 + a*out.y^2)
       src.y = out.y * (1 + b*out.x^2)

   with out and src in -1..1 from the centre of the picture. Only the corners
   move outward; this mapping leaves the edge midpoints where they are.

   That is what the overscan on top of it is for (getOverscan()): without it
   the picture would reach the last output row at the edge midpoints, and
   everything the shader draws outside the picture - the soft edge of the
   raster, the convergence fringe, the halo - would have nowhere to go. The
   whole raster therefore steps back from the edge of the glass, just far
   enough for both to fit. At curvature 0 it is 0, and the picture then covers
   exactly what the other filters cover - measured, zero shift in either axis
   against sharp-fit.

   The same mapping stands once more in C++ in warpToSource()/warpToOutput(),
   because the mouse position has to go through it; the inverse is a
   fixed-point iteration. Change it here and you change it there too.

   No #version: 110 on the desktop, 100 on the embedded shading language,
   compiled as both. No array constants and no loops with a variable bound -
   version 100 knows neither of them. */

/* What the shader and the C++ side both need stands here once: as text in the
   shader and as a double beside it. The curvature goes through the mouse
   (warpToSource/warpToOutput), the other two into getOverscan(). */

#define CRT_CURVE_X 0.10
#define CRT_CURVE_Y 0.13
/* Half the width of the raster's soft edge, in source rows; rasterMask fades
   out over two of them. */
#define CRT_EDGE_ROWS 1.0
/* Convergence offset per beam at the edge of the picture, in source columns.
   In full at the shader constant of the same name. */
#define CRT_CONVERGENCE_MAX 1.6
/* Line periods per second with the flicker at full; see CRAWL_JITTER. */
#define CRT_CRAWL_SPEED 1.2
/* After this many seconds the flicker repeats exactly. This too stands twice -
   in the shader and in present(), which takes the clock modulo it. */
#define CRT_FLICKER_CYCLE 8.0
#define CRT_STR2(x) #x
#define CRT_STR(x) CRT_STR2(x)

static const double crtCurveX = CRT_CURVE_X;
static const double crtCurveY = CRT_CURVE_Y;
static const double crtEdgeRows = CRT_EDGE_ROWS;
static const double crtConvergenceMax = CRT_CONVERGENCE_MAX;
static const double crtCrawlSpeed = CRT_CRAWL_SPEED;

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

	/* Which tube. 1.0 = VGA monitor, all 480 lines, no gaps (hence at 2x
	   practically no visible stripes). 2.0 = pretend 240 lines arrive: the
	   console look, visible at 2x as well. Values in between are allowed but
	   look like a fault. */
	"const float SCANLINE_PERIOD = 2.0;\n"

	/* Width of the electron beam, in line pitches. Smaller = narrower beam =
	   deeper and harder stripes. 0.5 is strong, 0.8 gentle. */
	"const float BEAM_WIDTH = 0.55;\n"

	/* The mask, in OUTPUT PIXELS per RGB triple. 3.0 is a real stripe mask:
	   one pixel red, one green, one blue. At 2x that beats against the source
	   raster (period 2) into a pattern of period 6; 2.0 or 4.0 are calmer
	   there. */
	"const float MASK_PITCH = 3.0;\n"
	/* How dark the stripes that are not currently excited become. 0 = no mask,
	   1 = the other two channels fully off (far too much). 0.3 is plainly
	   visible without colours tipping over. */
	"const float MASK_STRENGTH = 0.30;\n"

	/* Halation: light scatters in the glass and comes back as a soft halo.
	   Only what is brighter than the threshold glows - dark areas stay sharp,
	   and that asymmetry is exactly what tells it apart from a blur. Radius in
	   source pixels.

	   BLOOM_STRENGTH is the value with the slider at full; the slider (uniform
	   Bloom) scales it. Set to 0 the whole block is compiled away - see
	   below. */
	"const float BLOOM_STRENGTH  = 0.38;\n"
	"const float BLOOM_THRESHOLD = 0.22;\n"
	"const float BLOOM_RADIUS    = 2.5;\n"   /* inner ring, source pixels */
	"const float BLOOM_OUTER     = 2.6;\n"   /* outer ring as a multiple of it */

	/* Horizontal bandwidth. The video signal ran analog and band-limited, which
	   made a tube softer across than down. In fractions of a source pixel;
	   0 = as sharp as sharp-fit. */
	"const float SOFTNESS = 0.35;\n"

	/* Convergence. A colour tube has three electron beams, and they never meet
	   the mask at exactly the same place: in the centre they are converged,
	   and toward the rim they drift apart, because the deflection is largest
	   there. That shows up as a red and a blue fringe on vertical edges,
	   nothing in the middle and most at the edge.

	   That is expressly *not* chromatic aberration - that happens in a lens,
	   because glass bends wavelengths differently, and a tube has none. Here
	   three pictures simply lie side by side.

	   With the slider at full the value is the displacement *of each beam* at
	   the left and right edge of the picture, in source pixels; red and blue
	   move against each other, which makes the visible fringe twice as wide. A
	   well-adjusted set stayed below it, a tired cheap set reached one or two
	   triads in the corners, which is one or two source pixels here. Set to 0
	   the block is compiled away, as with the halation. */
	"const float CONVERGENCE_MAX = " CRT_STR(CRT_CONVERGENCE_MAX) ";\n"

	/* Curvature with the slider at full. Lottes takes 1/32 and 1/24; here it
	   may go further, since the slider does not normally sit at the stop. The
	   numbers come from the macros below - the shader and the mouse conversion
	   in engine.cpp have to compute the same curvature, and a pair of numbers
	   that has to be maintained in two places drifts apart eventually. The
	   preprocessor puts the same text in here that C++ sees as a double. */
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

	/* Flicker, the way an old television had it. It is two things, and each
	   has a slider of its own, because they are very much wanted separately:

	   "Flicker" (uniform Flicker) is the fast shimmer of the *brightness* -
	   three oscillations at 12, 19 and 29 Hz that keep re-overlapping and
	   never fall into a pattern - plus, much weaker, the mains hum: a broad,
	   dark bar that rolls slowly down the picture, because the mains frequency
	   beats against the frame frequency.

	   "Scan flicker" (uniform ScanFlicker) concerns the *position* of the
	   lines: they drift slowly downward and shimmer as they go. See
	   CRAWL_JITTER.

	   Every term has mean zero and therefore costs no brightness, and all of
	   them hang off the clock alone, never off the previous frame - the fault
	   xBR foundered on cannot arise here. Values with the slider at full;
	   where the bar is unwanted, set HUM_DEPTH to 0. */
	"const float FLICKER_DEPTH = 0.0367;\n"  /* fast brightness shimmer */
	"const float HUM_DEPTH     = 0.0147;\n"  /* depth of the rolling bar */
	"const float HUM_BARS      = 0.75;\n"    /* how many bars fit the picture */
	"const float HUM_ROLLS     = 3.0;\n"     /* runs per FLICKER_CYCLE */
	/* After this many seconds the flicker repeats exactly. All the frequencies
	   below are whole multiples of it, which makes the transition seamless and
	   lets the clock start over on every run - otherwise float would go coarse
	   eventually. From the macro above, because present() has to take the
	   clock modulo the same value. */
	"const float FLICKER_CYCLE = " CRT_STR(CRT_FLICKER_CYCLE) ";\n"

	/* The line structure does not stand still. On a real tube it drifts slowly
	   downward - the scan-line crawl, because the line and frame frequencies
	   never go into an exact ratio - and shimmers a little as it does. Without
	   that the stripes look painted on.

	   CRAWL_SPEED is in line periods per second with the slider at full and is
	   the only value computed on the CPU: the phase has to come from the
	   unwrapped clock, or it would jump by fract(Flicker * speed) of a period
	   at every wrap. That is why the number stands once more below as a macro,
	   like the curvature.
	   CRAWL_JITTER is the fast shimmer of the phase, in periods - that runs in
	   the shader, because it is an oscillation with a whole-number frequency
	   and wraps seamlessly by itself. */
	"const float CRAWL_JITTER = 0.05;\n"

	/* Gamma. A tube had about 2.4; in between everything is computed in linear
	   light, or the halo turns into grey haze. */
	"const float GAMMA_IN  = 2.4;\n"
	"const float GAMMA_OUT = 2.2;\n"
	/* Purely a matter of taste. Mask and stripes do take light away, but the
	   shader below computes that back out itself (MASK_AVG and SCAN_AVG), and
	   from the constants - change something above and the brightness comes
	   back on its own, with nothing to adjust here. 1.0 = as bright as with no
	   filter. */
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

	/* Fetch one texel, with the same piecewise linear remapping as sharp-fit,
	   only softened by SOFTNESS. The ramp at the pixel boundary is 1/N source
	   pixels wide there; here it gets wider, and that is exactly the limited
	   bandwidth. Vertically it stays sharp - a tube ran soft across and sharp
	   down. */
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

	/* The halo does not need the sharp remapping from fetch() - a soft image
	   of a soft image - which saves the ramp arithmetic eight times over.

	   The averaging happens in *linear* light, though, and per tap. Averaging
	   first and converting afterwards leaves no visible halo at all: the ring
	   around a bright spot is a mixture of bright and dark, and pow() on that
	   mean pushes it far below the threshold. The mean belongs in linear
	   light, where light actually adds.

	   The conversion is x*x rather than pow(x, GAMMA_IN) - gamma 2.0 instead
	   of 2.4. For a soft halo the difference is meaningless, and it costs a
	   multiplication instead of a pow(); eight of those per output pixel would
	   otherwise be the most expensive item in the whole shader. */
	"vec3 toLinear(vec3 c) { return pow(max(c, vec3(0.0)), vec3(GAMMA_IN)); }\n"

	/* The edge of the raster, as the distance function of a rounded rectangle:
	   1 inside, 0 outside, and between them an edge two edge wide.

	   wc is the source point *this channel* reads, not the output point. That
	   is what counts: a colour tube paints three rasters, and if the red one
	   is narrower than the green one then the red picture ends first - that is
	   exactly how you spot a misconverged set, before you look at any edge
	   inside the picture.

	   The rectangle is edge larger than the picture, which puts the edge
	   entirely outside it: inside every point is untouched, and without
	   curvature the picture is point for point as large as with every other
	   filter. The room to fade out is made by the overscan, see
	   getOverscan(). */
	"float rasterMask(vec2 wc, float r, float edge)\n"
	"{\n"
	"    vec2  q  = abs(wc) - (1.0 - r + edge);\n"
	"    float sd = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;\n"
	"    return 1.0 - smoothstep(-edge, edge, sd);\n"
	"}\n"

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
	/* And one step further out, keeping the raster off the edge of the glass.
	   The factor arrives ready-made from getOverscan(); with a flat tube it is
	   0 and this line does nothing. */
	"    w *= 1.0 + Overscan;\n"
	"    vec2 suv = w * 0.5 + 0.5;\n"

	/* --- convergence, part one: how far red and blue are off ---------- */
	/* Up here because the edge already needs it. cx is the offset per beam in
	   texture coordinates, cw the same in w - red reads cx further right, which
	   puts its picture cx further left. */
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
	/* Green stays put and is thereby the reference colour, the colour a set got
	   converged on at the bench. Red and blue are shifted against each other,
	   proportional to w.x - hence zero in the middle and largest at the edge.
	   The edge of the raster is already set per channel above; only the content
	   is added here.

	   Horizontal only, although a beam could sit off vertically too: that
	   would need its own two rows and its own beam profile per channel, eight
	   fetches instead of four, and the line structure of the tube hides a
	   vertical shift almost entirely anyway. The fringe anybody remembers
	   stands on vertical edges.

	   Four extra fetches - the resampling triples, and that is paid for,
	   because the slider sits at 0.5 like the others. Measured on llvmpipe at
	   1280x960, a presentFrame() then costs 30.6 instead of 25.3 ms, a fifth
	   more; on a real graphics card it is nothing. The if hands all of it back
	   to anyone who turns the slider right down: the condition is the same for
	   the whole draw call. */
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
	/* Eight taps on a ring. The halo is soft and the star shape does not show
	   in it. It is at the same time the most expensive part of the shader -
	   measured, just under half - which is why it sits inside an if on a
	   constant: set BLOOM_STRENGTH to 0 and the compiler throws the whole block
	   away (measured: 7.9 then falls to 4.2). */
	"    if(BLOOM_STRENGTH > 0.0 && Bloom > 0.0)\n"
	"    {\n"
	/* Two rings instead of one, at the same number of taps: four axial on the
	   inner one, four diagonal on the outer one. Eight taps on a single radius
	   give a hard-edged ring rather than a halo; with two radii it falls off
	   softly over the whole distance. */
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
	/* They are needed twice - for the brightness further down and for the
	   position of the lines right here - hence compute them once. All the
	   frequencies are whole runs per FLICKER_CYCLE, letting the clock wrap
	   seamlessly; 97, 151 and 233 per 8 s are about 12, 19 and 29 Hz, and they
	   are coprime, which keeps the superposition from repeating any earlier. */
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
	/* Distance to the nearest line centre, measured in periods. At period 1
	   both output rows sit equally far away at 2x and nothing is visible; that
	   is right, and it is the reason for SCANLINE_PERIOD.

	   ScanPhase pushes the whole pattern slowly downward, CRAWL_JITTER makes
	   it shimmer as it goes. With Scanline at 0 both drop out by themselves -
	   there are then no lines that could crawl. */
	"    float ph = sy / SCANLINE_PERIOD + ScanPhase + ScanFlicker * CRAWL_JITTER * wob;\n"
	"    float dc = abs(fract(ph) - 0.5) * 2.0;\n"
	"    float k  = BEAM_WIDTH * BEAM_WIDTH * 4.0;\n"
	"    float beam = exp(-(dc * dc) / k);\n"
	/* The mean of the profile over one period, approximated with five sample
	   points. That gives beam/SCAN_AVG a mean of 1, and mix(1, ., S) keeps it
	   for every slider setting - the stripes therefore cost no brightness,
	   however narrow the beam is set. All the inputs are constants, which the
	   compiler folds away. */
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

	/* --- give the light back, edge, gamma ----------------------------- */
	/* --- flicker, part 2: the brightness ------------------------------ */
	/* Both terms oscillate about zero; the mean brightness therefore stays
	   put. */
	"    col *= 1.0 + Flicker * (HUM_DEPTH * hum + FLICKER_DEPTH * wob);\n"

	"    col *= BRIGHTNESS;\n"
	"    float vig = 1.0 - VIGNETTE * dot(w, w) * 0.5;\n"
	"    col *= max(vig, 0.0) * vis;\n"
	"    gl_FragColor = vec4(pow(max(col, vec3(0.0)), vec3(1.0 / GAMMA_OUT)), 1.0);\n"
	"}\n";

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
	// The overscan hangs off the frame size. It is 640x480 throughout the
	// tree, and present() fills it in before every frame anyway - the value
	// here is only for the first logic tick, which already converts the mouse.
	frameSize = Vec2i(640, 480);
	scanline = 0.5;
	curvature = 0.5;
	bloom = 0.5;
	flicker = 0.5;
	scanFlicker = 0.5;
	convergence = 0.5;
}

U_Crt::~U_Crt()
{
}

bool U_Crt::createGL()
{
	if(!program.create(p_crtFragmentShader, "crt fragment")) return false;

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

void U_Crt::destroyGL()
{
	// The uniform locations stay as they are. They are only valid while there
	// is a program anyway, and a second list would be a second list that
	// somebody forgets to extend.
	program.destroy();
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

	// The wall clock, not Engine::getTime() - that counts logic ticks and
	// stops when the game pauses; a screen flickers anyway. The cycle is
	// CRT_FLICKER_CYCLE and every frequency in it is a whole multiple of it,
	// leaving nothing to jump at the wrap.
	const double seconds = static_cast<double>(SDL_GetTicks()) * 0.001;
	PresentProgram::setUniform(locTime, fmod(seconds, CRT_FLICKER_CYCLE));

	// The scan-line crawl is the one term computed here: it is a ramp, not an
	// oscillation, and its slope depends on the slider - from the
	// already-wrapped clock the phase would jump at every wrap.
	PresentProgram::setUniform(locScanPhase,
							   fmod(seconds * crtCrawlSpeed * scanFlicker, 1.0));

	program.drawQuad(context);
}

double U_Crt::getOverscan() const
{
	// How far the raster stands back from the edge of the glass, in fractions
	// of half the picture width. With a flat tube not at all: the picture then
	// sits point for point where it does with every other filter, and that is
	// exactly the point of the slider's zero setting.
	//
	// Otherwise there has to be room beside it for what the shader draws
	// outside the picture: the soft edge of the raster, which fades out over
	// twice EDGE_ROWS source rows, and the convergence offset by which the red
	// and the blue raster stand apart at the edge. Without that gap both are
	// cut off at the edge midpoint - there the curvature leaves the pixel
	// where it is, while it pushes it outward toward the corners.
	//
	// One single value for both axes, or the pixels would no longer be square:
	// horizontally the sum is needed, vertically only the first term, and the
	// rest is black surround there.
	if(curvature <= 0.0) return 0.0;

	const double fade   = 2.0 * (crtEdgeRows * 2.0 / frameSize.y);
	const double fringe = 2.0 * crtConvergenceMax / frameSize.x;
	return fade + fringe;
}

Vec2d U_Crt::warpToSource(const Vec2d& p) const
{
	// Exactly the formula from the shader above. p and the return value run
	// from -1 to 1, measured from the centre of the picture.
	if(curvature <= 0.0) return p;

	const double a = curvature * crtCurveX;
	const double b = curvature * crtCurveY;
	const double k = 1.0 + getOverscan();
	return Vec2d(p.x * (1.0 + a * p.y * p.y) * k,
				 p.y * (1.0 + b * p.x * p.x) * k);
}

Vec2d U_Crt::warpToOutput(const Vec2d& s) const
{
	if(curvature <= 0.0) return s;

	const double a = curvature * crtCurveX;
	const double b = curvature * crtCurveY;

	// The inverse. The pair of equations is coupled - x depends on y and vice
	// versa - and has no closed form; as a fixed point
	//
	//     x <- u / (1 + a*y^2)      y <- v / (1 + b*x^2)
	//
	// it contracts very fast: after eight rounds the error is under 2.3e-4
	// pixels even at an absurd curvature.
	// The overscan is a smooth factor and is taken back out beforehand.
	const double k = 1.0 + getOverscan();
	const double u = s.x / k;
	const double v = s.y / k;

	double x = u;
	double y = v;
	for(int i = 0; i < 8; i++)
	{
		x = u / (1.0 + a * y * y);
		y = v / (1.0 + b * x * x);
	}
	return Vec2d(x, y);
}

void U_Crt::setScanline(double value)    { scanline = clamp(value, 0.0, 1.0); }
void U_Crt::setCurvature(double value)   { curvature = clamp(value, 0.0, 1.0); }
void U_Crt::setBloom(double value)       { bloom = clamp(value, 0.0, 1.0); }
void U_Crt::setFlicker(double value)     { flicker = clamp(value, 0.0, 1.0); }
void U_Crt::setScanFlicker(double value) { scanFlicker = clamp(value, 0.0, 1.0); }
void U_Crt::setConvergence(double value) { convergence = clamp(value, 0.0, 1.0); }

void U_Crt::loadConfig(TiXmlElement* p_config)
{
	TiXmlElement* p_crt = p_config->FirstChildElement("CrtUpscaler");
	if(!p_crt) return;

	// Read only what is there and reset nothing: the options dialog's Cancel
	// button calls loadConfig() in the middle of the game, and on a first
	// start the file does not exist at all yet.
	double value = 0.0;
	if(p_crt->QueryDoubleAttribute("scanline", &value) == TIXML_SUCCESS)    setScanline(value);
	if(p_crt->QueryDoubleAttribute("curvature", &value) == TIXML_SUCCESS)   setCurvature(value);
	if(p_crt->QueryDoubleAttribute("bloom", &value) == TIXML_SUCCESS)       setBloom(value);
	if(p_crt->QueryDoubleAttribute("flicker", &value) == TIXML_SUCCESS)     setFlicker(value);
	if(p_crt->QueryDoubleAttribute("scanFlicker", &value) == TIXML_SUCCESS) setScanFlicker(value);
	if(p_crt->QueryDoubleAttribute("convergence", &value) == TIXML_SUCCESS) setConvergence(value);
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
