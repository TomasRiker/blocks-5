#ifndef _IMG_SAVE_H
#define _IMG_SAVE_H

/*** Bilder speichern ***/

// PNG-Kodierer, das Gegenstueck zu img_load.h - nur ohne fremde Bibliothek.
// Dekodieren ist schwer, dafuer liegt stb_image in libs/stb; kodieren ist es
// nicht, denn die Arbeit macht zlib, und die ist ohnehin in allen drei Builds
// einkompiliert: compress2() liefert genau den zlib-Datenstrom, aus dem ein
// IDAT-Chunk besteht, und crc32() ist die Pruefsumme, die jeder Chunk braucht.
//
// srcChannels ist die Anordnung im Speicher, dstChannels das, was in der Datei
// stehen soll; 3 heisst RGB, 4 RGBA, immer 8 Bit je Kanal. Die beiden duerfen
// sich unterscheiden, und der eine Aufrufer braucht das auch: glReadPixels muss
// RGBA lesen, weil WebGL 1 nichts anderes zulaesst, aber ein Alphakanal aus
// lauter 255 kostet in der Datei trotzdem ein Viertel - er faellt nicht in sich
// zusammen, sondern schiebt jede Vorhersage um ein Byte auseinander. Gemessen
// an einem Bildschirmfoto: 330 statt 247 KB.
//
// bottomUp dreht die Zeilen beim Kodieren um, was ebenfalls von glReadPixels
// kommt: OpenGL liefert die unterste Zeile zuerst, PNG will die oberste.
bool encodePNG(const uchar* p_pixels, const Vec2i& size,
			   int srcChannels, int dstChannels, bool bottomUp,
			   std::vector<uchar>* p_out);

#endif
