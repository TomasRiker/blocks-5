#include "pch.h"
#include "diamondmachine.h"
#include "presets.h"
#include "engine.h"
#include "particlesystem.h"
#include "sound.h"
#include "soundinstance.h"

/* The conversion's shower of sparks. The block is taken apart and put back
   together rather than faded out: sparks fly outward in its colours, then
   sparks come back from the cloud they leave, taking on the diamond's colour
   where they land.

   TIMETABLE, in ticks of the machine's counter, which runs to 100; its
   animation frame changes at 20, 40, 60 and 80, so sparks and animation
   share one clock:

       outward   emitted 0 to 53, each living 27 ticks: the last gone by 80
       inward    emitted 20 to 92, every one landing at 100
       visible   outward alone to 20, both to 80, inward alone to 100

   Outward emission stops a whole lifetime before 80 (80 - 27 = 53), leaving
   the end to the collecting. Inward the lifetime is worked out so a spark
   lands at 100 whenever it sets off; after 92 the time left would be a flash
   on the spot rather than a flight.

   LANDING EXACTLY. The integrator is position += velocity, then velocity *=
   damping, so n moves cover v0 * (1 - d^n) / (1 - d). Outward that says
   where the cloud ends, and so where inward sparks may start; inward, solved
   for v0, it lands a spark exactly on its target. d below 1 brakes, above 1
   accelerates: the way out peters out, the way in sucks the spark in.

   DUST, NOT EMBERS. An outward spark keeps the plain colour of its texel and
   only its opacity falls (OUT_BRIGHT and OUT_END are both 1): glowing sparks
   read as welding, and the machine takes rock, ice and grass as readily as
   metal. Hence also many large slow sparks rather than fewer small fast
   ones - a block falling apart. No spark is blended additively, where the
   same brown would be an ember over rock and glaring yellow over grass. An
   inward spark's colour starts above 1 and is clamped, not to glow but to
   avoid a green cast; the reason stands where it is computed.

   ABORT. The block can be pushed away, blown up or switched off at the last
   moment. The sparks must not vanish then, which would tear a hole in the
   motion: abortConversion() runs them back with a mirrored lifetime, as long
   as each has already flown. The inward ones always turn round - the diamond
   is not coming. The outward ones fly back into the block while it stands,
   pushed aside or unpowered, and fly on untouched once it is destroyed,
   since then flying apart is right. The machine finds its sparks by
   Particle::id, the one field that stays 0 everywhere else in the game. */

namespace
{
	// The phases, in ticks of the machine's counter.
	const int CONVERSION_TICKS = 100;  // then the block becomes the diamond
	const int SPARK_OUT_FULL = 20;
	const int SPARK_OUT_END  = 53;
	const int SPARK_IN_START = 20;
	const int SPARK_IN_FULL  = 60;
	const int SPARK_IN_END   = 92;

	// The sound of a conversion that is not going to happen: volume and
	// pitch run down together, a machine losing what it was doing. The
	// slides are exponential, once per logic tick, the speed being the
	// fraction of the way left covered each tick: at 0.04 the volume halves
	// every 17 ticks, a third of a second. diamondmachine.ogg lasts exactly
	// the conversion's two seconds, so only what the abort left is faded.
	const float SOUND_FADE_SPEED = 0.04f;
	const float SOUND_FADE_PITCH = 0.35f;

	// Every inward spark lives exactly as long as the conversion has left,
	// whenever it sets off, so they all arrive in the instant the diamond
	// appears. A floor for the last ones, which still need some distance; at
	// SPARK_IN_END 92 the shortest life is 9, so it does not bite.
	const int SPARK_IN_MIN_LIFE = 8;

	// The sprite in particles.png, in pixels. A particle is multiplied by its
	// texture region, so it must be neutral: (32,0) is a solid blob but
	// pre-coloured - orange (231,152,41) in blocks_01 - and turns a cyan
	// spark olive. (32,32), a white disc, is the only one both neutral and
	// dense; (0,32) is a white five-pointed star, should it ever sparkle.
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
	// How much faster a spark ends its flight than it starts. The damping is
	// derived from it per spark, since the lifetime is fixed only at take-off,
	// so the approach looks the same at any duration: a long creep, then a
	// snap.
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

	// The id for one conversion's sparks. The high bit is always set, so no
	// id is ever the 0 every other particle carries.
	ushort nextSparkId()
	{
		static uint counter = 0;
		return static_cast<ushort>(0x8000u | (++counter & 0x7FFFu));
	}

	// The distance covered in n = life moves, see LANDING EXACTLY above.
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

		// Start somewhere in the cloud the outward sparks leave: the same
		// distribution rolled again, not remembered - among dozens nobody sees
		// which spark is which, only the cloud's shape.
		const float radius = travelDistance(random(OUT_SPEED_MIN, OUT_SPEED_MAX),
											 OUT_DAMPING, OUT_LIFE);
		const float angle = atan2f(landing.y - middle.y, landing.x - middle.x);
		const Vec2f start = middle + Vec2f(fromOffset.x - 8.0f, fromOffset.y - 8.0f)
								   + Vec2f(cosf(angle), sinf(angle)) * radius;

