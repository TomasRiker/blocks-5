#ifndef _HINT_H
#define _HINT_H

#include "object.h"

/*** Class for a hint note ***/

class Font;
class Texture;
class Player;
class SoundInstance;

class Hint : public Object
{
public:
	Hint(Level& level, const Vec2i& position, const std::string& text);
	~Hint();

	void onRender(RenderLayer layer, const Vec4f& color);
	void updateSprites();
	void onUpdate();
	void onRemove();
	void onCollect(Player* p_player);
	bool dismiss();
	void saveAttributes(TiXmlElement* p_target);

	const std::string& getText() const;
	void setText(const std::string& text);

	// The language the level editor's preview shows, "" for the active one.
	// The preview layer alone reads it, so a note in a running level never
	// bakes in it.
	void setPreviewLanguage(const std::string& language);

private:
	// Where the note opens to. Must be decided before the first frame of it is
	// visible - see onUpdate().
	void updateTargetPosition();

	// Draws sheet and text together into one texture, so the writing flies,
	// turns and rolls with the paper. inLanguage picks the text's language,
	// "" the active one.
	void bakeNote(const std::string& inLanguage);

	// The paper as a strip of bands: flat in the middle, rolled up at the top
	// and at the bottom. unroll runs from 0 (fully rolled up) to 1 (flat).
	void renderNote(const Vec4f& color, float unroll) const;
	void renderNoteMesh(const RenderState& state, const Vec4f& color, float unroll) const;

	std::string text;
	float alpha;
	float shownAlpha;
	Font* p_font;
	Texture* p_sprite;
	Vec2i targetPosition;

	// The baked texture and the text baked into it. Borrowed from the Engine's
	// pool and handed back once the note is invisible, or every note ever
	// stepped on would keep a 1024x512 texture.
	uint noteTexture;
	std::string bakedText;
	std::string previewLanguage;

	void releaseNoteTexture();

	// The roll: by logic ticks and not by shownAlpha, which only approaches its
	// target and never arrives.
	float unroll;
	int activeTicks;

	// The rustle, held only to fade it when its motion is cut short. Sound
	// deletes an instance once it has played out, so Sound::isLiveInstance()
	// is asked, with the serial, before the pointer is used. scrollDirection
	// is the paper's motion in the last tick: 1 unrolling, -1 rolling up, 0 at
	// rest.
	SoundInstance* p_scrollSound;
	uint scrollSerial;
	int scrollDirection;
	void fadeScrollSound();

	// Dismissed with the player still on the field. Holds until they leave,
	// or the note would open again on the next tick.
	bool dismissed;
};

#endif
