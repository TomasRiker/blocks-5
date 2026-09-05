#include "pch.h"
#include "level.h"
#include "filesystem.h"
#include "tileset.h"
#include "presets.h"
#include "particlesystem.h"
#include "sound.h"
#include "soundinstance.h"
#include "engine.h"
#include "texture.h"
#include "object.h"
#include "arrow.h"
#include "barrage.h"
#include "barrage2.h"
#include "player.h"
#include "exit.h"
#include "elevator.h"
#include "rail.h"
#include "cannon.h"
#include "gui.h"
#include "font.h"
#include "electronics.h"
#include "lightbarriersender.h"
#include <algorithm>

SoundInstance* Level::p_rainSoundInst = 0;
SoundInstance* Level::p_thunderstormSoundInst = 0;
bool Level::rainSoundOn = false;
bool Level::thunderstormSoundOn = false;
// Der Skin, auf den zurueckgefallen wird: wenn eine Datei des gewuenschten
// Skins fehlt (dafuer gibt es die "default_"-Marker) und wenn sie sich nicht
// laden laesst.
const char* p_defaultSkin = "blocks_01";

// Wenn sich eine Leveldatei nicht laden laesst, wird nicht ein leerer Level
// gezeigt, sondern dieser: das Wort ERROR aus Bloecken, mit Bob im O
// eingesperrt. Der Wachposten haelt die Rekursion an, falls der Fehler-Level
// selbst einmal fehlen oder kaputt sein sollte.
const char* p_errorLevelFilename = "level_error.xml";
bool loadingErrorLevel = false;

const char* p_skinFilenames[] = {"tileset.xml", "sprites.png", "particles.png", "background.png", "hint.png", "hintfont.xml", "noise.png", "shine.png", "rain.png", "clouds.png", "snow.png"};

