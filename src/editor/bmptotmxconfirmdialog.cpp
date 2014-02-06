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

#include "bmptotmxconfirmdialog.h"
#include "ui_bmptotmxconfirmdialog.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>

BMPToTMXConfirmDialog::BMPToTMXConfirmDialog(const QStringList &fileNames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BMPToTMXConfirmDialog)
{
    ui->setupUi(this);

    foreach (QString fileName, fileNames) {
        QTreeWidgetItem *parent = itemForDirectory(QFileInfo(fileName).absolutePath());
        QTreeWidgetItem *item = new QTreeWidgetItem(parent, QStringList() << QFileInfo(fileName).fileName());
        ui->treeWidget->addTopLevelItem(item);
    }

    ui->treeWidget->expandAll();
}

BMPToTMXConfirmDialog::~BMPToTMXConfirmDialog()
{
    delete ui;
}

void BMPToTMXConfirmDialog::updateExisting()
{
    ui->label->setText(tr("The files in the list below already exist.\nThe pixels in the maps will be replaced by pixels from the BMP images."));
}

// C:/
//   Foo/
//     Bar/
//       hello.tmx

QTreeWidgetItem *BMPToTMXConfirmDialog::itemForDirectory(const QString &path)
{
    QString parentPath = QFileInfo(path).absolutePath();
    QTreeWidgetItem *parentItem;
    QString selfName;
    if (parentPath == path) {
        parentItem = ui->treeWidget->invisibleRootItem();
        selfName = parentPath;
    } else {
        parentItem = itemForDirectory(parentPath);
        selfName = QFileInfo(path).fileName() + QLatin1Char('/');
    }

    for (int i = 0; i < parentItem->childCount(); i++) {
        if (parentItem->child(i)->text(0) == selfName)
            return parentItem->child(i);
    }

    return new QTreeWidgetItem(parentItem, QStringList() << selfName);
}
