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
// The fallback skin: used where a file of the wanted skin is missing (that is
// what the "default_" markers are for) and where it will not load.
const char* p_defaultSkin = "blocks_01";

// A level file that will not load shows this one instead of an empty level:
// the word ERROR built out of blocks, with Bob locked inside the O. The guard
// stops the recursion should the error level itself ever be missing or broken.
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
	noiseOffset1 = Vec2i(0, 0);
	noiseOffset2 = Vec2i(0, 0);
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

	// create the rain sound
	if(!p_rainSoundInst)
	{
		Sound* p_sound = Manager<Sound>::inst().request("rain.ogg");
		p_rainSoundInst = p_sound->createInstance(true);
		p_rainSoundInst->setVolume(0.0);
		p_sound->release();
	}

	// create the thunderstorm sound
	if(!p_thunderstormSoundInst)
	{
		Sound* p_sound = Manager<Sound>::inst().request("thunderstorm.ogg");
		p_thunderstormSoundInst = p_sound->createInstance(true);
		p_thunderstormSoundInst->setVolume(0.0);
		p_sound->release();
	}

	Engine&	engine = Engine::inst();

	// create the texture for the effect buffer
	bufferID = engine.createFrameCopyTexture(false, true);
}

Level::~Level()
{
	clear();

	if(bufferID) Renderer::inst().deleteTexture(bufferID);
}

void Level::clear()
{
	// delete the objects
	removeOldObjects();
	addNewObjects();
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i) removeObject(*i);
	removeOldObjects();

	delete[] p_objectsAt;
	p_objectsAt = 0;
	objects.clear();
	objectsToAdd.clear();
	objectsToRemove.clear();
	nextUID = 0;

	// delete the tiles
	delete[] p_tiles;
	p_tiles = 0;
	// The layer arrays need no GL call to release and no context to be current
	// while they go.
	for(int i = 0; i < NUM_LAYERS; i++) tileVertices[i].clear();

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
	// The frame oracle's clock starts over with the level, before its first
	// tick: Engine::update seeds that tick on the clock as it stands, and
	// what stood was the previous level's last tick - a number the harness's
	// timing decided. A gas cloud's first particles then landed in different
	// slots from one run to the next, and the order of alpha-blended
	// particles is the order they are drawn in.
	Engine::inst().sceneTick = 0;
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

	// load the XML document
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
	// Both load() overloads come through here, and so does every object this
	// level is about to construct - which is what has to be reproducible.
	Engine::inst().seedForLoad();

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
		int ndc = 0;
		p_level->Attribute("numDiamondsCollected", &ndc);
		numDiamondsCollected = ndc;
	}

	// read the title
	title = "\xA7" "en:Unnamed Level\xA7" "de:Unbenannter Level";
	const char* p_temp = p_level->Attribute("title");
	if(p_temp) title = p_temp;

	// read the skins
	for(int i = 0; i < SKIN_MAX; i++)
	{
		skin[i] = "";
		char attrName[256] = "";
		sprintf(attrName, "skin%d", i);
		p_temp = p_level->Attribute(attrName);
		if(p_temp) skin[i] = requestedSkin[i] = p_temp;
	}

	if(!dontReallyLoad) loadSkin();

	// Size and layer count are fixed (Level::WIDTH, HEIGHT, NUM_LAYERS). The
	// file names them anyway, and here it is taken at its word: otherwise the
	// rows of a 60x40 file would land in a 40x25 grid. A missing attribute
	// counts as correct, because TiXmlElement::Attribute leaves the value
	// untouched.
	// Get the memory first, then check: of the fourteen call sites of load(),
	// only two look at the return value, and an abort must therefore leave no
	// half-built level behind.
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
		// process the Layer elements
		int layer = 0;
		TiXmlElement* p_layer = p_level->FirstChildElement("Layer");
		while(p_layer)
		{
			int row = 0;
			TiXmlElement* p_row = p_layer->FirstChildElement("Row");
			while(p_row)
			{
				// set the tiles of this row
				if(p_row->GetText())
				{
					std::string content = p_row->GetText();
					for(uint col = 0; col < content.length() && col < static_cast<uint>(WIDTH); col++)
					{
						// Through unsigned char: char is signed here, so an id
						// of 0x80 or above would sign-extend to a huge number,
						// and setTileAt would then find no destroy time for a
						// tile that cannot exist. A row holds one id per byte.
						uint tile = static_cast<unsigned char>(content[col]);
						if(tile == ' ') tile = 0;
						setTileAt(layer, Vec2i(col, row), tile);
					}
				}

				p_row = p_row->NextSiblingElement("Row");
				row++;
			}

			p_layer = p_layer->NextSiblingElement("Layer");
			layer++;

			// Never past NUM_LAYERS, even where the file brings more <Layer>:
			// p_tiles is reserved for exactly that many, and a file imported from
			// outside decides both numbers itself.
			if(layer >= NUM_LAYERS) break;
		}

		// process the Object elements
		TiXmlElement* p_object = p_level->FirstChildElement("Object");
		std::list<std::pair<Electronics*, TiXmlElement*> > electronics;
		while(p_object)
		{
			// read the type and the position
			std::string type = p_object->Attribute("type");
			Vec2i position;
			p_object->Attribute("x", &position.x);
			p_object->Attribute("y", &position.y);

			// instance the object
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
					int destroyTime = 0;
					p_object->QueryIntAttribute("destroyTime", &destroyTime);
					p_theObject->setDestroyTime(destroyTime);

					int ghost = 0;
					p_object->QueryIntAttribute("ghost", &ghost);
					p_theObject->setGhost(ghost ? true : false);

					p_theObject->loadExtendedAttributes(p_object);
				}
			}

			p_object = p_object->NextSiblingElement("Object");
		}

		// add and sort the objects
		addNewObjects();
		sortObjects();

		// load the connections of the electronics parts
		for(std::list<std::pair<Electronics*, TiXmlElement*> >::const_iterator i = electronics.begin(); i != electronics.end(); ++i)
		{
			i->first->loadConnections(i->second);
		}
	}

	if(!dontReallyLoad)
	{
		// Electricity on? Night vision? Rain?
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

		// read the light colour
		lightColor = Vec3i(255, 255, 255);
		p_level->QueryIntAttribute("lightColorR", &lightColor.r);
		p_level->QueryIntAttribute("lightColorG", &lightColor.g);
		p_level->QueryIntAttribute("lightColorB", &lightColor.b);

		if(!inEditor && !inCat)
		{
			// Begin the frame
			for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
			{
				(*i)->frameBegin();
			}

			// update the light barriers
			for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
			{
				LightBarrierSender *p_lbs = dynamic_cast<LightBarrierSender*>(*i);
				if(p_lbs) p_lbs->update();
			}

			// process the electronics a few times to begin with
			for(int i = 0; i < 20; i++) Electronics::updateAll(*this);

			// Move the objects
			for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
			{
				Object* p_obj = *i;
				p_obj->update();
				if(p_obj->toBeRemoved()) removeObject(p_obj);
			}
		}
	}

	// read the music filename
	musicFilename = "";
	p_temp = p_level->Attribute("musicFilename");
	if(p_temp) musicFilename = p_temp;

	// Every layer has to be built before it is first drawn.
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
	// remove the old objects, add the new ones
	removeOldObjects();
	addNewObjects();

	// sort the objects
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

	// write the skins
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
				// One id per byte is the format; getTileAt masks to that range.
				p_temp[x] = static_cast<char>(t ? t : ' ');
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

	// save the connections of the electronics parts
	for(std::list<std::pair<Electronics*, TiXmlElement*> >::const_iterator i = electronics.begin(); i != electronics.end(); ++i)
	{
		i->first->saveConnections(i->second);
	}

	p_doc->LinkEndChild(p_level);

	return p_doc;
}

