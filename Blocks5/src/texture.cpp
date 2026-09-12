#include "pch.h"
#include "texture.h"
#include "filesystem.h"
#include "engine.h"
#include "glstate.h"

Texture::Texture(const std::string& filename) : Resource(filename)
{
	p_rgba = 0;
	texID = 0;
	doKeepInMemory = false;
	offset = Vec2i(0, 0);
	size = Vec2i(-1, -1);
	texelScale = Vec2d(1.0, 1.0);
	p_parent = 0;

	reload();
}

Texture::~Texture()
{
	cleanUp();
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

	// set up the OpenGL texture
	glGenTextures(1, &texID);
	GL::bindTexture(texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	applyWrapMode();

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

	// copy the image data into the texture
	glPixelStorei(GL_UNPACK_ROW_LENGTH, p_rgba->pitch / p_rgba->format->BytesPerPixel);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, p_rgba->w, p_rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, p_rgba->pixels);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	texelScale = Vec2d(1.0 / size.x, 1.0 / size.y);
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
		// delete the texture
		glDeleteTextures(1, &texID);
		texID = 0;
	}
}

void Texture::bind() const
{
	if(!doKeepInMemory && p_rgba)
	{
		// unlock and free the surface
		SDL_UnlockSurface(p_rgba);
		SDL_FreeSurface(p_rgba);
		p_rgba = 0;
	}

	// Through GL::, which flushes the sprite batch: whatever is queued was
	// queued against the binding and the matrix about to be replaced.
	GL::setTexturing(true);
	GL::bindTexture(texID);

	// pixel texture coordinates
	GL::loadTexelMatrix(texelScale);
}

void Texture::unbind() const
{
	GL::setTexturing(false);
}

Texture* Texture::createSubTexture(const Vec2i& offset,
								   const Vec2i& size)
{
	if(!doKeepInMemory) return 0;

	Texture* p_texture = new Texture(filename);
	p_texture->p_parent = this;
	p_texture->offset = offset;
	p_texture->size = size;
	p_texture->reload();

	return p_texture;
}

void Texture::loadSubTexture(Texture* p_parent,
							 const Vec2i& offset,
							 const Vec2i& size)
{
	cleanUp();

	this->offset = offset;
	this->size = size;

	checkDimensions();

	// set up the OpenGL texture
	glGenTextures(1, &texID);
	GL::bindTexture(texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	applyWrapMode();

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

	// copy the image data into the texture
	glPixelStorei(GL_UNPACK_ROW_LENGTH, p_rgba->pitch / p_rgba->format->BytesPerPixel);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, p_rgba->w, p_rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, p_rgba->pixels);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	texelScale = Vec2d(1.0 / size.x, 1.0 / size.y);
}

const Vec2i& Texture::getSize() const
{
	return size;
}

void Texture::keepInMemory()
{
	if(doKeepInMemory) return;
	doKeepInMemory = true;

	// The pixels are already gone because the texture had been bound earlier.
	// The flag does not bring them back, hence the reload. If nothing could be
	// loaded at all (texID == 0), a second attempt would only give the same
	// error.
	if(!p_rgba && texID) reload();
}

bool Texture::hasPixels() const
{
	return p_rgba != 0;
}

Vec4d Texture::getPixel(const Vec2i& where) const
{
	if(!p_rgba) return Vec4d(0.0);

	uint pitchInPixels = p_rgba->pitch / p_rgba->format->BytesPerPixel;
	uint pixel = reinterpret_cast<const uint*>(p_rgba->pixels)[where.y * pitchInPixels + where.x];
	uint r = (pixel & p_rgba->format->Rmask) >> p_rgba->format->Rshift;
	uint g = (pixel & p_rgba->format->Gmask) >> p_rgba->format->Gshift;
	uint b = (pixel & p_rgba->format->Bmask) >> p_rgba->format->Bshift;
	uint a = (pixel & p_rgba->format->Amask) >> p_rgba->format->Ashift;
	double c = 1.0 / 255.0;
	return Vec4d(c * r, c * g, c * b, c * a);
}

void Texture::applyWrapMode() const
{
	// WebGL 1 treats a texture whose edge lengths are not powers of two as
	// complete only if it is sampled with CLAMP_TO_EDGE and without mipmaps.
	// Otherwise every access returns black - not as an error but silently.
	// The default is GL_REPEAT, and rain, snow and clouds rest on it: they
	// tile by a scrolling texture matrix, and wrapTextureOffset() keeps that
	// offset inside one period precisely because REPEAT makes a whole period
	// an exact no-op. Therefore do not switch it across the board, but exactly
	// where REPEAT could never have worked anyway.
	//
	// The game's own art is all power of two, which leaves imported skins as
	// the only case here. Deliberately under Windows too, where NPOT with
	// REPEAT would work: otherwise a 300x200 rain would tile for the author
	// and not for his players.
	if(nextPow2(size.x) == size.x && nextPow2(size.y) == size.y) return;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Texture::checkDimensions()
{
	if(nextPow2(size.x) != size.x || nextPow2(size.y) != size.y)
	{
		// This could cause trouble.
		printfLog("- WARNING: Creating non-pow2 texture! Filename=\"%s\", Size=%dx%d\n", filename.c_str(), size.x, size.y);
	}
}