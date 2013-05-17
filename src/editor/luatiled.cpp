/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "luatiled.h"

#include "mapcomposite.h"

#include "luaworlded.h"
#include "map.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "pathworld.h"
#include "tile.h"
#include "tileset.h"
#include "tilelayer.h"
#include "tilepainter.h"
#if 1
#include "mapwriter.h"
#else
#include "tmxmapwriter.h"
#endif

#include "tolua.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QString>
#include <QTextStream>

extern "C" {
#include "lualib.h"
#include "lauxlib.h"
}

using namespace Tiled;
using namespace Tiled::Lua;

TOLUA_API int tolua_tiled_open(lua_State *L);

const char *Lua::cstring(const QString &qstring)
{
    static QHash<QString,const char*> StringHash;
    if (!StringHash.contains(qstring)) {
        QByteArray b = qstring.toLatin1();
        char *s = new char[b.size() + 1];
        memcpy(s, (void*)b.data(), b.size() + 1);
        StringHash[qstring] = s;
    }
    return StringHash[qstring];
}

/////

#if 0
/* function to release collected object via destructor */
static int tolua_collect_QRect(lua_State* tolua_S)
{
    QRect* self = (QRect*) tolua_tousertype(tolua_S,1,0);
    tolua_release(tolua_S,self);
    delete self;
    return 0;
}

/* method: rects of class QRegion */
static int tolua_tiled_Region_rects00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
    tolua_Error tolua_err;
    if (
            !tolua_isusertype(tolua_S,1,"QRegion",0,&tolua_err) ||
            !tolua_isnoobj(tolua_S,2,&tolua_err)
            )
        goto tolua_lerror;
    else
#endif
    {
        QRegion* self = (QRegion*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
        if (!self) tolua_error(tolua_S,"invalid 'self' in function 'rects'",NULL);
#endif
        {
            lua_newtable(tolua_S);
            for (int i = 0; i < self->rectCount(); i++) {
                void* tolua_obj = new QRect(self->rects()[i]);
                void* v = tolua_clone(tolua_S,tolua_obj,tolua_collect_QRect);
                tolua_pushfieldusertype(tolua_S,2,i+1,v,"QRect");
            }
        }
        return 1;
    }
    return 0;
#ifndef TOLUA_RELEASE
tolua_lerror:
    tolua_error(tolua_S,"#ferror in function 'rects'.",&tolua_err);
    return 0;
#endif
}

/* method: tiles of class LuaBmpRule */
static int tolua_tiled_BmpRule_tiles00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
    tolua_Error tolua_err;
    if (
            !tolua_isusertype(tolua_S,1,"LuaBmpRule",0,&tolua_err) ||
            !tolua_isnoobj(tolua_S,2,&tolua_err)
            )
        goto tolua_lerror;
    else
#endif
    {
        LuaBmpRule* self = (LuaBmpRule*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
        if (!self) tolua_error(tolua_S,"invalid 'self' in function 'tiles'",NULL);
#endif
        {
            lua_newtable(tolua_S);
            for (int i = 0; i < self->mRule->tileChoices.size(); i++) {
                tolua_pushfieldstring(tolua_S,2,i+1,cstring(self->mRule->tileChoices[i]));
            }
        }
        return 1;
    }
    return 0;
#ifndef TOLUA_RELEASE
tolua_lerror:
    tolua_error(tolua_S,"#ferror in function 'tiles'.",&tolua_err);
    return 0;
#endif
}
#endif

/////

LuaScript::LuaScript(PathWorld *world, WorldScript *script) :
    L(0),
    mWorld(world),
    mWorldScript(script)
{
}

LuaScript::~LuaScript()
{
    if (L)
        lua_close(L);
}

lua_State *LuaScript::init()
{
    L = luaL_newstate();
    luaL_openlibs(L);
    tolua_tiled_open(L);

    tolua_beginmodule(L,NULL);
#if 0
    tolua_beginmodule(L,"Region");
    tolua_function(L,"rects",tolua_tiled_Region_rects00);
    tolua_endmodule(L);
    tolua_beginmodule(L,"BmpRule");
    tolua_function(L,"tiles",tolua_tiled_BmpRule_tiles00);
    tolua_endmodule(L);
#endif
    tolua_endmodule(L);

    return L;
}

