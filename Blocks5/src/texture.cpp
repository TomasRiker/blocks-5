#include "pch.h"
#include "texture.h"
#include "textureatlas.h"
#include "filesystem.h"

Texture::Texture(const std::string& filename, int options) : Resource(filename)
{
	p_rgba = 0;
	texID = 0;
	doKeepInMemory = false;
	offset = Vec2i(0, 0);
	size = Vec2i(-1, -1);
	texelScale = Vec2f(1.0f, 1.0f);
	uvOrigin = Vec2f(0.0f, 0.0f);
	wrapMode = static_cast<WrapMode>(options & WRAP_MASK);
	neverPack = (options & NEVER_PACK) != 0;
	ownsTexture = true;
	p_parent = 0;

	reload();
}

Texture::Texture(Texture* p_parent,
				 const Vec2i& offset,
				 const Vec2i& size,
				 WrapMode wrapMode) : Resource(p_parent->filename)
{
	p_rgba = 0;
	texID = 0;
	doKeepInMemory = false;
	this->offset = offset;
	this->size = size;
	texelScale = Vec2f(1.0f, 1.0f);
	uvOrigin = Vec2f(0.0f, 0.0f);
	this->wrapMode = wrapMode;
	neverPack = false;
	ownsTexture = true;
	this->p_parent = p_parent;

	loadSubTexture(p_parent, offset, size);
}

Texture::~Texture()
{
	cleanUp();
}

namespace
{
	// The rows of a 32-bit SDL surface are tight, so the upload needs no
	// GL_UNPACK_ROW_LENGTH, which WebGL 1 does not have: SDL_CalculatePitch
	// pads a row to four bytes, which four bytes a pixel already are, and
	// Emscripten's SDL makes the pitch width * 4 outright. Checked rather
	// than assumed, since a padded row would arrive skewed with no GL error
	// to say so.
	bool uploadRGBA(const SDL_Surface* p_rgba, const char* p_name)
	{
		if(p_rgba->pitch != p_rgba->w * 4)
		{
			printfLog("+ ERROR: The image \"%s\" has a pitch of %d bytes for a width of %d, which the upload cannot take.\n",
					  p_name, p_rgba->pitch, p_rgba->w);
			return false;
		}
		Renderer::DirectGL direct;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, p_rgba->w, p_rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, p_rgba->pixels);
		return true;
	}

	// The same picture into a rectangle of a page, with a texel of gutter all
	// round it carrying the picture's opposite edge and corner.
	//
	// Linear filtering reaches one texel past the coordinate it was given and
	// no further - there are no mipmaps in this game - so that one texel is
	// exactly what GL_REPEAT returned at an edge, which is the only thing this
	// game has ever sampled with: GL_TEXTURE_WRAP_S and GL_TEXTURE_WRAP_T are
	// set nowhere in its history, so every texture ran at GL's default. The
	// same texel value, not a near one.
	//
	// Built as one padded block and uploaded once rather than as nine calls
	// round the edges: the columns are not contiguous in the source, so they
	// would have to be gathered into a buffer anyway.
	bool uploadPadded(uint pageID, const Vec2i& origin, const SDL_Surface* p_rgba, const char* p_name)
	{
		if(p_rgba->pitch != p_rgba->w * 4)
		{
			printfLog("+ ERROR: The image \"%s\" has a pitch of %d bytes for a width of %d, which the upload cannot take.\n",
					  p_name, p_rgba->pitch, p_rgba->w);
			return false;
		}

		const int g = TextureAtlas::GUTTER;
		const int w = p_rgba->w;
		const int h = p_rgba->h;
		const int paddedW = w + 2 * g;
		const int paddedH = h + 2 * g;
		std::vector<uint> padded(static_cast<uint>(paddedW * paddedH));
		const uint* p_source = reinterpret_cast<const uint*>(p_rgba->pixels);
		for(int y = 0; y < paddedH; y++)
		{
			// + h before the modulo because y - g is -1 on the first row, and
			// a negative operand would take the remainder the other way.
			const int sy = (y - g + h) % h;
			for(int x = 0; x < paddedW; x++)
			{
				const int sx = (x - g + w) % w;
				padded[y * paddedW + x] = p_source[sy * w + sx];
			}
		}

		Renderer::DirectGL direct;
		glBindTexture(GL_TEXTURE_2D, pageID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, origin.x - g, origin.y - g, paddedW, paddedH,
						GL_RGBA, GL_UNSIGNED_BYTE, &padded[0]);
		return true;
	}
}

