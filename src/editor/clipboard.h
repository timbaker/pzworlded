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

#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include <QObject>

class World;
class WorldCellContents;

class Clipboard : public QObject
{
    Q_OBJECT
public:
    static Clipboard *instance();
    static void deleteInstance();

    bool isEmpty() const;

    void setWorld(World *world);
    World *world() const { return mWorld; }

    const QList<WorldCellContents*> &cellsInClipboard() const;
    const int cellsInClipboardCount() const;

signals:
    void clipboardChanged();
    
public slots:
    
private:
    Q_DISABLE_COPY(Clipboard)
    static Clipboard *mInstance;

    explicit Clipboard(QObject *parent = 0);
    ~Clipboard();

    World *mWorld;
    QList<WorldCellContents*> mCellContents;
};

#endif // CLIPBOARD_H
