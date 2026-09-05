#include "pch.h"
#include "diamondmachine.h"
#include "presets.h"
#include "engine.h"
#include "particlesystem.h"
#include "soundinstance.h"

/* Der Funkenflug der Umwandlung, in fuenf Fassungen zum Ansehen.

   Ein Block wird nicht ausgeblendet und durch einen Diamanten ersetzt, sondern
   auseinandergenommen und wieder zusammengesetzt: erst fliegen Funken in den
   Farben des Blocks nach aussen, dann kommen aus der Wolke, die sie
   hinterlassen haben, Funken zurueck, die unterwegs die Farbe annehmen, die der
   Diamant an der Stelle hat, wo sie landen und sterben.

   ZEITPLAN. Die Maschine zaehlt hundert Takte, und ihr eigenes Bild wechselt bei
   20, 40, 60 und 80 - das fuenfte steht von 80 bis 100 still. Daran haengen die
   Phasen, damit Bild und Funken nicht auf zwei Uhren laufen:

       Takt      0        20              53        80          100
       auswaerts |=== ausgestossen ======|                            fliegt bis 80
       einwaerts          |=== ausgestossen =========|                fliegt bis 100
       zu sehen  nur raus | beides ----------------- | nur rein |

   Die beiden Enden sind gerechnet und nicht gewaehlt: ein Funke lebt weiter,
   nachdem er ausgestossen wurde, also muss der letzte einwaerts eine ganze
   Lebensdauer vor Schluss los (100 - 20 = 80), und auswaerts muss eine
   Lebensdauer vor 80 Schluss sein (80 - 27 = 53), damit am Ende wirklich nur
   noch eingesammelt wird.

   GENAU LANDEN. Der Integrator ist position += velocity; velocity *= damping.
   Ueber n Takte legt ein Funke damit v0 * (1 - d^n) / (1 - d) zurueck, und die
   Formel dient in beide Richtungen: auswaerts sagt sie, wo die Wolke endet - und
   damit, wo die Einwaertsfunken starten duerfen -, einwaerts liefert sie das v0,
   mit dem einer genau auf seinem Ziel ankommt. d unter 1 bremst ab, d ueber 1
   zieht an; deshalb verpufft der Weg nach aussen und der nach innen saugt.

   HELL, OHNE ADDITIV ZU MISCHEN. Ein Funke soll gluehen und trotzdem die echte
   Farbe des Blocks tragen. Additiv geht beides nicht zugleich: dort haengt das
   Ergebnis am Hintergrund, und dasselbe Braun waere ueber Fels ein Glimmen und
   ueber Gras ein grelles Gelb. Statt dessen startet die Farbe ueber 1 - GL
   klemmt sie auf [0,1], der Funke ist also zuerst weiss - und faellt linear
   durch genau die Texelfarbe hindurch weiter ab. Das ist die abkuehlende Glut,
   und sie sieht auf jedem Untergrund gleich aus.

   WELCHE FASSUNG. position.y % 5. Das ist zum Vergleichen da und nichts, was so
   bleiben kann; die Testdatei dazu liegt in Tools/testlevels. */

namespace
{
	// Die Phasen, in Takten des Maschinenzaehlers.
	const int SPARK_OUT_FULL = 20;
	const int SPARK_OUT_END  = 53;
	const int SPARK_IN_START = 20;
	const int SPARK_IN_FULL  = 60;
	const int SPARK_IN_END   = 80;

	struct SparkVariant
	{
		// Welches Feld der 32er-Kachel in particles.png. (0,0) ist der weiche,
		// fast durchsichtige Wisch, den der Rauch nimmt; (32,0) ein satter
		// Klecks. Ein Funke will den Klecks - mit dem Wisch wird aus allem
		// Dunst.
		int    spriteX;

		double outRate;       // Funken je Takt bei voller Rate
		double outSpeedMin;
		double outSpeedMax;
		double outDamping;    // unter 1: bremst ab
		double outGravity;
		double outSwirl;      // Startrichtung gegen radial verdreht, im Bogenmass
		double outBright;     // Ueberhelligkeit im ersten Takt
		double outEnd;        // Helligkeit im letzten
		double outAlpha;      // Deckkraft beim Start
		int    outLife;
		double outSize;

		double inRate;
		double inDamping;     // ueber 1: zieht an
		double inSwirl;       // Startpunkt gegen die Richtung des Ziels verdreht
		double inBright;      // Helligkeit beim Aufschlag
		double inAlpha;       // Deckkraft beim Aufschlag
		int    inLife;
		double inSize;
	};

