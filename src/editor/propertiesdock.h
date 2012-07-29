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

#ifndef PROPERTIESDOCK_H
#define PROPERTIESDOCK_H

#include "worldcell.h"

#include <QDockWidget>
#include <QAbstractItemModel>
#include <QTimer>
#include <QTreeView>

class CellDocument;
class Document;
class WorldDocument;

class QLabel;
class QMenu;
class QTextEdit;
class QToolButton;

class PropertiesView;



class PropertiesModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    PropertiesModel(QObject *parent = 0);
    ~PropertiesModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    PropertyTemplate *toTemplate(const QModelIndex &index) const;
    Property *toProperty(const QModelIndex &index) const;
    QWidget *toWidget(const QModelIndex &index) const;

    void setView(PropertiesView *view) { mView = view; }

    void setPropertyHolder(WorldDocument *worldDoc, PropertyHolder *ph);
    void clearPropertyHolder();

//    WorldCell *cell() const { return mPropertyHolder.mCell; }
//    PropertyTemplate *propertyTemplate() const { return mPropertyHolder.mTemplate; }

    QToolButton *addTemplateWidget() { return mAddTemplateWidget; }
    QToolButton *addPropertyWidget() { return mAddPropertyWidget; }

    bool isEditable(const QModelIndex &index) const;
    bool isDeletable(const QModelIndex &index) const;

signals:
    void theModelWasReset();

private slots:
    void setTemplatesMenu();
    void setPropertiesMenu();

    void updateMenus();
    void updateMenusLater();

    void addTemplate(QAction *action);
    void addProperty(QAction *action);

    void propertyDefinitionChanged(PropertyDef *pd);

    void templateChanged(PropertyTemplate *pt);

    void templateAboutToBeRemoved(int index);

    void templateAdded(PropertyHolder *ph, int index);
    void templateAboutToBeRemoved(PropertyHolder *ph, int index);

    void propertyAdded(PropertyHolder *ph, int index);
    void propertyAboutToBeRemoved(PropertyHolder *ph, int index);

    void propertyValueChanged(PropertyHolder *ph, int index);

    void cellContentsAboutToChange(WorldCell *cell);
    void cellContentsChanged(WorldCell *cell);

    void cellObjectAboutToBeRemoved(WorldCell *cell, int index);

private:
    void setModelData();
    void setDocument(WorldDocument *worldDoc);
    void reset();

    class Item
    {
    public:
        enum Header {
            HeaderInvalid,
            HeaderTemplates,
            HeaderProperties
        };

        Item(PropertyHolder *ph)
            : parent(0)
            , pt(0)
            , p(0)
            , header(HeaderInvalid)
            , ph(ph)
            , widget(0)
        {
        }

        Item(Item *parent, int index, PropertyTemplate *pt)
            : parent(parent)
            , pt(pt)
            , p(0)
            , header(HeaderInvalid)
            , ph(0)
            , widget(0)
        {
            parent->children.insert(index, this);
        }
        Item(Item *parent, int index, Property *p)
            : parent(parent)
            , pt(0)
            , p(p)
            , header(HeaderInvalid)
            , ph(0)
            , widget(0)
        {
            parent->children.insert(index, this);
        }
        Item(Item *parent, int index, Header header)
            : parent(parent)
            , pt(0)
            , p(0)
            , header(header)
            , ph(0)
            , widget(0)
        {
            Q_ASSERT(header != HeaderInvalid);
            parent->children.insert(index, this);
        }
        Item(Item *parent, int index, QWidget *widget)
            : parent(parent)
            , pt(0)
            , p(0)
            , header(HeaderInvalid)
            , ph(0)
            , widget(widget)
        {
            parent->children.insert(index, this);
        }

        ~Item() {
            qDeleteAll(children);
        }

        int indexOf()
        {
            return parent->children.indexOf(this);
        }

        bool usesPropertyDef(PropertyDef *pd)
        {
            return (p && (p->mDefinition == pd));
        }

        Item *itemFor(PropertyTemplate *pt)
        {
            foreach (Item *item, children) {
                if (item->pt == pt)
                    return item;
                if (Item *subItem = item->itemFor(pt))
                    return subItem;
            }
            return 0;
        }

        QList<Item*> itemsFor(PropertyHolder *ph)
        {
            QList<Item*> items;
            if (ph == this->ph || ph == pt)
                items += this;
            foreach (Item *child, children)
                items += child->itemsFor(ph);
            return items;
        }

        Item *findChild(Header h)
        {
            foreach (Item *item, children) {
                if (item->header == h)
                    return item;
            }
            return 0;
        }

        Item *findChild(PropertyTemplate *pt)
        {
            foreach (Item *item, children) {
                if (item->pt == pt)
                    return item;
            }
            return 0;
        }

        Item *findChild(Property *p)
        {
            foreach (Item *item, children) {
                if (item->p == p)
                    return item;
            }
            return 0;
        }

        Item *parent;
        QList<Item *> children;
        PropertyTemplate *pt;
        Property *p;
        Header header;
        PropertyHolder *ph;
        QWidget *widget;
    };

    QModelIndex index(Item *item);
    Item *itemFor(PropertyHolder *ph);
    QList<Item*> itemsFor(PropertyHolder *ph);
    bool isPropertyOfCell(Item *item) const;

    void addTemplate(Item *parent, int index, PropertyTemplate *pt);
    void addProperty(Item *parent, int index, Property *p);
    Item *toItem(const QModelIndex &index) const;
    void createAddTemplateWidget();
    void createAddPropertyWidget();
    void redrawProperty(Item *item, PropertyDef *pd);
    void redrawTemplate(Item *item, PropertyTemplate *pt);

    WorldDocument *mWorldDoc;
    PropertyHolder *mPropertyHolder;
    Item *mRootItem;
    QToolButton *mAddTemplateWidget;
    QToolButton *mAddPropertyWidget;
    QMenu *mAddTemplateMenu;
    QMenu *mAddPropertyMenu;
    PropertiesView *mView;
    QTimer mTimer;
};

class PropertiesView : public QTreeView
{
    Q_OBJECT

public:
    PropertiesView(QWidget *parent = 0);

    void mousePressEvent(QMouseEvent *event);

    void setPropertyHolder(WorldDocument *worldDoc, PropertyHolder *holder);
    void clearPropertyHolder();

    PropertiesModel *model() const { return mModel; }

signals:
    void closeItem(const QModelIndex &index);

public:
    PropertiesModel *mModel;
};

class PropertiesDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit PropertiesDock(QWidget *parent = 0);
    
    void setDocument(Document *doc);

protected:
    void changeEvent(QEvent *e);

private:
    void retranslateUi();

private slots:
    void selectedCellsChanged();
    void selectedObjectsChanged();
    void setLabel(PropertyHolder *ph);
    void objectNameChanged(WorldCellObject *obj);
    void selectionChanged();
    void closeItem(const QModelIndex &index);

private:
    PropertiesView *mView;
    CellDocument *mCellDoc;
    WorldDocument *mWorldDoc;
    QModelIndex mAddTemplateIndex;
    QModelIndex mAddPropertyIndex;
    QTextEdit *mDescEdit;
    QLabel *mLabel;
    PropertyHolder *mPropertyHolder;
};

#endif // PROPERTIESDOCK_H
