#ifndef _PRESETS_H
#define _PRESETS_H

class Texture;
class Level;
class Object;
class Sprites;

/*** Klasse fuer Objektvoreinstellungen ***/

class Presets
{
public:
	Presets(Level& level, Texture* p_sprites);
	~Presets();

	void renderPreset(const std::string& name, const Vec2i& position, const Vec4d& color = Vec4d(1.0));
	Object* instancePreset(const std::string& name, const Vec2i& position, TiXmlElement* p_element, bool fromEditor = false);
	const std::vector<std::string>& getPresetNames() const;

	// Das Aussehen einer Voreinstellung, ohne ein Objekt davon zu bauen -
	// gedacht fuer Truemmer, die die Farben von etwas tragen sollen, das es
	// noch gar nicht gibt. Liefert false, wenn der Name in der Tabelle fehlt.
	bool getPresetSprites(const std::string& name, Sprites* p_out) const;

private:
	Level& level;
	Texture* p_sprites;
	std::vector<std::string> presetNames;
	std::unordered_map<std::string, Vec2i> texCoords;
};

#endif