#include "luaconsole.h"
extern "C" {

// see luaconf.h
// these are where print() calls go
void luai_writestring(const char *s, int len)
{
    LuaConsole::instance()->writestring(s, len);
}

void luai_writeline()
{
    LuaConsole::instance()->writeline();
}

void extern_lua_assert(const char *cond, const char *file, int line)
{
    qt_assert(cond, file, line);
}

static int traceback (lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  if (msg)
    luaL_traceback(L, L, msg, 1);
  else if (!lua_isnoneornil(L, 1)) {  /* is there an error object? */
    if (!luaL_callmeta(L, 1, "__tostring"))  /* try its 'tostring' metamethod */
      lua_pushliteral(L, "(no error message)");
  }
  return 1;
}
}

#define SCRIPT_CACHE 0
#if SCRIPT_CACHE
static QMap<QString,char*> LoadedScripts;
#include <QFile>
#endif

static int load_lua_file(lua_State *L, const QString &fileName)
{
#if SCRIPT_CACHE
    if (!LoadedScripts.contains(fileName)) {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
//            output = file.errorString();
            return LUA_ERRFILE;
        }
        QByteArray ba = file.readAll();
        char *buf = (char*)malloc(ba.size() + 1);
        memcpy(buf, ba.data(), ba.size() + 1);
        LoadedScripts[fileName] = buf;
    }
    return luaL_dostring(L, LoadedScripts[fileName]); // FIXME: loses the file name
#else
    return luaL_loadfile(L, cstring(fileName));
#endif
}

bool LuaScript::dofile(const QString &f, QString &output)
{
    lua_State *L = init();

    QElapsedTimer elapsed;
    elapsed.start();
#if 0
    tolua_pushusertype(L, &mMap, "LuaMap");
    lua_setglobal(L, "map");

    if (mPath) {
        tolua_pushusertype(L, mPath, "LuaPath");
        lua_setglobal(L, "path");
    }
#endif

    int status = load_lua_file(L, f);
    if (status == LUA_OK) {
        int base = lua_gettop(L);
        lua_pushcfunction(L, traceback);
        lua_insert(L, base);
        status = lua_pcall(L, 0, 0, base);
        lua_remove(L, base);
    }
    output = QString::fromLatin1(lua_tostring(L, -1));
    LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
    if (elapsed.elapsed() > 1000)
        LuaConsole::instance()->write(qApp->tr("---------- script completed in %1s ----------")
                                      .arg(elapsed.elapsed()/1000.0));
//    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    return status == LUA_OK;
}

bool LuaScript::runFunction(const char *name)
{
    QElapsedTimer elapsed;
    elapsed.start();

    lua_State *L = init();

    LuaWorld lw(mWorld);
    tolua_pushusertype(L, &lw, "LuaWorld");
    lua_setglobal(L, "world");

    LuaWorldScript lws(mWorldScript);
    tolua_pushusertype(L, &lws, "LuaWorldScript");
    lua_setglobal(L, "script");

    TilePainter tp(mWorld);
    LuaTilePainter ltp(&tp);
    tolua_pushusertype(L, &ltp, "LuaTilePainter");
    lua_setglobal(L, "painter");

    QString output;
    int status = load_lua_file(L, mWorldScript->mFileName);
    if (status == LUA_OK) {
        int base = lua_gettop(L);
        lua_pushcfunction(L, traceback);
        lua_insert(L, base);
        status = lua_pcall(L, 0, 0, base);
        if (status == LUA_OK) {
            lua_getglobal(L, name);
            status = lua_pcall(L, 0, 1, base);
        }
        lua_remove(L, base);
    }
    output = QString::fromLatin1(lua_tostring(L, -1));
    LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
    if (elapsed.elapsed() > 1000)
        LuaConsole::instance()->write(qApp->tr("---------- script completed in %1s ----------")
                                      .arg(elapsed.elapsed()/1000.0));
//    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    return status == LUA_OK;
}

