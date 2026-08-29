#ifndef _IMG_LOAD_H
#define _IMG_LOAD_H

/*** Bilder laden ***/

// Ersetzt SDL_image. Das Spiel braucht davon genau eine Funktion, IMG_Load_RW,
// und liest jede Textur über ein eigenes SDL_RWops aus dem verschlüsselten
// data.zip (siehe File::getRWOps). Dekodiert wird mit stb_image, das als eine
// einzige Headerdatei in libs/stb liegt - damit fallen sdl_image.dll,
// libpng15-15.dll und zlib1.dll weg, und der Emscripten-Build benutzt genau
// denselben Code wie der Windows-Build.
//
// Unterstützt werden PNG und JPEG. Alle Bilder des Spiels sind PNG, und
// zip_data.bat und zip_skins.bat packen auch nur *.png ein.
//
// Die zurückgegebene Oberfläche ist immer 32 Bit RGBA, SDL_SWSURFACE.

extern "C"
{
	SDL_Surface* IMG_Load_RW(SDL_RWops* p_src, int freeSrc);
	SDL_Surface* IMG_Load(const char* p_filename);
}

#endif
