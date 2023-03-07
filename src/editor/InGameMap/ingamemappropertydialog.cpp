#include "ingamemappropertydialog.h"
#include "ui_ingamemappropertydialog.h"

#include "celldocument.h"
#include "documentmanager.h"
#include "document.h"
#include "world.h"
#include "worlddocument.h"

#include <QCompleter>

InGameMapPropertyDialog::InGameMapPropertyDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::InGameMapPropertyDialog)
{
    ui->setupUi(this);
    QSet<QString> keySet, valueSet;
    Document *doc = DocumentManager::instance()->currentDocument();
    WorldDocument *worldDoc = doc->asWorldDocument();
    CellDocument *cellDoc = doc->asCellDocument();
    World *world = worldDoc ? worldDoc->world() : cellDoc->world();
    for (auto* cell : world->cells()) {
        const InGameMapFeatures &features = cell->inGameMap().features();
        for (const auto& feature : features) {
            for (const auto& prop : qAsConst(feature->properties())) {
                keySet += prop.mKey;
                valueSet += prop.mValue;
            }
        }
    }
    ui->keyEdit->setCompleter(new QCompleter(QStringList(keySet.begin(), keySet.end()), this));
    ui->valueEdit->setCompleter(new QCompleter(QStringList(valueSet.begin(), valueSet.end()), this));
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