bool LuaScript::getResultRegion(QRegion &rgn)
{
    tolua_Error err;
    if (lua_gettop(L) >= 1 && tolua_isusertype(L, -1, "LuaRegion", 0, &err) == 1) {
        LuaRegion *lr = (LuaRegion *) tolua_tousertype(L, -1, 0);
        rgn = *lr;
        return true;
    }
    if (lua_gettop(L) >= 1 && tolua_isusertype(L, -1, "QRect", 0, &err) == 1) {
        QRect *r = (QRect *) tolua_tousertype(L, -1, 0);
        rgn = QRegion(*r);
        return true;
    }
    return false;
}

/////

LuaLayer::LuaLayer() :
    mClone(0),
    mOrig(0),
    mMap(0)
{
}

LuaLayer::LuaLayer(Layer *orig, LuaMap *map) :
    mClone(0),
    mOrig(orig),
    mName(orig->name()),
    mMap(map)
{
}

LuaLayer::~LuaLayer()
{
    delete mClone;
}

const char *LuaLayer::name()
{
    return cstring(mName);
}

void LuaLayer::initClone()
{
    // A script-created layer will have mOrig == 0
    Q_ASSERT(mOrig || mClone);

    if (!mClone && mOrig) {
        mClone = mOrig->clone();
        cloned();
    }
}

void LuaLayer::cloned()
{

}

/////

LuaTileLayer::LuaTileLayer(TileLayer *orig, LuaMap *map) :
    LuaLayer(orig, map),
    mCloneTileLayer(0)
{
}

LuaTileLayer::LuaTileLayer(const char *name, int x, int y, int width, int height) :
    LuaLayer(),
    mCloneTileLayer(new TileLayer(QString::fromLatin1(name), x, y, width, height))
{
    mName = mCloneTileLayer->name();
    mClone = mCloneTileLayer;
}

LuaTileLayer::~LuaTileLayer()
{
}

void LuaTileLayer::cloned()
{
    LuaLayer::cloned();
    mCloneTileLayer = mClone->asTileLayer();
}

int LuaTileLayer::level()
{
    int level;
    MapComposite::levelForLayer(mName, &level);
    return level;
}

void LuaTileLayer::setTile(int x, int y, Tile *tile)
{
#if 0
    // Forbid changing tiles outside the current tile selection.
    // See the PaintTileLayer undo command.
    if (mMap && !mMap->mSelection.isEmpty() && !mMap->mSelection.contains(QPoint(x, y)))
        return;
#endif
    initClone();
    if (!mCloneTileLayer->contains(x, y))
        return; // TODO: lua error!
    mCloneTileLayer->setCell(x, y, Cell(tile));
    mAltered += QRect(x, y, 1, 1); // too slow?
}

Tile *LuaTileLayer::tileAt(int x, int y)
{
    if (mClone) {
        if (!mCloneTileLayer->contains(x, y))
            return 0; // TODO: lua error!
        return mCloneTileLayer->cellAt(x, y).tile;
    }
    Q_ASSERT(mOrig);
    if (!mOrig)
        return 0; // this layer was created by the script
    if (!mOrig->asTileLayer()->contains(x, y))
        return 0; // TODO: lua error!
    return mOrig->asTileLayer()->cellAt(x, y).tile;
}

void LuaTileLayer::clearTile(int x, int y)
{
    setTile(x, y, 0);
}

void LuaTileLayer::erase(int x, int y, int width, int height)
{
    fill(QRect(x, y, width, height), 0);
}

void LuaTileLayer::erase(QRect &r)
{
    fill(r, 0);
}

void LuaTileLayer::erase(LuaRegion &rgn)
{
    fill(rgn, 0);
}

void LuaTileLayer::erase()
{
    fill(0);
}

void LuaTileLayer::fill(int x, int y, int width, int height, Tile *tile)
{
    fill(QRect(x, y, width, height), tile);
}

void LuaTileLayer::fill(const QRect &r, Tile *tile)
{
    initClone();
    QRect r2 = r & mClone->bounds();
    for (int y = r2.y(); y <= r2.bottom(); y++) {
        for (int x = r2.x(); x <= r2.right(); x++) {
            mCloneTileLayer->setCell(x, y, Cell(tile));
        }
    }
    mAltered += r;
}

