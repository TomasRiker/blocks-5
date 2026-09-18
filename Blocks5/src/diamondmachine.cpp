#include "pch.h"
#include "diamondmachine.h"
#include "presets.h"
#include "engine.h"
#include "particlesystem.h"
#include "sound.h"
#include "soundinstance.h"

/* The conversion's shower of sparks.

   A block is not faded out and replaced by a diamond; it is taken apart and
   put back together: first sparks fly outward in the block's colours, then
   from the cloud they leave behind sparks come back, taking on the colour
   the diamond has where they land and expire.

   TIMETABLE. The machine counts a hundred ticks, and its own animation frame
   changes at 20, 40, 60 and 80 - the fifth stands still from 80 to 100. The
   phases hang off that, which keeps the animation and the sparks off two
   separate clocks:

       tick      0        20              53        80          100
       outward   |=== emitted ===========|                            flies to 80
       inward             |=== emitted ==============|                flies to 100
       visible   out only | both ------------------- | in only  |

   A spark lives on after it has been emitted, and the two ends are computed
   from that. Outward, emission has to stop a whole lifetime before 80
   (80 - 27 = 53), leaving the end nothing but collecting. Inward, every
   spark lands at 100 whenever it sets off - the lifetime is worked out for
   that - and 92 is the point from which the time left would no longer be a
   flight but a flash on the spot.

   LANDING EXACTLY. The integrator is position += velocity; velocity *=
   damping. Over n ticks a spark covers v0 * (1 - d^n) / (1 - d), and the
   formula serves both directions: outward it says where the cloud ends - and
   with it where the inward sparks may start - and inward it gives the v0
   with which one arrives exactly on its target. d below 1 brakes, d above 1
   accelerates; that is why the way out peters out and the way in sucks the
   spark in.

   DUST, NOT EMBERS. An outward spark carries the plain colour of the texel
   it came from and only grows paler - OUT_BRIGHT and OUT_END are both 1.
   Glowing sparks read as welding, and the machine is handed rock, ice and
   grass as readily as metal. For the same reason there are many, large and
   slow ones rather than fewer, smaller and faster: this is meant to be a
   block falling apart.

   Nothing here is blended additively. There the result depends on the
   background, and the same brown would be an ember over rock and a glaring
   yellow over grass. Inward the colour still starts above 1, where GL clamps
   it to [0,1] - but that is not a glow; it is the way around a green cast,
   and the reason stands where it is computed.

   ABORT. The block can be pushed away, blown up or switched off at the last
   moment. The sparks must not vanish then, which would be a hole in the
   middle of the motion: they run their own way back, with a mirrored
   lifetime, and what is still outstanding decides how long that takes
   (abortConversion()). For the machine to find them again they carry its id:
   Particle::id, the one field that stays 0 everywhere else in the game.

   The inward sparks always turn round - they were flying at a diamond that
   is not coming. The outward ones depend on whether the block still exists:
   pushed aside, or standing still because the power is off, and they fly
   back to it and are sucked in again; destroyed, and they fly on untouched,
   because then flying apart is exactly right.
 */

namespace
{
	// The phases, in ticks of the machine's counter.
	const int CONVERSION_TICKS = 100;  // then the block becomes the diamond
	const int SPARK_OUT_FULL = 20;
	const int SPARK_OUT_END  = 53;
	const int SPARK_IN_START = 20;
	const int SPARK_IN_FULL  = 60;
	const int SPARK_IN_END   = 92;

	// The sound of a conversion that is not going to happen. The slides are
	// exponential and run once per logic tick, so the speed is the fraction
	// of the way left that is covered each tick: at 0.04 the volume halves
	// every 17 ticks, a third of a second, and is inaudible after about one.
	// The pitch runs down with it, which is the sound a machine makes as it
	// loses what it was doing.
	//
	// diamondmachine.ogg lasts exactly the two seconds of the conversion, so
	// what is left to fade is whatever the abort came early enough to leave.
	const float SOUND_FADE_SPEED = 0.04f;
	const float SOUND_FADE_PITCH = 0.35f;

