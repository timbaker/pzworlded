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
#include <QSettings>

static const QString KEY_EXPORT_DIRECTORIES = QStringLiteral("GenerateLotsDialog/ExportDirectories");
static const QString KEY_SPAWNMAP_DIRECTORIES = QStringLiteral("GenerateLotsDialog/SpawnMapDirectories");
static const QString KEY_TILEDEF_DIRECTORIES = QStringLiteral("GenerateLotsDialog/TileDefDirectories");

GenerateLotsDialog::GenerateLotsDialog(WorldDocument *worldDoc, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GenerateLotsDialog),
    mWorldDoc(worldDoc)
{
    ui->setupUi(this);

    const GenerateLotsSettings &settings = mWorldDoc->world()->getGenerateLotsSettings();

    QSettings qSettings;
    QStringList directories = qSettings.value(KEY_EXPORT_DIRECTORIES).toStringList();
    ui->exportEdit->addItems(directories);

    directories = qSettings.value(KEY_SPAWNMAP_DIRECTORIES).toStringList();
    ui->spawnEdit->addItems(directories);

    directories = qSettings.value(KEY_TILEDEF_DIRECTORIES).toStringList();
    ui->tiledefEdit->addItems(directories);

    // Export directory
    mExportDir = QDir::toNativeSeparators(settings.exportDir);
    if (mExportDir.isEmpty() == false) {
        addComboItemIfAbsent(ui->exportEdit, mExportDir);
        ui->exportEdit->setCurrentText(mExportDir);
    }
    connect(ui->exportEdit, &QComboBox::currentTextChanged, this, &GenerateLotsDialog::exportChanged);
    connect(ui->exportBrowse, &QAbstractButton::clicked, this, &GenerateLotsDialog::exportBrowse);

    // Zombie Spawn Map
    mZombieSpawnMap = QDir::toNativeSeparators(settings.zombieSpawnMap);
    if (mZombieSpawnMap.isEmpty() == false) {
        addComboItemIfAbsent(ui->spawnEdit, mZombieSpawnMap);
        ui->spawnEdit->setCurrentText(mZombieSpawnMap);
    }
    connect(ui->spawnEdit, &QComboBox::currentTextChanged, this, &GenerateLotsDialog::spawnChanged);
    connect(ui->spawnBrowse, &QAbstractButton::clicked, this, &GenerateLotsDialog::spawnBrowse);

    // TileDef folder
    mTileDefFolder = QDir::toNativeSeparators(settings.tileDefFolder);
    if (mTileDefFolder.isEmpty() == false) {
        addComboItemIfAbsent(ui->tiledefEdit, mTileDefFolder);
        ui->tiledefEdit->setCurrentText(mTileDefFolder);
    }
    connect(ui->tiledefEdit, &QComboBox::currentTextChanged, this, &GenerateLotsDialog::tileDefChanged);
    connect(ui->tiledefBrowse, &QAbstractButton::clicked, this, &GenerateLotsDialog::tileDefBrowse);

    // World origin
    ui->xOrigin->setValue(settings.worldOrigin.x());
    ui->yOrigin->setValue(settings.worldOrigin.y());

    // Number of threads
    ui->numThreadsSlider->setMinimum(1);
    ui->numThreadsSlider->setMaximum(10);
    ui->numThreadsSlider->setValue(settings.numberOfThreads);

    ui->exportBinary->setChecked(settings.exportBinary);
    ui->exportText->setChecked(settings.exportText);

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
        ui->exportEdit->currentText());
    if (!f.isEmpty()) {
        mExportDir = QDir::toNativeSeparators(f);
        addComboItemIfAbsent(ui->exportEdit, mExportDir);
        ui->exportEdit->setCurrentText(mExportDir);
    }
}

void GenerateLotsDialog::exportChanged(const QString &text)
{
    mExportDir = text;
}

void GenerateLotsDialog::spawnChanged(const QString &text)
{
    mZombieSpawnMap = text;
}

void GenerateLotsDialog::tileDefChanged(const QString &text)
{
    mTileDefFolder = text;
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
    if (QFileInfo::exists(mZombieSpawnMap))
        initialDir = QFileInfo(mZombieSpawnMap).absolutePath();

    QString f = QFileDialog::getOpenFileName(this, tr("Choose the Zombie Spawn Map image"),
        initialDir, formatString);
    if (!f.isEmpty()) {
        mZombieSpawnMap = QDir::toNativeSeparators(f);
        addComboItemIfAbsent(ui->spawnEdit, mZombieSpawnMap);
        ui->spawnEdit->setCurrentText(mZombieSpawnMap);
    }
}

void GenerateLotsDialog::tileDefBrowse()
{
    QString f = QFileDialog::getExistingDirectory(this, tr("Choose the .tiles Folder"),
        ui->tiledefEdit->currentText());
    if (!f.isEmpty()) {
        mTileDefFolder = QDir::toNativeSeparators(f);
        addComboItemIfAbsent(ui->tiledefEdit, mTileDefFolder);
        ui->tiledefEdit->setCurrentText(mTileDefFolder);
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
    settings.numberOfThreads = ui->numThreadsSlider->value();
    settings.exportBinary = ui->exportBinary->isChecked();
    settings.exportText = ui->exportText->isChecked();
    if (settings != mWorldDoc->world()->getGenerateLotsSettings()) {
        mWorldDoc->changeGenerateLotsSettings(settings);
    }
    QSettings qSettings;
    qSettings.setValue(KEY_EXPORT_DIRECTORIES, comboboxStringList(ui->exportEdit));
    qSettings.setValue(KEY_SPAWNMAP_DIRECTORIES, comboboxStringList(ui->spawnEdit));
    qSettings.setValue(KEY_TILEDEF_DIRECTORIES, comboboxStringList(ui->tiledefEdit));

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
    settings.numberOfThreads = ui->numThreadsSlider->value();
    settings.exportBinary = ui->exportBinary->isChecked();
    settings.exportText = ui->exportText->isChecked();
    if (settings != mWorldDoc->world()->getGenerateLotsSettings()) {
        mWorldDoc->changeGenerateLotsSettings(settings);
    }
    QSettings qSettings;
    qSettings.setValue(KEY_EXPORT_DIRECTORIES, comboboxStringList(ui->exportEdit));
    qSettings.setValue(KEY_SPAWNMAP_DIRECTORIES, comboboxStringList(ui->spawnEdit));
    qSettings.setValue(KEY_TILEDEF_DIRECTORIES, comboboxStringList(ui->tiledefEdit));

    QDialog::reject();
}

void GenerateLotsDialog::addComboItemIfAbsent(QComboBox *comboBox, const QString &text)
{
    int index = comboBox->findText(text);
    if (index == -1) {
        comboBox->insertItem(0, text);
    }
}

QStringList GenerateLotsDialog::comboboxStringList(QComboBox *comboBox) const
{
    QStringList items;
    for (int i = 0; i < comboBox->count(); i++) {
        items << comboBox->itemText(i);
    }
    return items;
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
            !QFileInfo::exists(mTileDefFolder + QLatin1String("/newtiledefinitions.tiles"))) {
        QMessageBox::warning(this, tr("It's no good, Jim!"),
                             tr("Please choose the directory containing newtiledefinitions.tiles."));
        return false;
    }

    return true;
}
