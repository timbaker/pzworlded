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

#include "properties.h"

/////

PropertyDef *PropertyDefList::findPropertyDef(const QString &name) const
{
    const_iterator it = constBegin();
    while (it != constEnd()) {
        if ((*it)->mName == name)
            return *it;
        it++;
    }
    return 0;
}

PropertyDefList PropertyDefList::sorted() const
{
    PropertyDefList sorted;
    const_iterator it = constBegin();
    while (it != constEnd()) {
        PropertyDef *pd = *it;
        int index = 0;
        foreach (PropertyDef *pd2, sorted) {
            if (pd2->mName > pd->mName)
                break;
            ++index;
        }
        sorted.insert(index, pd);
        ++it;
    }
    return sorted;
}

/////

bool PropertyList::contains(PropertyDef *pd) const
{
    const_iterator it = constBegin();
    while (it != constEnd()) {
        if ((*it)->mDefinition == pd)
            return true;
        it++;
    }
    return false;
}

void PropertyList::removeAll(PropertyDef *pd)
{
    for (int i = size() - 1; i >= 0; --i) {
        if (at(i)->mDefinition == pd)
            removeAt(i);
    }
}

PropertyList PropertyList::clone() const
{
    PropertyList copy;
    const_iterator it = constBegin();
    while (it != constEnd()) {
        copy += new Property((*it)->mDefinition, (*it)->mValue);
        it++;
    }
    return copy;
}

PropertyList PropertyList::sorted() const
{
    PropertyList sorted;
    const_iterator it = constBegin();
    while (it != constEnd()) {
        Property *p = *it;
        int index = 0;
        foreach (Property *p2, sorted) {
            if (p2->mDefinition->mName > p->mDefinition->mName)
                break;
            ++index;
        }
        sorted.insert(index, p);
        ++it;
    }
    return sorted;
}

/////

PropertyTemplate *PropertyTemplateList::find(const QString &name) const
{
    const_iterator it = constBegin();
    while (it != constEnd()) {
        if ((*it)->mName == name)
            return *it;
        it++;
    }
    return 0;
}

PropertyTemplateList PropertyTemplateList::sorted() const
{
    PropertyTemplateList sorted;
    const_iterator it = constBegin();
    while (it != constEnd()) {
        PropertyTemplate *pt = *it;
        int index = 0;
        foreach (PropertyTemplate *pt2, sorted) {
            if (pt2->mName > pt->mName)
                break;
            ++index;
        }
        sorted.insert(index, pt);
        it++;
    }
    return sorted;
}

/////

PropertyHolder::PropertyHolder()
{
}

PropertyHolder::~PropertyHolder()
{
    qDeleteAll(mProperties);
}

QString PropertyHolder::setPropertyValue(Property *p, const QString &value)
{
    QString oldValue = p->mValue;
    p->mValue = value;
    return oldValue;
}

void PropertyHolder::setProperties(const PropertyList &properties)
{
    qDeleteAll(mProperties);
    mProperties = properties;
}

const PropertyTemplateList &PropertyHolder::templates() const
{
    return mTemplates;
}

const PropertyList &PropertyHolder::properties() const
{
    return mProperties;
}

bool PropertyHolder::canAddTemplate(PropertyTemplate *pt) const
{
    // Can add to a cell if it doesn't have it already.
    // Templates override this method to prevent recursion.
    return !mTemplates.contains(pt);
}

bool PropertyHolder::canAddProperty(PropertyDef *pd) const
{
    return !mProperties.contains(pd);
}

void PropertyHolder::addTemplate(int index, PropertyTemplate *pt)
{
    mTemplates.insert(index, pt);
}

void PropertyHolder::addProperty(int index, Property *p)
{
    mProperties.insert(index, p);
}

PropertyTemplate *PropertyHolder::removeTemplate(int index)
{
    return mTemplates.takeAt(index);
}

void PropertyHolder::setTemplates(const PropertyTemplateList &templates)
{
    mTemplates = templates;
}

Property *PropertyHolder::removeProperty(int index)
{
    return mProperties.takeAt(index);
}
