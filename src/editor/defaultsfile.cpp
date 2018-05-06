/*
 * Copyright 2018, Tim Baker <treectrl@users.sf.net>
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

#include "defaultsfile.h"

#include "mainwindow.h"
#include "preferences.h"
#include "simplefile.h"
#include "world.h"
#include "worldcell.h"

#include <QDir>
#include <QMessageBox>

void DefaultsFile::newWorld(World *world)
{
    DefaultsFile file;
    if (!file.read(file.txtPath())) {
        QMessageBox::critical(MainWindow::instance(), tr("It's no good, Jim!"),
                              tr("%1\n(while reading %2)")
                              .arg(file.errorString())
                              .arg(file.txtName()));
        return;
    }

    for (PropertyEnum* propEnum : file.mEnums)
        world->insertPropertyEnum(world->propertyEnums().size(), propEnum);
    for (PropertyDef* propDef : file.mPropertyDefs)
        world->addPropertyDefinition(world->propertyDefinitions().size(), propDef);
    for (PropertyTemplate* propTemplate : file.mTemplates)
        world->addPropertyTemplate(world->propertyTemplates().size(), propTemplate);
    for (ObjectType* objType : file.mObjectTypes)
        world->insertObjectType(world->objectTypes().size(), objType);
    for (WorldObjectGroup* objGroup : file.mObjectGroups) {
        if (objGroup->type()->isNull())
            objGroup->setType(world->nullObjectType());
        world->insertObjectGroup(world->objectGroups().size(), objGroup);
    }

    file.mEnums.clear();
    file.mPropertyDefs.clear();
    file.mTemplates.clear();
    file.mObjectTypes.clear();
    file.mObjectGroups.clear();
}

void DefaultsFile::oldWorld(World *world)
{
    DefaultsFile file;
    if (!file.read(file.txtPath())) {
        QMessageBox::critical(MainWindow::instance(), tr("It's no good, Jim!"),
                              tr("%1\n(while reading %2)")
                              .arg(file.errorString())
                              .arg(file.txtName()));
        return;
    }

    for (PropertyEnum* propEnum : file.mEnums) {
        PropertyEnum* oldEnum = world->propertyEnums().find(propEnum->name());
        if (oldEnum == nullptr) {
            world->insertPropertyEnum(world->propertyEnums().size(), propEnum);
        } else {
            oldEnum->setValues(propEnum->values());
            oldEnum->setMulti(propEnum->isMulti());
        }
    }

    for (PropertyDef* propDef : file.mPropertyDefs) {
        PropertyDef* oldDef = world->propertyDefinition(propDef->mName);
        if (oldDef == nullptr) {
            world->addPropertyDefinition(world->propertyDefinitions().size(), propDef);
        } else {
            oldDef->mDefaultValue = propDef->mDefaultValue;
            oldDef->mDescription = propDef->mDescription;
            if (oldDef->mEnum && propDef->mEnum) {
                if (oldDef->mEnum->name() != propDef->mEnum->name()) {
                    oldDef->mEnum = world->propertyEnums().find(propDef->mEnum->name());
                }
            } else if (propDef->mEnum) { // oldDef==nullptr
                oldDef->mEnum = world->propertyEnums().find(propDef->mEnum->name());
            } else {
                oldDef->mEnum = nullptr;
            }
        }
    }

    for (PropertyTemplate* propTemplate : file.mTemplates) {
        PropertyTemplate* oldTemplate = world->propertyTemplate(propTemplate->mName);
        if (oldTemplate == nullptr) {
            world->addPropertyTemplate(world->propertyTemplates().size(), propTemplate);
        }
    }

    for (PropertyTemplate* propTemplate : file.mTemplates) {
        PropertyTemplate* oldTemplate = world->propertyTemplate(propTemplate->mName);
        if (oldTemplate != propTemplate) {
            oldTemplate->mDescription = propTemplate->mDescription;
            oldTemplate->setProperties(PropertyList());
            oldTemplate->setTemplates(PropertyTemplateList());
            for (Property* prop : propTemplate->properties()) {
                PropertyDef* propDef = world->propertyDefinition(prop->mDefinition->mName);
                oldTemplate->addProperty(oldTemplate->properties().size(), new Property(propDef, prop->mValue));
            }
            for (PropertyTemplate* childTemplate : propTemplate->templates()) {
                oldTemplate->addTemplate(oldTemplate->templates().size(), world->propertyTemplate(childTemplate->mName));
            }
        }
    }

    for (ObjectType* objType : file.mObjectTypes) {
        ObjectType* oldType = world->objectType(objType->name());
        if (oldType == nullptr) {
            world->insertObjectType(world->objectTypes().size(), objType);
        }
    }

    for (WorldObjectGroup* objGroup : file.mObjectGroups) {
        objGroup->setType(world->objectType(objGroup->type()->name()));
        WorldObjectGroup* oldGroup = world->objectGroups().find(objGroup->name());
        if (oldGroup == nullptr) {
            world->insertObjectGroup(world->objectGroups().size(), objGroup);
        } else {
//            oldGroup->setColor(oldGroup->color());
        }
    }

    for (WorldObjectGroup* objGroup : world->objectGroups()) {
        Q_ASSERT(world->objectTypes().indexOf(objGroup->type()) != -1);
    }

    // FIXME: memory leak with existing elements
    file.mEnums.clear();
    file.mPropertyDefs.clear();
    file.mTemplates.clear();
    file.mObjectTypes.clear();
    file.mObjectGroups.clear();
}

DefaultsFile::DefaultsFile()
    : mNullObjectType(new ObjectType())
{

}

DefaultsFile::~DefaultsFile()
{
    delete mNullObjectType;
    qDeleteAll(mEnums);
    qDeleteAll(mPropertyDefs);
    qDeleteAll(mTemplates);
}

QString DefaultsFile::txtName()
{
    return QLatin1String("WorldDefaults.txt");
}

QString DefaultsFile::txtPath()
{
    return Preferences::instance()->appConfigPath(txtName());
}

bool DefaultsFile::read(const QString &fileName)
{
    SimpleFile simpleFile;
    if (!simpleFile.read(fileName)) {
        mError = tr("%1\n(while reading %2)")
                .arg(simpleFile.errorString())
                .arg(QDir::toNativeSeparators(fileName));
        return false;
    }

    for (SimpleFileBlock block : simpleFile.blocks) {
        SimpleFileKeyValue kv;
        if (block.name == QLatin1String("enum")) {
            QStringList values;
            if (block.keyValue("values", kv))
                values = kv.values();
            PropertyEnum* propEnum = new PropertyEnum(block.value("name"), values,
                                                      block.value("multi").compare(QLatin1String("true"), Qt::CaseInsensitive));
            mEnums += propEnum;
        } else if (block.name == QLatin1String("objecttype")) {
            QString name = block.value("name");
            if (name.isEmpty()) {
                mError = tr("Line %1: objecttype name missing")
                        .arg(block.lineNumber);
                return false;
            }
            if (mObjectTypes.find(name) != nullptr) {
                mError = tr("Line %1: Duplicate objecttype '%2'")
                        .arg(block.lineNumber)
                        .arg(name);
                return false;
            }
            ObjectType* objType = new ObjectType(name);
            mObjectTypes += objType;
        } else if (block.name == QLatin1String("objectgroup")) {
            QString name = block.value("name");
            if (name.isEmpty()) {
                mError = tr("Line %1: objectgroup name missing")
                        .arg(block.lineNumber);
                return false;
            }
            if (mObjectGroups.find(name) != nullptr) {
                mError = tr("Line %1: Duplicate objectgroup '%2'")
                        .arg(block.lineNumber)
                        .arg(name);
                return false;
            }
            ObjectType* objType = mNullObjectType;
            if (block.keyValue("defaulttype", kv)) {
                objType = mObjectTypes.find(kv.value);
                if (objType == nullptr) {
                    mError = tr("Line %1: Invalid objecttype '%2'")
                            .arg(kv.lineNumber)
                            .arg(kv.value);
                    return false;
                }
            }
            WorldObjectGroup* objGroup = new WorldObjectGroup(objType, block.value("name"), block.value("color"));
            mObjectGroups += objGroup;
        } else if (block.name == QLatin1String("property")) {
            PropertyEnum* propEnum = nullptr;
            if (block.keyValue("enum", kv)) {
                propEnum = mEnums.find(kv.value);
                if (propEnum == nullptr) {
                    mError = tr("Line %1: Invalid enum '%2'")
                            .arg(kv.lineNumber)
                            .arg(kv.value);
                    return false;
                }
            }
            PropertyDef* def = new PropertyDef(block.value("name"), block.value("default"),
                                               block.value("description"), propEnum);
            mPropertyDefs += def;
        } else if (block.name == QLatin1String("template")) {
            PropertyTemplate* propTemplate = new PropertyTemplate();
            propTemplate->mName = block.value("name");
            propTemplate->mDescription = block.value("description");
            for (SimpleFileBlock child : block.blocks) {
                if (child.name == QLatin1String("property")) {
                    PropertyDef* propDef = mPropertyDefs.findPropertyDef(child.value("name"));
                    if (propDef == nullptr) {
                        mError = tr("Line %1: Invalid property '%2'")
                                .arg(child.lineNumber)
                                .arg(child.value("name"));
                        return false;
                    }
                    if (!propTemplate->canAddProperty(propDef)) {
                        mError = tr("Line %1: Can't add property '%2'")
                                .arg(child.lineNumber)
                                .arg(child.value("name"));
                        return false;
                    }
                    Property* prop = new Property(propDef, child.value("value"));
                    propTemplate->addProperty(propTemplate->properties().size(), prop);
                } else if (child.name == QLatin1String("template")) {
                    PropertyTemplate* propTemplate2 = mTemplates.find(child.value("name"));
                    if (propTemplate2 == nullptr) {
                        mError = tr("Line %1: Invalid template '%2'")
                                .arg(child.lineNumber)
                                .arg(child.value("name"));
                        return false;
                    }
                    if (!propTemplate->canAddTemplate(propTemplate2)) {
                        mError = tr("Line %1: Can't add template '%2'")
                                .arg(child.lineNumber)
                                .arg(child.value("name"));
                        return false;
                    }
                    propTemplate->addTemplate(propTemplate->templates().size(), propTemplate2);
                } else {
                    mError = tr("Line %1: Unknown block name '%2'")
                            .arg(child.lineNumber)
                            .arg(child.name);
                    return false;
                }
            }
            mTemplates += propTemplate;
        } else {
            mError = tr("Line %1: Unknown block name '%2'")
                    .arg(block.lineNumber)
                    .arg(block.name);
            return false;
        }
    }
    return true;
}