Level::Level()
{
	p_activePlayer = 0;
	p_exit = 0;
	p_tiles = 0;
	p_aiFlags = 0;
	p_objectsAt = 0;

	inEditor = false;
	inCat = false;
	inPreview = false;
	inMenu = false;
	layerListBase = 0;
	cameraShake = 0.0;
	flash = 0.0;
	flashJitter = 0.0;
	actualFlash = 0.0;
	toxic = 0.0;
	finished = false;
	bufferID = 0;

	p_tileSet = 0;
	p_sprites = 0;
	p_lava[0] = p_lava[1] = 0;
	p_noise = 0;
	p_shine = 0;
	p_rain = 0;
	p_clouds = 0;
	p_snow = 0;
	p_background = 0;
	p_hint = 0;
	p_hintFont = 0;
	hintScroll = false;
	p_presets = 0;
	p_particleSystem = 0;
	p_fireParticleSystem = 0;
	p_rainParticleSystem = 0;
	p_particleSprites = 0;

	// Regen-Sound erstellen
	if(!p_rainSoundInst)
	{
		Sound* p_sound = Manager<Sound>::inst().request("rain.ogg");
		p_rainSoundInst = p_sound->createInstance(true);
		p_rainSoundInst->setVolume(0.0);
		p_sound->release();
	}

	// Gewitter-Sound erstellen
	if(!p_thunderstormSoundInst)
	{
		Sound* p_sound = Manager<Sound>::inst().request("thunderstorm.ogg");
		p_thunderstormSoundInst = p_sound->createInstance(true);
		p_thunderstormSoundInst->setVolume(0.0);
		p_sound->release();
	}

	Engine&	engine = Engine::inst();
	const Vec2i& screenPow2Size = engine.getScreenPow2Size();

	// Textur fuer den Effekt-Puffer erzeugen
	glGenTextures(1, &bufferID);
	glBindTexture(GL_TEXTURE_2D, bufferID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screenPow2Size.x, screenPow2Size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

Level::~Level()
{
	clear();

	if(bufferID) glDeleteTextures(1, &bufferID);
}

void Level::clear()
{
	// Objekte loeschen
	removeOldObjects();
	addNewObjects();
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i) removeObject(*i);
	removeOldObjects();

	delete[] p_objectsAt;
	p_objectsAt = 0;
	objects.clear();
	objectsToAdd.clear();
	objectsToRemove.clear();

	// Tiles loeschen
	delete[] p_tiles;
	p_tiles = 0;
	if(layerListBase)
	{
		glDeleteLists(layerListBase, NUM_LAYERS);
		layerListBase = 0;
	}

	delete[] p_aiFlags;
	p_aiFlags = 0;

	if(p_tileSet) p_tileSet->release();
	p_tileSet = 0;
	if(p_sprites) p_sprites->release();
	p_sprites = 0;
	if(p_lava[0]) p_lava[0]->release();
	if(p_lava[1]) p_lava[1]->release();
	p_lava[0] = p_lava[1] = 0;
	if(p_noise) p_noise->release();
	p_noise = 0;
	if(p_shine) p_shine->release();
	p_shine = 0;
	if(p_rain) p_rain->release();
	p_rain = 0;
	if(p_clouds) p_clouds->release();
	p_clouds = 0;
	if(p_snow) p_snow->release();
	p_snow = 0;
	if(p_background) p_background->release();
	p_background = 0;
	if(p_hint) p_hint->release();
	p_hint = 0;
	if(p_hintFont) p_hintFont->release();
	p_hintFont = 0;
	if(p_presets) delete p_presets;
	p_presets = 0;
	if(p_particleSystem) delete p_particleSystem;
	p_particleSystem = 0;
	if(p_fireParticleSystem) delete p_fireParticleSystem;
	p_fireParticleSystem = 0;
	if(p_rainParticleSystem) delete p_rainParticleSystem;
	p_rainParticleSystem = 0;
	if(p_particleSprites) p_particleSprites->release();
	p_particleSprites = 0;

	counter = 0;
	time = 0;
	numDiamondsNeeded = 0;
	numDiamondsCollected = 0;
	hudIconFlash[0] = hudIconFlash[1] = 0.0;
	electricityOn = false;
	nightVision = false;
	raining = false;
	cloudy = false;
	snowing = false;
	thunderstorm = false;
	lightColor = Vec3i(255, 255, 255);
	title = "";
	musicFilename = "";
	for(int i = 0; i < SKIN_MAX; i++) skin[i] = requestedSkin[i] = "";
	cameraShake = 0.0;
	flash = 0.0;
	flashJitter = 0.0;
	actualFlash = 0.0;
	toxic = 0.0;
	finished = false;
	lightningCounter = random(50, 150);
}

bool Level::load(const std::string& filename,
				 bool dontReallyLoad)
{
	this->filename = filename;

	// XML-Dokument laden
	std::string text = FileSystem::inst().readStringFromFile(filename);
	TiXmlDocument doc;
	doc.SetCondenseWhiteSpace(false);
	doc.Parse(text.c_str());
	if(doc.ErrorId())
	{
		printfLog("+ ERROR: Could not parse level XML file \"%s\" (Error: %d).\n",
				  filename.c_str(),
				  doc.ErrorId());
		loadErrorLevel();
		return false;
	}

	return load(&doc, dontReallyLoad);
}

bool Level::load(TiXmlDocument* p_doc,
				 bool dontReallyLoad)
{
	clear();

	TiXmlElement* p_level = p_doc->FirstChildElement("Level");
	if(!p_level)
	{
		printfLog("+ ERROR: Level XML file \"%s\" is invalid.\n",
				  filename.c_str());
		loadErrorLevel();
		return false;
	}

	int extendedAttributes = 0;
	p_level->QueryIntAttribute("extendedAttributes", &extendedAttributes);
	if(extendedAttributes)
	{
		int ndc;
		p_level->Attribute("numDiamondsCollected", &ndc);
		numDiamondsCollected = ndc;
	}

	// Titel lesen
	title = "\xA7" "en:Unnamed Level\xA7" "de:Unbenannter Level";
	const char* p_temp = p_level->Attribute("title");
	if(p_temp) title = p_temp;

	// Skins lesen
	for(int i = 0; i < SKIN_MAX; i++)
	{
		skin[i] = "";
		char attrName[256] = "";
		sprintf(attrName, "skin%d", i);
		p_temp = p_level->Attribute(attrName);
		if(p_temp) skin[i] = requestedSkin[i] = p_temp;
	}

	if(!dontReallyLoad) loadSkin();

	// Groesse und Ebenenzahl stehen fest (Level::WIDTH, HEIGHT, NUM_LAYERS). Die
	// Datei nennt sie trotzdem, und hier wird sie beim Wort genommen: sonst
	// landeten die Zeilen einer 60x40-Datei in einem 40x25-Raster. Fehlende
	// Attribute gelten als richtig, denn TiXmlElement::Attribute laesst den Wert
	// unberuehrt.
	// Erst den Speicher holen, dann pruefen: von den vierzehn Stellen, die load()
	// rufen, sehen sich nur zwei den Rueckgabewert an, ein Abbruch darf also
	// keinen halbfertigen Level hinterlassen.
	allocateTiles();

	int fileWidth = WIDTH, fileHeight = HEIGHT, fileNumLayers = NUM_LAYERS;
	p_level->Attribute("width", &fileWidth);
	p_level->Attribute("height", &fileHeight);
	p_level->Attribute("numLayers", &fileNumLayers);
	if(fileWidth != WIDTH || fileHeight != HEIGHT || fileNumLayers != NUM_LAYERS)
	{
		printfLog("+ ERROR: Level \"%s\" is %dx%d with %d layer(s); only %dx%d with %d is supported.\n",
				  filename.c_str(),
				  fileWidth, fileHeight, fileNumLayers,
				  WIDTH, HEIGHT, NUM_LAYERS);
		loadErrorLevel();
		return false;
	}

	int temp = 0;
	p_level->QueryIntAttribute("numDiamondsNeeded", &temp);
	numDiamondsNeeded = temp;

	if(!dontReallyLoad)
	{
		// Layer-Elemente verarbeiten
		int layer = 0;
		TiXmlElement* p_layer = p_level->FirstChildElement("Layer");
		while(p_layer)
		{
			int row = 0;
			TiXmlElement* p_row = p_layer->FirstChildElement("Row");
			while(p_row)
			{
				// Tiles dieser Reihe setzen
				if(p_row->GetText())
				{
					std::string content = p_row->GetText();
					for(uint col = 0; col < content.length() && col < static_cast<uint>(WIDTH); col++)
					{
						uint tile = static_cast<uint>(content[col]);
						if(tile == ' ') tile = 0;
						setTileAt(layer, Vec2i(col, row), tile);
					}
				}

				p_row = p_row->NextSiblingElement("Row");
				row++;
			}

			p_layer = p_layer->NextSiblingElement("Layer");
			layer++;

			// Nicht ueber NUM_LAYERS hinaus, auch wenn die Datei mehr <Layer>
			// mitbringt: p_tiles ist fuer genau so viele reserviert, und eine von
			// aussen eingefuehrte Datei bestimmt beide Zahlen selbst.
			if(layer >= NUM_LAYERS) break;
		}

		// Objekt-Elemente verarbeiten
		TiXmlElement* p_object = p_level->FirstChildElement("Object");
		std::list<std::pair<Electronics*, TiXmlElement*> > electronics;
		while(p_object)
		{
			// Typ und Position einlesen
			std::string type = p_object->Attribute("type");
			Vec2i position;
			p_object->Attribute("x", &position.x);
			p_object->Attribute("y", &position.y);

			// Objekt instanzieren
			Object* p_theObject = p_presets->instancePreset(type, position, p_object);
			if(p_theObject)
			{
				if(p_theObject->getFlags() & Object::OF_ELECTRONICS)
				{
					std::pair<Electronics*, TiXmlElement*> p;
					p.first = static_cast<Electronics*>(p_theObject);
					p.second = p_object;
					electronics.push_back(p);
				}

				if(extendedAttributes)
				{
					int destroyTime;
					p_object->QueryIntAttribute("destroyTime", &destroyTime);
					p_theObject->setDestroyTime(destroyTime);

					int ghost;
					p_object->QueryIntAttribute("ghost", &ghost);
					p_theObject->setGhost(ghost ? true : false);

					p_theObject->loadExtendedAttributes(p_object);
				}
			}

			p_object = p_object->NextSiblingElement("Object");
		}

		// Objekte hinzufuegen und sortieren
		addNewObjects();
		sortObjects();

		// Verbindungen der elektronischen Bauteile laden
		for(std::list<std::pair<Electronics*, TiXmlElement*> >::const_iterator i = electronics.begin(); i != electronics.end(); ++i)
		{
			i->first->loadConnections(i->second);
		}
	}

	if(!dontReallyLoad)
	{
		// Strom an? Nachtsicht? Regen?
		temp = 0;
		p_level->QueryIntAttribute("electricityOn", &temp);
		bool on = temp ? true : false;
		electricityOn = !on;
		setElectricityOn(on);
		temp = 0;
		p_level->QueryIntAttribute("nightVision", &temp);
		on = temp ? true : false;
		setNightVision(on);
		temp = 0;
		p_level->QueryIntAttribute("raining", &temp);
		on = temp ? true : false;
		setRaining(on);
		temp = 0;
		p_level->QueryIntAttribute("clouds", &temp);
		on = temp ? true : false;
		setCloudy(on);
		temp = 0;
		p_level->QueryIntAttribute("snowing", &temp);
		on = temp ? true : false;
		setSnowing(on);
		temp = 0;
		p_level->QueryIntAttribute("thunderstorm", &temp);
		on = temp ? true : false;
		setThunderstorm(on);

		// Lichtfarbe lesen
		lightColor = Vec3i(255, 255, 255);
		p_level->QueryIntAttribute("lightColorR", &lightColor.r);
		p_level->QueryIntAttribute("lightColorG", &lightColor.g);
		p_level->QueryIntAttribute("lightColorB", &lightColor.b);

		if(!inEditor && !inCat)
		{
			// Frame beginnen
			for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
			{
				(*i)->frameBegin();
			}

			// Lichtschranken aktualisieren
			for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
			{
				LightBarrierSender *p_lbs = dynamic_cast<LightBarrierSender*>(*i);
				if(p_lbs) p_lbs->update();
			}

			// Elektronik zu Beginn ein paar Mal verarbeiten
			for(int i = 0; i < 20; i++) Electronics::updateAll(*this);

			// Objekte bewegen
			for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
			{
				Object* p_obj = *i;
				p_obj->update();
				if(p_obj->toBeRemoved()) removeObject(p_obj);
			}
		}
	}

	// Musikdateiname lesen
	musicFilename = "";
	p_temp = p_level->Attribute("musicFilename");
	if(p_temp) musicFilename = p_temp;

	// Display-Lists fuer die Layer erzeugen
	layerListBase = glGenLists(NUM_LAYERS);
	layerDirty = ~0;

	return true;
}

bool Level::save(const std::string& filename)
{
	TiXmlDocument* p_doc = save();
	std::string xml;
	xml << *p_doc;
	bool r = FileSystem::inst().writeStringToFile(xml, filename);
	delete p_doc;
	return r;
}

TiXmlDocument* Level::save()
{
	// alte Objekte loeschen, neue Objekte hinzufuegen
	removeOldObjects();
	addNewObjects();

	// Objekte sortieren
	sortObjects();

	TiXmlDocument* p_doc = new TiXmlDocument;

	TiXmlDeclaration* p_decl = new TiXmlDeclaration("1.0", "", "");
	p_doc->LinkEndChild(p_decl);

	TiXmlElement* p_level = new TiXmlElement("Level");

	if(!inEditor)
	{
		p_level->SetAttribute("extendedAttributes", 1);
		p_level->SetAttribute("numDiamondsCollected", numDiamondsCollected);
	}

	p_level->SetAttribute("title", title);

	// Skins schreiben
	for(int i = 0; i < SKIN_MAX; i++)
	{
		char attrName[256] = "";
		sprintf(attrName, "skin%d", i);
		p_level->SetAttribute(attrName, requestedSkin[i]);
	}

	p_level->SetAttribute("width", WIDTH);
	p_level->SetAttribute("height", HEIGHT);
	p_level->SetAttribute("numLayers", NUM_LAYERS);
	p_level->SetAttribute("numDiamondsNeeded", numDiamondsNeeded);
	p_level->SetAttribute("electricityOn", electricityOn ? 1 : 0);
	p_level->SetAttribute("nightVision", nightVision ? 1 : 0);
	p_level->SetAttribute("raining", raining ? 1 : 0);
	p_level->SetAttribute("clouds", cloudy ? 1 : 0);
	p_level->SetAttribute("snowing", snowing ? 1 : 0);
	p_level->SetAttribute("thunderstorm", thunderstorm ? 1 : 0);
	p_level->SetAttribute("lightColorR", lightColor.r);
	p_level->SetAttribute("lightColorG", lightColor.g);
	p_level->SetAttribute("lightColorB", lightColor.b);
	p_level->SetAttribute("musicFilename", musicFilename);

	for(int layer = 0; layer < NUM_LAYERS; layer++)
	{
		TiXmlElement* p_layer = new TiXmlElement("Layer");
		for(int y = 0; y < HEIGHT; y++)
		{
			char* p_temp = new char[WIDTH + 1];
			for(int x = 0; x < WIDTH; x++)
			{
				uint t = getTileAt(layer, Vec2i(x, y));
				p_temp[x] = t ? t : ' ';
			}

			p_temp[WIDTH] = 0;

			TiXmlElement* p_row = new TiXmlElement("Row");
			TiXmlText* p_rowText = new TiXmlText(p_temp);
			p_row->LinkEndChild(p_rowText);

			p_layer->LinkEndChild(p_row);
			delete[] p_temp;
		}

		p_level->LinkEndChild(p_layer);
	}

	std::list<std::pair<Electronics*, TiXmlElement*> > electronics;
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		if((*i)->isAlive() && !(*i)->isFalling())
		{
			TiXmlElement* p_object = new TiXmlElement("Object");
			p_object->SetAttribute("type", (*i)->getType());
			const Vec2i& position = (*i)->getPosition();
			p_object->SetAttribute("x", position.x);
			p_object->SetAttribute("y", position.y);
			(*i)->saveAttributes(p_object);
			if(!inEditor)
			{
				p_object->SetAttribute("destroyTime", (*i)->getDestroyTime());
				p_object->SetAttribute("ghost", (*i)->isGhost() ? 1 : 0);
				(*i)->saveExtendedAttributes(p_object);
			}

			if((*i)->getFlags() & Object::OF_ELECTRONICS)
			{
				std::pair<Electronics*, TiXmlElement*> p;
				p.first = static_cast<Electronics*>(*i);
				p.second = p_object;
				electronics.push_back(p);
			}

			p_level->LinkEndChild(p_object);
		}
	}

	// Verbindungen der elektronischen Bauteile speichern
	for(std::list<std::pair<Electronics*, TiXmlElement*> >::const_iterator i = electronics.begin(); i != electronics.end(); ++i)
	{
		i->first->saveConnections(i->second);
	}

	p_doc->LinkEndChild(p_level);

	return p_doc;
}

