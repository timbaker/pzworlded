#include "generatelotsdialog.h"
#include "ui_generatelotsdialog.h"

#include "preferences.h"
#include "world.h"
#include "worlddocument.h"

#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QImageReader>
#include <QMessageBox>
#include <QPushButton>

GenerateLotsDialog::GenerateLotsDialog(WorldDocument *worldDoc, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GenerateLotsDialog),
    mWorldDoc(worldDoc)
{
    ui->setupUi(this);

    const GenerateLotsSettings &settings = mWorldDoc->world()->getGenerateLotsSettings();

    // Export directory
    mExportDir = settings.exportDir;
    ui->exportEdit->setText(QDir::toNativeSeparators(mExportDir));
    connect(ui->exportBrowse, &QAbstractButton::clicked, this, &GenerateLotsDialog::exportBrowse);

    // Zombie Spawn Map
    mZombieSpawnMap = settings.zombieSpawnMap;
    ui->spawnEdit->setText(QDir::toNativeSeparators(mZombieSpawnMap));
    connect(ui->spawnBrowse, &QAbstractButton::clicked, this, &GenerateLotsDialog::spawnBrowse);

    // TileDef folder
    mTileDefFolder = settings.tileDefFolder;
    ui->tiledefEdit->setText(QDir::toNativeSeparators(mTileDefFolder));
    connect(ui->tiledefBrowse, &QAbstractButton::clicked, this, &GenerateLotsDialog::tileDefBrowse);

    // World origin
    ui->xOrigin->setValue(settings.worldOrigin.x());
    ui->yOrigin->setValue(settings.worldOrigin.y());

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked,
            this, &GenerateLotsDialog::apply);
}

GenerateLotsDialog::~GenerateLotsDialog()
{
    delete ui;
}

void GenerateLotsDialog::exportBrowse()
{
    QString f = QFileDialog::getExistingDirectory(this, tr("Choose the .lot Folder"),
        ui->exportEdit->text());
    if (!f.isEmpty()) {
        mExportDir = f;
        ui->exportEdit->setText(QDir::toNativeSeparators(mExportDir));
    }
}

void GenerateLotsDialog::spawnBrowse()
{
    QStringList formats;
    foreach (QByteArray format, QImageReader::supportedImageFormats())
        if (format.toLower() == format)
            formats.append(QString::fromUtf8(QByteArray("*." + format)));
    QString formatString = tr("Image files (%1)").arg(formats.join(QLatin1String(" ")));
    formatString += tr(";;All files (*.*)");

    QString initialDir = QFileInfo(mWorldDoc->fileName()).absolutePath();
    if (QFileInfo(mZombieSpawnMap).exists())
        initialDir = QFileInfo(mZombieSpawnMap).absolutePath();

    QString f = QFileDialog::getOpenFileName(this, tr("Choose the Zombie Spawn Map image"),
        initialDir, formatString);
    if (!f.isEmpty()) {
        mZombieSpawnMap = f;
        ui->spawnEdit->setText(QDir::toNativeSeparators(mZombieSpawnMap));
    }
}

void GenerateLotsDialog::tileDefBrowse()
{
    QString f = QFileDialog::getExistingDirectory(this, tr("Choose the .tiles Folder"),
        ui->tiledefEdit->text());
    if (!f.isEmpty()) {
        mTileDefFolder = f;
        ui->tiledefEdit->setText(QDir::toNativeSeparators(mTileDefFolder));
    }
}

void GenerateLotsDialog::accept()
{
    if (!validate())
        return;

    GenerateLotsSettings settings;
    settings.exportDir = mExportDir;
    settings.zombieSpawnMap = mZombieSpawnMap;
    settings.tileDefFolder = mTileDefFolder;
    settings.worldOrigin = QPoint(ui->xOrigin->value(), ui->yOrigin->value());
    if (settings != mWorldDoc->world()->getGenerateLotsSettings())
        mWorldDoc->changeGenerateLotsSettings(settings);

    QDialog::accept();
}

void GenerateLotsDialog::apply()
{
    if (!validate())
        return;

    GenerateLotsSettings settings;
    settings.exportDir = mExportDir;
    settings.zombieSpawnMap = mZombieSpawnMap;
    settings.tileDefFolder = mTileDefFolder;
    settings.worldOrigin = QPoint(ui->xOrigin->value(), ui->yOrigin->value());
    if (settings != mWorldDoc->world()->getGenerateLotsSettings())
        mWorldDoc->changeGenerateLotsSettings(settings);

    QDialog::reject();
}

bool GenerateLotsDialog::validate()
{
    QDir dir(mExportDir);
    if (mExportDir.isEmpty() || !dir.exists()) {
        QMessageBox::warning(this, tr("It's no good, Jim!"),
                             tr("Please choose a valid directory to save the .lot files in."));
        return false;
    }
    QFileInfo info(mZombieSpawnMap);
    if (mZombieSpawnMap.isEmpty() || !info.exists()) {
        QMessageBox::warning(this, tr("It's no good, Jim!"),
                             tr("Please choose a Zombie Spawn Map image file."));
        return false;
    }
    QDir dir2(mTileDefFolder);
    if (mTileDefFolder.isEmpty() || !dir2.exists() ||
            !QFileInfo(mTileDefFolder + QLatin1String("/newtiledefinitions.tiles")).exists()) {
        QMessageBox::warning(this, tr("It's no good, Jim!"),
                             tr("Please choose the directory containing newtiledefinitions.tiles."));
        return false;
    }

    return true;
}
