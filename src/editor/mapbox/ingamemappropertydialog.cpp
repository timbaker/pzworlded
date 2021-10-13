#include "ingamemappropertydialog.h"
#include "ui_ingamemappropertydialog.h"

#include "ingamemapcell.h"

InGameMapPropertyDialog::InGameMapPropertyDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::InGameMapPropertyDialog)
{
    ui->setupUi(this);
}

InGameMapPropertyDialog::~InGameMapPropertyDialog()
{
    delete ui;
}

void InGameMapPropertyDialog::setProperty(const QString& key, const QString& value)
{
    ui->keyEdit->setText(key);
    ui->valueEdit->setText(value);
}

QString InGameMapPropertyDialog::getKey()
{
    return ui->keyEdit->text();
}

QString InGameMapPropertyDialog::getValue()
{
    return ui->valueEdit->text();
}
