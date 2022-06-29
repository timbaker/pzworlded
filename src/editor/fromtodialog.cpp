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

#include "fromtodialog.h"
#include "ui_fromtodialog.h"

#include "bmptotmx.h"
#include "world.h"
#include "worlddocument.h"

#include <QFileDialog>
#include <QSettings>

static QLatin1String KEY_FILENAME("FromToDialog/FromToFile");
static QLatin1String KEY_DEST_DIR("FromToDialog/DestDir");
static QLatin1String KEY_BACKUP_DIR("FromToDialog/BackupDir");
static QLatin1String KEY_DEST_CHECKED("FromToDialog/DestChecked");
static QLatin1String KEY_BACKUP_CHECKED("FromToDialog/BackupChecked");

FromToDialog::FromToDialog(WorldDocument *doc, QWidget *parent) :
    QDialog(parent),
    mDocument(doc),
    ui(new Ui::FromToDialog)
{
    ui->setupUi(this);

    ui->fileName->setReadOnly(true);

    connect(ui->browse, &QAbstractButton::clicked, this, &FromToDialog::browse);

    QSettings settings;
    QString fileName = settings.value(KEY_FILENAME).toString();
    ui->fileName->setText(QDir::toNativeSeparators(fileName));
    mFileName = fileName;

    QString path = settings.value(KEY_BACKUP_DIR).toString();
    ui->backup->setText(QDir::toNativeSeparators(path));
    ui->backupBox->setChecked(settings.value(KEY_BACKUP_CHECKED, false).toBool());

    path = settings.value(KEY_DEST_DIR).toString();
    ui->dest->setText(QDir::toNativeSeparators(path));
    ui->destBox->setChecked(settings.value(KEY_DEST_CHECKED, false).toBool());
}

FromToDialog::~FromToDialog()
{
    delete ui;
}

QString FromToDialog::backupDir() const
{
    return ui->backupBox->isChecked() ? ui->backup->text() : QString();
}

QString FromToDialog::destDir() const
{
    return ui->destBox->isChecked() ? ui->dest->text() : QString();
}

void FromToDialog::browse()
{
    QString defaultFile = QLatin1String("FromTo.txt");
    QString f = QFileDialog::getOpenFileName(this, tr("Choose FromTo.txt File"),
                                             defaultFile, tr("Text files (*.txt)"));
    if (f.isEmpty())
        return;
    mFileName = f;
    ui->fileName->setText(QDir::toNativeSeparators(f));
}

void FromToDialog::accept()
{
    QSettings settings;
    settings.setValue(KEY_FILENAME, mFileName);
    QDialog::accept();
}
