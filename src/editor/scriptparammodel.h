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


#ifndef SCRIPTPARAMMODEL_H
#define SCRIPTPARAMMODEL_H

#include <QAbstractItemModel>
#include <QList>
#include <QString>

#include "global.h"

class ScriptParamModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    ScriptParamModel(QObject *parent = 0);
    ~ScriptParamModel();

    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QModelIndex parent(const QModelIndex &index) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole);


    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    void setScripts(const ScriptList &scripts);
    const ScriptList &scripts() const;

private:
    struct Item
    {
        Item() :
            parent(0),
            script(0),
            pindex(-1)
        {
        }

        Item(Item *parent, WorldScript *script) :
            parent(parent),
            script(script),
            pindex(-1)
        {
            parent->children += this;
        }

        Item(Item *parent, int pindex) :
            parent(parent),
            script(0),
            pindex(pindex)
        {
            parent->children += this;
        }

        Item *parent;
        QList<Item*> children;
        WorldScript *script;
        int pindex;
    };

    Item *toItem(const QModelIndex &index) const;

    ScriptList mScripts;
    Item *mRoot;
};

#endif // SCRIPTPARAMMODEL_H
