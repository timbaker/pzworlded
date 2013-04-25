#ifndef UNKNOWNCOLORSDIALOG_H
#define UNKNOWNCOLORSDIALOG_H

#include <QDialog>

namespace Ui {
class UnknownColorsDialog;
}

class UnknownColorsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit UnknownColorsDialog(const QString &path, const QStringList &colors, QWidget *parent = 0);
    ~UnknownColorsDialog();
    
private:
    Ui::UnknownColorsDialog *ui;
};

#endif // UNKNOWNCOLORSDIALOG_H
