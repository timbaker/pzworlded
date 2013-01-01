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
    connect(ui->exportBrowse, SIGNAL(clicked()), SLOT(exportBrowse()));

    // Zombie Spawn Map
    mZombieSpawnMap = settings.zombieSpawnMap;
    ui->spawnEdit->setText(QDir::toNativeSeparators(mZombieSpawnMap));
    connect(ui->spawnBrowse, SIGNAL(clicked()), SLOT(spawnBrowse()));

    // Tilesets.txt
    mMetaTxt = settings.tileMetaInfo;
    if (mMetaTxt.isEmpty()) {
        QString configPath = QDir::homePath() + QLatin1String("/.TileZed");
        QString fileName = configPath + QLatin1String("/Tilesets.txt");
        if (QFileInfo(fileName).exists())
            mMetaTxt = fileName;
    }
    ui->metaInfoEdit->setText(QDir::toNativeSeparators(mMetaTxt));
    connect(ui->metaInfoBrowse, SIGNAL(clicked()), SLOT(metaInfoBrowse()));

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()),
            SLOT(apply()));
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
        ui->exportEdit->setText(mExportDir);
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

void GenerateLotsDialog::metaInfoBrowse()
{
    QString formatString = tr("Text files (*.txt);;All files (*.*)");

    QString initialDir = QDir::homePath() + QLatin1String("/.TileZed");
    if (!mMetaTxt.isEmpty() && QFileInfo(mMetaTxt).exists())
        initialDir = QFileInfo(mMetaTxt).absolutePath();

    QString f = QFileDialog::getOpenFileName(this, tr("Choose the Tilesets.txt file"),
        initialDir, formatString);
    if (!f.isEmpty()) {
        mMetaTxt = f;
        ui->metaInfoEdit->setText(QDir::toNativeSeparators(mMetaTxt));
    }
}

void GenerateLotsDialog::accept()
{
    if (!validate())
        return;

    GenerateLotsSettings settings;
    settings.exportDir = mExportDir;
    settings.zombieSpawnMap = mZombieSpawnMap;
    settings.tileMetaInfo = mMetaTxt;
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
    settings.tileMetaInfo = mMetaTxt;
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

    {
        QFileInfo info(mMetaTxt);
        if (mMetaTxt.isEmpty() || !info.exists()) {
            QMessageBox::warning(this, tr("It's no good, Jim!"),
                                 tr("Please choose the Tilesets.txt file."));
            return false;
        }
    }

    return true;
}