	// Every inward spark lives exactly as long as the conversion has left -
	// no matter when it sets off. They therefore do not arrive spread over a
	// minute but all in the same instant, and that instant is the one the
	// diamond stands in. The last ones still need some distance.
	const int SPARK_IN_MIN_LIFE = 8;

	// The image in particles.png, in pixels, and the only choice there is
	// here: the particle is multiplied by its texture region. (32,0) looks
	// right as a solid blob but is a pre-coloured orange (231,152,41) - a
	// cyan spark on it comes out olive. The only one that is neutral and
	// dense at once is (32,32): a round disc in pure white. (0,32) would be
	// a white four-pointed star, should it ever have to sparkle.
	const int SPARK_SPRITE_X = 32;
	const int SPARK_SPRITE_Y = 32;

	// Outward: the block falls apart.
	const float OUT_RATE     = 6.0f;   // sparks per tick at full rate
	const float OUT_SPEED_MIN = 0.5f;
	const float OUT_SPEED_MAX = 1.1f;
	const float OUT_DAMPING  = 0.95f;  // below 1: brakes
	const float OUT_BRIGHT   = 1.0f;   // brightness in the first tick
	const float OUT_END      = 1.0f;   // brightness in the last
	const float OUT_ALPHA    = 0.85f;  // opacity at the start
	const int   OUT_LIFE     = 27;
	const float OUT_SIZE     = 0.42f;

	// Inward: the diamond is put together.
	const float IN_RATE   = 4.0f;
	// How much faster a spark is at the end of its flight than at the start.
	// The damping is computed from it, because the lifetime is only fixed
	// when the spark sets off - the approach then looks the same at any
	// duration: creep for a long time, snap shut at the end.
	const float IN_ACCEL  = 3.0f;
	const float IN_START  = 1.4f;      // start colour brightness, see below
	const float IN_BRIGHT = 1.0f;      // brightness on impact
	const float IN_ALPHA  = 0.85f;     // opacity on impact
	const float IN_SIZE   = 0.38f;

	// Fraction of the full rate. Both ramps are linear and not switched: a
	// hard changeover would show the middle as a plateau with both
	// directions running flat out at once, rather than as a handover.
	float outRamp(int counter)
	{
		if(counter < 0 || counter >= SPARK_OUT_END) return 0.0f;
		if(counter < SPARK_OUT_FULL) return 1.0f;
		return static_cast<float>(SPARK_OUT_END - counter) /
			   static_cast<float>(SPARK_OUT_END - SPARK_OUT_FULL);
	}

	float inRamp(int counter)
	{
		if(counter < SPARK_IN_START || counter > SPARK_IN_END) return 0.0f;
		if(counter >= SPARK_IN_FULL) return 1.0f;
		return static_cast<float>(counter - SPARK_IN_START) /
			   static_cast<float>(SPARK_IN_FULL - SPARK_IN_START);
	}

	// A whole number of sparks out of a fractional rate.
	int spawnCount(float rate)
	{
		int n = static_cast<int>(rate);
		if(random(0.0f, 1.0f) < rate - n) n++;
		return n;
	}

	// How likely a smoke cloud is in this tick.
	float smokeRate(int counter)
	{
		if(counter >= SPARK_OUT_END) return 0.0f;
		return 0.3f;
	}

	// The id for the sparks of one conversion. The high bit is always set:
	// everything else in the game carries 0, and no id can therefore ever
	// collide with ordinary particles - not even the very first one.
	ushort nextSparkId()
	{
		static uint counter = 0;
		return static_cast<ushort>(0x8000u | (++counter & 0x7FFFu));
	}

	// The distance travelled after n ticks, see above.
	float travelDistance(float speed, float damping, int life)
	{
		if(damping == 1.0f) return speed * life;
		return speed * (1.0f - powf(damping, static_cast<float>(life))) / (1.0f - damping);
	}
}