		// As long as the conversion has left, plus one tick: they all arrive
		// in the frame that shows the block for the last time, and the next
		// shows the diamond and no sparks. Without the extra tick the handover
		// would fall into a gap showing neither.
		const int life = max(SPARK_IN_MIN_LIFE, CONVERSION_TICKS - counter + 1);

		// A particle does not move in its last update, which only counts down
		// and erases, so the arithmetic counts moves. Counting ticks would
		// stop it one move short, and accelerating, that is its longest.
		const int moves = life - 1;

		// The damping from the wanted increase, giving the approach the same
		// shape at any duration: d^moves = IN_ACCEL.
		const float d = powf(IN_ACCEL, 1.0f / static_cast<float>(moves));

		// The v0 with which the spark stands exactly on its target after its
		// last move. Without gravity, or it would miss.
		const float k = (1.0f - d) / (1.0f - IN_ACCEL);

		// Over-bright on purpose: a linear ramp from a blue to the diamond's
		// warm white passes through green (measured 0.16 of green cast at
		// t = 0.6, plainly visible). Started above 1, the strong channels stay
		// clamped while the weak one catches up and the path runs through
		// white; for the same block the cast falls to 0.05.
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
	// The block that started the conversion, if it still exists. Looked for
	// in the object list rather than read through p_objOnMe, which dangles in
	// exactly the interesting case: a destroyed block is deleted at the start
	// of a tick. One collapsing (!isAlive()) or teleporting counts as gone.
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

	// The sound runs down with the machine, quieter and lower at once. Slid
	// to zero, not to a negative target, which would pause it at the end: a
	// paused instance is never AL_STOPPED, so nothing reaps it and it holds an
	// audio source for the rest of the level. At zero it plays out inaudibly
	// and is reaped as usual. The pointer is dropped either way; it is a
	// one-shot.
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

	// Backwards: every delta changes sign and the damping, a factor, becomes
	// its reciprocal. The velocity is divided by the damping as well, since
	// the integrator moves before it damps; without that the way back runs a
	// tick out of step and misses the start. Gravity, added after the
	// damping, only reverses approximately by a sign flip; none of the
	// machine's sparks has any.
	ParticleSystem* p_sys = level.getParticleSystem();
	for(ParticleSystem::ParticleList::iterator i = p_sys->begin();
		i != p_sys->end(); ++i)
	{
		ParticleSystem::Particle& p = *i;
		if(p.id != sparkId) continue;

		// Inward or outward? The opacity tells them apart, with no second id:
		// one fades in along its way, the other fades out.
		const bool inward = (p.deltaColor.a > 0.0f);

		// Without the block the outward sparks fly on; the inward ones always
		// turn round.
		if(!inward && !p_block) continue;

		// The mirrored lifetime: as many moves as the spark has made. Inward
		// that is in the opacity, grown from 0 by deltaColor.a a move; outward
		// it is OUT_LIFE less the lifetime left.
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

		// The way back ends at the starting point, but the block may stand
		// elsewhere by now. One addend on the velocity, the travel formula
		// solved for v0, moves the end point by exactly that offset; it decays
		// with the same damping, so the offset is taken on in proportion to
		// the way covered.
		if(!inward && elapsed && !shift.isZero())
		{
			const float d = p.damping;
			const float k = (d == 1.0f)
				? 1.0f / elapsed
				: (1.0f - d) / (1.0f - powf(d, static_cast<float>(elapsed)));
			p.velocity += shift * k;
		}

		// One more than the moves, since the last update only erases. Never
		// 0: the update decrements first, and the unsigned counter would wrap.
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
					// Smoke, coloured from the block's image. A sample on a
					// transparent spot drops only this particle; the counter
					// runs on, or the conversion's duration would depend on
					// the image's coverage.
					//
					// Thin, and none after SPARK_OUT_END: a smoke cloud lives
					// 80 to 120 ticks and grows, a spark under 30 and shrinks,
					// so at one cloud a tick a haze would hide the sparks.
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

					// The block draws itself paler by the progress. Set every
					// tick, since the block clears it in its own frameBegin():
					// a machine that stops lets it stand full again by itself.
					// p_obj is fresh from getFrontObjectAt() and used only
					// now; p_objOnMe is only ever compared across ticks.
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
					// The block is converted. It goes fast, keeping its
					// CONVERSION_GHOST pallor while dying (Object::frameBegin),
					// no more than a breath over the finished diamond.
					p_obj->disappearNextFrame(0.15f);
					// This only queues the diamond for the level's next
					// addNewObjects(); adding it now would disturb
					// Level::update()'s walk over the objects.
					level.getPresets()->instancePreset("Diamond", position - Vec2i(0, 1), 0);
					counter = -1;

					// No abortConversion(): the inward sparks make their last
					// move later in this tick and expire in the next. The id
					// is only cleared, so the next conversion gets its own.
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