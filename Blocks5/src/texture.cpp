#include "pch.h"
#include "texture.h"
#include "filesystem.h"

Texture::Texture(const std::string& filename) : Resource(filename)
{
	p_rgba = 0;
	texID = 0;
	doKeepInMemory = false;
	doAddGaps = false;
	offset = Vec2i(0, 0);
	size = Vec2i(-1, -1);
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

	// Bild laden
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

	// OpenGL-Textur einrichten
	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_2D, texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// in das korrekte Format umwandeln
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

	// Bild sperren
	SDL_LockSurface(p_rgba);

	if(doAddGaps)
	{
		doAddGaps = false;
		addGaps();
	}
	else
	{
		// Bilddaten in die Textur kopieren
		glPixelStorei(GL_UNPACK_ROW_LENGTH, p_rgba->pitch / p_rgba->format->BytesPerPixel);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, p_rgba->w, p_rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, p_rgba->pixels);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		// Matrix erzeugen
		glPushAttrib(GL_TRANSFORM_BIT);
		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		glLoadIdentity();
		double w = static_cast<double>(size.x), h = static_cast<double>(size.y);
		glScaled(1.0 / w, 1.0 / h, 1.0);
		glGetDoublev(GL_TEXTURE_MATRIX, matrix);
		glPopMatrix();
		glPopAttrib();
	}
}

void Texture::cleanUp()
{
	if(p_rgba)
	{
		// Oberfläche entsperren und löschen
		SDL_UnlockSurface(p_rgba);
		SDL_FreeSurface(p_rgba);
		p_rgba = 0;
	}

	if(texID)
	{
		// Textur löschen
		glDeleteTextures(1, &texID);
		texID = 0;
	}
}

void Texture::bind() const
{
	if(!doKeepInMemory && p_rgba)
	{
		// Oberfläche entsperren und löschen
		SDL_UnlockSurface(p_rgba);
		SDL_FreeSurface(p_rgba);
		p_rgba = 0;
	}

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texID);

	// Pixel-Texturkoordinaten
	glPushAttrib(GL_TRANSFORM_BIT);
	glMatrixMode(GL_TEXTURE);
	glLoadMatrixd(matrix);
	glPopAttrib();
}

void Texture::unbind() const
{
	glDisable(GL_TEXTURE_2D);
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

	// OpenGL-Textur einrichten
	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_2D, texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// den gewünschten Teil kopieren
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

	// Bild sperren
	SDL_LockSurface(p_rgba);

	// Bilddaten in die Textur kopieren
	glPixelStorei(GL_UNPACK_ROW_LENGTH, p_rgba->pitch / p_rgba->format->BytesPerPixel);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, p_rgba->w, p_rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, p_rgba->pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	// Matrix erzeugen
	glPushAttrib(GL_TRANSFORM_BIT);
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();
	double w = static_cast<double>(size.x), h = static_cast<double>(size.y);
	glScaled(1.0 / w, 1.0 / h, 1.0);
	glGetDoublev(GL_TEXTURE_MATRIX, matrix);
	glPopMatrix();
	glPopAttrib();
}

const Vec2i& Texture::getSize() const
{
	return size;
}

void Texture::keepInMemory()
{
	doKeepInMemory = true;
}

void Texture::addGaps()
{
	if(!doKeepInMemory || doAddGaps) return;
	doAddGaps = true;

	SDL_Surface* p_new = SDL_CreateRGBSurface(SDL_SWSURFACE, size.x * 2, size.y * 2, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
	SDL_SetAlpha(p_new, 0, 0);
	SDL_FillRect(p_new, 0, 0x00000000);
	SDL_UnlockSurface(p_rgba);

	for(int x = 0; x < size.x; x += 16)
	{
		for(int y = 0; y < size.y; y += 16)
		{
			SDL_Rect srcRect;
			srcRect.x = x;
			srcRect.y = y;
			srcRect.w = 16;
			srcRect.h = 16;
			SDL_Rect destRect;
			destRect.x = 2 * x;
			destRect.y = 2 * y;
			destRect.w = 16;
			destRect.h = 16;
			SDL_BlitSurface(p_rgba, &srcRect, p_new, &destRect);
		}
	}

	SDL_FreeSurface(p_rgba);
	p_rgba = p_new;
	SDL_LockSurface(p_rgba);

	// Bild sperren
	SDL_LockSurface(p_rgba);

	// Bilddaten in die Textur kopieren
	glPixelStorei(GL_UNPACK_ROW_LENGTH, p_rgba->pitch / p_rgba->format->BytesPerPixel);
	glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, p_rgba->w, p_rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, p_rgba->pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	// Matrix erzeugen
	glPushAttrib(GL_TRANSFORM_BIT);
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();
	double w = static_cast<double>(size.x * 2), h = static_cast<double>(size.y * 2);
	glScaled(1.0 / w, 1.0 / h, 1.0);
	glGetDoublev(GL_TEXTURE_MATRIX, matrix);
	glPopMatrix();
	glPopAttrib();
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

void Texture::checkDimensions()
{
	if(nextPow2(size.x) != size.x || nextPow2(size.y) != size.y)
	{
		// Das könnte Ärger machen!
		printfLog("- WARNING: Creating non-pow2 texture! Filename=\"%s\", Size=%dx%d\n", filename.c_str(), size.x, size.y);
	}
}