void Level::render()
{
	// Einmal je Bild das Aussehen aller Objekte auf den Stand bringen. Danach
	// gehen zwoelf Ebenen darueber, die nur noch zeichnen, was hier steht - wer
	// stattdessen in onRender aktualisierte, taete es vierzehnmal und mit der
	// Farbe des jeweiligen Durchgangs.
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		(*i)->onBeforeRender();
	}

	bool targetRaining = raining;
	bool targetThunderstorm = thunderstorm;
	if(inEditor) targetRaining = false, targetThunderstorm = false;

	if(!inCat)
	{
		if(targetRaining != rainSoundOn)
		{
			if(targetRaining)
			{
				if(!inEditor)
				{
					p_rainSoundInst->play(true);
					p_rainSoundInst->slideVolume(0.5, 0.03);
					rainSoundOn = true;
				}
			}
			else
			{
				p_rainSoundInst->slideVolume(-1.0, 0.03);
				rainSoundOn = false;
			}
		}

		if(targetThunderstorm != thunderstormSoundOn)
		{
			if(targetThunderstorm)
			{
				if(!inEditor)
				{
					p_thunderstormSoundInst->play(true);
					p_thunderstormSoundInst->slideVolume(0.5, 0.03);
					thunderstormSoundOn = true;
				}
			}
			else
			{
				p_thunderstormSoundInst->slideVolume(-1.0, 0.03);
				thunderstormSoundOn = false;
			}
		}

		if(!inMenu)
		{
			// Hintergrundbild rendern
			p_background->bind();
			glBegin(GL_QUADS);
			glColor4d(1.0, 1.0, 1.0, 1.0);
			glTexCoord2i(0, 0);
			glVertex2i(0, 0);
			glTexCoord2i(640, 0);
			glVertex2i(640, 0);
			glTexCoord2i(640, 480);
			glVertex2i(640, 480);
			glTexCoord2i(0, 480);
			glVertex2i(0, 480);
			glEnd();
			p_background->unbind();
		}
	}

	Vec2i offset;
	if(cameraShake > 0.0)
	{
		// Kamera wackeln lassen
		offset.x = static_cast<int>(sin(static_cast<double>(counter) * 1.5) * cameraShake);
		offset.y = static_cast<int>(cos(static_cast<double>(counter) * 1.5) * 5.0 * cameraShake);
	}
	else offset = Vec2i(0, 0);

	glPushMatrix();
	glTranslated(offset.x, offset.y, 0.0);

	// Objekte sortieren
	sortObjects();

	// Hintergrund rendern
	renderTiles(0, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0));

	// Lava-Raender rendern
	Texture* p_lavaEdges = Manager<Texture>::inst().request("lava_edges.png");
	p_lavaEdges->bind();

	glClear(GL_STENCIL_BUFFER_BIT);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_NOTEQUAL, 0.0f);
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_ALWAYS, 1, ~0);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glColorMask(0, 0, 0, 0);

	renderObjects(735, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);
	renderObjects(736, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);

	glDisable(GL_ALPHA_TEST);
	glColorMask(1, 1, 1, 1);

	p_lavaEdges->unbind();
	p_lavaEdges->release();

	glStencilFunc(GL_EQUAL, 0, ~0);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	Engine& engine = Engine::inst();

	// Lava rendern
	p_lava[0]->bind();
	renderObjects(737, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);
	p_lava[0]->unbind();
	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
	p_lava[1]->bind();
	renderObjects(738, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);
	p_lava[1]->unbind();
	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

	glDisable(GL_STENCIL_TEST);

	// Hintergrundobjekte rendern
	renderObjects(0, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);

	// Elektronik-Verbindungen rendern
	glPushMatrix();
	glTranslated(0.5, 0.5, 0.0);
	renderObjects(939, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);
	glPopMatrix();

	// Regen-Partikelsystem rendern
	p_rainParticleSystem->render();

	// Schatten rendern
	Vec2i samples[] = {Vec2i(2, 1), Vec2i(1, 2), Vec2i(2, 2)};
	int start = 0;
	int numSamples = 2;
	int details = engine.getDetails();
	if(details == 0) start = 2, numSamples = 1;
	Vec4d shadowColor(0.0, 0.0, 0.0, 0.7 / numSamples);
	for(int i = 0; i < numSamples; i++)
	{
		renderTiles(1, samples[start + i], shadowColor);
		renderObjects(1, samples[start + i], shadowColor, true);
	}

	// Mittelgrund rendern
	renderTiles(1, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0));
	renderObjects(1, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);

	// Partikelsysteme rendern
	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
	p_fireParticleSystem->render();
	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	p_particleSystem->render();

	// Spezialeffekt-Layer rendern
	renderObjects(16, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);

	if(inEditor && !inCat && !inPreview) renderObjects(255, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);

	// Regen
	if(!inEditor && raining)
	{
		p_rain->bind();
		glMatrixMode(GL_TEXTURE);

		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_NOTEQUAL, 0.0f);

		int start = 2;
		int numLayers = 3;
		double alpha = 0.12;
		if(details == 0) start = 1, numLayers = 1, alpha = 0.2;
		else if(details == 1) start = 2, numLayers = 2, alpha = 0.16;

		for(int i = start; i > start - numLayers; i--)
		{
			double y = 100.0 * i + 1000.0 * 0.001 * time;
			double s[] = {1.0, 0.5, 0.25};
			double angle = 15.0 + sin(0.02 * y * s[i] + i);

			glPushMatrix();
			glScaled(s[i], s[i], s[i]);
			glTranslated(0.0, -y / s[i], 0.0);
			glRotated(angle, 0.0, 0.0, 1.0);
			glBegin(GL_QUADS);
			glColor4d(0.8, 0.8, 0.8, alpha);
			glTexCoord2i(0, 0);
			glVertex2i(0, 0);
			glTexCoord2i(640, 0);
			glVertex2i(640, 0);
			glTexCoord2i(640, 480);
			glVertex2i(640, 480);
			glTexCoord2i(0, 480);
			glVertex2i(0, 480);
			glEnd();
			glPopMatrix();
		}

		glDisable(GL_ALPHA_TEST);

		p_rain->unbind();
	}

	// Schnee
	if(!inEditor && snowing)
	{
		p_snow->bind();
		glMatrixMode(GL_TEXTURE);

		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_NOTEQUAL, 0.0f);

		int numLayers = 3;
		if(details == 0) numLayers = 1;
		else if(details == 1) numLayers = 2;
		double alphaFactor = 3.0 / static_cast<double>(numLayers);

		for(int i = numLayers - 1; i >= 0; i--)
		{
			double s[] = {1.0, 1.5, 1.75};
			double t = 0.001 * time;
			double f = 0.1 * (1.0 + 1.0 / (1.0 + i));
			double x = 500.0 * sin(t * f + i);
			double y = 150.0 * t + 300.0 * cos(t * f + i);

			glPushMatrix();
			glTranslated(-x, -y, 0.0);
			glScaled(s[i], s[i], s[i]);
			glBegin(GL_QUADS);
			glColor4d(1.0, 1.0, 1.0, 0.65 * alphaFactor);
			glTexCoord2i(0, 0);
			glVertex2i(0, 0);
			glTexCoord2i(640, 0);
			glVertex2i(640, 0);
			glTexCoord2i(640, 480);
			glVertex2i(640, 480);
			glTexCoord2i(0, 480);
			glVertex2i(0, 480);
			glEnd();
			glPopMatrix();
		}

		glDisable(GL_ALPHA_TEST);

		p_snow->unbind();
	}

	// Wolken
	if(!inEditor && cloudy)
	{
		p_clouds->bind();
		glMatrixMode(GL_TEXTURE);

		int numLayers = 3;
		if(details == 0) numLayers = 1;
		else if(details == 1) numLayers = 2;

		for(int i = numLayers - 1; i >= 0; i--)
		{
			double s[] = {1.0, 0.5, 0.25};
			double x = 100.0 * i + 50.0 * 0.001 * time;
			x += 2.0 * sin(0.02 * x * s[i] + i);

			glPushMatrix();
			glScaled(s[i], s[i] * 2.0, s[i]);
			glTranslated(-x / s[i], 0.0, 0.0);
			glRotated(15.0 + 5.0 * i, 0.0, 0.0, 1.0);
			glBegin(GL_QUADS);
			const double c = 1.0 - 0.05 * i;
			const double a = 0.175 - 0.05 * i;
			glColor4d(c, c, c, a);
			glTexCoord2i(0, 0);
			glVertex2i(0, 0);
			glTexCoord2i(640, 0);
			glVertex2i(640, 0);
			glTexCoord2i(640, 480);
			glVertex2i(640, 480);
			glTexCoord2i(0, 480);
			glVertex2i(0, 480);
			glEnd();
			glPopMatrix();
		}

		p_clouds->unbind();
	}

	glMatrixMode(GL_MODELVIEW);

	// Licht
	if(lightColor != Vec3i(255, 255, 255))
	{
		engine.setBlendFunc(GL_DST_COLOR, GL_ZERO, GL_ONE, GL_ONE);

		glBegin(GL_QUADS);
		glColor3dv(Vec3d(1.0 / 255.0) * lightColor);
		glVertex2i(-100, -100);
		glVertex2i(740, -100);
		glVertex2i(740, 580);
		glVertex2i(-100, 580);
		glEnd();

		engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	}

	// Gewitter
	if(!inEditor && thunderstorm) lightning.render();

	if(toxic > 0.1)
	{
		renderToxicEffect();
	}

	// Nachtsicht
	if(nightVision && !inEditor)
	{
		// alle Alphawerte auf null setzen
//		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
//		glClear(GL_COLOR_BUFFER_BIT);

		// dreckiger Workaround
		glColorMask(0, 0, 0, 1);
		engine.setBlendFunc(GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO);
		glBegin(GL_QUADS);
		glVertex2i(-100, -100);
		glVertex2i(740, -100);
		glVertex2i(740, 580);
		glVertex2i(-100, 580);
		glEnd();

		// Lichter ansammeln
		engine.setBlendFunc(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
		renderObjects(18, Vec2i(0, 0), Vec4d(1.0), false);
		if(thunderstorm) lightning.render();

		glColorMask(1, 1, 1, 0);

		// alles Unbeleuchtete dunkel machen
		Vec4d c(0.082352941176470588235294117647059);
		engine.setBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA, GL_ONE, GL_ONE);
		glBegin(GL_QUADS);
		glColor4dv(c);
		glVertex2i(-100, -100);
		glVertex2i(740, -100);
		glVertex2i(740, 580);
		glVertex2i(-100, 580);
		glEnd();
		engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

		glColorMask(1, 1, 1, 1);

		// "Funkel-Layer" rendern
		renderObjects(17, Vec2i(0, 0), Vec4d(1.0), false);

		// Rauschen rendern
		engine.setBlendFunc(GL_DST_COLOR, GL_ZERO, GL_ONE, GL_ONE);
		p_noise->bind();
		Vec2i o1(random(0, 512 - 200), random(0, 512 - 160));
		Vec2i o2(random(0, 512 - 300), random(0, 512 - 240));
		glBegin(GL_QUADS);
		glColor4d(0.4, 1.0, 0.4, 1.0);
		glTexCoord2i(o1.x, o1.y);
		glVertex2i(-100, -100);
		glTexCoord2i(o1.x + 200, o1.y);
		glVertex2i(740, -100);
		glTexCoord2i(o1.x + 200, o1.y + 160);
		glVertex2i(740, 580);
		glTexCoord2i(o1.x, o1.y + 160);
		glVertex2i(-100, 580);
		glTexCoord2i(o2.x, o2.y);
		glVertex2i(-100, -100);
		glTexCoord2i(o2.x + 300, o2.y);
		glVertex2i(740, -100);
		glTexCoord2i(o2.x + 300, o2.y + 240);
		glVertex2i(740, 580);
		glTexCoord2i(o2.x, o2.y + 240);
		glVertex2i(-100, 580);
		glEnd();
		p_noise->unbind();

		engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	}

	// Layer rendern, auf dem Overlays angezeigt werden
	renderObjects(42, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);

	if(flash > 0.0)
	{
		// Blitz zeichnen
		glBegin(GL_QUADS);
		if(nightVision) glColor4d(0.0, 1.0, 0.0, min(0.75, actualFlash));
		else glColor4d(1.0, 1.0, 1.0, min(0.5, actualFlash));
		glVertex2i(-100, -100);
		glVertex2i(740, -100);
		glVertex2i(740, 580);
		glVertex2i(-100, 580);
		glEnd();
	}

	glPopMatrix();
}

