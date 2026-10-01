#ifndef _OBJECT_H
#define _OBJECT_H

#include "sprite.h"
#include "renderlayer.h"

#include "level.h"

/*** Class for a game object ***/

// How brightly something lights up and how fast that fades (object.cpp).
// The HUD icons use the same two (Level::flashHudIcon), so they look alike.
extern const float FLASH_STRENGTH;
extern const float FLASH_DECAY;

// Where a beam is drawn against where its emitter traced it; see object.cpp.
extern const float BEAM_DRAW_OFFSET;

class Player;

class Object
{
public:
	enum Flags
	{
		OF_MASSIVE			= 0x00000001,
		OF_COLLECTABLE		= 0x00000002,
		OF_GRAVITY			= 0x00000004,
		OF_FIXED			= 0x00000008,
		OF_ARROWTYPE		= 0x00000010,
		OF_DESTROYABLE		= 0x00000020,
		OF_ACTIVATOR		= 0x00000040,
		OF_TRIGGER_PANELS	= 0x00000080,
		OF_NO_SHADOW		= 0x00000100,
		OF_DEADLY			= 0x00000200,
		OF_DEADLY_WEIGHT	= 0x00000400,
		OF_KILL_FIRE		= 0x00000800,
		OF_ELEVATOR			= 0x00001000,
		OF_RAIL				= 0x00002000,
		OF_TRANSPORTABLE	= 0x00004000,
		OF_BURSTABLE		= 0x00008000,
		OF_CONVERTABLE		= 0x00010000,
		OF_PROXY			= 0x00020000,
		OF_BLOCK_GAS		= 0x00040000,
		OF_DONT_FALL		= 0x00080000,
		OF_ELECTRONICS		= 0x00100000
	};

	Object(Level& level, int depth);
	virtual ~Object();

	void render(RenderLayer layer, const Vec2i& offset, const Vec4f& color);
	void update();
	virtual void onRemove();

	// Runs once per frame before Level::render walks the layers and by
	// default rebuilds the sprites; an override calls this one too. Not in
	// onRender, which runs once per layer (RL_MAIN again per shadow sample)
	// with the pass's colour, while the sprites carry the object's own tint.
	virtual void onBeforeRender();

	virtual void onRender(RenderLayer layer, const Vec4f& color);
	virtual void onUpdate();
	virtual void onElectricitySwitch(bool on);
	virtual void onCollect(Player* p_player);
	virtual void onTouchedByPlayer(Player* p_player);
	virtual void onCollision(Object* p_obj);
	virtual void onExplosion();
	// simulate asks whether it would get anywhere and does none of it (object.cpp).
	virtual bool move(const Vec2i& dir, uint force = ~0, bool simulate = false);
	virtual bool allowMovement(const Vec2i& dir);
	virtual bool reflectLaser(Vec2i& dir, bool lightBarrier = false);
	virtual bool reflectProjectile(Vec2f& velocity);
	virtual void onFire();
	virtual void burst();

	// Close what the player is being shown without leaving the field; only a
	// showing hint note can. False passes the key on - Escape then opens the
	// game menu.
	virtual bool dismiss() { return false; }

	virtual bool changeInEditor(int mod);
	virtual void saveAttributes(TiXmlElement* p_target);
	virtual void saveExtendedAttributes(TiXmlElement* p_element);
	virtual void loadExtendedAttributes(TiXmlElement* p_element);

	virtual void frameBegin();
	void disappear(float duration);

	void beginCollectFlight(const Object* p_collector);
	void disappearNextFrame(float duration);

	bool isPushedFromAbove();
	bool isPushedWithDeadlyWeight();

	void say(const std::string& text, float duration);

	const std::string& getType() const;
	void setType(const std::string& type);
	const Vec2i& getPosition() const;
	void moveTo(const Vec2i& position);
	void warpTo(const Vec2i& position);
	void teleportTo(const Vec2i& position);
	bool hasMoved() const;
	const Vec2f& getRealShownPosition() const;
	Vec2i getShownPosition() const;
	Vec2i getShownPositionInPixels() const;
	uint getFlags() const;
	void setFlags(uint flags);

