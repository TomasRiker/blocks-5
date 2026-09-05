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

   ABBRUCH. Der Block kann im letzten Augenblick weggeschoben, gesprengt oder
   abgeschaltet werden. Verschwinden duerfen die Funken dann nicht, das waere
   ein Loch mitten in der Bewegung: sie laufen ihren eigenen Weg zurueck, mit
   gespiegelter Lebensdauer, und was noch aussteht, entscheidet, wie lange das
   dauert (abortConversion()). Damit die Maschine sie wiederfindet, tragen sie
   ihre Kennung: Particle::id, das einzige Feld, das sonst im ganzen Spiel 0
   bleibt.

   Die Einwaertsfunken kehren immer um - ihr Ziel war ein Diamant, und der kommt
   nicht mehr. Die Auswaertsfunken haengen davon ab, ob es den Block noch gibt:
   ist er nur weggeschoben oder steht er still, weil der Strom weg ist, dann
   fliegen sie zu ihm zurueck und werden wieder eingesaugt; ist er zerstoert,
   fliegen sie unbeirrt weiter, denn dann ist Auseinanderfliegen genau das
   Richtige.

   WELCHE FASSUNG. position.y % 5. Das ist zum Vergleichen da und nichts, was so
   bleiben kann; die Testdatei dazu liegt in Tools/testlevels. */

namespace
{
	// Die Phasen, in Takten des Maschinenzaehlers.
	const int CONVERSION_TICKS = 100;  // dann wird aus dem Block der Diamant
	const int SPARK_OUT_FULL = 20;
	const int SPARK_OUT_END  = 53;
	const int SPARK_IN_START = 20;
	const int SPARK_IN_FULL  = 60;
	const int SPARK_IN_END   = 92;

	// Jeder Einwaertsfunke lebt genau so lange, wie bis zur Umwandlung noch
	// Zeit ist - egal, wann er losfliegt. Damit kommen sie nicht ueber eine
	// Minute verteilt an, sondern alle im selben Augenblick, und der ist der,
	// in dem der Diamant dasteht. Die letzten brauchen noch etwas Weg.
	const int SPARK_IN_MIN_LIFE = 8;

	struct SparkVariant
	{
		// Welches Feld der 32er-Kachel in particles.png, in Bildpunkten.
		//
		// Die Wahl entscheidet ueber die *Farbe*, nicht nur ueber die Form: das
		// Teilchen wird mit dem Feld multipliziert. (32,0) sieht als satter
		// Klecks richtig aus, ist aber ein fertig eingefaerbtes Orange
		// (231,152,41) - ein cyanfarbener Funke darauf wird oliv, und genau das
		// war an allen Fassungen ausser "Staub" zu sehen. Neutral und zugleich
		// dicht ist allein (32,32): eine runde Scheibe in reinem Weiss. (0,32)
		// waere ein weisser Vierstern, falls es einmal funkeln soll.
		int    spriteX;
		int    spriteY;

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
		// Um wie viel schneller ein Funke am Ende seines Fluges ist als am
		// Anfang. Daraus wird die Daempfung gerechnet, denn die Lebensdauer
		// steht erst beim Losfliegen fest - so sieht der Anflug bei jeder
		// Dauer gleich aus: lange schleichen, zum Schluss zuschnappen.
		double inAccel;
		double inSwirl;       // Startpunkt gegen die Richtung des Ziels verdreht
		double inStart;       // Helligkeit der Startfarbe, siehe unten
		double inBright;      // Helligkeit beim Aufschlag
		double inAlpha;       // Deckkraft beim Aufschlag
		double inSize;
	};