void Level::update()
{
	clearAIFlags(Vec2i(-1, -1));

	// alte Objekte loeschen, neue Objekte hinzufuegen
	removeOldObjects();
	addNewObjects();

	// Frame beginnen
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		(*i)->frameBegin();
	}

	// Und im selben Takt wie die Objekte klingen die Symbole in der Anzeige ab.
	for(int i = 0; i < 2; i++)
	{
		if(hudIconFlash[i] > 0.0)
		{
			hudIconFlash[i] *= FLASH_DECAY;
			if(hudIconFlash[i] < 1.0 / 256.0) hudIconFlash[i] = 0.0;
		}
	}

	// Objekte bewegen
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		(*i)->update();
		if((*i)->toBeRemoved()) removeObject(*i);
	}

	// Elektronik verarbeiten
	Electronics::updateAll(*this);

	// Partikelsysteme aktualisieren
	p_particleSystem->update();
	p_fireParticleSystem->update();
	p_rainParticleSystem->update();

	// "Spuren verwischen"
	for(int i = 0; i < WIDTH * HEIGHT; i++)
	{
		uint trace = p_aiFlags[i] & 0xFFFFFF00;
		if(trace) p_aiFlags[i] -= 0x100;
	}

	// Sind genuegend Diamanten eingesammelt worden?
	if(!inEditor && p_exit)
	{
		if(p_exit->isGhost() && getNumDiamondsCollected() >= numDiamondsNeeded)
		{
			// Ausgang erscheinen lassen
			p_exit->setGhost(false);
			Engine::inst().playSound("exit.ogg", false, 0.0, 100);

			// Sterne
			ParticleSystem::Particle p;
			for(int i = 0; i < 150; i++)
			{
				p.lifetime = random(50, 100);
				p.damping = 0.95f;
				p.gravity = 0.0f;
				p.positionOnTexture = Vec2b(0, 32);
				p.sizeOnTexture = Vec2b(16, 16);
				p.position = p_exit->getPosition() * 16 + Vec2i(8, 8);
				const double r = random(0.0, 6.283);
				p.velocity = random(2.0, 5.0) * Vec2d(sin(r), cos(r));
				p.color = Vec4d(random(0.75, 1.0), random(0.75, 1.0), random(0.75, 1.0), random(0.25, 0.9));
				p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
				p.rotation = random(0.0f, 10.0f);
				p.deltaRotation = random(-0.1f, 0.1f);
				p.size = random(0.25f, 0.8f);
				p.deltaSize = random(0.0f, 0.01f);
				if(randomInt() % 2) p_particleSystem->addParticle(p);
				else p_fireParticleSystem->addParticle(p);
			}
		}
	}

	// Regen
	if(!inEditor && raining)
	{
		int r = 10;
		int details = Engine::inst().getDetails();
		if(details == 0) r = 30;
		else if(details == 1) r = 20;
		for(int x = 0; x < WIDTH; x++)
		{
			for(int y = 0; y < HEIGHT; y++)
			{
				Vec2i pos(x, y);
				uint l0 = getTileAt(0, pos);
				if(l0 != 0)
				{
					const TileSet::TileInfo& t0 = p_tileSet->getTileInfo(l0);
					if(t0.type == 0)
					{
						uint l1 = getTileAt(1, pos);
						if(!l1)
						{
							if(random(0, r) == 0)
							{
								ParticleSystem::Particle p;
								p.lifetime = random(5, 10);
								p.damping = 0.95f;
								p.gravity = 0.1f;
								p.positionOnTexture = Vec2b(96, 32);
								p.sizeOnTexture = Vec2b(16, 16);
								p.position = 16 * pos + Vec2i(random(0, 15), random(0, 15));
								const double c = random(0.4, 0.6);
								p.color = Vec4d(c, c, c, random(0.1, 0.2));
								p.deltaColor = Vec4d(0.0, 0.0, 0.0, -p.color.a / p.lifetime);
								p.rotation = random(0.0f, 10.0f);
								p.deltaRotation = random(-0.1f, 0.1f);
								p.size = random(0.25f, 0.3f);
								p.deltaSize = random(-0.002f, -0.001f);

								for(int i = 0; i < 8; i++)
								{
									const double r = random(0.0, 6.283);
									p.velocity = random(2.0, 3.0) * Vec2d(sin(r), cos(r));
									p_rainParticleSystem->addParticle(p);
								}
							}
						}
					}
				}
			}
		}
	}

	// Gewitter
	if(!inEditor && thunderstorm)
	{
		if(lightningCounter)
		{
			lightning.update();
			lightningCounter--;

			if(lightningCounter == 10)
			{
				addFlash(random(1.5, 3.0));
				flashJitter = random(0.5, 1.0);
				Engine::inst().playSound("thunder.ogg", false, 0.2, 100);
			}
			else if(random(0, 700) == 0)
			{
				addFlash(random(0.5, 1.0));
				flashJitter = random(0.5, 1.0);
			}
		}
		else
		{
			lightning.generate();
			lightningCounter = random(50, 700);
		}
	}

	if(cameraShake > 0.0)
	{
		cameraShake -= 0.02;
		if(cameraShake < 0.0) cameraShake = 0.0;
	}

	if(flash > 0.0)
	{
		flash *= 0.8;
		if(flash < 1.0 / 256.0)
		{
			flash = 0.0;
			flashJitter = 0.0;
		}

		actualFlash = flash * random(1.0 - flashJitter, 1.0 + flashJitter);
	}
	else actualFlash = 0.0;

	if(toxic > 0.0)
	{
		toxic *= 0.95;
		if(toxic < 1.0 / 256.0) toxic = 0.0;
	}

	counter++;
	time += 20;
}

void Level::renderTiles(int layer,
						const Vec2i& offset,
						const Vec4d& color)
{
	glColor4dv(color);
	glPushMatrix();
	glTranslated(offset.x, offset.y, 0.0);

#ifdef __EMSCRIPTEN__
	// WebGL kennt keine Displaylisten, das Kachelnetz wird also jedes Bild neu
	// ausgegeben. Ein Level sind 1000 Vierecke je Ebene, das traegt sich.
	{
		p_tileSet->beginRender();

		for(int x = 0; x < WIDTH; x++)
		{
			for(int y = 0; y < HEIGHT; y++)
			{
				Vec2i p(x, y);
				uint tileID = getTileAt(layer, p);
				p_tileSet->renderTile(tileID, p * 16);
			}
		}

		p_tileSet->endRender();
	}
#else
	// Muss dieser Layer neu gezeichnet werden?
	if(layerDirty & (1 << layer))
	{
		glNewList(layerListBase + layer, GL_COMPILE);

		p_tileSet->beginRender();

		for(int x = 0; x < WIDTH; x++)
		{
			for(int y = 0; y < HEIGHT; y++)
			{
				Vec2i p(x, y);
				uint tileID = getTileAt(layer, p);
				p_tileSet->renderTile(tileID, p * 16);
			}
		}

		p_tileSet->endRender();

		glEndList();

		// Der Layer ist jetzt nicht mehr dirty.
		layerDirty &= ~(1 << layer);
	}

	glCallList(layerListBase + layer);
#endif

	glPopMatrix();
}

void Level::renderObjects(int layer,
						  const Vec2i& offset,
						  const Vec4d& color,
						  bool shadow)
{
	if(layer != 939 && layer != 735 && layer != 736 && layer != 737 && layer != 738) p_sprites->bind();

	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		if(!shadow || (shadow && !((*i)->getFlags() & Object::OF_NO_SHADOW)))
		{
			(*i)->shadowPass = shadow;
			(*i)->render(layer, offset, color);
		}
	}

	if(layer != 939 && layer != 735 && layer != 736 && layer != 737 && layer != 738) p_sprites->unbind();
}

void Level::sortObjects()
{
	struct
	{
		bool operator () (Object* o1, Object* o2) const
		{
			const int d1 = o1->getDepth();
			const int d2 = o2->getDepth();
			if(d1 > d2) return true;
			else if(d1 < d2) return false;
			else
			{
				const double y1 = o1->getRealShownPosition().y;
				const double y2 = o2->getRealShownPosition().y;
				if(y1 < y2) return true;
				else if(y1 > y2) return false;
			}

			return o1->getUID() < o2->getUID();
		}
	} cmp;

	std::sort(objects.begin(), objects.end(), cmp);
}

void Level::renderShine(double intensity,
						double size)
{
	Engine::inst().renderSprite(p_shine, Vec2i(-56, -56), Vec2i(0, 0), Vec2i(128, 128), Vec4d(intensity), false, 0.0, size);
}

bool Level::isFreeAt(const Vec2i& position,
					 int* p_tileTypeOut)
{
	// Versperrt ein massives Tile den Weg?
	uint tileID = getTileAt(1, position);
	if(tileID)
	{
		const TileSet::TileInfo& tileInfo = p_tileSet->getTileInfo(tileID);
		if(p_tileTypeOut) *p_tileTypeOut = tileInfo.type;
		switch(tileInfo.type)
		{
		case 1:
		case 2:
			return false;
			break;
		}
	}

	// Objekte?
	std::vector<Object*> objects = getObjectsAt(position);
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		if((*i)->getFlags() & Object::OF_MASSIVE) return false;
	}

	return true;
}

