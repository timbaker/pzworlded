#ifndef GLOBAL_H
#define GLOBAL_H

#include <QList>
#include <QSet>

class WorldNode;
class WorldPath;

typedef QList<WorldNode*> NodeList;
typedef QSet<WorldNode*> NodeSet;

typedef QList<WorldPath*> PathList;
typedef QSet<WorldPath*> PathSet;

#endif // GLOBAL_H
