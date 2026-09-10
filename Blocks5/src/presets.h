#ifndef _PRESETS_H
#define _PRESETS_H

class Texture;
class Level;
class Object;
class Sprites;

/*** Class for object presets ***/

class Presets
{
public:
	Presets(Level& level, Texture* p_sprites);
	~Presets();

	void renderPreset(const std::string& name, const Vec2i& position, const Vec4d& color = Vec4d(1.0));
	Object* instancePreset(const std::string& name, const Vec2i& position, TiXmlElement* p_element, bool fromEditor = false);
	const std::vector<std::string>& getPresetNames() const;

	// The look of a preset without building an object from it - meant for
	// debris that has to carry the colours of something that does not exist
	// yet. Returns false if the name is missing from the table.
	bool getPresetSprites(const std::string& name, Sprites* p_out) const;

private:
	Level& level;
	Texture* p_sprites;
	std::vector<std::string> presetNames;
	std::unordered_map<std::string, Vec2i> texCoords;
};

#endif