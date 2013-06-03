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

#ifndef ALIASFIXUPDIALOG_H
#define ALIASFIXUPDIALOG_H

#include <QDialog>

namespace Ui {
class FromToDialog;
}

class WorldDocument;

class FromToDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit FromToDialog(WorldDocument *doc, QWidget *parent = 0);
    ~FromToDialog();

    QString rulesFile() const
    { return mFileName; }

    QString backupDir() const;
    QString destDir() const;
    
private slots:
    void browse();

    void accept();

private:
    WorldDocument *mDocument;
    QString mFileName;
    Ui::FromToDialog *ui;
};

#endif // ALIASFIXUPDIALOG_H