bool Level::isFreeAt2(const Vec2i& positionInPixels,
					  Object* p_except,
					  Object** pp_objectOut,
					  Vec2i* p_tileOut,
					  double radiusSq)
{
	Vec2i position = positionInPixels / 16;
	*pp_objectOut = 0;

	const Vec2i q[] = {Vec2i(0, 0), Vec2i(-1, 0), Vec2i(1, 0), Vec2i(0, -1), Vec2i(0, 1)};
	for(int i = 0; i < sizeof(q) / sizeof(Vec2i); i++)
	{
		// Versperrt ein massives Tile den Weg?
		const Vec2i tilePos = (positionInPixels + q[i]) / 16;
		const uint tileID = getTileAt(1, tilePos);
		if(tileID)
		{
			const TileSet::TileInfo& tileInfo = p_tileSet->getTileInfo(tileID);
			switch(tileInfo.type)
			{
			case 1:
			case 2:
				*p_tileOut = tilePos;
				return false;
				break;
			}
		}
	}

	// Objekte der Umgebung testen
	const Vec2i p[] = {Vec2i(0, 0), Vec2i(-2, 0), Vec2i(-1, 0), Vec2i(1, 0), Vec2i(2, 0), Vec2i(0, -2), Vec2i(0, -1), Vec2i(0, 1), Vec2i(0, 2)};
	Object* p_closestObject = 0;
	double closestDist = 0.0;
	for(int i = 0; i < sizeof(p) / sizeof(Vec2i); i++)
	{
		const std::vector<Object*>& allObjectsHere = getAllObjectsAt(position + p[i]);
		for(std::vector<Object*>::const_iterator j = allObjectsHere.begin(); j != allObjectsHere.end(); ++j)
		{
			Object* const p_obj = *j;
			if(p_obj != p_except &&
			   (p_obj->getFlags() & Object::OF_MASSIVE) &&
			   !(p_obj->getFlags() & Object::OF_PROXY) &&
			   p_obj->isAlive() &&
			   !p_obj->isGhost() &&
			   !p_obj->isTeleporting() &&
			   !p_obj->isFalling())
			{
				const double dist = (static_cast<Vec2d>(p_obj->getShownPositionInPixels()) + Vec2d(7.5, 7.5) - positionInPixels).lengthSq();
				if(dist <= radiusSq && (!p_closestObject || dist < closestDist))
				{
					p_closestObject = p_obj;
					closestDist = dist;
				}
			}
		}
	}

	if(p_closestObject)
	{
		*pp_objectOut = p_closestObject;
		return false;
	}

	return true;
}

Object* Level::getFrontObjectAt(const Vec2i& position)
{
	if(!isValidPosition(position)) return 0;

	// alle Objekte an dieser Position heraussuchen
	Object* p_minObj = 0;
	const std::vector<Object*>& theList = p_objectsAt[position.y * WIDTH + position.x];
	for(std::vector<Object*>::const_iterator i = theList.begin(); i != theList.end(); ++i)
	{
		Object* p_obj = *i;
		if(p_obj->isAlive() && !p_obj->isGhost() && !(p_obj->getFlags() & Object::OF_PROXY))
		{
			if(!p_minObj) p_minObj = p_obj;
			else if(p_obj->getDepth() < p_minObj->getDepth()) p_minObj = p_obj;
		}
	}

	return p_minObj;

/*	Object* p_minObj = 0;

	// Sind dort Objekte? Das mit der kleinsten Tiefe liefern.
	for(std::list<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		Object* p_obj = *i;
		if(p_obj->isAlive() && !p_obj->isGhost() && p_obj->getPosition() == position)
		{
			if(!p_minObj) p_minObj = p_obj;
			else if(p_obj->getDepth() < p_minObj->getDepth()) p_minObj = p_obj;
		}
	}

	return p_minObj;*/
}

Object* Level::getBackObjectAt(const Vec2i& position)
{
	if(!isValidPosition(position)) return 0;

	// alle Objekte an dieser Position heraussuchen
	Object* p_maxObj = 0;
	const std::vector<Object*>& theList = p_objectsAt[position.y * WIDTH + position.x];
	for(std::vector<Object*>::const_iterator i = theList.begin(); i != theList.end(); ++i)
	{
		Object* p_obj = *i;
		if(p_obj->isAlive() && !p_obj->isGhost() && !(p_obj->getFlags() & Object::OF_PROXY))
		{
			if(!p_maxObj) p_maxObj = p_obj;
			else if(p_obj->getDepth() > p_maxObj->getDepth()) p_maxObj = p_obj;
		}
	}

	return p_maxObj;

/*	Object* p_maxObj = 0;

	// Sind dort Objekte? Das mit der groessten Tiefe liefern.
	for(std::list<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		Object* p_obj = *i;
		if(p_obj->isAlive() && !p_obj->isGhost() && p_obj->getPosition() == position)
		{
			if(!p_maxObj) p_maxObj = p_obj;
			else if(p_obj->getDepth() > p_maxObj->getDepth()) p_maxObj = p_obj;
		}
	}

	return p_maxObj;*/
}

std::vector<Object*> Level::getObjectsAt(const Vec2i& position)
{
	// alle Objekte an dieser Position heraussuchen
	std::vector<Object*> result;
	if(!isValidPosition(position)) return result;
	const std::vector<Object*>& theList = p_objectsAt[position.y * WIDTH + position.x];
	for(std::vector<Object*>::const_iterator i = theList.begin(); i != theList.end(); ++i)
	{
		Object* p_obj = *i;
		if(p_obj->isAlive() && !p_obj->isGhost() && !(p_obj->getFlags() & Object::OF_PROXY))
		{
			// zur Liste hinzufuegen
			result.push_back(p_obj);
		}
	}

	return result;

/*	std::set<Object*> list;
	for(std::list<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		Object* p_obj = *i;
		if(p_obj->isAlive() && !p_obj->isGhost() && p_obj->getPosition() == position) list.insert(p_obj);
	}

	return list;*/
}

std::vector<Object*> Level::getObjectsAt2(const Vec2i& position,
										  double radiusSq)
{
	std::vector<Object*> result;

	Vec2d positionInPixels = Vec2d(7.5, 7.5) + position * 16;
	Vec2i p[] = {Vec2i(0, 0), Vec2i(-2, 0), Vec2i(-1, 0), Vec2i(1, 0), Vec2i(2, 0), Vec2i(0, -2), Vec2i(0, -1), Vec2i(0, 1), Vec2i(0, 2)};
	for(int i = 0; i < sizeof(p) / sizeof(Vec2i); i++)
	{
		const std::vector<Object*>& allObjectsHere = getAllObjectsAt(position + p[i]);
		for(std::vector<Object*>::const_iterator j = allObjectsHere.begin(); j != allObjectsHere.end(); ++j)
		{
			Object* const p_obj = *j;
			if(p_obj->isAlive() && !p_obj->isGhost() && !(p_obj->getFlags() & Object::OF_PROXY))
			{
				const double dist = (static_cast<Vec2d>(p_obj->getShownPositionInPixels()) + Vec2d(7.5, 7.5) - positionInPixels).lengthSq();
				if(dist <= radiusSq) result.push_back(p_obj);
			}
		}
	}

	return result;
}

const std::vector<Object*>& Level::getAllObjectsAt(const Vec2i& position)
{
	// alle Objekte an dieser Position heraussuchen
	if(!isValidPosition(position)) return emptyObjectList;
	return p_objectsAt[position.y * WIDTH + position.x];
}

Elevator* Level::getElevatorAt(const Vec2i& position)
{
	if(!isValidPosition(position)) return 0;

	// alle Objekte an dieser Position heraussuchen
	const std::vector<Object*>& theList = p_objectsAt[position.y * WIDTH + position.x];
	for(std::vector<Object*>::const_iterator i = theList.begin(); i != theList.end(); ++i)
	{
		Object* p_obj = *i;
		if(p_obj->isAlive() && !p_obj->isGhost() && p_obj->getType() == "Elevator")
		{
			return static_cast<Elevator*>(p_obj);
		}
	}

	return 0;

/*	// Ist dort ein Aufzug?
	for(std::list<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		Object* p_obj = *i;
		if(p_obj->isAlive() && !p_obj->isGhost() && p_obj->getPosition() == position && p_obj->getType() == "Elevator")
		{
			return static_cast<Elevator*>(p_obj);
		}
	}

	return 0;*/
}

Rail* Level::getRailAt(const Vec2i& position)
{
	if(!isValidPosition(position)) return 0;

	// alle Objekte an dieser Position heraussuchen
	const std::vector<Object*>& theList = p_objectsAt[position.y * WIDTH + position.x];
	for(std::vector<Object*>::const_iterator i = theList.begin(); i != theList.end(); ++i)
	{
		Object* p_obj = *i;
		if(p_obj->isAlive() && !p_obj->isGhost() && p_obj->getType() == "Rail")
		{
			return static_cast<Rail*>(p_obj);
		}
	}

	return 0;
}

Player* Level::getPlayerAt(const Vec2i& position)
{
	if(!isValidPosition(position)) return 0;

	// alle Objekte an dieser Position heraussuchen
	const std::vector<Object*>& theList = p_objectsAt[position.y * WIDTH + position.x];
	for(std::vector<Object*>::const_iterator i = theList.begin(); i != theList.end(); ++i)
	{
		Object* p_obj = *i;
		if(p_obj->isAlive() && !p_obj->isGhost() && p_obj->getType() == "Player")
		{
			return static_cast<Player*>(p_obj);
		}
	}

	return 0;
}

bool Level::isValidPosition(const Vec2i& position) const
{
	return position.x >= 0 && position.y >= 0 &&
		   position.x < WIDTH && position.y < HEIGHT;
}

// Die Ebene gehoert genauso geprueft wie die Position: der Index ist
// layer * WIDTH * HEIGHT + ..., und layer kam bisher ungeprueft durch.
bool Level::isValidLayer(int layer) const
{
	return layer >= 0 && layer < NUM_LAYERS;
}

