#include "scriptparamdialog.h"
#include "ui_scriptparamdialog.h"

#include "scriptparammodel.h"

ScriptParametersDialog::ScriptParametersDialog(const ScriptList &scripts, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ScriptParametersDialog)
{
    ui->setupUi(this);

    ScriptParamModel *m = new ScriptParamModel(this);
    m->setScripts(scripts);
    ui->paramView->setModel(m);
}

ScriptParametersDialog::~ScriptParametersDialog()
{
    delete ui;
}
