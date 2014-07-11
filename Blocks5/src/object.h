#ifndef _OBJECT_H
#define _OBJECT_H

#include "level.h"

/*** Klasse für ein Spielobjekt ***/

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
	const Vec4d& getDebrisColor() const;
	void setDebrisColor(const Vec4d& debrisColor);
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
	int onConveyorBelt;
	Vec2i positionOnTexture;
	bool shadowPass;
	double noCollect;

protected:
	void handleSliding();

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
	Vec4d debrisColor;
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
	static int nextFallingDepth;
};

#endif