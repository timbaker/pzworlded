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

#ifndef BMPTOTMXDIALOG_H
#define BMPTOTMXDIALOG_H

#include <QDialog>

class WorldDocument;

namespace Ui {
class BMPToTMXDialog;
}

class BMPToTMXDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit BMPToTMXDialog(WorldDocument *worldDoc, QWidget *parent = 0);
    ~BMPToTMXDialog();

private slots:
    void exportBrowse();
    void rulesBrowse();
    void blendsBrowse();
    void mapbaseBrowse();
    void accept();

private:
    Ui::BMPToTMXDialog *ui;
    WorldDocument *mWorldDoc;
    QString mExportDir;
    QString mRulesFile;
    QString mBlendsFile;
    QString mMapBaseFile;
};

#endif // BMPTOTMXDIALOG_H
