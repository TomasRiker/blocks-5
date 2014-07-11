#ifndef _PRESETS_H
#define _PRESETS_H

class Texture;
class Level;
class Object;

/*** Klasse für Objektvoreinstellungen ***/

class Presets
{
public:
	Presets(Level& level, Texture* p_sprites);
	~Presets();

	void renderPreset(const std::string& name, const Vec2i& position);
	Object* instancePreset(const std::string& name, const Vec2i& position, TiXmlElement* p_element, bool fromEditor = false);
	const std::vector<std::string>& getPresetNames() const;

private:
	Level& level;
	Texture* p_sprites;
	std::vector<std::string> presetNames;
	stdext::hash_map<std::string, Vec2i> texCoords;
};

#endif