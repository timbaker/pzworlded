#include "waterflow.h"

#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include <QDebug>
#include <QFileInfo>
#include <QtMath>

extern "C" {
#include "lualib.h"
#include "lauxlib.h"
}

WaterFlow::WaterFlow()
{

}

struct OldFlow
{
    QString name;
    int x, y;
    float speed;
    float flow;
};

void WaterFlow::readOldWaterDotLua(WorldDocument* worldDoc)
{
    World* world = worldDoc->world();
    auto& settings = world->getGenerateLotsSettings();
    int worldMinX = settings.worldOrigin.x() * 300;
    int worldMinY = settings.worldOrigin.y() * 300;
    WorldObjectGroup* objectGroup = world->objectGroups().find(QStringLiteral("WaterFlow"));
    ObjectType* objectType = objectGroup->type();
    PropertyDef* propertyDirection = world->propertyDefinition(QStringLiteral("WaterDirection"));
    PropertyDef* propertySpeed = world->propertyDefinition(QStringLiteral("WaterSpeed"));

    QString fileName = QStringLiteral("D:\\pz\\zomboid-svn\\Anims2\\workdir\\media\\maps\\Muldraugh, KY\\water.lua");
    if (QFileInfo(fileName).exists()) {
        if (lua_State *L = luaL_newstate()) {
            luaL_openlibs(L);
            int status = luaL_loadfile(L, fileName.toLatin1().data());
            if (status == LUA_OK) {
                status = lua_pcall(L, 0, 0, -1); // call the closure?
                if (status == LUA_OK) {
                    lua_getglobal(L, "water");
                    int tblidx = lua_gettop(L);
                    if (lua_istable(L, tblidx)) {
                        lua_pushnil(L); // push space on stack for the key
                        while (lua_next(L, tblidx) != 0) {
//                            QString roomName(QLatin1String(lua_tostring(L, -2)));
                            // value at -1 is the table of items in the room
                            lua_pushnil(L); // space for key
                            OldFlow oldFlow;
                            while (lua_next(L, -2) != 0) {
                                QString key = QLatin1String(lua_tostring(L, -2));
                                QString value = QLatin1String(lua_tostring(L, -1));
                                if (key == QStringLiteral("name")) {
                                    oldFlow.name = value;
                                } else if (key == QStringLiteral("x")) {
                                    oldFlow.x = value.toInt();
                                } else if (key == QStringLiteral("y")) {
                                    oldFlow.y = value.toInt();
                                } else if (key == QStringLiteral("speed")) {
                                    oldFlow.speed = value.toFloat();
                                } else if (key == QStringLiteral("flow")) {
                                    qreal radians = value.toDouble();
                                    int degrees = int(qRadiansToDegrees(radians)) % 360;
                                    if (degrees < 0)
                                        degrees += 360;

                                    // Zero degrees is north in non-isometric, positive is counter-clockwise.
                                    // Convert to zero degrees is north in isometric, positive is clockwise.
                                    oldFlow.flow = (360 - degrees - 45) % 360;
                                    if (oldFlow.flow < 0)
                                        oldFlow.flow += 360;

                                    oldFlow.flow = int(oldFlow.flow / 5) * 5;
                                }
                                lua_pop(L, 1); // pop value
                            }
                            lua_pop(L, 1); // pop space for the key
#if 1
                            QString objectString = QStringLiteral(
                                        "  { name = \"%1\", type = \"WaterFlow\", x = %2, y = %3, width = 1, height = 1, properties = { speed = %4, flow = %5 }},")
                                    .arg(oldFlow.name).arg(oldFlow.x).arg(oldFlow.y).arg(oldFlow.speed).arg(/*qRadiansToDegrees*/(oldFlow.flow));
                            qDebug() << objectString;
#endif
                            if (WorldCell* cell = worldDoc->world()->cellAt(oldFlow.x / 300 - worldMinX, oldFlow.y / 300 - worldMinY)) {
                                WorldCellObject* object = new WorldCellObject(cell, oldFlow.name, objectType, objectGroup, oldFlow.x - cell->x() * 300, oldFlow.y - cell->y() * 300, 0, 1, 1);
                                Property* property1 = new Property(propertyDirection, QString::number(oldFlow.flow));
                                Property* property2 = new Property(propertySpeed, QString::number(oldFlow.speed));
                                object->addProperty(100, property1);
                                object->addProperty(100, property2);
                                cell->insertObject(cell->objects().size(), object);
                            }
                        }
                        lua_pop(L, 1); // pop space for the key
                    }
                }
            }
            lua_close(L);
        }
    }
}
