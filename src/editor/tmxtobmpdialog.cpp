#include "tmxtobmpdialog.h"
#include "ui_tmxtobmpdialog.h"

#include "world.h"
#include "worlddocument.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>

TMXToBMPDialog::TMXToBMPDialog(WorldDocument *worldDoc, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TMXToBMPDialog),
    mWorldDoc(worldDoc)
{
    ui->setupUi(this);

    const TMXToBMPSettings &settings = worldDoc->world()->getTMXToBMPSettings();

    mMainFile = settings.mainFile;
    ui->mainEdit->setText(QDir::toNativeSeparators(mMainFile));
    connect(ui->mainBrowse, SIGNAL(clicked()), SLOT(mainBrowse()));

    mVegFile = settings.vegetationFile;
    ui->vegEdit->setText(QDir::toNativeSeparators(mVegFile));
    connect(ui->vegBrowse, SIGNAL(clicked()), SLOT(vegetationBrowse()));

    mBldgFile = settings.buildingsFile;
    ui->buildingEdit->setText(QDir::toNativeSeparators(mBldgFile));
    connect(ui->buildingBrowse, SIGNAL(clicked()), SLOT(buildingBrowse()));

    ui->mainGroupBox->setChecked(settings.doMain);
    ui->vegGroupBox->setChecked(settings.doVegetation);
    ui->buildingGroupBox->setChecked(settings.doBuildings);

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()),
            SLOT(apply()));
}

TMXToBMPDialog::~TMXToBMPDialog()
{
    delete ui;
}

void TMXToBMPDialog::mainBrowse()
{
    QString f = QFileDialog::getSaveFileName(this, tr("Save Main Image As"),
        ui->mainEdit->text(), QLatin1String("BMP Files (*.bmp);;PNG Files (*.png)"));
    if (!f.isEmpty()) {
        mMainFile = f;
        ui->mainEdit->setText(QDir::toNativeSeparators(mMainFile));
    }
}

void TMXToBMPDialog::vegetationBrowse()
{
    QString f = QFileDialog::getSaveFileName(this, tr("Save Vegetation Image As"),
        ui->vegEdit->text(), QLatin1String("BMP Files (*.bmp);;PNG Files (*.png)"));
    if (!f.isEmpty()) {
        mVegFile = f;
        ui->vegEdit->setText(QDir::toNativeSeparators(mVegFile));
    }
}

void TMXToBMPDialog::buildingBrowse()
{
    QString f = QFileDialog::getSaveFileName(this, tr("Save Buildings Image As"),
        ui->buildingEdit->text(), QLatin1String("BMP Files (*.bmp);;PNG Files (*.png)"));
    if (!f.isEmpty()) {
        mBldgFile = f;
        ui->buildingEdit->setText(QDir::toNativeSeparators(mBldgFile));
    }
}

void TMXToBMPDialog::accept()
{
    if (!validate())
        return;

    toSettings();

    QDialog::accept();
}

void TMXToBMPDialog::apply()
{
    if (!validate())
        return;

    toSettings();

    QDialog::reject();
}

bool TMXToBMPDialog::validate()
{
    QFileInfo info(mMainFile);
    if (ui->mainGroupBox->isChecked() && (mMainFile.isEmpty() || !info.absoluteDir().exists())) {
        QMessageBox::warning(this, tr("It's no good, Jim!"),
                             tr("No 'main' image specified or parent directory doesn't exist."));
        return false;
    }

    QFileInfo info2(mVegFile);
    if (ui->vegGroupBox->isChecked() && (mVegFile.isEmpty() || !info2.absoluteDir().exists())) {
        QMessageBox::warning(this, tr("It's no good, Jim!"),
                             tr("No 'vegetation' image specified or parent directory doesn't exist."));
        return false;
    }

    QFileInfo info3(mBldgFile);
    if (ui->buildingGroupBox->isChecked() && (mBldgFile.isEmpty() || !info3.absoluteDir().exists())) {
        QMessageBox::warning(this, tr("It's no good, Jim!"),
                             tr("No 'buildings' image specified or parent directory doesn't exist."));
        return false;
    }

    return true;
}

void TMXToBMPDialog::toSettings()
{
    TMXToBMPSettings settings;
    settings.mainFile = mMainFile;
    settings.vegetationFile = mVegFile;
    settings.buildingsFile = mBldgFile;
    settings.doMain = ui->mainGroupBox->isChecked();
    settings.doVegetation = ui->vegGroupBox->isChecked();
    settings.doBuildings = ui->buildingGroupBox->isChecked();
    if (settings != mWorldDoc->world()->getTMXToBMPSettings())
        mWorldDoc->changeTMXToBMPSettings(settings);
}
