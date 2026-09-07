#include "pch.h"
#include "diamondmachine.h"
#include "presets.h"
#include "engine.h"
#include "particlesystem.h"
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
	const double OUT_RATE     = 6.0;   // sparks per tick at full rate
	const double OUT_SPEED_MIN = 0.5;
	const double OUT_SPEED_MAX = 1.1;
	const double OUT_DAMPING  = 0.95;  // below 1: brakes
	const double OUT_BRIGHT   = 1.0;   // brightness in the first tick
	const double OUT_END      = 1.0;   // brightness in the last
	const double OUT_ALPHA    = 0.85;  // opacity at the start
	const int    OUT_LIFE     = 27;
	const double OUT_SIZE     = 0.42;

	// Inward: the diamond is put together.
	const double IN_RATE   = 4.0;
	// How much faster a spark is at the end of its flight than at the start.
	// The damping is computed from it, because the lifetime is only fixed
	// when the spark sets off - the approach then looks the same at any
	// duration: creep for a long time, snap shut at the end.
	const double IN_ACCEL  = 3.0;
	const double IN_START  = 1.4;      // start colour brightness, see below
	const double IN_BRIGHT = 1.0;      // brightness on impact
	const double IN_ALPHA  = 0.85;     // opacity on impact
	const double IN_SIZE   = 0.38;

	// Fraction of the full rate. Both ramps are linear and not switched: a
	// hard changeover would show the middle as a plateau with both
	// directions running flat out at once, rather than as a handover.
	double outRamp(int counter)
	{
		if(counter < 0 || counter >= SPARK_OUT_END) return 0.0;
		if(counter < SPARK_OUT_FULL) return 1.0;
		return static_cast<double>(SPARK_OUT_END - counter) /
			   static_cast<double>(SPARK_OUT_END - SPARK_OUT_FULL);
	}

	double inRamp(int counter)
	{
		if(counter < SPARK_IN_START || counter > SPARK_IN_END) return 0.0;
		if(counter >= SPARK_IN_FULL) return 1.0;
		return static_cast<double>(counter - SPARK_IN_START) /
			   static_cast<double>(SPARK_IN_FULL - SPARK_IN_START);
	}

	// A whole number of sparks out of a fractional rate.
	int spawnCount(double rate)
	{
		int n = static_cast<int>(rate);
		if(random(0.0, 1.0) < rate - n) n++;
		return n;
	}

	// How likely a smoke cloud is in this tick.
	double smokeRate(int counter)
	{
		if(counter >= SPARK_OUT_END) return 0.0;
		return 0.3;
	}

	// The id for the sparks of one conversion. The high bit is always set:
	// everything else in the game carries 0, and no id can therefore ever
	// collide with ordinary particles - not even the very first one.
	uint nextSparkId()
	{
		static uint counter = 0;
		return 0x80000000u | (++counter & 0x7FFFFFFFu);
	}

	// The distance travelled after n ticks, see above.
	double travelDistance(double speed, double damping, int life)
	{
		if(damping == 1.0) return speed * life;
		return speed * (1.0 - pow(damping, static_cast<double>(life))) / (1.0 - damping);
	}
}