DiamondMachine::DiamondMachine(Level& level,
							   const Vec2i& position) : Object(level, 1)
{
	renderLayers = RL_MAIN;
	warpTo(position);
	flags = OF_MASSIVE | OF_FIXED | OF_BLOCK_GAS;
	p_objOnMe = 0;
	counter = -1;
	sparkId = 0;
	p_soundInst = 0;
}

DiamondMachine::~DiamondMachine()
{
}

void DiamondMachine::spawnSparks(Object* p_block)
{
	// A new id per conversion, not per machine: after an abort the old
	// sparks are still running home, and a second abort must not turn those
	// round a second time.
	if(!sparkId) sparkId = nextSparkId();

	ParticleSystem* p_sys = level.getParticleSystem();
	const Sprites& block = p_block->getSprites();

	// The diamond that does not exist yet: its appearance comes from the
	// preset table, and sample() yields landing point and target colour from
	// it in one go.
	Sprites diamond;
	const bool haveDiamond = level.getPresets()->getPresetSprites("Diamond", &diamond);

	// The field above the machine, in pixels: that is where the block
	// stands, and where the diamond will stand later.
	const Vec2f origin(position.x * 16.0f, (position.y - 1) * 16.0f);
	const Vec2f middle = origin + Vec2f(8.0f, 8.0f);

	int n = spawnCount(OUT_RATE * outRamp(counter));
	for(int i = 0; i < n; i++)
	{
		Vec4f sampled;
		Vec2i offset;
		if(!block.sample(&sampled, &offset)) continue;

		const Vec2f start = origin + static_cast<Vec2f>(offset);

		// Away from the centre of the field - the block falls apart, it does
		// not scatter. At the exact centre there is no direction; one is
		// rolled for there.
		const Vec2f radial = start - middle;
		float angle = (radial.length() > 0.5f) ? atan2f(radial.y, radial.x)
											   : random(0.0f, 6.2832f);

		const Vec4f hot = sampled * OUT_BRIGHT;
		const Vec4f cold(sampled.r * OUT_END, sampled.g * OUT_END, sampled.b * OUT_END, 0.0f);

		ParticleSystem::Particle p;
		p.lifetime = OUT_LIFE;
		p.damping = OUT_DAMPING;
		p.gravity = 0.0f;
		p.positionOnTexture = Vec2b(SPARK_SPRITE_X, SPARK_SPRITE_Y);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = start;
		p.velocity = Vec2f(cosf(angle), sinf(angle)) * random(OUT_SPEED_MIN, OUT_SPEED_MAX);
		// Not sampled.a: that is DEBRIS_ALPHA and therefore a quarter. A piece
		// of debris may be pale; a spark shines.
		const Vec4f begin(hot.r, hot.g, hot.b, OUT_ALPHA);
		p.color = begin;
		p.deltaColor = (cold - begin) / static_cast<float>(OUT_LIFE);
		p.rotation = 0.0f;
		p.deltaRotation = 0.0f;
		p.size = OUT_SIZE;
		p.deltaSize = -OUT_SIZE / (OUT_LIFE * 1.3f);
		p.id = sparkId;
		p_sys->addParticle(p);
	}

	if(!haveDiamond) return;

	n = spawnCount(IN_RATE * inRamp(counter));
	for(int i = 0; i < n; i++)
	{
		Vec4f target;
		Vec2i landOffset;
		if(!diamond.sample(&target, &landOffset)) continue;

		Vec4f from;
		Vec2i fromOffset;
		if(!block.sample(&from, &fromOffset)) continue;

		const Vec2f landing = origin + static_cast<Vec2f>(landOffset);

		// Start somewhere in the cloud the outward sparks leave behind: the
		// same distribution, only rolled for again rather than remembered.
		// Pairing up individual sparks does not matter - among dozens nobody
		// sees which belongs to which; what counts is the shape of the cloud.
		const float radius = travelDistance(random(OUT_SPEED_MIN, OUT_SPEED_MAX),
											 OUT_DAMPING, OUT_LIFE);
		const float angle = atan2f(landing.y - middle.y, landing.x - middle.x);
		const Vec2f start = middle + Vec2f(fromOffset.x - 8.0f, fromOffset.y - 8.0f)
								   + Vec2f(cosf(angle), sinf(angle)) * radius;

		// As long as the conversion has left, plus one tick: then they all
		// arrive together, and in exactly the frame in which the block is seen
		// for the last time. Only in the next one is the diamond there and the
		// sparks gone - otherwise the handover would fall into a gap in which
		// neither one nor the other could be seen.
		const int life = max(SPARK_IN_MIN_LIFE, CONVERSION_TICKS - counter + 1);

		// A particle does not move in its last tick: that update only counts
		// down and erases. The arithmetic therefore uses the distances it
		// really covers - otherwise it would stop exactly one short of the
		// target, and because it accelerates, that is the longest one.
		const int moves = life - 1;

		// The damping from the wanted increase, giving the approach the same
		// shape at any duration: d^moves = inAccel.
		const float d = powf(IN_ACCEL, 1.0f / static_cast<float>(moves));

		// The v0 with which the spark stands exactly on its target after its
		// last move. Without gravity, or it would miss.
		const float k = (1.0f - d) / (1.0f - IN_ACCEL);

		// The start colour is over-bright, and that is not decoration: a linear
		// ramp from a blue to the diamond's warm white passes straight through
		// green - measured 0.16 of green cast at t=0.6, and plainly visible at
		// that. Started above 1 the strong channels stay clamped while the
		// weak one catches up; the path then goes through white. For the same
		// block the green cast falls to 0.05.
		const Vec4f begin(from.r * IN_START, from.g * IN_START, from.b * IN_START, 0.0f);
		const Vec4f end(target.r * IN_BRIGHT, target.g * IN_BRIGHT,
						target.b * IN_BRIGHT, IN_ALPHA);

		ParticleSystem::Particle p;
		p.lifetime = static_cast<ushort>(life);
		p.damping = d;
		p.gravity = 0.0f;
		p.positionOnTexture = Vec2b(SPARK_SPRITE_X, SPARK_SPRITE_Y);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = start;
		p.velocity = (landing - start) * k;
		p.color = begin;
		p.deltaColor = (end - begin) / static_cast<float>(moves);
		p.rotation = 0.0f;
		p.deltaRotation = 0.0f;
		p.size = IN_SIZE;
		p.deltaSize = 0.0f;
		p.id = sparkId;
		p_sys->addParticle(p);
	}
}

