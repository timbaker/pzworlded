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

#ifndef PROPERTYHOLDER_H
#define PROPERTYHOLDER_H

#include <QList>
#include <QSet>
#include <QString>

class PropertyDef
{
public:
    QString mName;
    QString mDefaultValue;
    QString mDescription;
};

class PropertyDefList : public QList<PropertyDef*>
{
public:
    PropertyDef *findPropertyDef(const QString &name) const;
    PropertyDefList sorted() const;
};

class Property
{
public:
    Property(PropertyDef *def, const QString &value)
        : mDefinition(def)
        , mValue(value)
    {
    }

    PropertyDef *mDefinition;
    QString mValue;
    QString mNote;
};

class PropertyList : public QList<Property*>
{
public:
    bool contains(PropertyDef *pd) const;

    void removeAll(PropertyDef *pd);

    PropertyList clone() const;
    PropertyList sorted() const;
};

class PropertyTemplate;
class PropertyTemplateList : public QList<PropertyTemplate*>
{
public:
    PropertyTemplate *find(const QString &name) const;
    PropertyTemplateList sorted() const;
};

class PropertyHolder
{
public:
    PropertyHolder();
    virtual ~PropertyHolder();

    void addTemplate(int index, PropertyTemplate *pt);
    PropertyTemplate *removeTemplate(int index);
    void setTemplates(const PropertyTemplateList &templates);

    void addProperty(int index, Property *p);
    Property *removeProperty(int index);
    QString setPropertyValue(Property *p, const QString &value);
    void setProperties(const PropertyList &properties);

    const PropertyTemplateList &templates() const;
    const PropertyList &properties() const;

    virtual bool canAddTemplate(PropertyTemplate *pt) const;
    virtual bool canAddProperty(PropertyDef *pd) const;

    virtual bool isTemplate() const { return false; }

private:
    PropertyTemplateList mTemplates;
    PropertyList mProperties;

    friend class WorldCellContents;
};

class PropertyTemplate : public PropertyHolder
{
public:
    virtual bool isTemplate() const { return true; }

    bool canAddTemplate(PropertyTemplate *pt) const
    {
        QSet<const PropertyTemplate*> setMine = descendants() << this;
        QSet<const PropertyTemplate*> setTheirs = pt->descendants() << pt;
        return (setMine & setTheirs).isEmpty();
    }

    QSet<const PropertyTemplate*> descendants() const
    {
        QSet<const PropertyTemplate*> set;
        foreach (PropertyTemplate *child, templates()) {
            set += child;
            set += child->descendants();
        }
        return set;
    }

    QString mName;
    QString mDescription;
};

#endif // PROPERTYHOLDER_H
