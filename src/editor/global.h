#ifndef GLOBAL_H
#define GLOBAL_H

#include <QList>
#include <QMap>
#include <QSet>
#include <QString>

typedef unsigned long id_t;
const id_t InvalidId = 0;

class WorldLevel;
class WorldNode;
class WorldPath;
class WorldPathLayer;
class WorldScript;
class WorldTexture;
class WorldTile;
class WorldTileAlias;
class WorldTileRule;
class WorldTileset;
class WorldTileLayer;

typedef QList<WorldLevel*> LevelList;

typedef QList<WorldNode*> NodeList;
typedef QSet<WorldNode*> NodeSet;

typedef QList<WorldPath*> PathList;
typedef QSet<WorldPath*> PathSet;

typedef QList<WorldPathLayer*> PathLayerList;
typedef QList<WorldTileLayer*> TileLayerList;

typedef QList<WorldScript*> ScriptList;
typedef QSet<WorldScript*> ScriptSet;

typedef QList<WorldTile*> TileList;
typedef QList<WorldTileset*> TilesetList;

typedef QList<WorldTileAlias*> AliasList;
typedef QList<WorldTileRule*> TileRuleList;

typedef QMap<QString,QString> ScriptParams;

typedef QList<WorldTexture*> TextureList;
typedef QMap<QString,WorldTexture*> TextureMap;

#if defined(Q_OS_WIN) && (_MSC_VER == 1600)
// Hmmmm.  libtiled.dll defines the Properties class as so:
// class TILEDSHARED_EXPORT Properties : public QMap<QString,QString>
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

#endif // GLOBAL_H
