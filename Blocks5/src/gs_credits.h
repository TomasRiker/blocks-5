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

	// The stars' corners, baked once a frame and handed to the renderer in
	// one draw. Kept rather than built on the stack so that the four hundred
	// quads are allocated once and not once a frame.
	std::vector<Vertex3> starVertices;

	uint bufferID;
	int speed;

	// The full ending, or the names alone: gs_credits.cpp decides in onEnter
	// and the three numbers below are laid out from it - what the blocks that
	// are shown move up by, when the fade to black begins, and when the state
	// hands back to the menu. All three in seconds, as the block table is.
	bool full;
	float shift;
	float fadeAt;
	float endAt;

	// Whether a click may be taken as the player's. False for the tick the
	// screen is entered in, which still holds the click that opened it.
	bool clickArmed;
};

#endif