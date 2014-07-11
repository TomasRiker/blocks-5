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
	typedef std::map<dbKey, Vec4d> dbMap;
	dbMap db;
};

#endif