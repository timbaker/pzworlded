#include "generatelotsdialog.h"
#include "ui_generatelotsdialog.h"

#include "world.h"
#include "worlddocument.h"

#include <QDir>
#include <QFileDialog>

GenerateLotsDialog::GenerateLotsDialog(WorldDocument *worldDoc, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GenerateLotsDialog),
    mWorldDoc(worldDoc)
{
    ui->setupUi(this);

    const GenerateLotsSettings &settings = mWorldDoc->world()->getGenerateLotsSettings();

    // Export directory
    mExportDir = settings.exportDir;
    ui->exportEdit->setText(mExportDir);
    connect(ui->exportBrowse, SIGNAL(clicked()), SLOT(exportBrowse()));
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

void GenerateLotsDialog::accept()
{
    QDir dir(mExportDir);
    if (mExportDir.isEmpty() || !dir.exists()) {
        return;
    }
    GenerateLotsSettings settings;
    settings.exportDir = mExportDir;
    if (settings != mWorldDoc->world()->getGenerateLotsSettings())
        mWorldDoc->changeGenerateLotsSettings(settings);

    QDialog::accept();
}