	const SparkVariant sparkVariants[] =
	{
		// Glut - der besprochene Entwurf, ohne Zutaten.
		{32,  1.4, 1.0, 2.0, 0.90, 0.00,  0.0, 2.0, 0.0, 1.00, 27, 0.28,
		      1.4, 1.06,  0.0, 1.4, 1.00, 20, 0.24},
		// Schweissfunken - wenige, sehr heiss, sehr schnell und sehr klein.
		{32,  0.7, 2.2, 3.6, 0.85, 0.00,  0.0, 3.5, 0.0, 1.00, 27, 0.17,
		      0.8, 1.10,  0.0, 2.2, 1.00, 20, 0.15},
		// Staub - die Gegenprobe: der weiche Wisch, keine Ueberhelligkeit,
		// viele und langsam. Wenn das gewinnt, war die Glut die falsche Idee.
		{ 0,  2.6, 0.5, 1.1, 0.95, 0.00,  0.0, 1.0, 1.0, 0.85, 27, 0.50,
		      2.6, 1.02,  0.0, 1.0, 0.85, 20, 0.45},
		// Wurf - auswaerts mit Schwerkraft, die Truemmer fallen im Bogen.
		{32,  1.4, 1.4, 2.4, 0.93, 0.05,  0.0, 1.8, 0.3, 1.00, 27, 0.30,
		      1.4, 1.06,  0.0, 1.4, 1.00, 20, 0.24},
		// Wirbel - schraeg hinaus und von der anderen Seite herein.
		{32,  1.4, 1.2, 2.2, 0.91, 0.00,  0.8, 2.0, 0.0, 1.00, 27, 0.28,
		      1.4, 1.06, -1.3, 1.4, 1.00, 20, 0.24}
	};

	const int NUM_SPARK_VARIANTS = sizeof(sparkVariants) / sizeof(sparkVariants[0]);

	// Anteil der vollen Rate. Beide Rampen sind linear und nicht geschaltet:
	// ein harter Wechsel liesse die Mitte als Plateau erscheinen, auf dem beide
	// Richtungen gleichzeitig auf Anschlag laufen, statt als Uebergabe.
	double outRamp(int counter)
	{
		if(counter < 0 || counter >= SPARK_OUT_END) return 0.0;
		if(counter < SPARK_OUT_FULL) return 1.0;
		return static_cast<double>(SPARK_OUT_END - counter) /
			   static_cast<double>(SPARK_OUT_END - SPARK_OUT_FULL);
	}

	double inRamp(int counter)
	{
		if(counter < SPARK_IN_START || counter >= SPARK_IN_END) return 0.0;
		if(counter >= SPARK_IN_FULL) return 1.0;
		return static_cast<double>(counter - SPARK_IN_START) /
			   static_cast<double>(SPARK_IN_FULL - SPARK_IN_START);
	}

	// Aus einer gebrochenen Rate eine ganze Zahl von Funken.
	int spawnCount(double rate)
	{
		int n = static_cast<int>(rate);
		if(random(0.0, 1.0) < rate - n) n++;
		return n;
	}

	// Der zurueckgelegte Weg nach n Takten, siehe oben.
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
	p_soundInst = 0;
}

DiamondMachine::~DiamondMachine()
{
}