	const SparkVariant sparkVariants[] =
	{
		// Glut - der Entwurf, ohne Zutaten.
		{32, 32,  4.0, 1.0, 2.0, 0.90, 0.00,  0.0, 2.2, 0.0, 1.00, 27, 0.22,
		      2.6, 6.0,  0.0, 2.2, 1.5, 1.00, 0.20},
		// Schweissfunken - wenige, sehr heiss, sehr schnell und sehr klein.
		{32, 32,  2.2, 2.2, 3.6, 0.85, 0.00,  0.0, 3.5, 0.0, 1.00, 27, 0.15,
		      1.6, 9.0,  0.0, 3.0, 2.4, 1.00, 0.14},
		// Staub - die Gegenprobe: keine Ueberhelligkeit, viele, langsam und
		// gross. Wenn das gewinnt, war die Glut die falsche Idee.
		{32, 32,  6.0, 0.5, 1.1, 0.95, 0.00,  0.0, 1.0, 1.0, 0.85, 27, 0.42,
		      4.0, 3.0,  0.0, 1.4, 1.0, 0.85, 0.38},
		// Wurf - auswaerts mit Schwerkraft, die Truemmer fallen im Bogen.
		{32, 32,  4.0, 1.4, 2.4, 0.93, 0.05,  0.0, 1.8, 0.3, 1.00, 27, 0.24,
		      2.6, 6.0,  0.0, 2.2, 1.5, 1.00, 0.20},
		// Wirbel - schraeg hinaus und von der anderen Seite herein.
		{32, 32,  4.0, 1.2, 2.2, 0.91, 0.00,  0.8, 2.2, 0.0, 1.00, 27, 0.22,
		      2.6, 6.0, -1.3, 2.2, 1.5, 1.00, 0.20}
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
		if(counter < SPARK_IN_START || counter > SPARK_IN_END) return 0.0;
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

	// Wie wahrscheinlich in diesem Takt eine Rauchwolke entsteht.
	double smokeRate(int counter)
	{
		if(counter >= SPARK_OUT_END) return 0.0;
		return 0.3;
	}

	// Die Kennung fuer die Funken einer Umwandlung. Das hohe Bit ist immer
	// gesetzt: alles andere im Spiel traegt 0, also kann keine Kennung je mit
	// gewoehnlichen Teilchen zusammenfallen - auch die allererste nicht.
	uint nextSparkId()
	{
		static uint counter = 0;
		return 0x80000000u | (++counter & 0x7FFFFFFFu);
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
	sparkId = 0;
	p_soundInst = 0;
}

DiamondMachine::~DiamondMachine()
{
}

void DiamondMachine::spawnSparks(Object* p_block)
{
	const SparkVariant& v = sparkVariants[position.y % NUM_SPARK_VARIANTS];

	// Eine neue Kennung je Umwandlung, nicht je Maschine: nach einem Abbruch
	// laufen die alten Funken noch nach Hause, und die darf ein zweiter
	// Abbruch nicht ein zweites Mal umdrehen.
	if(!sparkId) sparkId = nextSparkId();

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
		p.positionOnTexture = Vec2b(v.spriteX, v.spriteY);
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
		p.id = sparkId;
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

		// So lange, wie bis zur Umwandlung noch Zeit ist: dann kommen alle
		// gemeinsam an, in dem Augenblick, in dem der Diamant erscheint.
		const int life = max(SPARK_IN_MIN_LIFE, CONVERSION_TICKS - counter);

		// Die Daempfung aus dem gewuenschten Zuwachs, damit der Anflug bei
		// jeder Dauer dieselbe Form hat: d^life = inAccel.
		const double d = pow(v.inAccel, 1.0 / static_cast<double>(life));

		// Das v0, mit dem der Funke am Ende seines Lebens genau auf dem Ziel
		// steht. Ohne Schwerkraft, sonst traefe er daneben.
		const double k = (1.0 - d) / (1.0 - v.inAccel);

		// Die Startfarbe ist ueberhell, und das ist kein Schmuck: linear von
		// einem Blau auf das warme Weiss des Diamanten fuehrt mitten durch
		// Gruen - gemessen 0.16 Gruenstich bei t=0.6, und das war deutlich zu
		// sehen. Ueber 1 gestartet bleiben die starken Kanaele geklemmt,
		// waehrend der schwache aufholt; der Weg geht dann ueber Weiss. Fuer
		// denselben Block faellt der Gruenstich damit auf 0.05.
		const Vec4d begin(from.r * v.inStart, from.g * v.inStart, from.b * v.inStart, 0.0);
		const Vec4d end(target.r * v.inBright, target.g * v.inBright,
						target.b * v.inBright, v.inAlpha);

		ParticleSystem::Particle p;
		p.lifetime = static_cast<uint>(life);
		p.damping = static_cast<float>(d);
		p.gravity = 0.0f;
		p.positionOnTexture = Vec2b(v.spriteX, v.spriteY);
		p.sizeOnTexture = Vec2b(16, 16);
		p.position = start;
		p.velocity = (landing - start) * k;
		p.color = begin;
		p.deltaColor = (end - begin) / static_cast<double>(life);
		p.rotation = 0.0f;
		p.deltaRotation = 0.0f;
		p.size = static_cast<float>(v.inSize);
		p.deltaSize = 0.0f;
		p.id = sparkId;
		p_sys->addParticle(p);
	}
}

Object* DiamondMachine::findLivingBlock()
{
	// Der Block, der die Umwandlung angefangen hat - falls es ihn noch gibt.
	// Gesucht wird in der Objektliste und nicht ueber p_objOnMe hineingegriffen,
	// denn genau im interessanten Fall ist der Zeiger nichts mehr wert: ein
	// zerstoerter Block wird zu Beginn eines Taktes geloescht. isAlive() ist
	// falsch, solange er in sich zusammenfaellt, und wer wegteleportiert wird,
	// ist gleich ganz woanders - beides zaehlt als "nicht mehr da".
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

	const SparkVariant& v = sparkVariants[position.y % NUM_SPARK_VARIANTS];

	// Um wie viel der Block seit dem Losfliegen versetzt ist, in Bildpunkten.
	// Sein *logisches* Feld und nicht sein gezeigtes: geschoben wird er ueber
	// mehrere Takte, und wenn die Funken ankommen, steht er dort schon.
	Object* p_block = findLivingBlock();
	Vec2d shift(0.0, 0.0);
	if(p_block)
		shift = Vec2d((p_block->getPosition().x - position.x) * 16.0,
					  (p_block->getPosition().y - (position.y - 1)) * 16.0);

	// Rueckwaerts heisst: jeder Delta kehrt sich um, und die Daempfung wird ihr
	// Kehrwert, denn sie ist ein Faktor und kein Summand. Die Geschwindigkeit
	// bekommt diesen Kehrwert obendrein, weil der Integrator erst schiebt und
	// dann daempft - ohne ihn laege die Rueckreise um einen Takt versetzt und
	// traefe den Startpunkt nicht. Nur die Schwerkraft laesst sich so nicht
	// genau umkehren (sie kommt nach der Daempfung dazu, nicht davor); ihr
	// Vorzeichen zu wenden ist eine Naeherung, und die eine Fassung mit
	// Schwerkraft findet auf dem Rueckweg einen leicht anderen Bogen.
	ParticleSystem* p_sys = level.getParticleSystem();
	for(ParticleSystem::ParticleList::iterator i = p_sys->begin();
		i != p_sys->end(); ++i)
	{
		ParticleSystem::Particle& p = *i;
		if(p.id != sparkId) continue;

		// Einwaerts oder auswaerts? Die Deckkraft sagt es, ohne zweite Kennung:
		// der eine blendet auf seinem Weg ein, der andere aus.
		const bool inward = (p.deltaColor.a > 0.0f);

		// Ein zerstoerter Block hat nichts mehr, wohin die Truemmer
		// zurueckkoennten; die fliegen also weiter, als waere nichts gewesen.
		// Die Einwaertsfunken kehren immer um: ihr Ziel war ein Diamant, und
		// der kommt in keinem Fall mehr.
		if(!inward && !p_block) continue;

		// Die gespiegelte Lebensdauer: so viele Takte, wie er schon fliegt. Wer
		// gerade erst los ist, ist sofort vorbei; wer fast am Ziel war, hat den
		// ganzen Weg vor sich. Einwaerts steht sie in der Deckkraft, die von 0
		// aus je Takt um deltaColor.a gewachsen ist; auswaerts ist die ganze
		// Dauer bekannt, und was davon noch aussteht, ist lifetime.
		uint elapsed = 0;
		if(inward) elapsed = static_cast<uint>(p.color.a / p.deltaColor.a + 0.5f);
		else if(p.lifetime < static_cast<uint>(v.outLife))
			elapsed = static_cast<uint>(v.outLife) - p.lifetime;

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

		// Der Rueckweg trifft den Startpunkt, aber der Block steht inzwischen
		// vielleicht woanders. Ein Zuschlag auf die Geschwindigkeit verschiebt
		// die ganze Bahn um genau diesen Versatz - dieselbe Wegformel wie beim
		// Anflug, nur nach v0 aufgeloest, also keine Verzerrung der Bahn,
		// sondern eine Parallelverschiebung.
		if(!inward && elapsed && !shift.isZero())
		{
			const double d = p.damping;
			const double k = (d == 1.0)
				? 1.0 / elapsed
				: (1.0 - d) / (1.0 - pow(d, static_cast<double>(elapsed)));
			p.velocity += shift * k;
		}

		// Ein Takt mehr als Wege: der letzte Aufruf zaehlt nur herunter und
		// loescht, er bewegt nicht mehr. Und 0 waere hier toedlich - der
		// Zaehler ist vorzeichenlos und liefe ueber.
		p.lifetime = elapsed + 1;
	}

	sparkId = 0;
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
					//
					// Duenner als frueher und zum Schluss gar nicht mehr: eine
					// Rauchwolke lebt achtzig bis hundertzwanzig Takte und
					// waechst dabei, ein Funke lebt keine dreissig und
					// schrumpft. Bei einem Teilchen je Takt liegt darum immer
					// eine Dunstglocke ueber allem, und die Funken verschwinden
					// darin. Das letzte Viertel gehoert dem Einsammeln allein.
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

					// Wie weit die Umwandlung ist, dem Block in die Hand
					// gedrueckt - er zeichnet sich selbst blasser. Jeden Takt
					// neu, denn er loescht den Wert in seinem frameBegin();
					// bleibt der Druck aus, steht er von allein wieder voll da.
					// p_obj kommt frisch aus getFrontObjectAt() und wird nur
					// hier und jetzt angefasst; p_objOnMe bleibt ein Zeiger,
					// der ueber Takte hinweg nur verglichen wird.
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
					// Der Block wird umgewandelt.
					p_obj->disappearNextFrame(0.5);
					level.getPresets()->instancePreset("Diamond", position - Vec2i(0, 1), 0);
//					level.addNewObjects();
					counter = -1;

					// Kein abortConversion(): die Einwaertsfunken sind in
					// eben diesem Takt angekommen und gestorben. Die Kennung
					// wird nur weggelegt, damit die naechste Umwandlung eine
					// eigene bekommt.
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