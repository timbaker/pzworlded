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

    mBldgFile = settings.buildingsFile;
    ui->buildingEdit->setText(QDir::toNativeSeparators(mBldgFile));
    connect(ui->buildingBrowse, &QAbstractButton::clicked, this, &TMXToBMPDialog::buildingBrowse);

    ui->updateMain->setChecked(settings.doMain);
    ui->updateVeg->setChecked(settings.doVegetation);
    ui->buildingGroupBox->setChecked(settings.doBuildings);

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked,
            this, &TMXToBMPDialog::apply);
}

TMXToBMPDialog::~TMXToBMPDialog()
{
    delete ui;
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
    settings.buildingsFile = mBldgFile;
    settings.doMain = ui->updateMain->isChecked();
    settings.doVegetation = ui->updateVeg->isChecked();
    settings.doBuildings = ui->buildingGroupBox->isChecked();
    if (settings != mWorldDoc->world()->getTMXToBMPSettings())
        mWorldDoc->changeTMXToBMPSettings(settings);
}
