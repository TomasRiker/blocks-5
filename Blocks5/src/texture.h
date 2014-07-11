#ifndef _TEXTURE_H
#define _TEXTURE_H

/*** Klasse für eine Textur ***/

#include "resource.h"

class Texture : public Resource<Texture>
{
	friend class Manager<Texture>;

public:
	void reload();
	void cleanUp();

	void bind() const;
	void unbind() const;
	const Vec2i& getSize() const;

	Texture* createSubTexture(const Vec2i& offset, const Vec2i& size);
	void loadSubTexture(Texture* p_parent, const Vec2i& offset, const Vec2i& size);

	void keepInMemory();
	void addGaps();
	Vec4d getPixel(const Vec2i& where) const;

private:
	Texture(const std::string& filename);
	~Texture();

	void checkDimensions();

	static bool forceReload() { return false; }

	mutable SDL_Surface* p_rgba;
	unsigned int texID;
	Vec2i offset;
	Vec2i size;
	bool doKeepInMemory;
	bool doAddGaps;
	double matrix[16];
	Texture* p_parent;
};

#endif