	// The layers this object may draw on, as RL_* bits; Level::renderObjects
	// skips an object whose bit is clear. A plain member, not a virtual, so a
	// subclass cannot answer differently from the onRender it inherits. It
	// may name a layer the object does not always draw on but never omit one
	// it does, so bits are only ever added.
	uint getRenderLayers() const { return renderLayers; }
	int getDepth() const;
	void setDepth(int depth);
	bool isGhost() const;
	void setGhost(bool ghost);
	int getDestroyTime() const;
	void setDestroyTime(int destroyTime);
	bool isAlive() const;
	bool toBeRemoved() const;
	void setCollisionSound(const std::string& collisionSound);
	bool isTeleporting() const;
	bool hasTeleportFailed() const;
	bool isFalling() const;

	// The current sprites, rebuilt on the spot: an object created and burst
	// in the same tick, or one asked before its level was first drawn, has
	// had no onBeforeRender yet.
	const Sprites& getSprites();

	// Light up briefly: a switch's acknowledgement, above all for the three
	// whose picture does not change (barrage, cannon, magnet). Called by
	// whoever wants it; not from onTouchedByPlayer, which runs for every
	// block the player bumps into.
	void flash();

	// How far a diamond machine's conversion of this block has got, 0 to 1.
	// render() fades the block towards CONVERSION_GHOST but never to nothing,
	// since it stays solid and pushable. frameBegin() clears it every tick
	// and the machine sets it again in its update(), so when the machine
	// stops (block pushed away, teleported, power off) it is zero the next
	// tick with no undo, and the opacity snaps back: a half-transparent block
	// sliding away looks like a bug. A dying block, blown up or converted,
	// keeps its value (frameBegin()).
	void setConversionProgress(float progress) { conversionProgress = progress; }

	uint getMass() const;
	void setMass(uint mass);
	uint getUID() const;
	void setUID(uint UID);
	virtual std::string getToolTip() const;
	void setToolTip(const std::string& toolTip);

	static Vec4f getStdColor(uint color);
	static Vec2i intToDir(int dir);
	static int dirToInt(const Vec2i& dir);

	int lastHashedAt;
	// Has onRemove() already run? Level::removeObject() must not unregister
	// twice - see there.
	bool removed;
	int onConveyorBelt;
	bool shadowPass;
	float noCollect;
	float flashAmount;

	// A random value in [-1, 1] that the shines and the two beams vary their
	// brightness by. Redrawn once per tick in frameBegin(), never in a render
	// path: a random() there would shift the shared generator by however many
	// frames the machine drew, and the logic would read other numbers. One
	// value per object, so different objects flicker independently. An
	// object never updated (an editor palette, a preview) keeps 0, the
	// brightness the caller asked for.
	float glowJitter;

	float conversionProgress;

protected:
	void handleSliding();

	// Enters this object's sprites into sprites. It is always called with an
	// empty list: never clear it yourself and never chain to the base class,
	// unless its sprites are wanted as well (Electronics draws the box the
	// others sit on).
	virtual void updateSprites();

	Sprites sprites;

	Level& level;
	std::string type;
	Vec2i position;
	Vec2f shownPosition;
	uint flags;
	int depth;
	bool ghost;
	int destroyTime;
	float deathCountDown;
	float deathSpeed;

	// The flight a collected item makes to whoever took it (object.cpp).
	bool collectFlight;
	uint collectorUID;
	Vec2f collectTarget;
	float newDeathCountDown;
	float newDeathSpeed;
	int newDeathTime;
	float interpolation;
	std::string collisionSound;
	bool moved;
	Vec2i lastMoveDir;
	float teleporting;
	Vec2i teleportingTo;
	bool teleportFailed;
	int oldDepth;
	float falling;
	uint mass;
	uint uid;
	std::string burstSound;
	int fall;
	std::string sayText;
	float sayTime;
	float sayAlpha;
	std::string toolTip;
	int slideDir;
	bool slideMove;
	// The layer this object draws its sprite on, and therefore the one
	// Object::render() adds the flash onto. Almost all of them draw on the
	// middle ground; the panels lie on the floor.
	RenderLayer flashLayer;

	// Set by whichever class defines onRender, in its own constructor - see
	// getRenderLayers(). say() and flash() add their own bits when they first
	// have something to draw.
	uint renderLayers;
	static int nextFallingDepth;

private:
	// Clear, enter the texture, call updateSprites(). The only way sprites is
	// ever filled - which is why updateSprites() can rely on an empty list.
	void rebuildSprites();
};

#endif