uint Level::getTileAt(int layer,
					  const Vec2i& position) const
{
	return isValidPosition(position) && isValidLayer(layer)
		   ? p_tiles[layer * WIDTH * HEIGHT + position.y * WIDTH + position.x] & 0x000000FF : -1;
}

void Level::setTileAt(int layer,
					  const Vec2i& position,
					  uint tile)
{
	if(isValidPosition(position) && isValidLayer(layer))
	{
		int index = layer * WIDTH * HEIGHT + position.y * WIDTH + position.x;
		p_tiles[index] = tile;
		setTileDestroyTimeAt(layer, position, p_tileSet->getTileInfo(tile).destroyTime);
		layerDirty |= 1 << layer;
	}
}

uint Level::getTileDestroyTimeAt(int layer,
								 const Vec2i& position) const
{
	return isValidPosition(position) && isValidLayer(layer)
		   ? (p_tiles[layer * WIDTH * HEIGHT + position.y * WIDTH + position.x] & 0xFFFFFF00) >> 8 : 1;
}

void Level::setTileDestroyTimeAt(int layer,
								 const Vec2i& position,
								 uint destroyTime)
{
	if(isValidPosition(position) && isValidLayer(layer))
	{
		int index = layer * WIDTH * HEIGHT + position.y * WIDTH + position.x;
		p_tiles[index] &= ~0xFFFFFF00;
		p_tiles[index] |= destroyTime << 8;
	}
}

bool Level::clearPosition(const Vec2i& position,
						  const std::string& except)
{
	// alle Objekte an dieser Stelle loeschen
	const std::vector<Object*> objects = getObjectsAt(position);
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		if((*i)->getType() != except) removeObject(*i);
	}

	return !objects.empty();
}

void Level::turnArrows()
{
	bool playSound = false;
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		if((*i)->getType() == "Arrow")
		{
			Arrow* p_arrow = reinterpret_cast<Arrow*>(*i);
			p_arrow->turn();
			playSound = true;
		}
	}

	if(playSound) Engine::inst().playSound("magnet.ogg", false, 0.15, 100);
}

bool Level::changeBarrages(uint color)
{
	std::vector<Barrage*> changed;

	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		if((*i)->getType() == "Barrage")
		{
			Barrage* p_barrage = reinterpret_cast<Barrage*>(*i);
			if(p_barrage->getColor() == color)
			{
				if(p_barrage->change()) changed.push_back(p_barrage);
				else
				{
					// alle vorherigen Hindernisse wieder aendern
					for(std::vector<Barrage*>::const_iterator j = changed.begin(); j != changed.end(); ++j) (*j)->change();
					Engine::inst().playSound("barrageswitch_failed.ogg", false, 0.0, 100);
					return false;
				}
			}
		}
	}

	if(!changed.empty()) Engine::inst().playSound("barrageswitch.ogg", false, 0.0, 100);
	return true;
}

bool Level::changeBarrages2(uint color,
						   bool up)
{
	std::vector<Barrage2*> changed;

	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		if((*i)->getType() == "Barrage2")
		{
			Barrage2* p_barrage = reinterpret_cast<Barrage2*>(*i);
			if(p_barrage->getColor() == color)
			{
				int code = p_barrage->change(up);
				if(code == 1) changed.push_back(p_barrage);
				else if(code == -1)
				{
					// alle vorherigen Hindernisse wieder aendern
					for(std::vector<Barrage2*>::const_iterator j = changed.begin(); j != changed.end(); ++j)
					{
						(*j)->change(!up);
					}

					Engine::inst().playSound("barrageswitch_failed.ogg", false, 0.0, 100);
					return false;
				}
			}
		}
	}

	if(!changed.empty()) Engine::inst().playSound("barrageswitch.ogg", false, 0.0, 100);
	return true;
}

int Level::fireCannons(uint color)
{
	int c = 0;
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		if((*i)->getType() == "Cannon")
		{
			Cannon* p_cannon = reinterpret_cast<Cannon*>(*i);
			if(p_cannon->getColor() == color)
			{
				if(p_cannon->fire()) c++;
			}
		}
	}

	if(c) Engine::inst().playSound("cannon_fire.ogg", false, 0.05, 100);

	return c;
}

int Level::rotateCannons(uint color)
{
	int c = 0;
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		if((*i)->getType() == "Cannon")
		{
			Cannon* p_cannon = reinterpret_cast<Cannon*>(*i);
			if(p_cannon->getColor() == color)
			{
				p_cannon->rotate();
				c++;
			}
		}
	}

	if(c) Engine::inst().playSound("cannon_turn.ogg", false, 0.05);

	return c;
}

const std::string& Level::getTitle() const
{
	return title;
}

void Level::setTitle(const std::string& title)
{
	this->title = title;
}

std::string Level::getSkin(uint index) const
{
	if(index >= SKIN_MAX) return "";
	return requestedSkin[index];
}

bool Level::setSkin(uint index,
					const std::string& skin)
{
	if(index >= SKIN_MAX) return false;
	if(this->requestedSkin[index] == skin) return false;
	this->skin[index] = this->requestedSkin[index] = skin;
	loadSkin();
	return true;
}

// Der Fehler-Level tritt an die Stelle einer Datei, die sich nicht laden
// laesst. Der Dateiname des Aufrufers wird danach wiederhergestellt: er
// steht in den Protokollzeilen und soll weiter die Datei benennen, die
// gemeint war, nicht den Platzhalter.
//
// Hier laufen alle drei Fehlerwege von load() zusammen - kaputtes XML,
// fehlendes <Level>, falsche Groesse -, also steht hier auch die Meldung.
bool Level::loadErrorLevel()
{
	if(loadingErrorLevel) return false;

	const std::string wanted(filename);

	// Die Palettenlevel cat<N>.xml gehoeren zum Spiel und sind nicht die Datei,
	// die jemand aufmachen wollte; fuer sie bleibt es beim Logeintrag. In der
	// Vorschau kommt die Meldung ohne Ton, sonst gaebe es beim Durchgehen einer
	// kaputten Kampagne bei jedem Tastendruck einen Fehlton. Genannt wird der
	// blosse Dateiname - der ganze Pfad fuehrt durch das Archiv samt Passwort.
	if(!inCat)
	{
		const std::string::size_type slash = wanted.find_last_of('/');
		Engine::inst().showToast(Engine::TOAST_ERROR,
								 localizeString("$ERROR_LEVEL_INVALID") + " \"" +
								 (slash == std::string::npos ? wanted : wanted.substr(slash + 1)) + "\"",
								 0.0, inPreview);
	}

	loadingErrorLevel = true;
	const bool ok = load(p_errorLevelFilename);
	loadingErrorLevel = false;
	filename = wanted;
	return ok;
}

void Level::allocateTiles()
{
	if(p_tiles) return;

	const int n = NUM_LAYERS * WIDTH * HEIGHT;
	p_tiles = new uint[n];
	for(int i = 0; i < n; i++) p_tiles[i] = 0;

	p_objectsAt = new std::vector<Object*>[WIDTH * HEIGHT];

	p_aiFlags = new uint[WIDTH * HEIGHT];
	memset(p_aiFlags, 0, WIDTH * HEIGHT * sizeof(uint));

	layerDirty = ~0;
}

bool Level::isInEditor() const
{
	return inEditor;
}

void Level::setInEditor(bool inEditor)
{
	this->inEditor = inEditor;
}

bool Level::isInCat() const
{
	return inCat;
}

void Level::setInCat(bool inCat)
{
	this->inCat = inCat;
}

bool Level::isInPreview() const
{
	return inPreview;
}

void Level::setInPreview(bool inPreview)
{
	this->inPreview = inPreview;
}

bool Level::isInMenu() const
{
	return inMenu;
}

void Level::setInMenu(bool inMenu)
{
	this->inMenu = inMenu;
}

TileSet* Level::getTileSet()
{
	return p_tileSet;
}

void Level::setTileSet(TileSet* p_tileSet)
{
	if(p_tileSet == this->p_tileSet) return;

	// altes Tile-Set freigeben
	if(this->p_tileSet) this->p_tileSet->release();

	// neues uebernehmen
	this->p_tileSet = p_tileSet;
	if(p_tileSet) p_tileSet->addRef();

	layerDirty = ~0;
}

ParticleSystem* Level::getParticleSystem()
{
	return p_particleSystem;
}

ParticleSystem* Level::getFireParticleSystem()
{
	return p_fireParticleSystem;
}

Texture* Level::getSpritesTexture()
{
	return p_sprites;
}

Texture** Level::getLava()
{
	return p_lava;
}

Texture* Level::getBackground()
{
	return p_background;
}

Texture* Level::getHint()
{
	return p_hint;
}

Font* Level::getHintFont()
{
	return p_hintFont;
}

bool Level::isHintScroll() const
{
	return hintScroll;
}

Presets* Level::getPresets()
{
	return p_presets;
}

const std::vector<Object*>& Level::getObjects() const
{
	return objects;
}

Player* Level::getActivePlayer()
{
	return p_activePlayer;
}

void Level::switchToNextPlayer()
{
	Player* p_firstPlayer = 0;
	bool currentPlayerFound = false;
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		if((*i)->getType() == "Player")
		{
			Player* p = static_cast<Player*>(*i);
			if(currentPlayerFound)
			{
				// Dies ist der naechste Spieler!
				p->activate();
				return;
			}

			if(!p_firstPlayer) p_firstPlayer = p;
			if(p == p_activePlayer) currentPlayerFound = true;
		}
	}

	// Kein weiterer Spieler gefunden!
	if(p_firstPlayer) p_firstPlayer->activate();
}

uint Level::getNumDiamondsCollected() const
{
	return numDiamondsCollected;
}

void Level::flashHudIcon(uint index)
{
	if(index < 2) hudIconFlash[index] = FLASH_STRENGTH;
}

double Level::getHudIconFlash(uint index) const
{
	return (index < 2) ? hudIconFlash[index] : 0.0;
}

void Level::setNumDiamondsCollected(uint numDiamondsCollected)
{
	this->numDiamondsCollected = numDiamondsCollected;
}

Exit* Level::getExit()
{
	return p_exit;
}

uint Level::getNumDiamondsNeeded() const
{
	return numDiamondsNeeded;
}