void LuaTileLayer::fill(const LuaRegion &rgn, Tile *tile)
{
    foreach (QRect r, rgn.rects()) {
        fill(r, tile);
    }
}

#include "luaworlded.h"
#include "path.h"
void LuaTileLayer::fill(LuaPath *path, Tile *tile)
{;
    QPolygonF polygon = path->mPath->polygon();
    QRegion region = ::polygonRegion(polygon);
    region.translate(polygon.boundingRect().toAlignedRect().topLeft() - mMap->mCellPos * 300);
    fill(LuaRegion(region), tile);
}

void LuaTileLayer::fill(Tile *tile)
{
    fill(mClone ? mClone->bounds() : mOrig->bounds(), tile);
}

void LuaTileLayer::replaceTile(Tile *oldTile, Tile *newTile)
{
    initClone();
    for (int y = 0; y < mClone->width(); y++) {
        for (int x = 0; x < mClone->width(); x++) {
            if (mCloneTileLayer->cellAt(x, y).tile == oldTile) {
                mCloneTileLayer->setCell(x, y, Cell(newTile));
                mAltered += QRect(x, y, 1, 1);
            }
        }
    }
}

/////

LuaMap::LuaMap(Map *orig) :
    mClone(new Map(orig->orientation(), orig->width(), orig->height(),
                   orig->tileWidth(), orig->tileHeight())),
    mOrig(orig),
    mBmpMain(mClone->rbmpMain()),
    mBmpVeg(mClone->rbmpVeg())
{
    foreach (Layer *layer, orig->layers()) {
        if (TileLayer *tl = layer->asTileLayer())
            mLayers += new LuaTileLayer(tl, this);
        else if (ObjectGroup *og = layer->asObjectGroup())
            mLayers+= new LuaObjectGroup(og, this);
        else
            mLayers += new LuaLayer(layer, this);
        mLayerByName[layer->name()] = mLayers.last(); // could be duplicates & empty names
//        mClone->addLayer(layer);
    }

    mClone->rbmpSettings()->clone(*mOrig->bmpSettings());
    mBmpMain.mBmp = orig->bmpMain();
    mBmpVeg.mBmp = orig->bmpVeg();

    foreach (BmpAlias *alias, mClone->bmpSettings()->aliases()) {
        mAliases += new LuaBmpAlias(alias);
        mAliasByName[alias->name] = mAliases.last();
    }
    foreach (BmpRule *rule, mClone->bmpSettings()->rules()) {
        mRules += new LuaBmpRule(rule);
        if (!rule->label.isEmpty())
            mRuleByName[rule->label] = mRules.last();
    }
    foreach (BmpBlend *blend, mClone->bmpSettings()->blends()) {
        mBlends += new LuaBmpBlend(blend);
    }

    foreach (Tileset *ts, mOrig->tilesets())
        addTileset(ts);
}

LuaMap::LuaMap(LuaMap::Orientation orient, int width, int height, int tileWidth, int tileHeight) :
    mClone(new Map((Map::Orientation)orient, width, height, tileWidth, tileHeight)),
    mOrig(0),
    mBmpMain(mClone->rbmpMain()),
    mBmpVeg(mClone->rbmpVeg())
{
}

LuaMap::~LuaMap()
{
    qDeleteAll(mAliases);
    qDeleteAll(mRules);
    qDeleteAll(mBlends);

    // Remove all layers from the clone map.
    // Either they are the original unmodified layers or they are clones.
    // Original layers aren't to be deleted, clones delete themselves.
    for (int i = mClone->layerCount() - 1; i >= 0; i--) {
        mClone->takeLayerAt(i);
    }

    qDeleteAll(mLayers);
    qDeleteAll(mRemovedLayers);
    delete mClone;
}

LuaMap::Orientation LuaMap::orientation()
{
    return (Orientation) mClone->orientation();
}

int LuaMap::width() const
{
    return mClone->width();
}

int LuaMap::height() const
{
    return mClone->height();
}

