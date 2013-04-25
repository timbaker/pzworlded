#include "unknowncolorsdialog.h"
#include "ui_unknowncolorsdialog.h"

#include <QFileInfo>

UnknownColorsDialog::UnknownColorsDialog(const QString &path,
                                         const QStringList &colors,
                                         QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UnknownColorsDialog)
{
    ui->setupUi(this);

    ui->prompt->setText(tr("Some unknown colors were found in %1:\n") .arg(path));

    ui->list->addItems(colors);
}

UnknownColorsDialog::~UnknownColorsDialog()
{
    delete ui;
}
