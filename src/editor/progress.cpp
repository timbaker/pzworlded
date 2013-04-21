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

#include "progress.h"

#include "mainwindow.h"

#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>

Progress *Progress::mInstance = 0;

Progress *Progress::instance()
{
    if (!mInstance)
        mInstance = new Progress();
    return mInstance;
}

Progress::Progress()
    : mMainWindow(0)
    , mDialog(0)
    , mDepth(0)
{
    mInstance = this;
}

void Progress::setMainWindow(QWidget *mainWindow)
{
    if (mMainWindow) {
        mDialog->setParent(mMainWindow = mainWindow);
        mDialog->setWindowFlags(Qt::CustomizeWindowHint | Qt::Dialog);
        return;
    }
    mMainWindow = mainWindow;
    mDialog = new QDialog(mainWindow);
    QVBoxLayout *layout = new QVBoxLayout();
    mLabel = new QLabel();
    mLabel->setMinimumSize(300, 20);
    layout->addWidget(mLabel);
    mDialog->setWindowModality(Qt::WindowModal);
    mDialog->setLayout(layout);
    mDialog->setWindowFlags(Qt::CustomizeWindowHint | Qt::Dialog);
}

void Progress::begin(const QString &text)
{
    mLabel->setText(text);
    if (mDepth++ == 0)
        mDialog->show();
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void Progress::update(const QString &text)
{
    Q_ASSERT(mDepth > 0);
    mLabel->setText(text);
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void Progress::end()
{
    Q_ASSERT(mDepth > 0);
//    mDialog->setValue(mDialog->maximum()); // hides dialog!
    if (--mDepth == 0)
        mDialog->hide();
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

