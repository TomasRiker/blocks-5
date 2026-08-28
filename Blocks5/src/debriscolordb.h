#ifndef _DEBRISCOLORDB_H
#define _DEBRISCOLORDB_H

#include "singleton.h"
#include <map>

/*** Klasse für die Datenbank der Trümmerfarben ***/

class Texture;

class DebrisColorDB : public Singleton<DebrisColorDB>
{
	friend class Singleton<DebrisColorDB>;

public:
	Vec4d getDebrisColor(Texture* p_texture, const Vec2i& positionOnTexture);

private:
	DebrisColorDB();
	~DebrisColorDB();

	typedef std::pair<std::string, Vec2i> dbKey;

	// Vec has an implicit 'operator const T*' so that it can be handed straight
	// to glColor4dv and friends. That conversion also makes 'a < b' on two Vec2i
	// decay to a POINTER comparison, so the default std::pair ordering compared
	// the addresses of the two vectors instead of their values - not a strict
	// weak ordering, and never equal for two separate objects. Every lookup below
	// therefore missed, the 16x16 average was recomputed on every call, and the
	// map grew by one entry per call for the lifetime of the process. Compare the
	// components explicitly.
	struct dbKeyLess
	{
		bool operator () (const dbKey& lhs, const dbKey& rhs) const
		{
			if(lhs.first != rhs.first) return lhs.first < rhs.first;
			if(lhs.second.x != rhs.second.x) return lhs.second.x < rhs.second.x;
			return lhs.second.y < rhs.second.y;
		}
	};

	typedef std::map<dbKey, Vec4d, dbKeyLess> dbMap;
	dbMap db;
};

#endif