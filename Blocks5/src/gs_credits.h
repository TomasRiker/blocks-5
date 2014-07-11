#ifndef _GS_CREDITS_H
#define _GS_CREDITS_H

/*** Klasse für den Abspann ***/

#include "gamestate.h"
#include "engine.h"
#include "level.h"

class Font;
class Texture;

class GS_Credits : public GameState
{
public:
	GS_Credits();
	~GS_Credits();

	void onRender();
	void onUpdate();
	void onEnter(const ParameterBlock& context);
	void onLeave(const ParameterBlock& context);
	void onGetFocus();
	void onLoseFocus();

private:
	void renderStars();
	void updateStars();

	struct Star
	{
		Vec3d position;
		double size;
		Vec3d rotation;
		Vec3d deltaRotation;
		Vec2i positionOnTexture;
	};

	Engine& engine;
	Font* p_font;
	Level* p_level;
	Texture* p_sprites;
	int time;
	Vec3d cameraPos;
	Vec3d cameraDir;
	std::list<Star> stars;
	uint bufferID;
	int speed;
};

#endif