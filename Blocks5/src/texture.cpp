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

Texture::Texture(const Vec2i& size, const uchar* p_pixels, const std::string& name) : Resource(name)
{
	p_rgba = 0;
	texID = 0;
	doKeepInMemory = false;
	offset = Vec2i(0, 0);
	this->size = size;
	texelScale = Vec2f(1.0f, 1.0f);
	uvOrigin = Vec2f(0.0f, 0.0f);
	wrapMode = WM_CLAMP;
	neverPack = false;
	ownsTexture = true;
	p_parent = 0;

	// Into a surface of the layout place() expects, a byte each of R, G, B and
	// A; place() then packs it or gives it a texture of its own.
	p_rgba = SDL_CreateRGBSurface(SDL_SWSURFACE, size.x, size.y, 32,
								  0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
	if(!p_rgba)
	{
		printfLog("+ ERROR: No surface for the picture \"%s\".\n", name.c_str());
		error = 1;
		return;
	}
	SDL_SetAlpha(p_rgba, 0, 0);
	SDL_LockSurface(p_rgba);
	memcpy(p_rgba->pixels, p_pixels, static_cast<size_t>(size.x) * size.y * 4);

	place();

	// Nothing reads these again: this picture is not in the Manager, so the
	// sweep never reaches it, and nothing asks it for a pixel.
	freePixels();
}

Texture* Texture::createFromPixels(const Vec2i& size, const uchar* p_rgba, const std::string& name)
{
	return new Texture(size, p_rgba, name);
}

Texture::~Texture()
{
	cleanUp();
}

namespace
{
	// A 32-bit SDL surface has tight rows (SDL pads a row to four bytes, and
	// Emscripten's SDL makes the pitch width * 4), so the upload needs no
	// GL_UNPACK_ROW_LENGTH, which WebGL 1 lacks. Checked anyway: a padded row
	// would arrive skewed with no GL error to say so.
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
	// round. Linear filtering reaches one texel past the edge and no further
	// (there are no mipmaps), so a copy of the picture's own edge samples
	// exactly as GL_CLAMP_TO_EDGE did, and a copy of the opposite edge as
	// GL_REPEAT did. One padded block and one upload, since the edge columns
	// would have to be gathered into a buffer anyway.
	bool uploadPadded(uint pageID, const Vec2i& origin, const SDL_Surface* p_rgba, bool wrap, const char* p_name)
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
			int sy = y - g;
			if(wrap) sy = (sy + h) % h;
			else sy = (sy < 0) ? 0 : ((sy >= h) ? h - 1 : sy);
			for(int x = 0; x < paddedW; x++)
			{
				int sx = x - g;
				if(wrap) sx = (sx + w) % w;
				else sx = (sx < 0) ? 0 : ((sx >= w) ? w - 1 : sx);
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
		if(!uploadPadded(texID, slot.origin, p_rgba, wrapMode == WM_WRAP, filename.c_str())) error = 1;
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
	// The extent is what checkTiling needs to know where this picture ends,
	// which inside a page is nowhere near where the texture does.
	return TextureRef(texID, texelScale, uvOrigin,
					  Vec2f(size.x * texelScale.x, size.y * texelScale.y),
					  wrapMode == WM_REPEAT);
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

	// A sub-texture is not in the Manager, so freeUnkeptPixels() never
	// reaches it, and nothing asks to keep one: it frees its own.
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

	// A tick after the load the sweep has freed the pixels, and the flag does
	// not bring them back. With texID 0 nothing loaded at all, and a second
	// attempt would only fail the same way.
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
	// GL_REPEAT, a fresh texture's default, only for WM_REPEAT: the weather
	// scrolls without end, and scrollOffset() and wrapTextureOffset() reduce
	// it to whole periods because REPEAT makes one an exact no-op. Everything
	// else is clamped, WM_WRAP too: the renderer cuts it at its edges, and in
	// a page REPEAT would wrap at the page's edge, not the picture's.
	//
	// So is a WM_REPEAT picture that is not a power of two, since WebGL 1
	// samples one as black, with no GL error, unless it is clamped. The game's
	// own art is all power of two, so this is imported skins alone, and it is
	// clamped on every platform so that its author sees what players will.
	if(wrapMode == WM_REPEAT && nextPow2(size.x) == size.x && nextPow2(size.y) == size.y) return;
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