void Level::setNumDiamondsNeeded(uint numDiamondsNeeded)
{
	this->numDiamondsNeeded = numDiamondsNeeded;
}

void Level::addObject(Object* p_object)
{
	objectsToAdd.push_back(p_object);
}

void Level::removeObject(Object* p_object)
{
	// Abmelden darf genau einmal passieren. onRemove() laeuft sofort, das
	// Loeschen erst beim naechsten removeOldObjects() - dazwischen steht das
	// Objekt noch in objects und wird von jedem weiteren removeObject() erneut
	// erwischt, etwa von clean() (F5) oder von clearPosition() im selben Tick.
	//
	// Player::numInstances ist ein uint: der zweite Abgang macht aus 0 den Wert
	// 0xFFFFFFFF, danach ist numInstances nie wieder 1, und kein Spieler legt
	// mehr die Sound-Instanzen fuer Giftgas und Gasmaske an. Laser, Aufzug,
	// Foerderband und Giftgas zaehlen genauso.
	if(p_object->removed) return;
	p_object->removed = true;

	p_object->onRemove();
	objectsToRemove.push_back(p_object);
}

void Level::addNewObjects()
{
	if(objectsToAdd.empty()) return;

	// neue Objekte hinzufuegen
	objects.insert(objects.end(), objectsToAdd.begin(), objectsToAdd.end());

	// neue Objekte hashen und ihnen ihre UIDs geben
	uint uid = objects.back()->getUID();
	for(std::vector<Object*>::const_iterator i = objectsToAdd.begin(); i != objectsToAdd.end(); ++i)
	{
		hashObject(*i);
		(*i)->setUID(++uid);
	}

	objectsToAdd.clear();
}

void Level::removeOldObjects()
{
	if(objectsToRemove.empty()) return;

	for(std::vector<Object*>::const_iterator i = objectsToRemove.begin(); i != objectsToRemove.end(); ++i)
	{
		for(std::vector<Object*>::iterator j = objects.begin(); j != objects.end(); ++j)
		{
			if(*i == *j)
			{
				objects.erase(j);
				unhashObject(*i);
				delete *i;
				break;
			}
		}
	}

	objectsToRemove.clear();
}

void Level::hashObject(Object* p_obj)
{
	// Objekt in die Liste des entsprechenden Feldes einfuegen
	const Vec2i& p = p_obj->getPosition();
	int index = p.y * WIDTH + p.x;
	if(index >= 0 && index < WIDTH * HEIGHT)
	{
		if(p_obj->lastHashedAt == index) return;
		else unhashObject(p_obj);

		p_obj->lastHashedAt = index;
		p_objectsAt[index].push_back(p_obj);
	}
}

void Level::unhashObject(Object* p_obj)
{
	if(p_obj->lastHashedAt != -1)
	{
		// Objekt aus seiner Liste entfernen
		std::vector<Object*>& oldList = p_objectsAt[p_obj->lastHashedAt];
		for(std::vector<Object*>::iterator it = oldList.begin(); it != oldList.end(); ++it)
		{
			if(*it == p_obj)
			{
				if(oldList.size() > 1) *it = oldList.back();
				oldList.pop_back();
				p_obj->lastHashedAt = -1;
				return;
			}
		}
	}
}

void Level::setAIFlag(const Vec2i& where,
					  uint flag)
{
	if(where == Vec2i(-1, -1)) for(int i = 0; i < WIDTH * HEIGHT; i++) p_aiFlags[i] |= flag;
	else if(!isValidPosition(where)) return;
	else p_aiFlags[where.y * WIDTH + where.x] |= flag;
}

void Level::unsetAIFlag(const Vec2i& where,
						uint flag)
{
	if(where == Vec2i(-1, -1)) for(int i = 0; i < WIDTH * HEIGHT; i++) p_aiFlags[i] &= ~flag;
	else if(!isValidPosition(where)) return;
	else p_aiFlags[where.y * WIDTH + where.x] &= ~flag;
}

void Level::clearAIFlags(const Vec2i& where)
{
	if(where == Vec2i(-1, -1)) for(int i = 0; i < WIDTH * HEIGHT; i++) p_aiFlags[i] &= 0xFFFFFF00;
	else if(!isValidPosition(where)) return;
	else p_aiFlags[where.y * WIDTH + where.x] &= 0xFFFFFF00;
}

uint Level::getAIFlags(const Vec2i& where) const
{
	if(!isValidPosition(where)) return ~0;
	else return p_aiFlags[where.y * WIDTH + where.x];
}

uint Level::getAITrace(const Vec2i& where) const
{
	if(!isValidPosition(where)) return 0;
	else return (p_aiFlags[where.y * WIDTH + where.x] & 0xFFFFFF00) >> 8;
}

void Level::setAITrace(const Vec2i& where,
					   uint value)
{
	if(isValidPosition(where))
	{
		uint index = where.y * WIDTH + where.x;
		p_aiFlags[index] &= ~0xFFFFFF00;
		p_aiFlags[index] |= value << 8;
	}
}

void Level::clean()
{
	// alle Tiles zuruecksetzen
	for(int layer = 0; layer < NUM_LAYERS; layer++)
	{
		for(int x = 0; x < WIDTH; x++)
		{
			for(int y = 0; y < HEIGHT; y++)
			{
				setTileAt(layer, Vec2i(x, y), 0);
			}
		}
	}

	// alle Objekte loeschen
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i) removeObject(*i);
}

bool Level::isElectricityOn() const
{
	return electricityOn;
}

void Level::setElectricityOn(bool electricityOn)
{
	if(this->electricityOn == electricityOn) return;
	this->electricityOn = electricityOn;

	if(!inEditor)
	{
		// allen Objekten bescheid sagen
		for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
		{
			Object* p_obj = *i;
			p_obj->onElectricitySwitch(electricityOn);
		}
	}
}

bool Level::isNightVision() const
{
	return nightVision;
}

void Level::setNightVision(bool nightVision)
{
	this->nightVision = nightVision;
}

bool Level::isRaining() const
{
	return raining;
}

void Level::setRaining(bool raining)
{
	this->raining = raining;
}

bool Level::isSnowing() const
{
	return snowing;
}

void Level::setSnowing(bool snowing)
{
	this->snowing = snowing;
}

bool Level::isCloudy() const
{
	return cloudy;
}

void Level::setCloudy(bool cloudy)
{
	this->cloudy = cloudy;
}

bool Level::isThunderstorm() const
{
	return thunderstorm;
}

void Level::setThunderstorm(bool thunderstorm)
{
	this->thunderstorm = thunderstorm;
}

const Vec3i& Level::getLightColor() const
{
	return lightColor;
}

void Level::setLightColor(const Vec3i& lightColor)
{
	this->lightColor = lightColor;
}

const std::string& Level::getMusicFilename() const
{
	return musicFilename;
}

void Level::setMusicFilename(const std::string& musicFilename)
{
	this->musicFilename = musicFilename;
}

void Level::addCameraShake(double value)
{
	cameraShake += value;
	cameraShake = min(cameraShake, 4.0);
}

void Level::addFlash(double value)
{
	flash += value;
}

void Level::addToxic(double value)
{
	toxic += value;
}

void Level::renderToxicEffect()
{
	static double phase[65][41];
	static bool tablesInitialized = false;

	if(!tablesInitialized)
	{
		tablesInitialized = true;

		double temp[65][41];
		for(int x = 0; x <= 64; x++)
		{
			for(int y = 0; y <= 40; y++)
			{
				temp[x][y] = random(0.0, 2.5);
			}
		}

		for(int x = 1; x < 64; x++)
		{
			for(int y = 1; y < 40; y++)
			{
				phase[x][y] = (1.0 / 9.0) *
							  (temp[x - 1][y - 1] + temp[x][y - 1] + temp[x + 1][y - 1] +
							   temp[x - 1][y] + temp[x][y] + temp[x + 1][y] +
							   temp[x - 1][y + 1] + temp[x][y + 1] + temp[x + 1][y + 1]);
			}
		}
	}

	Engine& engine = Engine::inst();
	const Vec2i& screenSize = engine.getScreenSize();
	const Vec2i& screenPow2Size = engine.getScreenPow2Size();

	glBindTexture(GL_TEXTURE_2D, bufferID);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, screenPow2Size.y - screenSize.y, 0, 0, screenSize.x, screenSize.y);

	const double t = static_cast<double>(time) / 1000.0;
	const double r = min(6.0, toxic * 6.0);

	// Gitter erzeugen
	Vec2d grid[65][41];
	Vec4d color[65][41];
	for(int x = 0; x <= 64; x++)
	{
		for(int y = 0; y <= 40; y++)
		{
			grid[x][y] = Vec2i(x * 10, y * 10);
			color[x][y] = Vec4d(1.0);

			if(!x || !y || x == 64 || y == 40)
			{
				// Am Rand bleibt alles normal.
			}
			else
			{
				const double p = phase[x][y];
				grid[x][y] += r * Vec2d(sin(p + t), cos(p + t));
				color[x][y] = Vec4d(0.85 + 0.15 * sin(p + 0.31 + 1.1 * t),
								    0.85 + 0.15 * cos(p + 0.94 + 1.42 * t),
									0.85 + 0.15 * sin(p + 1.46 + 1.27 * t),
					                min(toxic, 1.0) * (0.75 + 0.25 * sin(p + 0.71 + 1.23 * t)));
			}
		}
	}

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	double w = static_cast<double>(screenPow2Size.x), h = static_cast<double>(screenPow2Size.y);
	glScaled(1.0 / w, -1.0 / h, 1.0);
	glMatrixMode(GL_MODELVIEW);

	// Gitter zeichnen
	engine.setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);

	for(int x = 0; x < 64; x++)
	{
		for(int y = 0; y < 40; y++)
		{
			Vec2i p(x * 10, y * 10);
			glColor4dv(color[x][y]);
			glTexCoord2dv(grid[x][y]);
			glVertex2i(p.x, p.y);
			glColor4dv(color[x + 1][y]);
			glTexCoord2dv(grid[x + 1][y]);
			glVertex2i(p.x + 10, p.y);
			glColor4dv(color[x + 1][y + 1]);
			glTexCoord2dv(grid[x + 1][y + 1]);
			glVertex2i(p.x + 10, p.y + 10);
			glColor4dv(color[x][y + 1]);
			glTexCoord2dv(grid[x][y + 1]);
			glVertex2i(p.x, p.y + 10);
		}
	}

	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void Level::invalidate()
{
	layerDirty = ~0;
}

