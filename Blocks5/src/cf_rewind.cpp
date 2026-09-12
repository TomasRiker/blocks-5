#include "pch.h"
#include "cf_rewind.h"
#include "engine.h"
#include "texture.h"

namespace
{
	// How tall a strip is. A video head reads one track, and one track is one
	// video field - here it is a few rows, fine enough that the seam between
	// the two source images nowhere stands as a straight edge, and coarse
	// enough that 160 strips have to be drawn and not 480.
	const int STRIP_HEIGHT = 3;

	// How far the picture travels during the rewind, in picture heights. The
	// tape races and therefore the picture rolls - at that speed the vertical
	// hold no longer keeps up.
	//
	// A whole number, and not an arbitrary one: the offset then lands back on
	// a multiple of the picture height - zero - exactly as the crossfade ends.
	// At 6.5 the picture would sit half a screen out and jump straight when
	// the effect stops.
	//
	// Ten rather than seven because this is a distance and not a speed: over
	// the transition's 1.5 seconds, seven heights would read as leisurely
	// rather than frantic.
	const double ROLL_SCREENS = 10.0;

	// Over what part of the end the transport brakes and the vertical hold
	// locks again. Without it the garbled picture would stop with a cut.
	const double SETTLE = 0.18;

	// Sideways offset per strip: the head meets the track at an angle, and
	// every line therefore starts a little early or a little late.
	const double TRACKING_JITTER = 2.5;

	// And the same at the seam, where the two source images meet - the
	// tracking is at its worst there.
	const double SEAM_SHIFT = 16.0;

	// How frayed the seam is, as a fraction of the picture height. A sharp
	// seam would show an edge travelling across the picture with the finished
	// new picture behind it - exactly what is to be hidden here.
	const double SEAM_WIDTH = 0.30;

	// Noise bars: where the head lands between two tracks there is no picture
	// at all but snow. How many there are depends on the speed of the tape.
	const int NOISE_BARS = 5;
	const int NOISE_BAR_MIN = 6;
	const int NOISE_BAR_MAX = 22;

	// The snow over the whole picture and the grey wash on top of it. The
	// wash does half the work: VHS puts the colour under the picture as a
	// carrier of its own, and that does not survive the spooling - a picture
	// in search is almost grey.
	const double SNOW_ALPHA = 0.16;
	const double WASH_ALPHA = 0.22;

	// Edge length of the noise image. A power of two, because it is tiled.
	const int NOISE_SIZE = 256;

	// drawSnow writes its texture coordinates in fractions of the image, so
	// the noise is bound with the identity rather than with a texel scale.
	const Vec2d NOISE_TEXEL_SCALE(1.0, 1.0);

	// Where the recorder's on-screen display sits. Far enough in that the CRT
	// filter's curvature does not cut it off at the corner.
	const int OSD_X = 50;
	const int OSD_Y = 50;

	// And how rewind.png is divided up: the word on the left, the two
	// triangles next to it on the right. The height is that of the whole
	// image; whatever is empty below draws nothing, and this therefore
	// copes with taller lettering too.
	const int OSD_TEXT_WIDTH = 162;
	const int OSD_ARROWS_WIDTH = 56;
	const int OSD_HEIGHT = 64;

	// How long the arrows are on and off each time. A character generator
	// knows no crossfade: it switches.
	const uint OSD_BLINK_MS = 500;

	double wrap(double value, double range)
	{
		value = fmod(value, range);
		return (value < 0.0) ? value + range : value;
	}
}