DiamondMachine::DiamondMachine(Level& level,
							   const Vec2i& position) : Object(level, 1)
{
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
	const Vec2d origin(position.x * 16.0, (position.y - 1) * 16.0);
	const Vec2d middle = origin + Vec2d(8.0, 8.0);

	int n = spawnCount(OUT_RATE * outRamp(counter));
	for(int i = 0; i < n; i++)
	{
		Vec4d sampled;
		Vec2i offset;
		if(!block.sample(&sampled, &offset)) continue;

		const Vec2d start = origin + Vec2d(offset.x, offset.y);

		// Away from the centre of the field - the block falls apart, it does
		// not scatter. At the exact centre there is no direction; one is
		// rolled for there.
		const Vec2d radial = start - middle;
		double angle = (radial.length() > 0.5) ? atan2(radial.y, radial.x)
											   : random(0.0, 6.2832);

		const Vec4d hot = sampled * OUT_BRIGHT;
		const Vec4d cold(sampled.r * OUT_END, sampled.g * OUT_END, sampled.b * OUT_END, 0.0);

		ParticleSystem::Particle p;
		p.lifetime = OUT_LIFE;
		p.damping = static_cast<float>(OUT_DAMPING);
		p.gravity = 0.0f;
		p.positionOnTexture = Vec2b(SPARK_SPRITE_X, SPARK_SPRITE_Y);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = start;
		p.velocity = Vec2d(cos(angle), sin(angle)) * random(OUT_SPEED_MIN, OUT_SPEED_MAX);
		// Not sampled.a: that is DEBRIS_ALPHA and therefore a quarter. A piece
		// of debris may be pale; a spark shines.
		const Vec4d begin(hot.r, hot.g, hot.b, OUT_ALPHA);
		p.color = begin;
		p.deltaColor = (cold - begin) / static_cast<double>(OUT_LIFE);
		p.rotation = 0.0f;
		p.deltaRotation = 0.0f;
		p.size = static_cast<float>(OUT_SIZE);
		p.deltaSize = static_cast<float>(-OUT_SIZE / (OUT_LIFE * 1.3));
		p.id = sparkId;
		p_sys->addParticle(p);
	}

	if(!haveDiamond) return;

	n = spawnCount(IN_RATE * inRamp(counter));
	for(int i = 0; i < n; i++)
	{
		Vec4d target;
		Vec2i landOffset;
		if(!diamond.sample(&target, &landOffset)) continue;

		Vec4d from;
		Vec2i fromOffset;
		if(!block.sample(&from, &fromOffset)) continue;

		const Vec2d landing = origin + Vec2d(landOffset.x, landOffset.y);

		// Start somewhere in the cloud the outward sparks leave behind: the
		// same distribution, only rolled for again rather than remembered.
		// Pairing up individual sparks does not matter - among dozens nobody
		// sees which belongs to which; what counts is the shape of the cloud.
		const double radius = travelDistance(random(OUT_SPEED_MIN, OUT_SPEED_MAX),
											 OUT_DAMPING, OUT_LIFE);
		const double angle = atan2(landing.y - middle.y, landing.x - middle.x);
		const Vec2d start = middle + Vec2d(fromOffset.x - 8.0, fromOffset.y - 8.0)
								   + Vec2d(cos(angle), sin(angle)) * radius;

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
		const double d = pow(IN_ACCEL, 1.0 / static_cast<double>(moves));

		// The v0 with which the spark stands exactly on its target after its
		// last move. Without gravity, or it would miss.
		const double k = (1.0 - d) / (1.0 - IN_ACCEL);

		// The start colour is over-bright, and that is not decoration: a linear
		// ramp from a blue to the diamond's warm white passes straight through
		// green - measured 0.16 of green cast at t=0.6, and plainly visible at
		// that. Started above 1 the strong channels stay clamped while the
		// weak one catches up; the path then goes through white. For the same
		// block the green cast falls to 0.05.
		const Vec4d begin(from.r * IN_START, from.g * IN_START, from.b * IN_START, 0.0);
		const Vec4d end(target.r * IN_BRIGHT, target.g * IN_BRIGHT,
						target.b * IN_BRIGHT, IN_ALPHA);

		ParticleSystem::Particle p;
		p.lifetime = static_cast<uint>(life);
		p.damping = static_cast<float>(d);
		p.gravity = 0.0f;
		p.positionOnTexture = Vec2b(SPARK_SPRITE_X, SPARK_SPRITE_Y);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = start;
		p.velocity = (landing - start) * k;
		p.color = begin;
		p.deltaColor = (end - begin) / static_cast<double>(moves);
		p.rotation = 0.0f;
		p.deltaRotation = 0.0f;
		p.size = static_cast<float>(IN_SIZE);
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
	if(!sparkId) return;

	// How far the block has moved since the sparks set off, in pixels. Its
	// *logical* cell and not its shown one: it is pushed over several ticks,
	// and by the time the sparks arrive it is already there.
	Object* p_block = findLivingBlock();
	Vec2d shift(0.0, 0.0);
	if(p_block)
		shift = Vec2d((p_block->getPosition().x - position.x) * 16.0,
					  (p_block->getPosition().y - (position.y - 1)) * 16.0);

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
		else if(p.lifetime < static_cast<uint>(OUT_LIFE))
			elapsed = static_cast<uint>(OUT_LIFE) - p.lifetime;

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
			const double d = p.damping;
			const double k = (d == 1.0)
				? 1.0 / elapsed
				: (1.0 - d) / (1.0 - pow(d, static_cast<double>(elapsed)));
			p.velocity += shift * k;
		}

		// One tick more than moves: the last update only counts down and
		// erases, it no longer moves anything. And 0 would be fatal here - the
		// counter is unsigned and would wrap.
		p.lifetime = elapsed + 1;
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

void DiamondMachine::onRender(int layer,
							  const Vec4d& color)
{
	if(layer == 1) Engine::inst().renderSprites(sprites, color);
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
					Vec4d sampled;
					Vec2i offset;
					if(random(0.0, 1.0) < smokeRate(counter) &&
					   p_obj->getSprites().sample(&sampled, &offset))
					{
						ParticleSystem* p_particleSystem = level.getParticleSystem();
						ParticleSystem* p_fireParticleSystem = level.getFireParticleSystem();
						ParticleSystem::Particle p;
						p.lifetime = random(80, 120);
						p.damping = 0.99f;
						p.gravity = 0.005f;
						p.positionOnTexture = Vec2b(0, 0);
						p.sizeOnTexture = Vec2b(16, 16);
						p.position = position * 16 - Vec2i(0, 16) + offset;
						p.velocity = Vec2d(random(-0.5, 0.5), -1.0);
						p.color = sampled;
						p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
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
						clamp(static_cast<double>(counter) / CONVERSION_TICKS, 0.0, 1.0));

					spawnSparks(p_obj);

					counter++;
					if(!counter)
					{
						Engine::inst().playSound("diamondmachine.ogg", false, 0.0, 100);
					}
				}
				else
				{
					p_objOnMe = p_obj;
					abortConversion();
				}

				if(counter >= 100)
				{
					// The block is converted. It disappears fast: it stands at
					// CONVERSION_GHOST and keeps that while dying too (see
					// Object::frameBegin), leaving no more than a breath over
					// the finished diamond. Half a second would be an eternity
					// for that.
					p_obj->disappearNextFrame(0.15);
					level.getPresets()->instancePreset("Diamond", position - Vec2i(0, 1), 0);
//					level.addNewObjects();
					counter = -1;

					// No abortConversion(): the inward sparks arrived and
					// expired in this very tick. The id is merely put aside for
					// the next conversion to get one of its own.
					sparkId = 0;
				}
			}
			else abortConversion();
		}
		else
		{
			p_objOnMe = 0;
			abortConversion();
		}
	}
	else abortConversion();

	if(counter == -1 && p_soundInst)
	{
		p_soundInst->stop();
		p_soundInst = 0;
	}
}