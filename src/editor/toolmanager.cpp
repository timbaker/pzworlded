/*
 * toolmanager.cpp
 * Copyright 2009-2010, Thorbj�rn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
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

#include "toolmanager.h"

#include "basegraphicsscene.h"
#include "scenetools.h"

#include <QAction>
#include <QActionGroup>
#include <QEvent>
#include <QToolBar>

ToolBar::ToolBar(QWidget *parent)
    : QToolBar(parent)
{}

void ToolBar::changeEvent(QEvent *event)
{
    QToolBar::changeEvent(event);
    switch (event->type()) {
    case QEvent::LanguageChange:
        emit languageChanged();
        break;
    default:
        break;
    }
}

ToolManager *ToolManager::mInstance = 0;

ToolManager *ToolManager::instance()
{
    if (!mInstance)
        mInstance = new ToolManager;
    return mInstance;
}

void ToolManager::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

ToolManager::ToolManager()
    : mToolBar(new ToolBar)
    , mActionGroup(new QActionGroup(this))
    , mSelectedTool(0)
    , mPreviouslyDisabledTool(0)
    , mSceneWithTool(0)
{
    mToolBar->setObjectName(QLatin1String("toolsToolBar"));
    mToolBar->setWindowTitle(tr("Tools"));
    // FIXME: QToolBar has no signal 'languageChanged'
    connect(mToolBar, SIGNAL(languageChanged()),
            this, SLOT(languageChanged()));

    mActionGroup->setExclusive(true);
    connect(mActionGroup, &QActionGroup::triggered,
            this, &ToolManager::actionTriggered);
}

ToolManager::~ToolManager()
{
    delete mToolBar;
}

void ToolManager::registerTool(AbstractTool *tool)
{
    QAction *toolAction = new QAction(tool->icon(), tool->name(), this);
    toolAction->setShortcut(tool->shortcut());
    toolAction->setData(QVariant::fromValue<AbstractTool*>(tool));
    toolAction->setCheckable(true);
    QString shortcut = tool->shortcut().toString();
    if (shortcut.length())
        toolAction->setToolTip(QString::fromLatin1("%1 (%2)").arg(tool->name(), shortcut));
    else
        toolAction->setToolTip(tool->name());
    toolAction->setEnabled(tool->isEnabled());
    mActionGroup->addAction(toolAction);
    mToolBar->addAction(toolAction);

    connect(tool, &AbstractTool::enabledChanged,
            this, &ToolManager::toolEnabledChanged);

    // Select the first added tool
    if (!mSelectedTool && tool->isEnabled()) {
        setSelectedTool(tool);
        toolAction->setChecked(true);
    }
}

void ToolManager::addSeparator()
{
    mToolBar->addSeparator();
}

void ToolManager::selectTool(AbstractTool *tool)
{
    if (tool && !tool->isEnabled()) // Refuse to select disabled tools
        return;

    foreach (QAction *action, mActionGroup->actions()) {
        if (action->data().value<AbstractTool*>() == tool) {
            action->trigger();
            return;
        }
    }

    // The given tool was not found. Don't select any tool.
    foreach (QAction *action, mActionGroup->actions())
        action->setChecked(false);
    setSelectedTool(0);
}

void ToolManager::setScene(BaseGraphicsScene *scene)
{
    if (mSceneWithTool) {
        mSceneWithTool->setTool(0);
        mSceneWithTool = 0;
    }

    foreach (QAction *action, mActionGroup->actions()) {
        AbstractTool *tool = action->data().value<AbstractTool*>();
        tool->setScene(scene);
        tool->updateEnabledState();
    }

    // The updateEnabledState() calls above schedule a delayed call to
    // selectEnabledTool.
    selectEnabledTool();

    if (mSelectedTool && scene) {
        mSceneWithTool = scene;
        mSceneWithTool->setTool(mSelectedTool);
    }
}

void ToolManager::beginClearScene(BaseGraphicsScene *scene)
{
    if ((scene == mSceneWithTool) && mSelectedTool)
        mSelectedTool->beginClearScene();
}

void ToolManager::endClearScene(BaseGraphicsScene *scene)
{
    if ((scene == mSceneWithTool) && mSelectedTool)
        mSelectedTool->endClearScene();
}

void ToolManager::actionTriggered(QAction *action)
{
    setSelectedTool(action->data().value<AbstractTool*>());
}

void ToolManager::languageChanged()
{
    // Allow the tools to adapt to the new language
    foreach (QAction *action, mActionGroup->actions()) {
        AbstractTool *tool = action->data().value<AbstractTool*>();
        tool->languageChanged();

        // Update the text, shortcut and tooltip of the action
        action->setText(tool->name());
        action->setShortcut(tool->shortcut());
        action->setToolTip(QString(QLatin1String("%1 (%2)")).arg(
                tool->name(), tool->shortcut().toString()));
    }
}

void ToolManager::toolEnabledChanged(bool enabled)
{
    AbstractTool *tool = qobject_cast<AbstractTool*>(sender());

    foreach (QAction *action, mActionGroup->actions()) {
        if (action->data().value<AbstractTool*>() == tool) {
            action->setEnabled(enabled);
            break;
        }
    }

    // Switch to another tool when the current tool gets disabled. This is done
    // with a delayed call since we first want all the tools to update their
    // enabled state.
    if ((!enabled && tool == mSelectedTool) || (enabled && !mSelectedTool)) {
        QMetaObject::invokeMethod(this, "selectEnabledTool",
                                  Qt::QueuedConnection);
    }
}

void ToolManager::selectEnabledTool()
{
    // Avoid changing tools when it's no longer necessary
    if (mSelectedTool && mSelectedTool->isEnabled())
        return;

    AbstractTool *currentTool = mSelectedTool;

    // Prefer the tool we switched away from last time
    if (mPreviouslyDisabledTool && mPreviouslyDisabledTool->isEnabled())
        selectTool(mPreviouslyDisabledTool);
    else
        selectTool(firstEnabledTool());

    mPreviouslyDisabledTool = currentTool;
}

AbstractTool *ToolManager::firstEnabledTool() const
{
    foreach (QAction *action, mActionGroup->actions())
        if (AbstractTool *tool = action->data().value<AbstractTool*>())
            if (tool->isEnabled())
                return tool;

    return 0;
}

void ToolManager::setSelectedTool(AbstractTool *tool)
{
    if (mSelectedTool == tool)
        return;

    if (mSelectedTool) {
        disconnect(mSelectedTool, &AbstractTool::statusInfoChanged,
                   this, &ToolManager::statusInfoChanged);
    }

    mSelectedTool = tool;
    emit selectedToolChanged(mSelectedTool);

    if (mSceneWithTool)
        mSceneWithTool->setTool(mSelectedTool);

    if (mSelectedTool) {
        emit statusInfoChanged(mSelectedTool->statusInfo());
        connect(mSelectedTool, &AbstractTool::statusInfoChanged,
                this, &ToolManager::statusInfoChanged);
    }
}