void Level::render()
{
	// Bring the appearance of every object up to date once per frame. Twelve
	// layers then go over it and draw nothing but what stands here - anything
	// that updated in onRender instead would do it fourteen times over, and
	// with the colour of the pass it happens to be in.
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		(*i)->onBeforeRender();
	}

	Renderer& renderer = Renderer::inst();

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
			// render the background image
			renderer.setTexture(p_background->ref());
			const Vec2f corners[4] = {Vec2f(0.0f, 0.0f), Vec2f(640.0f, 0.0f), Vec2f(640.0f, 480.0f), Vec2f(0.0f, 480.0f)};
			renderer.quad(renderer.state(), corners, corners, Vec4f(1.0f, 1.0f, 1.0f, 1.0f));
		}
	}

	Vec2i offset;
	if(cameraShake > 0.0)
	{
		// shake the camera
		offset.x = static_cast<int>(sin(static_cast<double>(counter) * 1.5) * cameraShake);
		offset.y = static_cast<int>(cos(static_cast<double>(counter) * 1.5) * 5.0 * cameraShake);
	}
	else offset = Vec2i(0, 0);

	renderer.push();
	renderer.translate(offset.x, offset.y);

	// sort the objects
	sortObjects();

	// render the background
	renderTiles(0, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0));

	// The lava edges write the stencil wherever they have any alpha at all
	// and nothing into the colour; the lava then draws only where they did
	// not. Scopes, so that the mask, the stencil and the discard cannot be
	// left on: each puts the previous state back when it ends.
	Texture* p_lavaEdges = Manager<Texture>::inst().request("lava_edges.png");
	renderer.setTexture(p_lavaEdges->ref());
	renderer.clearStencil();
	{
		Renderer::DiscardTransparentScope discard;
		Renderer::StencilWriteScope write(1);
		Renderer::ColorMaskScope mask(false, false, false, false);
		renderObjects(RL_LAVA_EDGE, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);
	}
	p_lavaEdges->release();
	Engine& engine = Engine::inst();

	// render the lava
	{
		Renderer::StencilTestScope test(0);
		renderer.setTexture(p_lava[0]->ref());
		renderObjects(RL_LAVA_BACK, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);
		renderer.setBlend(BM_ADDITIVE);
		renderer.setTexture(p_lava[1]->ref());
		renderObjects(RL_LAVA_FRONT, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);
		renderer.setBlend(BM_NORMAL);
	}

	// render the background objects
	renderObjects(RL_FLOOR, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);

	// render the electronics connections
	renderer.push();
	renderer.translate(0.5, 0.5);
	renderObjects(RL_WIRE, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);
	renderer.pop();

	// render the rain particle system
	p_rainParticleSystem->render();

	// render the shadows
	Vec2i samples[] = {Vec2i(2, 1), Vec2i(1, 2), Vec2i(2, 2)};
	int start = 0;
	int numSamples = 2;
	int details = engine.getDetails();
	if(details == 0) start = 2, numSamples = 1;
	Vec4d shadowColor(0.0, 0.0, 0.0, 0.7 / numSamples);
	for(int i = 0; i < numSamples; i++)
	{
		renderTiles(1, samples[start + i], shadowColor);
		renderObjects(RL_MAIN, samples[start + i], shadowColor, true);
	}

	// render the middle ground
	renderTiles(1, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0));
	renderObjects(RL_MAIN, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);

	// render the particle systems
	renderer.setBlend(BM_ADDITIVE);
	p_fireParticleSystem->render();
	renderer.setBlend(BM_NORMAL);
	p_particleSystem->render();

	// render the special effect layer
	renderObjects(RL_EFFECT, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);

	if(inEditor && !inCat && !inPreview) renderObjects(RL_EDITOR, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);

	// The weather: layers of one picture each, scrolling on top of the
	// picture's own scale through a texture matrix of their own, which the
	// renderer applies to the corners in GL's own arithmetic.
	const Vec2f screen[4] = {Vec2f(0.0f, 0.0f), Vec2f(640.0f, 0.0f), Vec2f(640.0f, 480.0f), Vec2f(0.0f, 480.0f)};

	// Rain
	if(!inEditor && raining)
	{
		const TextureRef rain = p_rain->ref();

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
			// After the angle, which reads the unwrapped offset. Rain scrolls
			// twenty texels a tick, so it is the first of these to go steppy.
			y = wrapTextureOffset(y, p_rain->getSize().y);

			Mat4 scroll = Mat4::scaling(rain.texelScale.x, rain.texelScale.y, 1.0);
			scroll.scale(s[i], s[i], s[i]);
			scroll.translate(0.0, -y / s[i], 0.0);
			scroll.rotate(angle, 0.0, 0.0, 1.0);
			renderer.scrolledQuad(rain.id, scroll, screen, screen, Vec4f(0.8f, 0.8f, 0.8f, static_cast<float>(alpha)));
		}
	}

	// Snow
	if(!inEditor && snowing)
	{
		const TextureRef snow = p_snow->ref();

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
			// x is bounded by its own sine and y is not, but both are wrapped:
			// the snow translates on both axes, and one rule is easier to keep
			// right than two.
			x = wrapTextureOffset(x, p_snow->getSize().x);
			y = wrapTextureOffset(y, p_snow->getSize().y);

			Mat4 scroll = Mat4::scaling(snow.texelScale.x, snow.texelScale.y, 1.0);
			scroll.translate(-x, -y, 0.0);
			scroll.scale(s[i], s[i], s[i]);
			renderer.scrolledQuad(snow.id, scroll, screen, screen, Vec4f(1.0f, 1.0f, 1.0f, static_cast<float>(0.65 * alphaFactor)));
		}
	}

	// Clouds
	if(!inEditor && cloudy)
	{
		const TextureRef clouds = p_clouds->ref();

		int numLayers = 3;
		if(details == 0) numLayers = 1;
		else if(details == 1) numLayers = 2;

		for(int i = numLayers - 1; i >= 0; i--)
		{
			double s[] = {1.0, 0.5, 0.25};
			double x = 100.0 * i + 50.0 * 0.001 * time;
			x += 2.0 * sin(0.02 * x * s[i] + i);
			// After the wobble, whose phase has to follow the unwrapped offset.
			x = wrapTextureOffset(x, p_clouds->getSize().x);

			Mat4 scroll = Mat4::scaling(clouds.texelScale.x, clouds.texelScale.y, 1.0);
			scroll.scale(s[i], s[i] * 2.0, s[i]);
			scroll.translate(-x / s[i], 0.0, 0.0);
			scroll.rotate(15.0 + 5.0 * i, 0.0, 0.0, 1.0);
			const float c = static_cast<float>(1.0 - 0.05 * i);
			const float a = static_cast<float>(0.175 - 0.05 * i);
			renderer.scrolledQuad(clouds.id, scroll, screen, screen, Vec4f(c, c, c, a));
		}
	}

	// Light
	if(lightColor != Vec3i(255, 255, 255))
	{
		renderer.setBlend(BM_MULTIPLY);
		const Vec3d tint = Vec3d(1.0 / 255.0) * lightColor;
		renderer.rect(Vec2f(-100.0f, -100.0f), Vec2f(740.0f, 580.0f),
					  Vec4f(static_cast<float>(tint.r), static_cast<float>(tint.g), static_cast<float>(tint.b), 1.0f));
		renderer.setBlend(BM_NORMAL);
	}

	// Thunderstorm
	if(!inEditor && thunderstorm) lightning.render();

	if(toxic > 0.1)
	{
		renderToxicEffect();
	}

	// Night vision
	if(nightVision && !inEditor)
	{
		const Vec2f screen[4] = {Vec2f(-100.0f, -100.0f), Vec2f(740.0f, -100.0f), Vec2f(740.0f, 580.0f), Vec2f(-100.0f, 580.0f)};
		{
			// The alpha channel becomes the light field: wiped to zero, then
			// the lights are added into it and nothing else is written.
			Renderer::ColorMaskScope alphaOnly(false, false, false, true);
			renderer.setBlend(BM_ZERO);
			renderer.rect(screen[0], screen[2], Vec4f(0.0f, 0.0f, 0.0f, 0.0f));
			renderer.setBlend(BM_ADD_ALL);
			renderObjects(RL_LIGHT, Vec2i(0, 0), Vec4d(1.0), false);
			if(thunderstorm) lightning.render();
		}
		{
			// darken everything unlit
			Renderer::ColorMaskScope colorOnly(true, true, true, false);
			const float c = static_cast<float>(21.0 / 255.0);
			renderer.setBlend(BM_DARKEN_UNLIT);
			renderer.rect(screen[0], screen[2], Vec4f(c, c, c, c));
			renderer.setBlend(BM_NORMAL);
		}

		// render the sparkle layer
		renderObjects(RL_SPARKLE, Vec2i(0, 0), Vec4d(1.0), false);

		// render the noise
		renderer.setBlend(BM_MULTIPLY);
		renderer.setTexture(p_noise->ref());
		const Vec2f o1 = static_cast<Vec2f>(noiseOffset1);
		const Vec2f o2 = static_cast<Vec2f>(noiseOffset2);
		const Vec2f uv1[4] = {o1, o1 + Vec2f(200.0f, 0.0f), o1 + Vec2f(200.0f, 160.0f), o1 + Vec2f(0.0f, 160.0f)};
		const Vec2f uv2[4] = {o2, o2 + Vec2f(300.0f, 0.0f), o2 + Vec2f(300.0f, 240.0f), o2 + Vec2f(0.0f, 240.0f)};
		const Vec4f green(0.4f, 1.0f, 0.4f, 1.0f);
		renderer.quad(renderer.state(), screen, uv1, green);
		renderer.quad(renderer.state(), screen, uv2, green);
		renderer.setBlend(BM_NORMAL);
	}

	// render the layer on which overlays are shown
	renderObjects(RL_OVERLAY, Vec2i(0, 0), Vec4d(1.0, 1.0, 1.0, 1.0), false);

	if(flash > 0.0)
	{
		// draw the flash
		const Vec4f color = nightVision ? Vec4f(0.0f, 1.0f, 0.0f, static_cast<float>(min(0.75, actualFlash)))
										: Vec4f(1.0f, 1.0f, 1.0f, static_cast<float>(min(0.5, actualFlash)));
		renderer.rect(Vec2f(-100.0f, -100.0f), Vec2f(740.0f, 580.0f), color);
	}

	renderer.pop();
}