void Texture::reload()
{
	cleanUp();

	if(p_parent)
	{
		loadSubTexture(p_parent, offset, size);
		return;
	}

	FileSystem& fs = FileSystem::inst();
	File* p_file = fs.openFile(filename);
	if(!p_file)
	{
		error = 1;
		return;
	}

	SDL_RWops* p_rwOps = p_file->getRWOps();

	// load the image
	SDL_Surface* p_surface = IMG_Load_RW(p_rwOps, 1);
	if(!p_surface)
	{
		printfLog("+ ERROR: Could not load image \"%s\" (Error: %s).\n",
				  filename.c_str(),
				  SDL_GetError());
		error = 1;
		return;
	}

	if(size.x == -1) size.x = p_surface->w;
	if(size.y == -1) size.y = p_surface->h;

	checkDimensions();

	// convert into the correct format
	p_rgba = SDL_CreateRGBSurface(SDL_SWSURFACE, size.x, size.y, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_SetAlpha(p_surface, 0, 0);
	SDL_SetAlpha(p_rgba, 0, 0);
	SDL_Rect srcRect;
	srcRect.x = offset.x;
	srcRect.y = offset.y;
	srcRect.w = size.x;
	srcRect.h = size.y;
	SDL_BlitSurface(p_surface, &srcRect, p_rgba, 0);
	SDL_FreeSurface(p_surface);

	// lock the image
	SDL_LockSurface(p_rgba);

	// The pixels come first and the texture afterwards, because which texture
	// it is depends on them fitting somewhere.
	place();
}

void Texture::place()
{
	TextureAtlas& atlas = TextureAtlas::inst();
	TextureAtlas::Slot slot;
	if(!neverPack && wrapMode != WM_REPEAT && atlas.reserve(this, size, &slot))
	{
		// In a page: uv reads in page texels from the picture's corner, and
		// the caller goes on writing its own.
		ownsTexture = false;
		texID = slot.pageID;
		const float edge = static_cast<float>(atlas.getPageEdge());
		texelScale = Vec2f(1.0f / edge, 1.0f / edge);
		uvOrigin = Vec2f(static_cast<float>(slot.origin.x) / edge, static_cast<float>(slot.origin.y) / edge);
		if(!uploadPadded(texID, slot.origin, p_rgba, filename.c_str())) error = 1;
		return;
	}

	// A texture of its own: raw, in a bracket, so that what was queued before
	// goes up first and the renderer forgets the binding afterwards.
	ownsTexture = true;
	texelScale = Vec2f(1.0f / size.x, 1.0f / size.y);
	uvOrigin = Vec2f(0.0f, 0.0f);

	Renderer::DirectGL direct;
	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_2D, texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	applyWrapMode();
	if(!uploadRGBA(p_rgba, filename.c_str())) error = 1;
}

void Texture::movedTo(uint pageID, const Vec2i& origin)
{
	const float edge = static_cast<float>(TextureAtlas::inst().getPageEdge());
	texID = pageID;
	texelScale = Vec2f(1.0f / edge, 1.0f / edge);
	uvOrigin = Vec2f(static_cast<float>(origin.x) / edge, static_cast<float>(origin.y) / edge);
}

void Texture::cleanUp()
{
	if(p_rgba)
	{
		// unlock and free the surface
		SDL_UnlockSurface(p_rgba);
		SDL_FreeSurface(p_rgba);
		p_rgba = 0;
	}

	if(texID)
	{
		// A page belongs to the atlas and is not this picture's to delete.
		if(ownsTexture) Renderer::inst().deleteTexture(texID);
		else TextureAtlas::inst().giveBack(this);
		texID = 0;
		ownsTexture = true;
	}
}

TextureRef Texture::ref() const
{
	return TextureRef(texID, texelScale, uvOrigin, wrapMode == WM_REPEAT);
}

Texture::WrapMode Texture::getWrapMode() const
{
	return wrapMode;
}

void Texture::reuseWithOptions(int options)
{
	const bool wantsRepeat = static_cast<WrapMode>(options & WRAP_MASK) == WM_REPEAT && wrapMode != WM_REPEAT;
	const bool wantsOut = (options & NEVER_PACK) != 0 && !neverPack;
	if(!wantsRepeat && !wantsOut) return;

	printfLog("> INFO: The image \"%s\" is now wanted %s; reloading.\n", filename.c_str(),
			  wantsRepeat ? "with tiling" : "outside the atlas");
	if(wantsRepeat) wrapMode = WM_REPEAT;
	if(wantsOut) neverPack = true;
	if(texID) reload();
}

uint Texture::createGLTexture(const Vec2i& size,
							  const uchar* p_pixels,
							  bool withAlpha,
							  bool smooth,
							  bool clamp)
{
	Renderer::DirectGL direct;
	uint id = 0;
	glGenTextures(1, &id);
	if(!id) return 0;
	const GLenum format = withAlpha ? GL_RGBA : GL_RGB;
	const GLint filter = smooth ? GL_LINEAR : GL_NEAREST;
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	if(clamp)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	glTexImage2D(GL_TEXTURE_2D, 0, format, size.x, size.y, 0, format, GL_UNSIGNED_BYTE, p_pixels);
	return id;
}

Texture* Texture::createSubTexture(const Vec2i& offset,
								   const Vec2i& size,
								   WrapMode wrapMode)
{
	if(!doKeepInMemory) return 0;

	return new Texture(this, offset, size, wrapMode);
}

void Texture::loadSubTexture(Texture* p_parent,
							 const Vec2i& offset,
							 const Vec2i& size)
{
	cleanUp();

	this->offset = offset;
	this->size = size;

	checkDimensions();

	// copy the wanted part
	p_rgba = SDL_CreateRGBSurface(SDL_SWSURFACE, size.x, size.y, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
	SDL_SetAlpha(p_rgba, 0, 0);
	SDL_Rect srcRect;
	srcRect.x = offset.x;
	srcRect.y = offset.y;
	srcRect.w = size.x;
	srcRect.h = size.y;
	SDL_UnlockSurface(p_parent->p_rgba);
	SDL_BlitSurface(p_parent->p_rgba, &srcRect, p_rgba, 0);
	SDL_LockSurface(p_parent->p_rgba);

	// lock the image
	SDL_LockSurface(p_rgba);

	place();

	// A sub-texture is not in the Manager, so freeUnkeptPixels() never reaches
	// it - and nothing asks to keep one, since createSubTexture() is called on
	// a parent that was asked and hands the result straight to a caller that
	// draws it.
	if(!doKeepInMemory) freePixels();
}

void Texture::freePixels()
{
	if(!p_rgba) return;
	SDL_UnlockSurface(p_rgba);
	SDL_FreeSurface(p_rgba);
	p_rgba = 0;
}

void Texture::freeUnkeptPixels()
{
	typedef std::unordered_multimap<std::string, Texture*> mapType;
	const mapType& items = Manager<Texture>::inst().getItems();
	for(mapType::const_iterator i = items.begin(); i != items.end(); ++i)
	{
		if(!i->second->doKeepInMemory) i->second->freePixels();
	}
}

const Vec2i& Texture::getSize() const
{
	return size;
}

void Texture::keepInMemory()
{
	if(doKeepInMemory) return;
	doKeepInMemory = true;

	// The pixels are already gone where a tick has passed since the load. The
	// flag does not bring them back, hence the reload. If nothing could be
	// loaded at all (texID == 0), a second attempt would only give the same
	// error.
	if(!p_rgba && texID) reload();
}

bool Texture::hasPixels() const
{
	return p_rgba != 0;
}

Vec4f Texture::getPixel(const Vec2i& where) const
{
	if(!p_rgba) return Vec4f(0.0f);

	uint pitchInPixels = p_rgba->pitch / p_rgba->format->BytesPerPixel;
	uint pixel = reinterpret_cast<const uint*>(p_rgba->pixels)[where.y * pitchInPixels + where.x];
	uint r = (pixel & p_rgba->format->Rmask) >> p_rgba->format->Rshift;
	uint g = (pixel & p_rgba->format->Gmask) >> p_rgba->format->Gshift;
	uint b = (pixel & p_rgba->format->Bmask) >> p_rgba->format->Bshift;
	uint a = (pixel & p_rgba->format->Amask) >> p_rgba->format->Ashift;
	float c = 1.0f / 255.0f;
	return Vec4f(c * r, c * g, c * b, c * a);
}

void Texture::applyWrapMode() const
{
	Renderer::DirectGL direct;
	// A texture of its own, so GL's own wrap applies - and GL_REPEAT, its
	// default, is what this game has always sampled with, since
	// GL_TEXTURE_WRAP_S and GL_TEXTURE_WRAP_T are set nowhere in its history.
	// Leaving the default alone is therefore the faithful answer here, and
	// leaving it alone is also all WM_REPEAT wants: the rain, the snow and the
	// clouds scroll without bound, and wrapTextureOffset() reduces that offset
	// to one period precisely because REPEAT makes a whole period an exact
	// no-op. For a WM_WRAP picture the mode is unobservable anyway - the
	// renderer's checkTiling() holds every such quad's uv inside its own
	// picture - beyond the one texel linear filtering reaches at an outer
	// edge, which is the texel the default gets right.
	//
	// WebGL 1 is the exception, and it is not optional: a non-power-of-two
	// texture is complete only sampled with CLAMP_TO_EDGE and without mipmaps,
	// and otherwise every access returns black, silently and with no GL error.
	// The game's own art is all power of two, so this is imported skins alone -
	// deliberately under Windows too, where NPOT with REPEAT would work,
	// because a 300x200 rain that tiled for its author and not for his players
	// is the worse failure.
	if(nextPow2(size.x) == size.x && nextPow2(size.y) == size.y) return;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Texture::checkDimensions()
{
	// Only a GL_REPEAT picture has to be a power of two, and only because
	// WebGL 1 will not repeat anything else (applyWrapMode above). Every other
	// one is complete at any size on every platform this builds for.
	if(wrapMode != WM_REPEAT) return;
	if(nextPow2(size.x) != size.x || nextPow2(size.y) != size.y)
	{
		printfLog("- WARNING: The image \"%s\" is %dx%d, which is not a power of two, so it cannot tile in a browser and will be clamped.\n",
				  filename.c_str(), size.x, size.y);
	}
}