int LuaMap::maxLevel()
{
    return 10; // FIXME
}

int LuaMap::layerCount() const
{
    return mLayers.size();
}

LuaLayer *LuaMap::layerAt(int index) const
{
    if (index < 0 || index >= mLayers.size())
        return 0; // TODO: lua error
    return mLayers.at(index);
}

LuaLayer *LuaMap::layer(const char *name)
{
    QString _name = QString::fromLatin1(name);
    if (mLayerByName.contains(_name))
        return mLayerByName[_name];
    return 0;
}

LuaTileLayer *LuaMap::tileLayer(const char *name)
{
    if (LuaLayer *layer = this->layer(name))
        return layer->asTileLayer();
    return 0;
}

LuaTileLayer *LuaMap::newTileLayer(const char *name)
{
    LuaTileLayer *tl = new LuaTileLayer(name, 0, 0, width(), height());
    return tl;
}

void LuaMap::addLayer(LuaLayer *layer)
{
    if (mLayers.contains(layer) || layer->mMap)
        return; // error!

    if (mRemovedLayers.contains(layer))
        mRemovedLayers.removeAll(layer);
    mLayers += layer;
    layer->mMap = this;
//    mClone->addLayer(layer->mClone ? layer->mClone : layer->mOrig);

    mLayerByName.clear(); // FIXME: make more efficient
    foreach (LuaLayer *ll, mLayers)
        mLayerByName[ll->mName] = ll;
}

void LuaMap::insertLayer(int index, LuaLayer *layer)
{
    if (mLayers.contains(layer) || layer->mMap)
        return; // error!

    if (mRemovedLayers.contains(layer))
        mRemovedLayers.removeAll(layer);

    index = qBound(0, index, mLayers.size());
    mLayers.insert(index, layer);
    layer->mMap = this;
//    mClone->insertLayer(index, layer->mClone ? layer->mClone : layer->mOrig);

    mLayerByName.clear(); // FIXME: make more efficient
    foreach (LuaLayer *ll, mLayers)
        mLayerByName[ll->mName] = ll;
}

void LuaMap::removeLayer(int index)
{
    if (index < 0 || index >= mLayers.size())
        return; // error!
    LuaLayer *layer = mLayers.takeAt(index);
    mRemovedLayers += layer;
    layer->mMap = 0;
//    mClone->takeLayerAt(index);

    mLayerByName.clear(); // FIXME: make more efficient
    foreach (LuaLayer *ll, mLayers)
        mLayerByName[layer->mName] = ll;
}

static bool parseTileName(const QString &tileName, QString &tilesetName, int &index)
{
    int n = tileName.lastIndexOf(QLatin1Char('_'));
    if (n == -1)
        return false;
    tilesetName = tileName.mid(0, n);
    QString indexString = tileName.mid(n + 1);

    // Strip leading zeroes from the tile index
    int i = 0;
    while (i < indexString.length() - 1 && indexString[i] == QLatin1Char('0'))
        i++;
    indexString.remove(0, i);

    bool ok;
    index = indexString.toUInt(&ok);
    return !tilesetName.isEmpty() && ok;
}

Tile *LuaMap::tile(const char *name)
{
    QString tilesetName;
    int tileID;
    if (!parseTileName(QString::fromLatin1(name), tilesetName, tileID))
        return 0;

    if (Tileset *ts = _tileset(tilesetName))
        return ts->tileAt(tileID);
    return 0;
}

Tile *LuaMap::tile(const char *tilesetName, int tileID)
{
    if (Tileset *ts = tileset(tilesetName))
        return ts->tileAt(tileID);
    return 0;
}

void LuaMap::addTileset(Tileset *tileset)
{
    if (mClone->tilesets().contains(tileset))
        return; // error!

    mClone->addTileset(tileset);
    mTilesetByName[tileset->name()] = tileset;
}

int LuaMap::tilesetCount()
{
    return mClone->tilesets().size();
}

Tileset *LuaMap::_tileset(const QString &name)
{
    if (mTilesetByName.contains(name))
        return mTilesetByName[name];
    return 0;
}

