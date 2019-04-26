/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#include <QApplication>
#include "mainwindow.h"

#ifdef ZOMBOID
#include "assettaskmanager.h"
#include "documentmanager.h"
#include "idletasks.h"
#include "toolmanager.h"
#include "preferences.h"
#include "mapassetmanager.h"
#include "mapimagemanager.h"
#include "mapmanager.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
using namespace Tiled;
using namespace Tiled::Internal;
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setOrganizationName(QLatin1String("TheIndieStone"));
    a.setApplicationName(QLatin1String("PZWorldEd"));
#ifdef BUILD_INFO_VERSION
    a.setApplicationVersion(QLatin1String(AS_STRING(BUILD_INFO_VERSION)));
#else
    a.setApplicationVersion(QLatin1String("0.0.1"));
#endif

#ifdef Q_WS_MAC
    a.setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

    new IdleTasks();
    new AssetTaskManager();
    new TilesetManager(); // before MapManager()
    new MapAssetManager();
    new MapManager();
    new MapImageManager();

    MainWindow w;
    w.show();

    if (!w.InitConfigFiles())
        return 0;

#if 0
    QObject::connect(&a, SIGNAL(fileOpenRequest(QString)),
                     &w, SLOT(openFile(QString)));
#endif

    w.openLastFiles();

#if 1
    int ret = a.exec();

    DocumentManager::deleteInstance();
    ToolManager::deleteInstance();
    Preferences::deleteInstance();
    MapImageManager::deleteInstance();
    MapManager::deleteInstance();
    MapAssetManager::deleteInstance();
    TileMetaInfoMgr::deleteInstance();
    TilesetManager::deleteInstance();

    return ret;
#else
    return a.exec();
#endif
}
