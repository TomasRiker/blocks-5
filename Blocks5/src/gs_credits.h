#ifndef _GS_CREDITS_H
#define _GS_CREDITS_H

/*** Class for the credits ***/

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
	void leaveToMenu();
	void renderStarField(float t);
	void renderStars(const Mat4& projection, const Mat4& view);
	void updateStars();

	struct Star
	{
		Vec3f position;
		float size;
		Vec3f rotation;
		Vec3f deltaRotation;
		Vec2i positionOnTexture;
	};

	Engine& engine;
	Font* p_font;
	Level* p_level;
	Texture* p_sprites;
	int time;
	Vec3f cameraPos;
	Vec3f cameraDir;
	std::list<Star> stars;

	// The stars' corners, rebuilt every frame and drawn in one call; a member
	// so that their storage is allocated once.
	std::vector<Vertex3> starVertices;

	uint bufferID;
	int speed;

	// The full ending, or the names alone, decided in onEnter. shift moves the
	// shown blocks earlier, fadeAt is when the fade to black begins and endAt
	// when the state hands back to the menu; all in seconds, as the block
	// table is.
	bool full;
	float shift;
	float fadeAt;
	float endAt;

	// Whether a click or a key may be taken as the player asking to leave.
	// False for the tick the screen is entered in, which still holds the
	// press that opened it.
	bool exitArmed;

	// Set once the way out is taken - see leaveToMenu().
	bool leaving;
};

#endif