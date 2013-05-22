#ifndef GLOBAL_H
#define GLOBAL_H

#include <QList>
#include <QSet>

typedef unsigned long id_t;
const id_t InvalidId = 0;

class WorldNode;
class WorldPath;
class WorldScript;
class WorldTile;
class WorldTileAlias;
class WorldTileRule;

typedef QList<WorldNode*> NodeList;
typedef QSet<WorldNode*> NodeSet;

typedef QList<WorldPath*> PathList;
typedef QSet<WorldPath*> PathSet;

typedef QList<WorldScript*> ScriptList;

typedef QList<WorldTile*> TileList;
typedef QList<WorldTileAlias*> AliasList;
typedef QList<WorldTileRule*> TileRuleList;

#endif // GLOBAL_H
