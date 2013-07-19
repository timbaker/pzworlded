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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>

namespace Ui {
class MainWindow;
}

class Document;
class DocumentManager;
class LayersDock;
class LotsDock;
class LotPackWindow;
class MapsDock;
class ObjectsDock;
class PropertiesDock;
class RoadsDock;
class UndoDock;
class World;
class WorldDocument;
//class WorldScene;
class Zoomable;

class QComboBox;
class QMenu;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    static MainWindow *instance();

    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    DocumentManager *docman() const;

    bool saveFile(const QString &fileName);
    bool openFile(const QString &fileName);

    void openLastFiles();

    bool InitConfigFiles();

protected:
    void closeEvent(QCloseEvent *event);
    void changeEvent(QEvent *event);
    void retranslateUi();

public slots:
    void updateActions();
    void openFile();
    void newWorld();
    void editCell();
    void goToXY();
    void setShowGrid(bool show);

    void documentAdded(Document *doc);
    void documentAboutToClose(int index, Document *doc);
    void currentDocumentTabChanged(int tabIndex);
    void currentDocumentChanged(Document *doc);
    void documentCloseRequested(int tabIndex);

    void updateZoom();

    void selectLevelAbove();
    void selectLevelBelow();

    void zoomIn();
    void zoomOut();
    void zoomNormal();

    bool saveFile();
    bool saveFileAs();
    void closeFile();
    void closeAllFiles();

    void LUAObjectDump();

    void updateWindowTitle();

    void generateLotsAll();
    void generateLotsSelected();

    void BMPToTMXAll();
    void BMPToTMXSelected();

    void resizeWorld();

    void preferencesDialog();
    void objectGroupsDialog();
    void objectTypesDialog();
    void propertyEnumsDialog();
    void properyDefinitionsDialog();
    void templatesDialog();

    void copy();
    void paste();
    void showClipboard();

    void removeRoad();
    void removeBMP();

    void removeLot();
    void removeObject();
    void extractLots();
    void extractObjects();
    void clearCells();
    void clearMapOnly();

    void setStatusBarCoords(int x, int y);

    void aboutToShowCurrentLevelMenu();
    void currentLevelMenuTriggered(QAction *action);

    void aboutToShowObjGrpMenu();
    void objGrpMenuTriggered(QAction *action);

    void lotpackviewer();

    void FromToAll();
    void FromToSelected();

    void BuildingsToPNG();

    void heightMapEditor();
    void setHeightMapAsMesh(bool mesh);
    void setHeightMapAsFlat(bool flat);

private:
    void FromToAux(bool selectedOnly);

    bool confirmSave();
    bool confirmAllSave();

    void writeSettings();
    void readSettings();

    void enableDeveloperFeatures();

    struct ViewHint
    {
        qreal scale;
        int scrollX;
        int scrollY;
        bool valid;
    } mViewHint;
    void setDocumentViewHint(qreal scale, int scrollX, int scrollY)
    {
        mViewHint.scale = scale;
        mViewHint.scrollX = scrollX;
        mViewHint.scrollY = scrollY;
        mViewHint.valid = true;
    }

private:
    Ui::MainWindow *ui;
    UndoDock *mUndoDock;
    LayersDock *mLayersDock;
    LotsDock *mLotsDock;
    MapsDock *mMapsDock;
    ObjectsDock *mObjectsDock;
    PropertiesDock *mPropertiesDock;
#ifdef ROAD_UI
    RoadsDock *mRoadsDock;
#endif
    Document *mCurrentDocument;
    QComboBox *mZoomComboBox;
    QMenu *mCurrentLevelMenu;
    QMenu *mObjectGroupMenu;
    Zoomable *mZoomable;
    QSettings mSettings;
    LotPackWindow *mLotPackWindow;

    static MainWindow *mInstance;
};

#endif // MAINWINDOW_H