void DiamondMachine::spawnSparks(Object* p_block)
{
	const SparkVariant& v = sparkVariants[position.y % NUM_SPARK_VARIANTS];

	ParticleSystem* p_sys = level.getParticleSystem();
	const Sprites& block = p_block->getSprites();

	// Der Diamant, den es noch gar nicht gibt: sein Aussehen kommt aus der
	// Tabelle der Voreinstellungen, und sample() liefert daraus Landepunkt und
	// Zielfarbe in einem Zug.
	Sprites diamond;
	const bool haveDiamond = level.getPresets()->getPresetSprites("Diamond", &diamond);

	// Das Feld ueber der Maschine, in Bildpunkten: dort steht der Block, und
	// dort steht spaeter der Diamant.
	const Vec2d origin(position.x * 16.0, (position.y - 1) * 16.0);
	const Vec2d middle = origin + Vec2d(8.0, 8.0);

	int n = spawnCount(v.outRate * outRamp(counter));
	for(int i = 0; i < n; i++)
	{
		Vec4d sampled;
		Vec2i offset;
		if(!block.sample(&sampled, &offset)) continue;

		const Vec2d start = origin + Vec2d(offset.x, offset.y);

		// Fort von der Mitte des Feldes - der Block faellt auseinander, er
		// verstreut sich nicht. Genau in der Mitte gibt es keine Richtung,
		// dort wird eine gewuerfelt.
		const Vec2d radial = start - middle;
		double angle = (radial.length() > 0.5) ? atan2(radial.y, radial.x)
											   : random(0.0, 6.2832);
		angle += v.outSwirl;

		const Vec4d hot = sampled * v.outBright;
		const Vec4d cold(sampled.r * v.outEnd, sampled.g * v.outEnd, sampled.b * v.outEnd, 0.0);

		ParticleSystem::Particle p;
		p.lifetime = v.outLife;
		p.damping = static_cast<float>(v.outDamping);
		p.gravity = static_cast<float>(v.outGravity);
		p.positionOnTexture = Vec2b(v.spriteX, 0);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = start;
		p.velocity = Vec2d(cos(angle), sin(angle)) * random(v.outSpeedMin, v.outSpeedMax);
		// Nicht sampled.a: das ist DEBRIS_ALPHA und damit ein Viertel. Ein
		// Truemmerstueck darf blass sein, ein Funke leuchtet.
		const Vec4d begin(hot.r, hot.g, hot.b, v.outAlpha);
		p.color = begin;
		p.deltaColor = (cold - begin) / static_cast<double>(v.outLife);
		p.rotation = 0.0f;
		p.deltaRotation = 0.0f;
		p.size = static_cast<float>(v.outSize);
		p.deltaSize = static_cast<float>(-v.outSize / (v.outLife * 1.3));
		p_sys->addParticle(p);
	}

	if(!haveDiamond) return;

	n = spawnCount(v.inRate * inRamp(counter));
	for(int i = 0; i < n; i++)
	{
		Vec4d target;
		Vec2i landOffset;
		if(!diamond.sample(&target, &landOffset)) continue;

		Vec4d from;
		Vec2i fromOffset;
		if(!block.sample(&from, &fromOffset)) continue;

		const Vec2d landing = origin + Vec2d(landOffset.x, landOffset.y);

		// Start irgendwo in der Wolke, welche die Auswaertsfunken hinterlassen:
		// dieselbe Verteilung, nur noch einmal gewuerfelt statt gemerkt. Auf die
		// Paarung einzelner Funken kommt es nicht an - unter Dutzenden sieht
		// niemand, welcher zu welchem gehoert, es zaehlt die Form der Wolke.
		const double radius = travelDistance(random(v.outSpeedMin, v.outSpeedMax),
											 v.outDamping, v.outLife);
		const double angle = atan2(landing.y - middle.y, landing.x - middle.x) + v.inSwirl;
		const Vec2d start = middle + Vec2d(fromOffset.x - 8.0, fromOffset.y - 8.0)
								   + Vec2d(cos(angle), sin(angle)) * radius;

		// Das v0, mit dem der Funke am Ende seines Lebens genau auf dem Ziel
		// steht. Ohne Schwerkraft, sonst traefe er daneben.
		const double k = (1.0 - v.inDamping) /
						 (1.0 - pow(v.inDamping, static_cast<double>(v.inLife)));

		const Vec4d begin(from.r, from.g, from.b, 0.0);
		const Vec4d end(target.r * v.inBright, target.g * v.inBright,
						target.b * v.inBright, v.inAlpha);

		ParticleSystem::Particle p;
		p.lifetime = v.inLife;
		p.damping = static_cast<float>(v.inDamping);
		p.gravity = 0.0f;
		p.positionOnTexture = Vec2b(v.spriteX, 0);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = start;
		p.velocity = (landing - start) * k;
		p.color = begin;
		p.deltaColor = (end - begin) / static_cast<double>(v.inLife);
		p.rotation = 0.0f;
		p.deltaRotation = 0.0f;
		p.size = static_cast<float>(v.inSize);
		p.deltaSize = 0.0f;
		p_sys->addParticle(p);
	}
}

void DiamondMachine::updateSprites()
{
	// Maschine
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
		// Befindet sich ein Objekt auf der Maschine?
		Object* p_obj = level.getFrontObjectAt(position - Vec2i(0, 1));
		if(p_obj)
		{
			if(!p_obj->isTeleporting() && (p_obj->getFlags() & OF_CONVERTABLE))
			{
				if(p_obj == p_objOnMe)
				{
					// Rauch. Die Farbe kommt aus dem Bild des Blocks; faellt die
					// Stichprobe auf eine durchsichtige Stelle, entfaellt nur
					// dieses eine Teilchen - der Zaehler unten laeuft weiter,
					// sonst haengte die Umwandlungsdauer an der Deckung des
					// Bildes und waere von Mal zu Mal eine andere.
					Vec4d sampled;
					Vec2i offset;
					if(p_obj->getSprites().sample(&sampled, &offset))
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
					counter = -1;
				}

				if(counter >= 100)
				{
					// Der Block wird umgewandelt.
					p_obj->disappearNextFrame(0.5);
					level.getPresets()->instancePreset("Diamond", position - Vec2i(0, 1), 0);
//					level.addNewObjects();
					counter = -1;
				}
			}
			else counter = -1;
		}
		else
		{
			p_objOnMe = 0;
			counter = -1;
		}
	}
	else counter = -1;

	if(counter == -1 && p_soundInst)
	{
		p_soundInst->stop();
		p_soundInst = 0;
	}
}