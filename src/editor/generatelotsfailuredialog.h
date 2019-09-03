#ifndef GENERATELOTSFAILUREDIALOG_H
#define GENERATELOTSFAILUREDIALOG_H

#include <QDialog>

namespace Ui {
class GenerateLotsFailureDialog;
}

class GenerateLotsFailureDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GenerateLotsFailureDialog(const QStringList &errorList, QWidget *parent = nullptr);
    ~GenerateLotsFailureDialog();

private:
    Ui::GenerateLotsFailureDialog *ui;
};

#endif // GENERATELOTSFAILUREDIALOG_H