Object* DiamondMachine::findLivingBlock()
{
	// The block that started the conversion - if it still exists. The search
	// runs over the object list rather than reaching through p_objOnMe,
	// because in exactly the interesting case that pointer is worth nothing
	// any more: a destroyed block is deleted at the start of a tick.
	// isAlive() is false while it collapses in on itself, and anything
	// teleported away is somewhere else entirely - both count as "no longer
	// there".
	if(!p_objOnMe) return 0;

	const std::vector<Object*>& all = level.getObjects();
	for(std::vector<Object*>::const_iterator i = all.begin(); i != all.end(); ++i)
	{
		if(*i != p_objOnMe) continue;
		if(!(*i)->isAlive() || (*i)->isTeleporting()) return 0;
		return *i;
	}

	return 0;
}

void DiamondMachine::abortConversion()
{
	counter = -1;

	// The machine loses what it was doing, and its sound goes down with it:
	// quieter and slower at once, which is the shape everything uses for
	// something running out of power.
	//
	// Slid to zero and not to a negative target, although that would pause it
	// at the end: a paused instance is never AL_STOPPED, so nothing reaps it
	// and it would hold an audio source for the rest of the level. At zero it
	// plays itself out inaudibly and goes the ordinary way. The pointer is
	// dropped either way - it is one-shot, and after this nothing here has any
	// business with it.
	if(p_soundInst)
	{
		if(Sound::isLiveInstance(p_soundInst))
		{
			p_soundInst->slideVolume(0.0f, SOUND_FADE_SPEED);
			p_soundInst->slidePitch(SOUND_FADE_PITCH, SOUND_FADE_SPEED);
		}
		p_soundInst = 0;
	}

	if(!sparkId) return;

	// How far the block has moved since the sparks set off, in pixels. Its
	// *logical* cell and not its shown one: it is pushed over several ticks,
	// and by the time the sparks arrive it is already there.
	Object* p_block = findLivingBlock();
	Vec2f shift(0.0f, 0.0f);
	if(p_block)
		shift = Vec2f((p_block->getPosition().x - position.x) * 16.0f,
					  (p_block->getPosition().y - (position.y - 1)) * 16.0f);

	// Backwards means: every delta reverses, and the damping becomes its
	// reciprocal, because it is a factor and not a summand. The velocity
	// gets that reciprocal on top, because the integrator shifts before it
	// damps - without it the way back would be a tick out of step and would
	// miss the starting point. Only gravity cannot be reversed exactly this
	// way (it is added after the damping, not before); flipping its sign is
	// an approximation, and the one variant with gravity finds a slightly
	// different arc on the way back.
	ParticleSystem* p_sys = level.getParticleSystem();
	for(ParticleSystem::ParticleList::iterator i = p_sys->begin();
		i != p_sys->end(); ++i)
	{
		ParticleSystem::Particle& p = *i;
		if(p.id != sparkId) continue;

		// Inward or outward? The opacity tells them apart, with no second id:
		// one fades in along its way, the other fades out.
		const bool inward = (p.deltaColor.a > 0.0f);

		// A destroyed block has nothing left for the debris to fly back into,
		// and the debris flies on as if nothing had happened. The inward sparks
		// always turn round: they were flying at a diamond, and it is not
		// coming in any case.
		if(!inward && !p_block) continue;

		// The mirrored lifetime: as many ticks as the spark has already been
		// flying. One that has just set off is over at once; one nearly home
		// has the whole way in front of it. Inward it is in the opacity, which
		// has grown from 0 by deltaColor.a per tick; outward the whole duration
		// is known, and what is left of it is lifetime.
		uint elapsed = 0;
		if(inward) elapsed = static_cast<uint>(p.color.a / p.deltaColor.a + 0.5f);
		else if(p.lifetime < OUT_LIFE) elapsed = OUT_LIFE - p.lifetime;

		if(p.damping != 0.0f)
		{
			p.velocity = -p.velocity / p.damping;
			p.damping = 1.0f / p.damping;
		}
		else p.velocity = -p.velocity;

		p.gravity = -p.gravity;
		p.deltaColor = -p.deltaColor;
		p.deltaSize = -p.deltaSize;
		p.deltaRotation = -p.deltaRotation;

		// The way back hits the starting point, but the block may be standing
		// somewhere else by now. One addend on the velocity translates the
		// whole trajectory by exactly that offset - the same travel formula as
		// for the approach, solved for v0: no distortion of the trajectory,
		// a parallel shift.
		if(!inward && elapsed && !shift.isZero())
		{
			const float d = p.damping;
			const float k = (d == 1.0f)
				? 1.0f / elapsed
				: (1.0f - d) / (1.0f - powf(d, static_cast<float>(elapsed)));
			p.velocity += shift * k;
		}

		// One tick more than moves: the last update only counts down and
		// erases, it no longer moves anything. And 0 would be fatal here - the
		// counter is unsigned and would wrap.
		p.lifetime = static_cast<ushort>(elapsed + 1);
	}

	sparkId = 0;
}

