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
#include <QStringList>

class World;

class PropertyEnum
{
public:
    PropertyEnum(const QString &name, const QStringList &values, bool multi);
    PropertyEnum(const PropertyEnum *other);

    void setName(const QString &name)
    { mName = name; }
    const QString &name() const
    { return mName; }

    void setValues(const QStringList &values)
    { mValues = values; }
    const QStringList &values() const
    { return mValues; }

    void setMulti(bool multi)
    { mMulti = multi; }
    bool isMulti() const
    { return mMulti; }

    bool operator ==(const PropertyEnum &other) const;
    bool operator !=(const PropertyEnum &other) const
    { return !(*this == other); }

private:
    QString mName;
    QStringList mValues;
    bool mMulti;
};


class PropertyEnumList : public QList<PropertyEnum*>
{
public:
    PropertyEnum *find(const QString &name) const;
    PropertyEnumList sorted() const;
};

class PropertyDef
{
public:
    PropertyDef(const QString &name, const QString &defaultValue,
                const QString &description, PropertyEnum *pe);
    PropertyDef(World *world, PropertyDef *other);

    bool operator ==(const PropertyDef &other) const;
    bool operator !=(const PropertyDef &other) const
    { return !(*this == other); }

    QString mName;
    QString mDefaultValue;
    QString mDescription;
    PropertyEnum *mEnum;
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

    Property(World *world, Property *other);

    bool operator ==(const Property &other) const;
    bool operator !=(const Property &other) const
    { return !(*this == other); }

    PropertyDef *mDefinition;
    QString mValue;
    QString mNote;
};

class PropertyList : public QList<Property*>
{
public:
    Property *find(PropertyDef *pd) const;

    bool contains(PropertyDef *pd) const;

    void removeAll(PropertyDef *pd);

    PropertyList clone() const;
    PropertyList sorted() const;

    bool operator ==(const PropertyList &other) const;
    bool operator !=(const PropertyList &other) const
    { return !(*this == other); }
};

class PropertyTemplate;
class PropertyTemplateList : public QList<PropertyTemplate*>
{
public:
    PropertyTemplate *find(const QString &name) const;
    PropertyTemplateList sorted() const;

    bool operator ==(const PropertyTemplateList &other) const;
    bool operator !=(const PropertyTemplateList &other) const
    { return !(*this == other); }
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

    bool operator ==(const PropertyHolder &other) const;
    bool operator !=(const PropertyHolder &other) const
    { return !(*this == other); }

    bool usesTemplate(PropertyTemplate *pt) const;

private:
    PropertyTemplateList mTemplates;
    PropertyList mProperties;

    friend class WorldCellContents;
};

class PropertyTemplate : public PropertyHolder
{
public:
    PropertyTemplate() {}
    PropertyTemplate(World *world, PropertyTemplate *other);

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

    bool operator ==(const PropertyTemplate &other) const;

    QString mName;
    QString mDescription;
};

#endif // PROPERTYHOLDER_H