void Level::loadSkin(bool forceReload)
{
	// Welche Skins nicht zu gebrauchen waren - nach Namen, nicht nach Datei.
	// Fehlt ein Archiv ganz, fehlen alle elf Dateien darin, und elf Meldungen
	// ueber denselben Skin will niemand lesen.
	std::set<std::string> badSkins;

	// pruefen, ob alle benoetigten Skins da sind
	for(uint i = 0; i < SKIN_MAX; i++)
	{
		if(!requestedSkin[i].empty())
		{
			skin[i] = requestedSkin[i];
			std::string f = getSkinFilename(i);
			if(f.empty())
			{
				badSkins.insert(requestedSkin[i]);
				skin[i] = "";
			}
		}
	}

	// Tiles laden. Schlaegt das fehl - eine kaputte oder eine mit anderer
	// Kachelgroesse eingeschleuste tileset.xml -, liefert request() eine Null,
	// und die 13 Stellen, die p_tileSet danach ohne Pruefung anfassen, wuerden
	// abstuerzen. Ein Skin, der sich nicht laden laesst, faellt deshalb auf den
	// mitgelieferten zurueck.
	TileSet* p_oldTileSet = p_tileSet;
	p_tileSet = Manager<TileSet>::inst().request(getSkinFilename(Level::SKIN_TILESET));
	if(!p_tileSet && skin[Level::SKIN_TILESET] != p_defaultSkin)
	{
		printfLog("+ WARNING: Skin \"%s\" has no usable tileset; falling back to \"%s\".\n",
				  skin[Level::SKIN_TILESET].c_str(), p_defaultSkin);
		badSkins.insert(skin[Level::SKIN_TILESET]);
		skin[Level::SKIN_TILESET] = p_defaultSkin;
		p_tileSet = Manager<TileSet>::inst().request(getSkinFilename(Level::SKIN_TILESET));
	}
	if(p_oldTileSet) p_oldTileSet->release();

	// Sprites laden
	Texture* p_oldSprites = p_sprites;
	p_sprites = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_SPRITES));
	if(!p_sprites && skin[Level::SKIN_SPRITES] != p_defaultSkin)
	{
		printfLog("+ WARNING: Skin \"%s\" has no usable sprites; falling back to \"%s\".\n",
				  skin[Level::SKIN_SPRITES].c_str(), p_defaultSkin);
		badSkins.insert(skin[Level::SKIN_SPRITES]);
		skin[Level::SKIN_SPRITES] = p_defaultSkin;
		p_sprites = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_SPRITES));
	}
	if(p_sprites) p_sprites->keepInMemory();
	if(p_oldSprites) p_oldSprites->release();

	// Lava herauskopieren
	Texture* p_oldLava[2] = {p_lava[0], p_lava[1]};
	p_lava[0] = p_sprites->createSubTexture(Vec2i(0, 480), Vec2i(16, 16));
	p_lava[1] = p_sprites->createSubTexture(Vec2i(32, 480), Vec2i(16, 16));
	if(p_oldLava[0]) p_oldLava[0]->release();
	if(p_oldLava[1]) p_oldLava[1]->release();

	// Rauschen laden
	Texture* p_oldNoise = p_noise;
	p_noise = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_NOISE));
	if(p_oldNoise) p_oldNoise->release();

	// "Schein" laden
	Texture* p_oldShine = p_shine;
	p_shine = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_SHINE));
	if(p_oldShine) p_oldShine->release();

	// Regen laden
	Texture* p_oldRain = p_rain;
	p_rain = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_RAIN));
	if(p_oldRain) p_oldRain->release();

	// Wolken laden
	Texture* p_oldClouds = p_clouds;
	p_clouds = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_CLOUDS));
	if(p_oldClouds) p_oldClouds->release();

	// Schnee laden
	Texture* p_oldSnow = p_snow;
	p_snow = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_SNOW));
	if(p_oldSnow) p_oldSnow->release();

	// Hintergrundbild laden
	Texture* p_oldBackground = p_background;
	p_background = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_BACKGROUND));
	if(p_oldBackground) p_oldBackground->release();

	// Hinweiszettel laden. Ob er sich aufrollen darf, sagt eine Merkdatei neben
	// dem Bild - Inhalt egal, es zaehlt nur, dass es sie gibt. Sie gehoert
	// neben das Bild und nicht in die tileset.xml, weil jeder Skin-Platz fuer
	// sich gewaehlt wird: ein Level kann seine Kacheln aus dem einen und seinen
	// Zettel aus einem anderen Skin nehmen. Und weil getSkinFilename() dem
	// default_hint.png-Verweis schon gefolgt ist, gilt hier die Datei neben dem
	// Bild, das wirklich geladen wird.
	Texture* p_oldHint = p_hint;
	const std::string hintFile = getSkinFilename(Level::SKIN_HINT);
	p_hint = Manager<Texture>::inst().request(hintFile);
	if(p_oldHint) p_oldHint->release();

	hintScroll = false;
	const std::string::size_type slash = hintFile.find_last_of('/');
	if(slash != std::string::npos)
	{
		hintScroll = FileSystem::inst().fileExists(hintFile.substr(0, slash + 1) + "hintscroll.txt");
	}

	// Schriftart des Hinweiszettels laden
	Font* p_oldHintFont = p_hintFont;
	p_hintFont = Manager<Font>::inst().request(getSkinFilename(Level::SKIN_HINTFONT));
	if(p_oldHintFont) p_oldHintFont->release();

	// Objektvoreinstellungen erzeugen
	Presets* p_oldPresets = p_presets;
	p_presets = new Presets(*this, p_sprites);
	if(p_oldPresets) delete p_oldPresets;

	// Partikelsysteme initialisieren
	Texture* p_oldParticleSprites = p_particleSprites;
	p_particleSprites = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_PARTICLES));
	if(p_oldParticleSprites) p_oldParticleSprites->release();
	ParticleSystem* p_oldParticleSystem = p_particleSystem;
	p_particleSystem = new ParticleSystem(p_particleSprites);
	if(p_oldParticleSystem) delete p_oldParticleSystem;
	ParticleSystem* p_oldFireParticleSystem = p_fireParticleSystem;
	p_fireParticleSystem = new ParticleSystem(p_particleSprites);
	if(p_oldFireParticleSystem) delete p_oldFireParticleSystem;
	ParticleSystem* p_oldRainParticleSystem = p_rainParticleSystem;
	p_rainParticleSystem = new ParticleSystem(p_particleSprites);
	if(p_oldRainParticleSystem) delete p_oldRainParticleSystem;

	layerDirty = ~0;

	if(forceReload)
	{
		// alles neu laden
		Manager<Texture>::inst().reload();
		Manager<TileSet>::inst().reload();
		Manager<Font>::inst().reload();
	}

	// Sagen, dass etwas fehlt. Die Palette des Editors nicht - sie ist selbst
	// ein Level und laedt denselben Skin gleich fuenfmal mit. In der Vorschau
	// der Levelauswahl bleibt die Meldung stumm, sonst klaenge bei einer
	// kaputten Kampagne der Fehlerton bei jedem Tastendruck.
	if(!inCat)
	{
		for(std::set<std::string>::const_iterator i = badSkins.begin(); i != badSkins.end(); ++i)
		{
			Engine::inst().showToast(Engine::TOAST_ERROR,
									 localizeString("$ERROR_SKIN_MISSING") + " \"" + *i + "\"",
									 0.0, inPreview);
		}
	}
}

std::string Level::getAlternative(const std::string& filename,
								  const std::string& dir1,
								  const std::string& dir2)
{
	FileSystem& fs = FileSystem::inst();
	if(fs.fileExists(dir1 + filename)) return dir1 + filename;
	else if(fs.fileExists(dir2 + filename)) return dir2 + filename;
	else return "";
}

std::string Level::getSkinFilename(uint index)
{
	if(index >= SKIN_MAX) return "";

	if(skin[index].empty())
	{
		// Standard-Skin
		skin[index] = p_defaultSkin;
		std::string result = getSkinFilename(index);
		skin[index] = "";
		return result;
	}
	else
	{
		// Existiert die gewuenschte Datei in einem normalen Ordner?
		FileSystem& fs = FileSystem::inst();
		std::string check = FileSystem::inst().getAppHomeDirectory() + "levels/skins/" + skin[index] + "/" + p_skinFilenames[index];
		if(fs.fileExists(check))
		{
			return check;
		}
		else
		{
			// Standard verwenden?
			if(fs.fileExists(FileSystem::inst().getAppHomeDirectory() + "levels/skins/" + skin[index] + "/default_" + p_skinFilenames[index]))
			{
				// Standard-Skin
				skin[index] = p_defaultSkin;
				std::string result = getSkinFilename(index);
				skin[index] = "";
				return result;
			}
			else
			{
				// Existiert das Skin-Archiv?
				std::string archiveFile = FileSystem::inst().getAppHomeDirectory() + "levels/skins/" + skin[index] + ".zip";
				if(fs.fileExists(archiveFile))
				{
					// Standard verwenden?
					if(fs.fileExists(archiveFile + "/default_" + p_skinFilenames[index]))
					{
						// Standard-Skin
						skin[index] = p_defaultSkin;
						std::string result = getSkinFilename(index);
						skin[index] = "";
						return result;
					}
					else
					{
						// Existiert die Datei da drin?
						std::string skinFile = archiveFile + "/" + p_skinFilenames[index];
						if(fs.fileExists(skinFile))
						{
							// Gibt es dort eine password.txt?
							std::string pwFile = archiveFile + "/password.txt";
							if(fs.fileExists(pwFile))
							{
								// Einlesen!
								std::string encryptedPassword = fs.readStringFromFile(pwFile);
								return archiveFile + "[" + encryptedPassword + "]/" + p_skinFilenames[index];
							}
							else
							{
								return skinFile;
							}
						}
					}
				}
			}
		}
	}

	return "";
}