void Level::update()
{
	clearAIFlags(Vec2i(-1, -1));

	// The night vision's noise, on the tick like everything else that moves.
	// The two spans are the quads' own in render().
	noiseOffset1 = Vec2i(random(0, 512 - 200), random(0, 512 - 160));
	noiseOffset2 = Vec2i(random(0, 512 - 300), random(0, 512 - 240));

	// remove the old objects, add the new ones
	removeOldObjects();
	addNewObjects();

	// Walked in the order render() paints in - depth, shown position, UID -
	// whether or not a frame was rendered since the last tick. render() sorts
	// the vector for its own sake, and a tick that runs straight after
	// another, the machine catching up, would otherwise walk the objects in
	// the order the spawns appended them: which gas cell got which of a
	// tick's random draws then depended on the frame rate.
	sortObjects();

	// Begin the frame
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		(*i)->frameBegin();
	}

	// And the icons in the HUD fade on the same tick as the objects.
	for(int i = 0; i < 2; i++)
	{
		if(hudIconFlash[i] > 0.0)
		{
			hudIconFlash[i] *= FLASH_DECAY;
			if(hudIconFlash[i] < 1.0 / 256.0) hudIconFlash[i] = 0.0;
		}
	}

	// Move the objects
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		(*i)->update();
		if((*i)->toBeRemoved()) removeObject(*i);
	}

	// process the electronics
	Electronics::updateAll(*this);

	// update the particle systems
	p_particleSystem->update();
	p_fireParticleSystem->update();
	p_rainParticleSystem->update();

	// Let the AI traces fade, one step per tick off every cell that still
	// carries one.
	for(int i = 0; i < WIDTH * HEIGHT; i++)
	{
		uint trace = p_aiFlags[i] & 0xFFFFFF00;
		if(trace) p_aiFlags[i] -= 0x100;
	}

	// Have enough diamonds been collected?
	if(!inEditor && p_exit)
	{
		if(p_exit->isGhost() && getNumDiamondsCollected() >= numDiamondsNeeded)
		{
			// make the exit appear
			p_exit->setGhost(false);
			Engine::inst().playSound("exit.ogg", false, 0.0, 100);

			// stars
			ParticleSystem::Particle p;
			for(int i = 0; i < 150; i++)
			{
				p.lifetime = static_cast<ushort>(random(50, 100));
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

	// Rain
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
								p.lifetime = static_cast<ushort>(random(5, 10));
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

	// Thunderstorm
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

	// The clock the frame oracle runs on - see Engine::sceneTick. Reported
	// from here because a level is the only thing in the game with a clock
	// that starts at zero when the screen does. Not behind
	// BLOCKS5_TEST_HOOKS: that define reaches engine.cpp and testhooks.cpp
	// and no other translation unit, so a guard here would simply never
	// compile.
	Engine::inst().sceneTick = static_cast<uint>(time);
}

void Level::renderTiles(int layer,
						const Vec2i& offset,
						const Vec4d& color)
{
	if(Engine::inst().isRenderSuppressed()) return;

	// Before the matrix is pushed, so that nothing has to be popped again. A
	// level whose skin would not load has no tile set, and drawing nothing is
	// what Level::loadSkin's toast already promises the player.
	if(!isValidLayer(layer) || !p_tileSet) return;

	Renderer& renderer = Renderer::inst();
	renderer.push();
	renderer.translate(offset.x, offset.y);

	std::vector<QuadVertex>& vertices = tileVertices[layer];

	// Built only where layerDirty says the grid changed. Six places set that
	// mask - load(), allocateTiles(), setTileAt(), setTileSet(), invalidate()
	// and loadSkin() - which between them cover every way a tile or the picture
	// it is cut from can move. A tile id alone would not: the texture
	// coordinates come from the TileSet, so a skin change moves every tile
	// without moving a single id.
	if(layerDirty & (1 << layer))
	{
		// Not reserved to WIDTH * HEIGHT * 4, though that is the ceiling and is
		// known here: clear() keeps the capacity, so a layer reaches the size it
		// needs on its first build and never allocates again, and a full one
		// ends up holding the same 64 KB either way. Reserving would only add
		// it to the five palette levels the editor keeps alongside, which are
		// nearly empty.
		vertices.clear();

		for(int x = 0; x < WIDTH; x++)
		{
			for(int y = 0; y < HEIGHT; y++)
			{
				const Vec2i p(x, y);
				const Vec2f corner(static_cast<float>(p.x * TileSet::TILE_SIZE),
								   static_cast<float>(p.y * TileSet::TILE_SIZE));
				p_tileSet->writeTile(getTileAt(layer, p), corner, vertices);
			}
		}

		layerDirty &= ~(1 << layer);
	}

	p_tileSet->drawVertices(vertices.empty() ? 0 : &vertices[0],
							static_cast<uint>(vertices.size()), static_cast<Vec4f>(color));

	renderer.pop();
}

void Level::renderObjects(RenderLayer layer,
						  const Vec2i& offset,
						  const Vec4d& color,
						  bool shadow)
{
	// The passes that bring a texture of their own: the wires draw untextured
	// and the three lava passes bind lava_edges or the lava itself.
	const uint ownTexture = RL_WIRE | RL_LAVA_EDGE | RL_LAVA_BACK | RL_LAVA_FRONT;
	if(!(layer & ownTexture)) Renderer::inst().setTexture(p_sprites->ref());

	// One pass over one layer is one draw of the renderer's, or a few where
	// an object in the middle of it changes the texture or the blend.
	for(std::vector<Object*>::const_iterator i = objects.begin(); i != objects.end(); ++i)
	{
		// Most objects draw on one or two of the twelve layers, so most of
		// this walk is a matrix bracket and a virtual call that would draw
		// nothing. The mask is a plain member, so asking costs a load.
		if(!((*i)->getRenderLayers() & layer)) continue;

		if(!shadow || (shadow && !((*i)->getFlags() & Object::OF_NO_SHADOW)))
		{
			(*i)->shadowPass = shadow;
			(*i)->render(layer, offset, color);
		}
	}
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
						double size,
						const Vec2d& offset)
{
	Engine::inst().renderSprite(p_shine, offset + Vec2d(-56.0, -56.0), Vec2i(0, 0), Vec2i(128, 128), Vec4d(intensity), false, 0.0, size);
}

namespace
{
	// A value in [-1, 1] for one point of a beam: steady for as long as the
	// seed is, different from its neighbours along the beam.
	//
	// One value for the whole object - the caller's bare glowJitter - is what
	// this replaces, and it is the thing to keep away from: every point of the
	// beam then breathes in unison, which reads as the beam pulsing rather
	// than as light scattering along it.
	//
	// A random() per point would look the same as this and is what stood here
	// before. What it costs is not shimmer - the loop renders at most once per
	// tick, so it cannot shimmer faster than the jitter is meant to - but
	// draws from the shared generator, a variable number of them, since the
	// beam's length moves with its mirrors. In a shipped build there is no
	// per-frame reseed, so those draws shift the sequence the logic reads.
	// The hash is the usual fract(sin(x) * large): no state, no draws.
	double pointJitter(double seed, int index)
	{
		double h = sin(seed * 12.9898 + index * 78.233) * 43758.5453;
		h -= floor(h);
		return h * 2.0 - 1.0;
	}
}

void Level::renderBeamShines(const std::list<Vec2d>& beam,
							 const Vec2i& origin,
							 double intensity,
							 double size,
							 double jitter,
							 double seed)
{
	if(beam.empty()) return;

	// A beam holds a point every four pixels, and a glow on every fourth of
	// them is what carries the light along it. What such a line lays down
	// goes as intensity * size / spacing, and the size is the delicate half:
	// the disc is 128 * size pixels across, so below about 0.25 the next
	// glow's centre falls outside it and the field beads instead of running.
	// The night vision darkens the picture by the alpha this field writes, so
	// a beam lying between two glows that no longer meet comes out dark
	// rather than dim.
	//
	// The corners are drawn whatever the count. A corner is a mirror - the
	// one place along a beam the light really is brightest - and a stride of
	// four would land on it three times in four by luck alone. So are the two
	// ends: the emitter, and whatever the beam stops against.
	Vec2d previous(0.0);
	int n = 0;

	for(std::list<Vec2d>::const_iterator i = beam.begin(); i != beam.end(); ++i, n++)
	{
		std::list<Vec2d>::const_iterator after = i;
		++after;

		bool corner = (i == beam.begin() || after == beam.end());
		if(!corner)
		{
			// The step in against the step out, as a cross product rather
			// than a comparison: the walk snaps the point at a mirror onto
			// the mirror's own centre, so the step into a corner is shorter
			// than a whole one and two steps of unequal length along one
			// straight run would otherwise read as a turn.
			const Vec2d in(*i - previous);
			const Vec2d out(*after - *i);
			corner = (in.x * out.y != in.y * out.x);
		}

		previous = *i;
		if(n % 4 && !corner) continue;

		renderShine(intensity, size + jitter * pointJitter(seed, n),
					*i - origin - Vec2d(7.5, 7.5));
	}
}

bool Level::isFreeAt(const Vec2i& position,
					 int* p_tileTypeOut)
{
	// Does a solid tile block the way?
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

	// Objects?
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
		// Does a solid tile block the way?
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

	// test the objects around it
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

	// pick out every object at this position
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

	// Any objects there? Return the one with the smallest depth.
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

	// pick out every object at this position
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

	// Any objects there? Return the one with the greatest depth.
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
	// pick out every object at this position
	std::vector<Object*> result;
	if(!isValidPosition(position)) return result;
	const std::vector<Object*>& theList = p_objectsAt[position.y * WIDTH + position.x];
	for(std::vector<Object*>::const_iterator i = theList.begin(); i != theList.end(); ++i)
	{
		Object* p_obj = *i;
		if(p_obj->isAlive() && !p_obj->isGhost() && !(p_obj->getFlags() & Object::OF_PROXY))
		{
			// add to the list
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
	// pick out every object at this position
	if(!isValidPosition(position)) return emptyObjectList;
	return p_objectsAt[position.y * WIDTH + position.x];
}

Elevator* Level::getElevatorAt(const Vec2i& position)
{
	if(!isValidPosition(position)) return 0;

	// pick out every object at this position
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

/*	// Is there an elevator there?
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

	// pick out every object at this position
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

	// pick out every object at this position
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

// The layer needs checking exactly as the position does: the index is
// layer * WIDTH * HEIGHT + ...
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
	// delete every object at this spot
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
					// change every previous barrage back
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
					// change every previous barrage back
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

// The error level takes the place of a file that will not load. The caller's
// filename is restored afterwards: it stands in the log lines and must go on
// naming the file that had been meant, not the stand-in.
//
// All three of load()'s failure paths meet here - broken XML, a missing
// <Level>, the wrong size - and the message therefore stands here too.
bool Level::loadErrorLevel()
{
	if(loadingErrorLevel) return false;

	const std::string wanted(filename);

	// The palette levels cat<N>.xml belong to the game and are not the file
	// somebody wanted to open; for them it stays at the log entry. In the
	// preview the message comes without a sound, or stepping through a broken
	// campaign would play the error sound at every key press. What is named is
	// the bare filename - the full path leads through the archive, password
	// and all.
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

	// release the old tile set
	if(this->p_tileSet) this->p_tileSet->release();

	// take on the new one
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

bool Level::dismissDisplay()
{
	// Only on the field the player stands on: that is where the note they are
	// reading stands. Object::dismiss() returns false everywhere else.
	if(!p_activePlayer) return false;

	const std::vector<Object*>& here = getAllObjectsAt(p_activePlayer->getPosition());
	for(std::vector<Object*>::const_iterator i = here.begin(); i != here.end(); ++i)
	{
		if((*i)->dismiss()) return true;
	}

	return false;
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
				// This is the next player!
				p->activate();
				return;
			}

			if(!p_firstPlayer) p_firstPlayer = p;
			if(p == p_activePlayer) currentPlayerFound = true;
		}
	}

	// No further player found!
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
	// Unregistering may happen exactly once. onRemove() runs at once, the
	// deletion only at the next removeOldObjects() - in between, the object
	// still stands in objects and is caught again by every further
	// removeObject(), such as by clean() (F5) or by clearPosition() in the
	// same tick.
	//
	// Player::numInstances is a uint: the second removal turns 0 into
	// 0xFFFFFFFF, after which numInstances is never 1 again and no player
	// creates the sound instances for toxic gas and gas mask any more. Laser,
	// elevator, conveyor belt and toxic gas count the same way.
	if(p_object->removed) return;
	p_object->removed = true;

	p_object->onRemove();
	objectsToRemove.push_back(p_object);
}

void Level::addNewObjects()
{
	if(objectsToAdd.empty()) return;

	// add the new objects
	objects.insert(objects.end(), objectsToAdd.begin(), objectsToAdd.end());

	// Hash the new objects and give them their UIDs, from a counter of the
	// level's own so that no two objects ever share one: the UID is the
	// last word in sortObjects()' comparison, and with a duplicate in it
	// two objects of one depth and row have no order - the sort then puts
	// them either way round, and a tick walks them in whichever it was.
	for(std::vector<Object*>::const_iterator i = objectsToAdd.begin(); i != objectsToAdd.end(); ++i)
	{
		hashObject(*i);
		(*i)->setUID(++nextUID);
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
	// insert the object into the list of the corresponding field. Both
	// coordinates are tested, not the flat index: x == WIDTH would otherwise
	// land in the first cell of the next row.
	const Vec2i& p = p_obj->getPosition();
	if(p.x >= 0 && p.x < WIDTH && p.y >= 0 && p.y < HEIGHT)
	{
		const int index = p.y * WIDTH + p.x;
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
		// remove the object from its list
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
	// reset every tile
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

	// delete every object
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
		// tell every object
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

		// From a generator of its own with a fixed seed, not from random():
		// the table is built once, on the first frame that needs it, and the
		// shared generator stands then wherever the frames before it left it
		// - so the same level came out with a different table depending on
		// how many frames the process had rendered by then. Noise from a
		// fixed seed is the same noise, and now the same on every run.
		MTRand table(0x70C1);
		double temp[65][41];
		for(int x = 0; x <= 64; x++)
		{
			for(int y = 0; y <= 40; y++)
			{
				temp[x][y] = table.rand(2.5);
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

	// The frame so far, the level's queued quads included, into the buffer
	// the grid samples in pixels.
	engine.captureFrame(bufferID);

	const double t = static_cast<double>(time) / 1000.0;
	const double r = min(6.0, toxic * 6.0);

	// build the grid
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
				// At the edge everything stays as it is.
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

	// draw the grid: 2560 quads with a colour and a texture coordinate a
	// corner, the corners in the same order as the cells
	static std::vector<Vertex> vertices;
	vertices.resize(64 * 40 * 4);
	Vertex* p_vertex = &vertices[0];
	for(int x = 0; x < 64; x++)
	{
		for(int y = 0; y < 40; y++)
		{
			const int cx[4] = {x, x + 1, x + 1, x};
			const int cy[4] = {y, y, y + 1, y + 1};
			for(int k = 0; k < 4; k++)
			{
				p_vertex->position = static_cast<Vec2f>(Vec2i(cx[k] * 10, cy[k] * 10));
				p_vertex->uv = static_cast<Vec2f>(grid[cx[k]][cy[k]]);
				p_vertex->color = static_cast<Vec4f>(color[cx[k]][cy[k]]);
				p_vertex++;
			}
		}
	}
	Renderer::inst().quads(RenderState(engine.getFrameCopyRef(bufferID), BM_NORMAL), &vertices[0], static_cast<uint>(vertices.size()));
}

void Level::invalidate()
{
	layerDirty = ~0;
}

void Level::loadSkin(bool forceReload)
{
	// Which skins were unusable - by name, not by file. Where an archive is
	// missing altogether, all eleven files inside it are missing, and nobody
	// wants to read eleven messages about the same skin.
	std::set<std::string> badSkins;

	// check that every needed skin is there
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

	// Load the tiles. Where that fails - a broken tileset.xml, or one imported
	// with a different tile size - request() returns a null, and the 13 places
	// that touch p_tileSet afterwards without checking would crash. A skin
	// that will not load therefore falls back to the shipped one.
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

	// load the sprites
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

	// copy the lava out
	Texture* p_oldLava[2] = {p_lava[0], p_lava[1]};
	p_lava[0] = p_sprites->createSubTexture(Vec2i(0, 480), Vec2i(16, 16));
	p_lava[1] = p_sprites->createSubTexture(Vec2i(32, 480), Vec2i(16, 16));
	if(p_oldLava[0]) p_oldLava[0]->release();
	if(p_oldLava[1]) p_oldLava[1]->release();

	// load the noise
	Texture* p_oldNoise = p_noise;
	p_noise = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_NOISE));
	if(p_oldNoise) p_oldNoise->release();

	// load the shine
	Texture* p_oldShine = p_shine;
	p_shine = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_SHINE));
	if(p_oldShine) p_oldShine->release();

	// load the rain
	Texture* p_oldRain = p_rain;
	p_rain = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_RAIN));
	if(p_oldRain) p_oldRain->release();

	// load the clouds
	Texture* p_oldClouds = p_clouds;
	p_clouds = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_CLOUDS));
	if(p_oldClouds) p_oldClouds->release();

	// load the snow
	Texture* p_oldSnow = p_snow;
	p_snow = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_SNOW));
	if(p_oldSnow) p_oldSnow->release();

	// load the background image
	Texture* p_oldBackground = p_background;
	p_background = Manager<Texture>::inst().request(getSkinFilename(Level::SKIN_BACKGROUND));
	if(p_oldBackground) p_oldBackground->release();

	// Load the hint note. A marker file beside the picture says whether it may
	// roll up - contents ignored, only its existence counts. It belongs beside
	// the picture and not in the tileset.xml, because each skin slot is chosen
	// separately: a level can take its tiles from one skin and its note from
	// another. And because getSkinFilename() has already followed the
	// default_hint.png link, what counts here is the file beside the picture
	// that is really loaded.
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

	// load the hint note's font
	Font* p_oldHintFont = p_hintFont;
	p_hintFont = Manager<Font>::inst().request(getSkinFilename(Level::SKIN_HINTFONT));
	if(p_oldHintFont) p_oldHintFont->release();

	// create the object presets
	Presets* p_oldPresets = p_presets;
	p_presets = new Presets(*this, p_sprites);
	if(p_oldPresets) delete p_oldPresets;

	// initialize the particle systems
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
		// reload everything
		Manager<Texture>::inst().reload();
		Manager<TileSet>::inst().reload();
		Manager<Font>::inst().reload();
	}

	// Say that something is missing. Not for the editor's palette - it is
	// itself a level and loads the same skin five times over. In the level
	// select preview the message stays silent, or a broken campaign would play
	// the error sound at every key press.
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
		// default skin
		skin[index] = p_defaultSkin;
		std::string result = getSkinFilename(index);
		skin[index] = "";
		return result;
	}
	else
	{
		// Does the wanted file exist in an ordinary folder?
		// resolveContentPath() asks the game folder first and then the user
		// directory: the four shipped skins sit with the game, imported and
		// self-built ones with the player.
		FileSystem& fs = FileSystem::inst();
		const std::string skinDir("levels/skins/" + skin[index]);
		std::string check = fs.resolveContentPath(skinDir + "/" + p_skinFilenames[index]);
		if(fs.fileExists(check))
		{
			return check;
		}
		else
		{
			// use the default?
			if(fs.fileExists(fs.resolveContentPath(skinDir + "/default_" + p_skinFilenames[index])))
			{
				// default skin
				skin[index] = p_defaultSkin;
				std::string result = getSkinFilename(index);
				skin[index] = "";
				return result;
			}
			else
			{
				// Does the skin archive exist?
				std::string archiveFile = fs.resolveContentPath(skinDir + ".zip");
				if(fs.fileExists(archiveFile))
				{
					// use the default?
					if(fs.fileExists(archiveFile + "/default_" + p_skinFilenames[index]))
					{
						// default skin
						skin[index] = p_defaultSkin;
						std::string result = getSkinFilename(index);
						skin[index] = "";
						return result;
					}
					else
					{
						// Does the file exist in there?
						std::string skinFile = archiveFile + "/" + p_skinFilenames[index];
						if(fs.fileExists(skinFile))
						{
							// Is there a password.txt in there?
							std::string pwFile = archiveFile + "/password.txt";
							if(fs.fileExists(pwFile))
							{
								// Read it in!
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