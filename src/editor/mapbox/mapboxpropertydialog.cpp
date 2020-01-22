#include "mapboxpropertydialog.h"
#include "ui_mapboxpropertydialog.h"

#include "worldcellmapbox.h"

MapboxPropertyDialog::MapboxPropertyDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MapboxPropertyDialog)
{
    ui->setupUi(this);
}

MapboxPropertyDialog::~MapboxPropertyDialog()
{
    delete ui;
}

void MapboxPropertyDialog::setProperty(const QString& key, const QString& value)
{
    ui->keyEdit->setText(key);
    ui->valueEdit->setText(value);
}

QString MapboxPropertyDialog::getKey()
{
    return ui->keyEdit->text();
}

QString MapboxPropertyDialog::getValue()
{
    return ui->valueEdit->text();
}
