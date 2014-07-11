#ifndef _FONT_H
#define _FONT_H

/*** Klasse für eine Schriftart ***/

#include "resource.h"

class Texture;

class Font : public Resource<Font>
{
	friend class Manager<Font>;

public:
	struct Options
	{
		int tabSize;
		int charSpacing;
		double lineSpacing;
		double charScaling;
		int shadows;
		int italic;
	};

	void reload();
	void cleanUp();

	void renderText(const std::string& text, const Vec2i& position, const Vec4d& color);
	void renderTextPure(const std::string& text);
	void measureText(const std::string& text, Vec2i* p_outDimensions, std::vector<Vec2i>* p_outCharPositions, const Vec2i& offset = Vec2i(0, 0));
	std::string adjustText(const std::string& text, int maxWidth);

	int getLineHeight() const;

	const Options& getOptions() const;
	void setOptions(const Options& options);
	void pushOptions();
	void popOptions();

	Texture* getTexture();

private:
	struct CharacterInfo
	{
		Vec2i position;
		Vec2i size;
	};

	struct StringCacheEntry
	{
		uint lastTimeUsed;
		uint listIndex;
	};

	Font(const std::string& filename);
	~Font();

	static bool forceReload() { return false; }

	int lineHeight;
	int offset;
	CharacterInfo charInfo[256];
	Texture* p_texture;
	Options options;
	uint listBase;
	uint numLists;
	uint listFree;
	stdext::hash_map<std::string, StringCacheEntry> stringCache;
	std::stack<Options> optionsStack;
};

#endif