void DiamondMachine::updateSprites()
{
	// Machine
	Vec2i positionOnTexture(0, 128);
	if(level.isElectricityOn())
	{
		if(counter == -1) positionOnTexture.x = 32;
		else positionOnTexture.x = 64 + 32 * (min(counter, 80) / 20);
	}
	sprites.add(positionOnTexture);
}

void DiamondMachine::onRender(RenderLayer layer,
							  const Vec4f& color)
{
	if(layer == RL_MAIN) Engine::inst().renderSprites(sprites, color);
}

void DiamondMachine::onUpdate()
{
	if(level.isElectricityOn())
	{
		// Is there an object on the machine?
		Object* p_obj = level.getFrontObjectAt(position - Vec2i(0, 1));
		if(p_obj)
		{
			if(!p_obj->isTeleporting() && (p_obj->getFlags() & OF_CONVERTABLE))
			{
				if(p_obj == p_objOnMe)
				{
					// Smoke. The colour comes from the block's image; if the
					// sample lands on a transparent spot only this one
					// particle is dropped - the counter below runs on, or the
					// conversion time would hang on the image's coverage and
					// be a different one every time.
					//
					// Thin, and at the end none at all: a smoke cloud lives
					// eighty to a hundred and twenty ticks and grows while it
					// does, a spark lives less than thirty and shrinks. At one
					// particle per tick there is therefore always a haze over
					// everything and the sparks disappear into it. The last
					// quarter belongs to the collecting alone.
					Vec4f sampled;
					Vec2i offset;
					if(random(0.0f, 1.0f) < smokeRate(counter) &&
					   p_obj->getSprites().sample(&sampled, &offset))
					{
						ParticleSystem* p_particleSystem = level.getParticleSystem();
						ParticleSystem* p_fireParticleSystem = level.getFireParticleSystem();
						ParticleSystem::Particle p;
						p.lifetime = static_cast<ushort>(random(80, 120));
						p.damping = 0.99f;
						p.gravity = 0.005f;
						p.positionOnTexture = Vec2b(0, 0);
						p.sizeOnTexture = Vec2b(16, 16);
						p.position = position * 16 - Vec2i(0, 16) + offset;
						p.velocity = Vec2f(random(-0.5f, 0.5f), -1.0f);
						p.color = sampled;
						p.deltaColor = Vec4f(0.0f, 0.0f, 0.0f, -p.color.a / p.lifetime);
						p.rotation = random(0.0f, 10.0f);
						p.deltaRotation = random(-0.1f, 0.1f);
						p.size = random(0.3f, 0.5f);
						p.deltaSize = random(0.01f, 0.05f);
						if(randomInt() % 2) p_particleSystem->addParticle(p);
						else p_fireParticleSystem->addParticle(p);
					}

					// How far the conversion has got, pressed into the block's
					// hand - it draws itself paler. Fresh every tick, because
					// it clears the value in its own frameBegin(); a machine
					// that stops pushing lets it stand full again by itself.
					// p_obj comes straight out of getFrontObjectAt() and is
					// touched only here and now; p_objOnMe stays a pointer
					// that is only ever compared across ticks.
					p_obj->setConversionProgress(
						clamp(static_cast<float>(counter) / CONVERSION_TICKS, 0.0f, 1.0f));

					spawnSparks(p_obj);

					counter++;
					if(!counter)
					{
						// Kept, so that an abort can slide it down. It is a
						// one-shot, so it may well have been reaped before
						// then - Sound::isLiveInstance is what asks.
						p_soundInst = Engine::inst().playSound("diamondmachine.ogg", false, 0.0f, 100);
					}
				}
				else
				{
					// abortConversion() first: it finds the block the sparks belong
					// to through p_objOnMe, and that is still the old one.
					abortConversion();
					p_objOnMe = p_obj;
				}

				if(counter >= CONVERSION_TICKS)
				{
					// The block is converted. It disappears fast: it stands at
					// CONVERSION_GHOST and keeps that while dying too (see
					// Object::frameBegin), leaving no more than a breath over
					// the finished diamond. Half a second would be an eternity
					// for that.
					p_obj->disappearNextFrame(0.15f);
					level.getPresets()->instancePreset("Diamond", position - Vec2i(0, 1), 0);
//					level.addNewObjects();
					counter = -1;

					// No abortConversion(): the inward sparks arrived and
					// expired in this very tick. The id is merely put aside for
					// the next conversion to get one of its own.
					sparkId = 0;

					// The sound is let go of rather than stopped: the machine
					// did what it was doing, and the sample ends with it.
					p_soundInst = 0;
				}
			}
			else abortConversion();
		}
		else
		{
			abortConversion();
			p_objOnMe = 0;
		}
	}
	else abortConversion();
}