CF_Rewind::CF_Rewind()
{
	p_osd = Manager<Texture>::inst().request("misc.png");
	startTicks = SDL_GetTicks();

	// The transport. The sound belongs to the effect and not to the place
	// that triggers it: there is only one way in here, and nobody can
	// therefore have the one without the other. It runs a little longer than
	// the crossfade to keep the run-down from being cut off with the picture.
	Engine::inst().playSound("rewind.ogg", false, 0.0, 100);

	// Snow, once and for all. Grey, not coloured: what the head picks up
	// between two tracks is noise with no colour carrier.
	unsigned char* p_pixels = new unsigned char[NOISE_SIZE * NOISE_SIZE * 3];
	for(int i = 0; i < NOISE_SIZE * NOISE_SIZE; i++)
	{
		const unsigned char v = static_cast<unsigned char>(randomInt() & 255);
		p_pixels[i * 3 + 0] = v;
		p_pixels[i * 3 + 1] = v;
		p_pixels[i * 3 + 2] = v;
	}

	glGenTextures(1, &noiseID);
	GL::bindTexture(noiseID, NOISE_TEXEL_SCALE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, NOISE_SIZE, NOISE_SIZE, 0,
				 GL_RGB, GL_UNSIGNED_BYTE, p_pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GL::bindTexture(0, Vec2d(1.0, 1.0));

	delete[] p_pixels;
}

CF_Rewind::~CF_Rewind()
{
	// The Engine deletes the crossfade in the main loop; the GL context is
	// therefore still up.
	GL::deleteTexture(noiseID);
	if(p_osd) p_osd->release();
}

void CF_Rewind::drawStrip(int y,
						  int height,
						  int sourceY,
						  double shift) const
{
	// The texture coordinates are in pixels, which is what screenTexelScale
	// is for; the strip's own bind is what puts it in the matrix.
	glBegin(GL_QUADS);
	glTexCoord2d(shift, sourceY);
	glVertex2i(0, y);
	glTexCoord2d(shift + screenSize.x, sourceY);
	glVertex2i(screenSize.x, y);
	glTexCoord2d(shift + screenSize.x, sourceY + height);
	glVertex2i(screenSize.x, y + height);
	glTexCoord2d(shift, sourceY + height);
	glVertex2i(0, y + height);
	glEnd();
}

void CF_Rewind::drawSnow(int y,
						 int height,
						 double alpha) const
{
	const double u = random(0.0, 1.0);
	const double v = random(0.0, 1.0);
	const double du = static_cast<double>(screenSize.x) / NOISE_SIZE;
	const double dv = static_cast<double>(height) / NOISE_SIZE;

	glColor4d(1.0, 1.0, 1.0, alpha);
	glBegin(GL_QUADS);
	glTexCoord2d(u, v);
	glVertex2i(0, y);
	glTexCoord2d(u + du, v);
	glVertex2i(screenSize.x, y);
	glTexCoord2d(u + du, v + dv);
	glVertex2i(screenSize.x, y + height);
	glTexCoord2d(u, v + dv);
	glVertex2i(0, y + height);
	glEnd();
}

/* Why a rewind and not just any effect: on a restart the game jumps from the
   current state to the beginning with nothing in between. A video recorder in
   picture search does exactly the same and nobody minds - because the tape
   runs faster than the head can follow a track, and every strip of the
   picture is therefore read from a different place on the tape, which is to
   say from a different moment. Strips of two pictures side by side are not a
   trick standing in for something the game never had; they are what a
   recorder actually puts out.

   What follows from that, and what hides the cut:

   - Nothing lies between the tracks; snow comes through there instead. Those
     are the noise bars travelling through the picture.
   - The vertical hold no longer keeps up, and the picture rolls.
   - The head meets the track at an angle, and every line slips a little
     sideways - the picture frays.
   - VHS carries the colour separately and at a low frequency under the
     picture; that does not survive the search. Hence the grey wash.

   One thing must NOT jitter: the on-screen display "<< REW". It comes from the
   recorder's own character generator and is mixed in behind the tape path. It
   stands steady while everything else tears - and that is exactly what makes
   the garbled picture read as a machine. */
void CF_Rewind::render(double t,
					   uint oldImageID,
					   uint newImageID)
{
	Engine& engine = Engine::inst();

	GL::setTexturing(true);
	glColor4d(1.0, 1.0, 1.0, 1.0);

	// The tape spins up and brakes again.
	const double eased = t * t * (3.0 - 2.0 * t);
	const double roll = eased * ROLL_SCREENS * screenSize.y;

	// And at the end the picture settles: tracking, snow and wash go back
	// while the tape coasts to a stop.
	const double settle = clamp((1.0 - t) / SETTLE, 0.0, 1.0);

	// The seam travels up through the picture: the tape runs backwards. Over
	// SEAM_WIDTH chance decides, to keep it from being an edge.
	const double seam = (1.0 + SEAM_WIDTH) * (1.0 - t) - 0.5 * SEAM_WIDTH;

	for(int y = 0; y < screenSize.y; y += STRIP_HEIGHT)
	{
		const int height = min(STRIP_HEIGHT, screenSize.y - y);
		const double where = static_cast<double>(y) / screenSize.y;

		// Above and below the seam the answer is clear, in between it is not.
		const double distance = (where - seam) / SEAM_WIDTH;
		const bool useNew = (distance + random(-0.5, 0.5) > 0.0);

		// How bad the tracking is here: worst at the seam.
		const double closeness = clamp(1.0 - fabs(distance), 0.0, 1.0);
		const double shift = settle * (random(-TRACKING_JITTER, TRACKING_JITTER)
									 + closeness * random(-SEAM_SHIFT, SEAM_SHIFT));

		// The row this strip shows. Wrapped by hand and not left to the
		// texture's GL_REPEAT: the picture fills only 480 of the 512 rows,
		// the rest of the power of two has never been written.
		const int sourceY = static_cast<int>(wrap(y + roll, screenSize.y));
		const int overlap = sourceY + height - screenSize.y;

		GL::bindTexture(useNew ? newImageID : oldImageID, screenTexelScale);
		if(overlap <= 0) drawStrip(y, height, sourceY, shift);
		else
		{
			// This is exactly where a recorder has its head switching
			// point: the end of one video field and the start of the
			// next, with a torn strip in between. Hence both halves
			// separately, with different offsets.
			drawStrip(y, height - overlap, sourceY, shift);
			drawStrip(y + height - overlap, overlap, 0,
					  shift + settle * random(-SEAM_SHIFT, SEAM_SHIFT));
		}
	}

	// --- Noise ------------------------------------------------------------
	// A scale of its own, which the bind carries: the noise image is sampled
	// in 0..1 and not in pixels of the screen.
	GL::bindTexture(noiseID, NOISE_TEXEL_SCALE);

	// The bars travel downward and are fully opaque: no picture lies there.
	for(int i = 0; i < NOISE_BARS; i++)
	{
		const double speed = 0.6 + 0.5 * i;
		const int height = random(NOISE_BAR_MIN, NOISE_BAR_MAX);
		const int y = static_cast<int>(wrap((static_cast<double>(i) / NOISE_BARS + eased * speed)
											* screenSize.y, screenSize.y));
		drawSnow(y, min(height, screenSize.y - y), settle);
	}

	// And the snow over everything, added.
	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
	drawSnow(0, screenSize.y, settle * SNOW_ALPHA);
	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

	// --- The grey wash -----------------------------------------------------
	GL::setTexturing(false);
	glBegin(GL_QUADS);
	glColor4d(0.62, 0.63, 0.60, settle * WASH_ALPHA);
	glVertex2i(0, 0);
	glVertex2i(screenSize.x, 0);
	glVertex2i(screenSize.x, screenSize.y);
	glVertex2i(0, screenSize.y);
	glEnd();

	// --- The on-screen display ---------------------------------------------
	// It belongs to the recorder's character generator and not to the tape: it
	// therefore neither fades in nor out and takes no part in settle. The word
	// stands the whole time, the arrows blink - hard, as if switched, and
	// counted from the start of the effect, which is what makes them start
	// visible.
	if(p_osd)
	{
		engine.renderSprite(p_osd, Vec2i(OSD_X, OSD_Y), Vec2i(0, 112),
							Vec2i(OSD_TEXT_WIDTH, OSD_HEIGHT), Vec4d(1.0));

		if(((SDL_GetTicks() - startTicks) / OSD_BLINK_MS) % 2 == 0)
		{
			engine.renderSprite(p_osd, Vec2i(OSD_X + OSD_TEXT_WIDTH, OSD_Y),
								Vec2i(OSD_TEXT_WIDTH, 112),
								Vec2i(OSD_ARROWS_WIDTH, OSD_HEIGHT), Vec4d(1.0));
		}
	}

	GL::setTexturing(false);
}
