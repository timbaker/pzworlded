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

#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>

class WorldDocument;

class QListWidgetItem;

namespace Ui {
class PreferencesDialog;
}

class PreferencesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PreferencesDialog(WorldDocument *worldDoc, QWidget *parent = 0);
    
private:
    void setPathsList();
    void synchPathsButtons();

    int selectedPath();
    
private slots:
    void gridColorChanged(const QColor &gridColor);

    void addPath();
    void removePath();
    void movePathUp();
    void movePathDown();

    void pathSelectionChanged();

    void lotsDirectoryBrowse();

    void dialogAccepted();
    
private:
    Ui::PreferencesDialog *ui;
    WorldDocument *mWorldDoc;
    QStringList mSearchPaths;
    QColor mGridColor;
    QString mLotsDirectory;
};

#endif // PREFERENCESDIALOG_H