Tileset *LuaMap::tileset(const char *name)
{
    return _tileset(QString::fromLatin1(name));
}

Tileset *LuaMap::tilesetAt(int index)
{
    if (index >= 0 && index < mClone->tilesets().size())
        return mClone->tilesets()[index];
    return 0;
}

LuaMapBmp &LuaMap::bmp(int index)
{
    return index ? mBmpVeg : mBmpMain;
}

QList<LuaBmpAlias *> LuaMap::aliases()
{
    return mAliases;
}

LuaBmpAlias *LuaMap::alias(const char *name)
{
    QString qname(QString::fromLatin1(name));
    if (mAliasByName.contains(qname))
        return mAliasByName[qname];
    return 0;
}

int LuaMap::ruleCount()
{
    return mRules.size();
}

QList<LuaBmpRule *> LuaMap::rules()
{
    return mRules;
}

LuaBmpRule *LuaMap::ruleAt(int index)
{
    if (index >= 0 && index < mRules.size())
        return mRules[index];
    return 0;
}

LuaBmpRule *LuaMap::rule(const char *name)
{
    QString qname(QString::fromLatin1(name));

    if (mRuleByName.contains(qname))
        return mRuleByName[qname];
    return 0;
}

QList<LuaBmpBlend *> LuaMap::blends()
{
    return mBlends;
}

