#ifndef GLOBAL_H
#define GLOBAL_H

#include <QList>
#include <QSet>

typedef unsigned long id_t;
const id_t InvalidId = 0;

class WorldNode;
class WorldPath;
class WorldScript;

typedef QList<WorldNode*> NodeList;
typedef QSet<WorldNode*> NodeSet;

typedef QList<WorldPath*> PathList;
typedef QSet<WorldPath*> PathSet;

typedef QList<WorldScript*> ScriptList;

#endif // GLOBAL_H
