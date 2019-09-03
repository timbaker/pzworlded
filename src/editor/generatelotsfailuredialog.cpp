#include "generatelotsfailuredialog.h"
#include "ui_generatelotsfailuredialog.h"

GenerateLotsFailureDialog::GenerateLotsFailureDialog(const QStringList &errorList, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GenerateLotsFailureDialog)
{
    ui->setupUi(this);
    ui->list->addItems(errorList);
}

GenerateLotsFailureDialog::~GenerateLotsFailureDialog()
{
    delete ui;
}