bool LuaMap::write(const char *path)
{
    QScopedPointer<Map> map(mClone->clone());
    Q_ASSERT(map->layerCount() == 0);
    foreach (LuaLayer *ll, mLayers) {
        Layer *newLayer = ll->mClone ? ll->mClone->clone() : ll->mOrig->clone();
        if (LuaObjectGroup *og = ll->asObjectGroup()) {
            foreach (LuaMapObject *o, og->objects())
                newLayer->asObjectGroup()->addObject(o->mClone->clone());
        }
        map->addLayer(newLayer);
    }

#if 1
    MapWriter writer;
    writer.setLayerDataFormat(MapWriter::Base64Zlib);
    writer.setDtdEnabled(false);
    if (!writer.writeMap(map.data(), QString::fromLatin1(path))) {
#else
    Internal::TmxMapWriter writer;
    if (!writer.write(map.data(), QString::fromLatin1(path))) {
#endif
        // mError = writer.errorString();
        return false;
    }
    return true;
}

/////

LuaMapBmp::LuaMapBmp(MapBmp &bmp) :
    mBmp(bmp)
{
}

bool LuaMapBmp::contains(int x, int y)
{
    return QRect(0, 0, mBmp.width(), mBmp.height()).contains(x, y);
}

void LuaMapBmp::setPixel(int x, int y, LuaColor &c)
{
    if (!contains(x, y)) return; // error!
    mBmp.setPixel(x, y, c.pixel);
    mAltered += QRect(x, y, 1, 1);
}

unsigned int LuaMapBmp::pixel(int x, int y)
{
    if (!contains(x, y)) return qRgb(0,0,0); // error!
    return mBmp.pixel(x, y);
}

void LuaMapBmp::erase(int x, int y, int width, int height)
{
    fill(x, y, width, height, LuaColor());
}

void LuaMapBmp::erase(const QRect &r)
{
    fill(r, LuaColor());
}

void LuaMapBmp::erase(const LuaRegion &rgn)
{
    fill(rgn, LuaColor());
}

void LuaMapBmp::erase()
{
    fill(LuaColor());
}

void LuaMapBmp::fill(int x, int y, int width, int height, const LuaColor &c)
{
    fill(QRect(x, y, width, height), c);
}

void LuaMapBmp::fill(const QRect &r, const LuaColor &c)
{
    QRect r2 = r & QRect(0, 0, mBmp.width(), mBmp.height());

    for (int y = r2.y(); y <= r2.bottom(); y++) {
        for (int x = r2.x(); x <= r2.right(); x++) {
            mBmp.setPixel(x, y, c.pixel);
        }
    }

    mAltered += r;
}

void LuaMapBmp::fill(const LuaRegion &rgn, const LuaColor &c)
{
    foreach (QRect r, rgn.rects())
        fill(r, c);
}

void LuaMapBmp::fill(const LuaColor &c)
{
    fill(QRect(0, 0, mBmp.width(), mBmp.height()), c);
}

void LuaMapBmp::replace(LuaColor &oldColor, LuaColor &newColor)
{
    for (int y = 0; y < mBmp.height(); y++) {
        for (int x = 0; x < mBmp.width(); x++) {
            if (mBmp.pixel(x, y) == oldColor.pixel)
                mBmp.setPixel(x, y, newColor.pixel);
        }
    }
}

/////

LuaColor Lua::Lua_rgb(int r, int g, int b)
{
    return LuaColor(r, g, b);
}

/////

const char *LuaBmpRule::label()
{
    return cstring(mRule->label);
}

int LuaBmpRule::bmpIndex()
{
    return mRule->bitmapIndex;
}

LuaColor LuaBmpRule::color()
{
    return mRule->color;
}

QStringList LuaBmpRule::tiles()
{
    return mRule->tileChoices;
}

const char *LuaBmpRule::layer()
{
    return cstring(mRule->targetLayer);
}

/////

LuaColor LuaBmpRule::condition()
{
    return mRule->condition;
}

////

LuaMapObject::LuaMapObject(MapObject *orig) :
    mClone(0),
    mOrig(orig)
{
}

LuaMapObject::LuaMapObject(const char *name, const char *type,
                           int x, int y, int width, int height) :
    mClone(new MapObject(QString::fromLatin1(name), QString::fromLatin1(type),
           QPointF(x, y), QSizeF(width, height))),
    mOrig(0)
{

}

const char *LuaMapObject::name()
{
    return mClone ? cstring(mClone->name()) : cstring(mOrig->name());
}

const char *LuaMapObject::type()
{
    return mClone ? cstring(mClone->type()) : cstring(mOrig->type());
}

QRect LuaMapObject::bounds()
{
    return mClone ? mClone->bounds().toAlignedRect() : mOrig->bounds().toAlignedRect();
}

/////

LuaObjectGroup::LuaObjectGroup(ObjectGroup *orig, LuaMap *map) :
    LuaLayer(orig, map),
    mCloneObjectGroup(0),
    mOrig(orig),
    mColor(orig->color())
{
    foreach (MapObject *mo, orig->objects())
        addObject(new LuaMapObject(mo));
}

LuaObjectGroup::LuaObjectGroup(const char *name, int x, int y, int width, int height) :
    LuaLayer(),
    mCloneObjectGroup(new ObjectGroup(QString::fromLatin1(name), x, y, width, height))
{
    mName = mCloneObjectGroup->name();
    mClone = mCloneObjectGroup;
}

LuaObjectGroup::~LuaObjectGroup()
{
    qDeleteAll(mObjects);
}

void LuaObjectGroup::cloned()
{
    LuaLayer::cloned();
    mCloneObjectGroup = mClone->asObjectGroup();
}

void LuaObjectGroup::setColor(LuaColor &color)
{
    mColor = QColor(color.r, color.g, color.b);
}

LuaColor LuaObjectGroup::color()
{
    return LuaColor(mColor.red(), mColor.green(), mColor.blue());
}

void LuaObjectGroup::addObject(LuaMapObject *object)
{
    initClone();
    // FIXME: MainWindow::LuaScript must use these
//    mCloneObjectGroup->addObject(object->mClone ? object->mClone : object->mOrig);
    mObjects += object;
}

QList<LuaMapObject *> LuaObjectGroup::objects()
{
    return mObjects;
}

/////

QStringList LuaBmpAlias::tiles()
{
    return mAlias->tiles;
}

/////

const char *LuaBmpBlend::layer()
{
    return cstring(mBlend->targetLayer);
}

const char *LuaBmpBlend::mainTile()
{
    return cstring(mBlend->mainTile);
}

const char *LuaBmpBlend::blendTile()
{
    return cstring(mBlend->blendTile);
}

LuaBmpBlend::Direction LuaBmpBlend::direction()
{
    return (Direction) mBlend->dir;
}

QStringList LuaBmpBlend::exclude()
{
    return mBlend->ExclusionList;
}
