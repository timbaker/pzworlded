#ifndef INGAMEMAP_PROPERTYDIALOG_H
#define INGAMEMAP_PROPERTYDIALOG_H

#include <QDialog>

namespace Ui {
class InGameMapPropertyDialog;
}

class InGameMapFeature;

class InGameMapPropertyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InGameMapPropertyDialog(QWidget *parent = nullptr);
    ~InGameMapPropertyDialog();

    void setProperty(const QString& key, const QString& value);

    QString getKey();
    QString getValue();

private:
    Ui::InGameMapPropertyDialog *ui;
};

#endif // INGAMEMAP_PROPERTYDIALOG_H
