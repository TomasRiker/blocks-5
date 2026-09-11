#ifndef _HINT_H
#define _HINT_H

#include "object.h"

/*** Class for a hint note ***/

class Font;
class Texture;
class Player;

class Hint : public Object
{
public:
	Hint(Level& level, const Vec2i& position, const std::string& text);
	~Hint();

	void onRender(RenderLayer layer, const Vec4d& color);
	void updateSprites();
	void onUpdate();
	void onRemove();
	void onCollect(Player* p_player);
	bool dismiss();
	void saveAttributes(TiXmlElement* p_target);

	const std::string& getText() const;
	void setText(const std::string& text);

private:
	// Where the note opens to. Must be decided before the first frame of it is
	// visible - see onUpdate().
	void updateTargetPosition();

	// Draw note and text together into one texture. After that the writing is
	// part of the paper: it flies with it, turns with it and rolls up with it,
	// instead of appearing on top at the end.
	void bakeNote();

	// The paper as a strip of quads: flat in the middle, rolled up at the top
	// and at the bottom. unroll runs from 0 (fully rolled up) to 1 (flat).
	void renderNote(const Vec4d& color, double unroll) const;
	void renderNoteMesh(const Vec4d& color, double unroll) const;

	// The path without a framebuffer object: note and text one after the other,
	// no roll - instead the height shrinks to the part that is still flat.
	void renderNoteFlat(const Vec4d& color, double unroll) const;

	std::string text;
	double alpha;
	double shownAlpha;
	Font* p_font;
	Texture* p_sprite;
	Vec2i targetPosition;

	// The baked texture together with the text it belongs to. It is borrowed
	// from the Engine and goes back as soon as the note is no longer visible -
	// otherwise every note ever stepped on would hold on to a whole screen
	// texture.
	uint noteTexture;
	std::string bakedText;

	void releaseNoteTexture();

	// The roll: by logic ticks and not by shownAlpha, which only approaches its
	// target and never arrives.
	double unroll;
	int activeTicks;

	// Dismissed even though the player is still standing on the field. Holds
	// until they leave it - otherwise the note would open again on the next
	// tick and the key would have done nothing.
	bool dismissed;
};

#endif
