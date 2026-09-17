#include "pch.h"
#include "lava.h"
#include "engine.h"
#include "particlesystem.h"

// The flow scrolls one texel a tick and never stops, so the offset is reduced
// to whole periods of the picture before it reaches GL - see wrapTextureOffset
// for what an unbounded one does to a phone. Twice the 16-texel tile and not
// once, because the front pass halves the whole coordinate: a jump of 16 would
// move that pass by half a tile, where 32 moves it by exactly one.
const int SCROLL_PERIOD = 32;

Lava::Lava(Level& level,
		   const Vec2i& position,
		   int dir) : Object(level, 401)
{
	renderLayers = RL_LAVA_EDGE | RL_LAVA_BACK | RL_LAVA_FRONT | RL_EDITOR | RL_LIGHT;
	warpTo(position);
	flags = OF_FIXED | OF_DONT_FALL | OF_NO_SHADOW;
	this->dir = dir;
	anim = 0;
}

Lava::~Lava()
{
}

void Lava::onRender(RenderLayer layer,
					const Vec4d& color)
{
	if(layer == RL_LAVA_EDGE)
	{
		Engine& engine = Engine::inst();

		uint tl = level.getAIFlags(position + Vec2i(-1, -1)) & 4;
		uint t = level.getAIFlags(position + Vec2i(0, -1)) & 4;
		uint tr = level.getAIFlags(position + Vec2i(1, -1)) & 4;
		uint l = level.getAIFlags(position + Vec2i(-1, 0)) & 4;
		uint r = level.getAIFlags(position + Vec2i(1, 0)) & 4;
		uint bl = level.getAIFlags(position + Vec2i(-1, 1)) & 4;
		uint b = level.getAIFlags(position + Vec2i(0, 1)) & 4;
		uint br = level.getAIFlags(position + Vec2i(1, 1)) & 4;

		if(!l && !t && !tl)	engine.renderSprite(Vec2i(0, 0), Vec2i(0, 0), Vec2i(16, 16), Vec4d(1.0));
		if(!t)				engine.renderSprite(Vec2i(0, 0), Vec2i(16, 0), Vec2i(16, 16), Vec4d(1.0));
		if(!r && !t && !tr)	engine.renderSprite(Vec2i(0, 0), Vec2i(32, 0), Vec2i(16, 16), Vec4d(1.0));
		if(!l)				engine.renderSprite(Vec2i(0, 0), Vec2i(0, 16), Vec2i(16, 16), Vec4d(1.0));
		if(!r)				engine.renderSprite(Vec2i(0, 0), Vec2i(32, 16), Vec2i(16, 16), Vec4d(1.0));
		if(!l && !b && !bl)	engine.renderSprite(Vec2i(0, 0), Vec2i(0, 32), Vec2i(16, 16), Vec4d(1.0));
		if(!b)				engine.renderSprite(Vec2i(0, 0), Vec2i(16, 32), Vec2i(16, 16), Vec4d(1.0));
		if(!r && !b && !br)	engine.renderSprite(Vec2i(0, 0), Vec2i(32, 32), Vec2i(16, 16), Vec4d(1.0));

		if(l && t && !tl)	engine.renderSprite(Vec2i(0, 0), Vec2i(64, 16), Vec2i(16, 16), Vec4d(1.0));
		if(l && b && !bl)	engine.renderSprite(Vec2i(0, 0), Vec2i(64, 0), Vec2i(16, 16), Vec4d(1.0));
		if(r && t && !tr)	engine.renderSprite(Vec2i(0, 0), Vec2i(48, 16), Vec2i(16, 16), Vec4d(1.0));
		if(r && b && !br)	engine.renderSprite(Vec2i(0, 0), Vec2i(48, 0), Vec2i(16, 16), Vec4d(1.0));
	}
	else if(layer == RL_LAVA_BACK || layer == RL_LAVA_FRONT)
	{
		// render the lava. The pass bound the picture and set the blend; the
		// alpha runs across the quad, a corner each.
		Renderer& renderer = Renderer::inst();
		const RenderState state = renderer.state();
		const Vec2f corners[4] = {Vec2f(0.0f, 0.0f), Vec2f(16.0f, 0.0f), Vec2f(16.0f, 16.0f), Vec2f(0.0f, 16.0f)};
		Vec2d shift(2.0 * sin(0.1 * anim), 3.0 * cos(0.05 * anim));
		double a[4];
		if(layer == RL_LAVA_BACK) getAlpha1(position, a);
		else if(layer == RL_LAVA_FRONT) getAlpha2(position, a);

		int numPasses = 1;
		if(dir > 3) numPasses = 2;

		for(int i = 0; i < numPasses; i++)
		{
			double tl = 1.0, tr = 1.0, br = 1.0, bl = 1.0;
			int ndir = dir;

			if(i == 1)
			{
				switch(dir)
				{
				case 4: ndir += 3, tl = 1.0, tr = 1.0, br = 0.0, bl = 0.0; break;
				case 5: ndir += 3, tl = 0.0, tr = 1.0, br = 1.0, bl = 0.0; break;
				case 6: ndir += 3, tl = 0.0, tr = 0.0, br = 1.0, bl = 1.0; break;
				case 7: ndir += 3, tl = 1.0, tr = 0.0, br = 0.0, bl = 1.0; break;

				case 8: ndir += 1, tl = 1.0, tr = 1.0, br = 0.0, bl = 0.0; break;
				case 9: ndir += 1, tl = 0.0, tr = 1.0, br = 1.0, bl = 0.0; break;
				case 10: ndir += 1, tl = 0.0, tr = 0.0, br = 1.0, bl = 1.0; break;
				case 11: ndir += 1, tl = 1.0, tr = 0.0, br = 0.0, bl = 1.0; break;

				case 12: ndir += 2, tl = 0.0, tr = 1.0, br = 1.0, bl = 0.0; break;
				case 13: ndir += 2, tl = 0.0, tr = 0.0, br = 1.0, bl = 1.0; break;
				case 14: ndir += 2, tl = 1.0, tr = 0.0, br = 0.0, bl = 1.0; break;
				case 15: ndir += 2, tl = 1.0, tr = 1.0, br = 0.0, bl = 0.0; break;

				case 16: ndir += 2, tl = 1.0, tr = 0.0, br = 0.0, bl = 1.0; break;
				case 17: ndir += 2, tl = 1.0, tr = 1.0, br = 0.0, bl = 0.0; break;
				case 18: ndir += 2, tl = 0.0, tr = 1.0, br = 1.0, bl = 0.0; break;
				case 19: ndir += 2, tl = 0.0, tr = 0.0, br = 1.0, bl = 1.0; break;
				}
			}

			// Only the scroll is wrapped. The shift above reads the same
			// anim and its two periods divide neither the tile nor each
			// other, so wrapping what feeds those sines would jog the wobble
			// every time it came round.
			const double scroll = wrapTextureOffset(anim, SCROLL_PERIOD);

			Vec2d t;
			switch(ndir % 4)
			{
			case 0: t = Vec2d(0.0, scroll); break;
			case 1: t = Vec2d(-scroll, 0.0); break;
			case 2: t = Vec2d(0.0, -scroll); break;
			case 3: t = Vec2d(scroll, 0.0); break;
			}

			t += shift;

			const Vec4f colors[4] = {Vec4f(1.0f, 1.0f, 1.0f, static_cast<float>(a[0] * tl)),
									 Vec4f(1.0f, 1.0f, 1.0f, static_cast<float>(a[1] * tr)),
									 Vec4f(1.0f, 1.0f, 1.0f, static_cast<float>(a[2] * br)),
									 Vec4f(1.0f, 1.0f, 1.0f, static_cast<float>(a[3] * bl))};

			if(layer == RL_LAVA_FRONT) t /= 2.0;
			const Vec2f uvs[4] = {Vec2f(t.x, t.y), Vec2f(t.x + 16.0, t.y), Vec2f(t.x + 16.0, t.y + 16.0), Vec2f(t.x, t.y + 16.0)};
			renderer.quad(state, corners, uvs, colors);
		}
	}
	else if(layer == RL_LIGHT)
	{
		level.renderShine(0.75, 0.35 + 0.05 * glowJitter);
	}

	if(layer == RL_EDITOR)
	{
		// show the flow direction: an arrow per shape, as pairs of end
		// points, turned by the direction's quarter
		static const int ARROWS[5][24] =
		{
			{0, 5, 0, -5, 0, -5, -3, -2, 0, -5, 3, -2},
			{5, 5, 5, -5, 5, -5, -5, -5, -5, -5, -2, -8, -5, -5, -2, -2},
			{-5, 5, -5, -5, -5, -5, 5, -5, 5, -5, 2, -8, 5, -5, 2, -2},
			{-5, 5, -5, -5, -5, -5, -8, -2, -5, -5, -2, -2, 5, -5, 5, 5, 5, 5, 2, 2, 5, 5, 8, 2},
			{-5, -5, -5, 5, -5, 5, -8, 2, -5, 5, -2, 2, 5, 5, 5, -5, 5, -5, 2, -2, 5, -5, 8, -2}
		};
		static const int ARROW_LENGTH[5] = {12, 16, 16, 24, 24};
		int shape = -1;
		if(dir <= 3) shape = 0;
		else if(dir <= 7) shape = 1;
		else if(dir <= 11) shape = 2;
		else if(dir <= 15) shape = 3;
		else if(dir <= 19) shape = 4;

		if(shape >= 0)
		{
			Renderer& renderer = Renderer::inst();
			renderer.push();
			renderer.translate(7.5, 7.5);
			renderer.rotate(90.0 * (dir % 4));
			const Vec4f white(1.0f, 1.0f, 1.0f, 1.0f);
			const int* p = ARROWS[shape];
			for(int i = 0; i < ARROW_LENGTH[shape]; i += 4)
			{
				renderer.line(Vec2f(static_cast<float>(p[i]), static_cast<float>(p[i + 1])),
							  Vec2f(static_cast<float>(p[i + 2]), static_cast<float>(p[i + 3])), 1.0f, white);
			}
			renderer.pop();
		}
	}
}

