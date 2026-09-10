#ifndef _OBJECT_H
#define _OBJECT_H

#include "sprite.h"

#include "level.h"

/*** Class for a game object ***/

// How brightly something lights up and how fast that fades; the numbers are
// in object.cpp. Not only objects light up with them - the HUD icons do too
// (gs_game.cpp), and those must look exactly the same.
extern const double FLASH_STRENGTH;
extern const double FLASH_DECAY;

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

	void render(int layer, const Vec2i& offset, const Vec4d& color);
	void update();
	virtual void onRemove();

	// Runs once per frame, before Level::render walks the layers. By default
	// it brings sprites up to date; anything that does its own preparation
	// here calls Object::onBeforeRender() as well.
	//
	// Why not in onRender: that runs fourteen times per frame (twelve layers,
	// of which layer 1 runs twice for the shadows and once properly), and the
	// colour it is handed is the pass's - the shadow colour in the shadow
	// pass. The sprites carry the object's own tint, which is independent of
	// that.
	virtual void onBeforeRender();

	virtual void onRender(int layer, const Vec4d& color);
	virtual void onUpdate();
	virtual void onElectricitySwitch(bool on);
	virtual void onCollect(Player* p_player);
	virtual void onTouchedByPlayer(Player* p_player);
	virtual void onCollision(Object* p_obj);
	virtual void onExplosion();
	virtual bool move(const Vec2i& dir, uint force = ~0);
	virtual bool allowMovement(const Vec2i& dir);
	virtual bool reflectLaser(Vec2i& dir, bool lightBarrier = false);
	virtual bool reflectProjectile(Vec2d& velocity);
	virtual void onFire();
	virtual void burst();

	// Close something the player is currently being shown, without having to
	// leave the field. Only the hint note can do that; anything that shows
	// nothing answers false, and then the key belongs to whoever would
	// otherwise have got it - for Escape, the game menu.
	virtual bool dismiss() { return false; }

	virtual bool changeInEditor(int mod);
	virtual void saveAttributes(TiXmlElement* p_target);
	virtual void saveExtendedAttributes(TiXmlElement* p_element);
	virtual void loadExtendedAttributes(TiXmlElement* p_element);

	virtual void frameBegin();
	void disappear(double duration);
	void disappearNextFrame(double duration);

	bool isPushedFromAbove();
	bool isPushedWithDeadlyWeight();

	void say(const std::string& text, double duration);

	const std::string& getType() const;
	void setType(const std::string& type);
	const Vec2i& getPosition() const;
	void moveTo(const Vec2i& position);
	void warpTo(const Vec2i& position);
	void teleportTo(const Vec2i& position);
	bool hasMoved() const;
	const Vec2d& getRealShownPosition() const;
	Vec2i getShownPosition() const;
	Vec2i getShownPositionInPixels() const;
	uint getFlags() const;
	void setFlags(uint flags);
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

	// The sprites this object currently consists of - fresh, even if
	// onBeforeRender has not run yet in this frame. That is exactly the case
	// when an object is created and bursts in the same tick, and on the very
	// first tick, which Level::update runs before the first Level::render.
	const Sprites& getSprites();

	// Light up briefly. It is meant for the switches: three of them - barrage,
	// cannon, magnet - have no state of their own and act somewhere else in
	// the level, and are therefore the same picture before as after. Anything
	// that wants to light up calls it itself; Object::onTouchedByPlayer does
	// not, because that runs for every block the player bumps into.
	void flash();

	// How far the conversion in a diamond machine has got, 0 to 1. It makes
	// the block transparent - but never completely: the block stays solid the
	// whole time and can still be pushed, and whatever blocks the way has to
	// be visible.
	//
	// The value belongs to the block, not to the machine, and frameBegin()
	// clears it every tick; the machine sets it again in its update(), which
	// runs after all the frameBegin() calls. When the machine stops - because
	// the block has been pushed away, blown up or teleported, or the power
	// went off - the value stands at zero of its own accord in the next tick,
	// without anybody having to trigger an undo. That is exactly why the
	// opacity snaps back instead of fading back: a half-transparent block
	// sliding away looks like a bug.
	//
	// The one exception is in frameBegin(): a dying block keeps its value, or
	// the successful conversion would be the loudest case of snapping back
	// there is.
	void setConversionProgress(double progress) { conversionProgress = progress; }
	double getConversionProgress() const { return conversionProgress; }

	uint getMass() const;
	void setMass(uint mass);
	uint getUID() const;
	void setUID(uint UID);
	virtual std::string getToolTip() const;
	void setToolTip(const std::string& toolTip);

	static Vec4d getStdColor(uint color);
	static Vec2i intToDir(int dir);
	static int dirToInt(const Vec2i& dir);

	int lastHashedAt;
	// Has onRemove() already run? Level::removeObject() must not unregister
	// twice - see there.
	bool removed;
	int onConveyorBelt;
	bool shadowPass;
	double noCollect;
	double flashAmount;
	double conversionProgress;

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
	Vec2d shownPosition;
	uint flags;
	int depth;
	bool ghost;
	int destroyTime;
	double deathCountDown;
	double deathSpeed;
	double newDeathCountDown;
	double newDeathSpeed;
	int newDeathTime;
	double interpolation;
	std::string collisionSound;
	bool moved;
	Vec2i lastMoveDir;
	double teleporting;
	Vec2i teleportingTo;
	bool teleportFailed;
	int oldDepth;
	double falling;
	uint mass;
	uint uid;
	std::string burstSound;
	int fall;
	std::string sayText;
	double sayTime;
	double sayAlpha;
	std::string toolTip;
	int slideDir;
	bool slideMove;
	// The layer this object draws its sprite on, and therefore the one
	// Object::render() adds the flash onto. Almost all of them draw on 1; the
	// panels lie on the floor and draw on 0.
	int flashLayer;
	static int nextFallingDepth;

private:
	// Clear, enter the texture, call updateSprites(). The only way sprites is
	// ever filled - which is why updateSprites() can rely on an empty list.
	void rebuildSprites();
};

#endif