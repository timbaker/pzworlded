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

#include "scriptparammodel.h"

#include "pathworld.h"

#include <QFileInfo>

#if defined(_MSC_VER)
// Hmmmm.  libtiled.dll defines the Properties class as so:
// class TILEDSHARED_EXPORT Properties : public QMap<QString,QString>
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

ScriptParamModel::ScriptParamModel(QObject *parent):
    QAbstractTableModel(parent),
    mRoot(0)
{
}

ScriptParamModel::~ScriptParamModel()
{
    delete mRoot;
}

QModelIndex ScriptParamModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!mRoot)
        return QModelIndex();

    if (!parent.isValid()) {
        if (row < mRoot->children.size())
            return createIndex(row, column, mRoot->children.at(row));
        return QModelIndex();
    }

    if (Item *item = toItem(parent)) {
        if (row < item->children.size())
            return createIndex(row, column, item->children.at(row));
        return QModelIndex();
    }

    return QModelIndex();
}

QModelIndex ScriptParamModel::parent(const QModelIndex &index) const
{
    if (Item *item = toItem(index)) {
        if (item->parent != mRoot) {
            Item *grandParent = item->parent->parent;
            return createIndex(grandParent->children.indexOf(item->parent), 0, item->parent);
        }
    }
    return QModelIndex();
}

int ScriptParamModel::rowCount(const QModelIndex &parent) const
{
    if (!mRoot)
        return 0;
    if (!parent.isValid())
        return mRoot->children.size();
    if (Item *item = toItem(parent))
        return item->children.size();
    return 0;
}

int ScriptParamModel::columnCount(const QModelIndex &parent) const
{
    return 2;
}

QVariant ScriptParamModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (Item *item = toItem(index)) {
            if (item->script) {
                switch (index.column()) {
                case 0: return QFileInfo(item->script->mFileName).fileName();
                case 1: return QVariant();
                }
            } else {
                QString key = item->parent->script->mParams.keys().at(item->pindex);
                switch (index.column()) {
                case 0: return key;
                case 1: return item->parent->script->mParams[key];
                }
            }
        }
    }
    return QVariant();
}

Qt::ItemFlags ScriptParamModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (toItem(index) && !toItem(index)->script && index.column() == 1)
        f |= Qt::ItemIsEditable;
    return f;
}

bool ScriptParamModel::setData(const QModelIndex &index, const QVariant &value,
                              int role)
{
    if (role != Qt::EditRole)
        return false;

    if (index.column() == 1) {
        QString text = value.toString();
        if (Item *item = toItem(index)) {
            if (!item->script) {
                QString key = item->parent->script->mParams.keys().at(item->pindex);
                item->parent->script->mParams[key] = text;
                return true;
            }
        }
    }
    return false;
}

QVariant ScriptParamModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const
{
    static QString sectionHeaders[] = {
        tr("Name"),
        tr("Value")
    };
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section < 2) {
        return sectionHeaders[section];
    }
    return QVariant();
}

ScriptParamModel::Item *ScriptParamModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

void ScriptParamModel::setScripts(const ScriptList &scripts)
{
    delete mRoot;
    mRoot = 0;

    mScripts = scripts;
    if (scripts.size()) {
        mRoot = new Item;
        foreach (WorldScript *ws, scripts) {
            Item *item = new Item(mRoot, ws);
            for (int i = 0; i < ws->mParams.keys().size(); i++)
                new Item(item, i);
        }
    }

    reset();
}

const ScriptList &ScriptParamModel::scripts() const
{
    return mScripts;
}