void Lava::onUpdate()
{
	ParticleSystem* p_particleSystem = level.getParticleSystem();
	ParticleSystem::Particle p;

	if(!(randomInt() % 20))
	{
		// steam
		p.lifetime = static_cast<ushort>(random(20, 30));
		p.damping = 0.9f;
		p.gravity = -0.04f;
		p.positionOnTexture = Vec2b(0, 0);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = position * 16 + Vec2i(random(0, 15), random(0, 15));
		p.velocity = Vec2d(random(-0.5, 0.5), random(-0.5, 0.5));
		p.color = Vec4d(random(0.9, 1.0), random(0.9, 1.0), random(0.9, 1.0), random(0.75, 1.0));
		p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
		p.rotation = random(0.0f, 10.0f);
		p.deltaRotation = random(-0.1f, 0.1f);
		p.size = random(0.75f, 1.2f);
		p.deltaSize = random(0.015f, 0.075f);
		p_particleSystem->addParticle(p);
	}

	// Elevators protect the objects from the lava.
	bool elevatorFound = false;
	const std::vector<Object*> objectsOnMe = level.getObjectsAt(position);
	for(std::vector<Object*>::const_iterator i = objectsOnMe.begin(); i != objectsOnMe.end(); ++i)
	{
		if((*i)->getFlags() & OF_ELEVATOR)
		{
			elevatorFound = true;
			break;
		}
	}

	if(!elevatorFound)
	{
		// Is there an object in the lava?
		for(std::vector<Object*>::const_iterator i = objectsOnMe.begin(); i != objectsOnMe.end(); ++i)
		{
			Object* p_obj = *i;
			if(p_obj == this) continue;

			p_obj->onFire();

			if(p_obj->getFlags() & OF_DESTROYABLE)
			{
				p_obj->setDestroyTime(p_obj->getDestroyTime() - 1);
				if(!p_obj->getDestroyTime())
				{
					p_obj->disappear(0.2);

					Engine::inst().playSound("vaporize.ogg", false, 0.15);

					// debris
					const Sprites& debris = p_obj->getSprites();
					int n = debris.getTryCount(random(50, 80));
					for(int i = 0; i < n; i++)
					{
						p.lifetime = static_cast<ushort>(random(60, 120));
						p.damping = 0.9f;
						p.gravity = -0.1f;
						p.positionOnTexture = Vec2b(96, 0);
						p.sizeOnTexture = Vec2b(16, 16);
						Vec4d sampled;
						Vec2i offset;
						if(!debris.sample(&sampled, &offset)) continue;

						p.position = p_obj->getPosition() * 16 + offset + Vec2i(random(-2, 2), random(-2, 2));
						p.velocity = Vec2d(random(-0.2, 0.2), random(-0.2, 0.2));
						p.color = sampled;
						p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
						p.rotation = random(0.0f, 10.0f);
						p.deltaRotation = random(-0.1f, 0.1f);
						p.size = random(0.5f, 1.5f);
						p.deltaSize = random(0.01f, 0.05f);
						p_particleSystem->addParticle(p);
					}
				}
			}
		}
	}

	anim++;
}

