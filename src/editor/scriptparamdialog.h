#ifndef SCRIPTPARAMDIALOG_H
#define SCRIPTPARAMDIALOG_H

#include <QDialog>

#include "global.h"

namespace Ui {
class ScriptParametersDialog;
}

class ScriptParametersDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit ScriptParametersDialog(const ScriptList &scripts, QWidget *parent = 0);
    ~ScriptParametersDialog();
    
private:
    Ui::ScriptParametersDialog *ui;
};

#endif // SCRIPTPARAMDIALOG_H