void Lava::frameBegin()
{
	Object::frameBegin();
	level.setAIFlag(position, 4);
}

bool Lava::changeInEditor(int mod)
{
	if(!mod)
	{
		int o = dir / 4;
		dir = 4 * o + ((dir + 1) % 4);
	}
	else
	{
		dir += 4;
	}

	dir %= 20;

	return true;
}

void Lava::saveAttributes(TiXmlElement* p_target)
{
	p_target->SetAttribute("dir", dir);
}

void Lava::getAlpha1(const Vec2i& where,
					 double* p_out)
{
	double po = 0.3 * (where.x + where.y);
	double x = 0.1 * anim;
	p_out[0] = 1.0 + 0.1 * sin(po + x);
	p_out[1] = 1.0 + 0.1 * sin(po + 0.3 + x);
	p_out[2] = 1.0 + 0.1 * sin(po + 0.6 + x);
	p_out[3] = 1.0 + 0.1 * sin(po + 0.3 + x);
}

void Lava::getAlpha2(const Vec2i& where,
					 double* p_out)
{
	double po = 0.3 * (where.x + where.y);
	double x = 0.1 * anim;
	p_out[0] = 0.5 + 0.5 * cos(po + x);
	p_out[1] = 0.5 + 0.5 * cos(po + 0.3 + x);
	p_out[2] = 0.5 + 0.5 * cos(po + 0.6 + x);
	p_out[3] = 0.5 + 0.5 * cos(po + 0.3 + x);
}